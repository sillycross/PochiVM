#include "pochivm/common.h"
#include "runtime_lib_builder/symbol_list_util.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/User.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/Transforms/Utils/Debugify.h"

using namespace llvm;
using namespace llvm::orc;

static void ExtractFunction(const std::string& generatedFileDir,
                            const std::string& filenameBase,
                            const std::string& functionName)
{
    // important to extract symbol from optimized bc file!
    //
    std::string bcFileName = filenameBase + ".optimized.bc";

    SMDiagnostic llvmErr;
    std::unique_ptr<LLVMContext> context(new LLVMContext);
    std::unique_ptr<Module> module = parseIRFile(bcFileName, llvmErr, *context.get());

    if (module == nullptr)
    {
        fprintf(stderr, "[ERROR] An error occurred while parsing IR file '%s', detail:\n", bcFileName.c_str());
        llvmErr.print("update_symbol_matches", errs());
        abort();
    }

    std::map< Constant*, std::set<GlobalValue*> > constExprUsageGraph;
    std::map< Operator*, std::set<GlobalValue*> > operatorUsageGraph;

    std::function<void(Value*, std::set<GlobalValue*>&)> computeDependencies =
            [&constExprUsageGraph, &operatorUsageGraph, &computeDependencies](
                    Value *value, std::set<GlobalValue*>& allUsers /*out*/)
    {
        if (isa<Instruction>(value))
        {
            Instruction* inst = dyn_cast<Instruction>(value);
            Function* func = inst->getParent()->getParent();
            allUsers.insert(func);
        }
        else if (isa<GlobalValue>(value))
        {
            GlobalValue* gv = dyn_cast<GlobalValue>(value);
            allUsers.insert(gv);
        }
        else if (isa<Constant>(value))
        {
            Constant* cst = dyn_cast<Constant>(value);
            if (!constExprUsageGraph.count(cst))
            {
                // memorized search is needed to avoid exponential time
                //
                std::set<GlobalValue*>& cstUsers = constExprUsageGraph[cst];
                for (User* cstUser : cst->users())
                {
                    computeDependencies(cstUser, cstUsers /*out*/);
                }
            }
            std::set<GlobalValue*>& users = constExprUsageGraph[cst];
            allUsers.insert(users.begin(), users.end());
        }
        else if (isa<Operator>(value))
        {
            Operator* op = dyn_cast<Operator>(value);
            if (!operatorUsageGraph.count(op))
            {
                // memorized search is needed to avoid exponential time
                //
                std::set<GlobalValue*>& opUsers = operatorUsageGraph[op];
                for (User* opUser : op->users())
                {
                    computeDependencies(opUser, opUsers /*out*/);
                }
            }
            std::set<GlobalValue*>& users = operatorUsageGraph[op];
            allUsers.insert(users.begin(), users.end());
        }
        else
        {
            ReleaseAssert(false && "unhandled type");
        }
    };

    std::map< GlobalValue*, std::set<GlobalValue*> > usageGraph;

    std::function<void(GlobalValue*)> addUserEdgesOfGlobalValue =
            [&computeDependencies, &usageGraph](GlobalValue* gv)
    {
        std::set<GlobalValue*> allUsers;
        for (User *user : gv->users())
        {
            computeDependencies(user, allUsers);
        }
        for (GlobalValue* user : allUsers)
        {
            usageGraph[user].insert(gv);
        }
    };

    for (GlobalValue& gv : module->global_values())
    {
        addUserEdgesOfGlobalValue(&gv);
    }

    Function* functionTarget = module->getFunction(functionName);
    if (functionTarget == nullptr)
    {
        fprintf(stderr, "[ERROR] Module '%s' does not contain function '%s'.\n",
                bcFileName.c_str(), functionName.c_str());
        abort();
    }
    // We have already excluded declarations in the symbol list, no reason it can be empty here
    //
    ReleaseAssert(!functionTarget->empty());

    // Currently we don't have a 'always inline' option, the inlining option is just a hint.
    // So lockdown functions with internal linkage: otherwise if LLVM choose not to inline it
    // the generated code will not be able to call the function in host process address space.
    //
    if (functionTarget->getLinkage() == GlobalValue::LinkageTypes::PrivateLinkage ||
        functionTarget->getLinkage() == GlobalValue::LinkageTypes::InternalLinkage)
    {
        // This should not really happen here? We have excluded internal linkage symbols in symbol list
        // but anyway.. no adverse effect to check it here again
        //
        fprintf(stderr, "[ERROR] Function '%s' in module '%s' has internal linkage type. "
                        "To include it in the runtime library, you must change its linkage "
                        "type to external (by removing the 'static' keyword etc).\n",
                functionTarget->getName().str().c_str(),
                bcFileName.c_str());
        abort();
    }

    std::set<GlobalValue*> isNeeded;
    std::set<Function*> callees;
    std::function<void(GlobalValue*, bool)> dfs =
            [&dfs, &isNeeded, &callees, &usageGraph](GlobalValue* cur, bool isRoot)
    {
        if (isNeeded.count(cur))
        {
            return;
        }
        isNeeded.insert(cur);
        if (isa<Function>(cur) && !isRoot)
        {
            // Stop at function. The function will be turned into an extern.
            //
            Function* callee = dyn_cast<Function>(cur);
            callees.insert(callee);
            return;
        }
        if (usageGraph.count(cur))
        {
            for (GlobalValue* dependent: usageGraph[cur])
            {
                dfs(dependent, false /*isRoot*/);
            }
        }
    };
    dfs(functionTarget, true /*isRoot*/);

    for (Function* callee : callees)
    {
        // If the callee has non-public linkage, the runtime library will not be
        // able to find the definition at runtime! Lock it down now and ask the
        // user to remove the 'static' keyword.
        // We cannot fix the problem by somehow changing the linkage type by ourselves,
        // as that could cause name collisions.
        //
        if (callee->getLinkage() == GlobalValue::LinkageTypes::PrivateLinkage ||
            callee->getLinkage() == GlobalValue::LinkageTypes::InternalLinkage)
        {
            fprintf(stderr, "[ERROR] Function '%s' in module '%s' called function '%s', which "
                            "has internal linkage type. To include the function in runtime libary, "
                            "you have to either (1) make callee '%s' have external linkage type"
                            "(by removing the 'static' keyword etc) "
                            "or (2) make caller '%s' non-inlinable by generated code by specifying "
                            "the option in pochivm_register_runtime.cpp (not the C++ declaration).\n",
                    functionTarget->getName().str().c_str(),
                    bcFileName.c_str(),
                    callee->getName().str().c_str(),
                    callee->getName().str().c_str(),
                    functionTarget->getName().str().c_str());
            abort();
        }
    }

    // Record all the global value identifiers for now,
    // the module and context will be modified in the next step
    // We later use these information to verify that the generated module matches our expectation
    // (that it keeps and only keeps the globals we expected).
    //
    std::vector<std::string> allGlobalValueIds;
    std::set<std::string> neededGlobalValueIds;
    for (GlobalValue& gv : module->global_values())
    {
        allGlobalValueIds.push_back(gv.getGlobalIdentifier());
    }
    for (GlobalValue* gv : isNeeded)
    {
        std::string s = gv->getGlobalIdentifier();
        ReleaseAssert(!neededGlobalValueIds.count(s));
        neededGlobalValueIds.insert(s);
    }

    // The logic below basically follows llvm_extract.cpp.
    // We first do a ExtractGV pass, which does not actually remove stuff here,
    // but only delete the body and make them declarations instead of definitions.
    // Then we call dead code elimination pass to actually delete declarations (which
    // contains some tricky logic that we probably should not try to write by ourselves).
    //
    // Below is basically the ExtractGV pass logic, with modifications to fit our purpose.
    //

    // Copied directly from ExtractGV.cpp, no idea if it fits us completely...
    // "Visit the global inline asm." <-- what is that?
    //
    module->setModuleInlineAsm("");

    // Delete bodies of unneeded global vars
    //
    for (GlobalVariable& gv : module->globals())
    {
        if (!isNeeded.count(&gv))
        {
            // a deleted symbol becomes an external declaration,
            // and since it is unused, it will be dropped by dead code elimination
            //
            bool isLocalLinkage = gv.hasLocalLinkage();
            gv.setLinkage(GlobalValue::ExternalLinkage);
            if (isLocalLinkage)
            {
                gv.setVisibility(GlobalValue::HiddenVisibility);
            }

            gv.setInitializer(nullptr);
            gv.setComdat(nullptr);
        }
    }

    for (Function& fn : module->functions())
    {
        // We should not need to keep any function body other than our target
        //
        if (&fn != functionTarget)
        {
            ReleaseAssert(!isNeeded.count(&fn) || callees.count(&fn));
            fn.deleteBody();
            fn.setComdat(nullptr);
        }
        else
        {
            ReleaseAssert(isNeeded.count(&fn));
            fn.setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
        }
    }

    // Copied directly from ExtractGV.cpp, no idea if it fits us completely...
    //
    for (Module::alias_iterator I = module->alias_begin(), E = module->alias_end(); I != E;)
    {
        Module::alias_iterator CurI = I;
        ++I;

        if (!isNeeded.count(&*CurI))
        {
            Type *Ty =  CurI->getValueType();

            CurI->removeFromParent();
            Value* Declaration;
            if (FunctionType *FTy = dyn_cast<FunctionType>(Ty)) {
                Declaration = Function::Create(FTy, GlobalValue::ExternalLinkage,
                                               CurI->getAddressSpace(),
                                               CurI->getName(), module.get());

            } else {
                Declaration =
                        new GlobalVariable(*module.get(), Ty, false, GlobalValue::ExternalLinkage,
                                           nullptr, CurI->getName());

            }
            CurI->replaceAllUsesWith(Declaration);
            delete &*CurI;
        }
    }

    {
        // Copied from llvm_extract.cpp
        // Delete dead declarations
        // TODO: switch to new pass manager?
        //
        legacy::PassManager Passes;

        // Delete unreachable globals
        //
        Passes.add(createGlobalDCEPass());

        if (OPTION_KEEP_DEBUG_INFO)
        {
            // Only remove dead debug info
            // When KEEP_DEBUG_INFO is false, all debug info has been removed
            // at the time we optimize the IR file.
            //
            Passes.add(createStripDeadDebugInfoPass());
        }

        // Remove dead func decls
        //
        Passes.add(createStripDeadPrototypesPass());

        Passes.run(*module.get());
    }

    // Finally, change the linkage type of our target function to 'available externally'
    //
    functionTarget = module->getFunction(functionName);
    ReleaseAssert(functionTarget != nullptr);
    ReleaseAssert(!functionTarget->empty());
    functionTarget->setLinkage(GlobalValue::LinkageTypes::AvailableExternallyLinkage);
    // AvailableExternallyLinkage is not allowed to have comdat.
    // It should be fine that we simply drop the comdat, since C++ always follows ODR rule.
    //
    functionTarget->setComdat(nullptr);

    // Output extracted bitcode file
    //
    std::string uniqueSymbolHash = GetUniqueSymbolHash(functionName);
    std::string outputFileName = std::string("extracted.") + uniqueSymbolHash + ".bc";
    {
        int fd = creat(outputFileName.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (fd == -1)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    outputFileName.c_str(), errno, strerror(errno));
            abort();
        }
        raw_fd_ostream fdStream(fd, true /*shouldClose*/);
        WriteBitcodeToFile(*module.get(), fdStream);
        if (fdStream.has_error())
        {
            std::error_code ec = fdStream.error();
            fprintf(stderr, "Writing to file '%s' failed with errno = %d (%s)\n",
                    outputFileName.c_str(), ec.value(), ec.message().c_str());
            abort();
        }
        /* fd closed when fdStream is destructed here */
    }

    // Make sure that the generated IR file does not contain errors,
    // and that set of 'needed' symbols that we determined is exactly the set
    // of symbols kept by the dead code elimination pass
    //
    {
        // Load the extracted bitcode file, replace the current context and module
        //
        std::unique_ptr<LLVMContext> newContext(new LLVMContext);
        std::unique_ptr<Module> newModule = parseIRFile(outputFileName, llvmErr, *newContext.get());

        if (newModule == nullptr)
        {
            fprintf(stderr, "[INTERNAL ERROR] Generated IR file '%s' contains error. "
                            "Please report a bug. Detail:\n", outputFileName.c_str());
            llvmErr.print("update_symbol_matches", errs());
            abort();
        }
        module = std::move(newModule);
        context = std::move(newContext);
    }

    std::set<std::string> allGlobalIdsInNewModule;
    for (GlobalValue& gv : module->global_values())
    {
        std::string s = gv.getGlobalIdentifier();
        ReleaseAssert(!allGlobalIdsInNewModule.count(s));
        allGlobalIdsInNewModule.insert(s);
    }

    for (const std::string& gid : allGlobalValueIds)
    {
        bool expectExist = (neededGlobalValueIds.count(gid) > 0);
        bool exist = (allGlobalIdsInNewModule.count(gid) > 0);
        if (expectExist != exist)
        {
            fprintf(stderr, "[INTERNAL ERROR] We expected generated IR file '%s' to%s contain "
                            "global '%s', but in reality it does%s. Please report a bug.\n",
                    outputFileName.c_str(), (!expectExist ? " not" : ""), gid.c_str(), (exist ? "" : " not"));
            abort();
        }
    }

    // These two checks should not really fail.. but just be paranoid
    //
    for (const std::string& gid : neededGlobalValueIds)
    {
        if (!allGlobalIdsInNewModule.count(gid))
        {
            fprintf(stderr, "[INTERNAL ERROR] We expected generated IR file '%s' to contain "
                            "global '%s', but in reality it does not [check2]. Please report a bug.\n",
                    outputFileName.c_str(), gid.c_str());
            abort();
        }
    }

    for (const std::string& gid : allGlobalIdsInNewModule)
    {
        if (!neededGlobalValueIds.count(gid))
        {
            fprintf(stderr, "[INTERNAL ERROR] We expected generated IR file '%s' to not contain "
                            "global '%s', but in reality it does [check3]. Please report a bug.\n",
                    outputFileName.c_str(), gid.c_str());
            abort();
        }
    }

    // Generate data file in header file format
    // build_runtime_lib will generate the CPP file that include these headers
    //
    size_t bitcodeSize;
    uint8_t* bitcodeRawData = nullptr;
    {
        struct stat st;
        if (stat(outputFileName.c_str(), &st) != 0)
        {
            fprintf(stderr, "Failed to access file '%s' for file size, errno = %d (%s)\n",
                    outputFileName.c_str(), errno, strerror(errno));
            abort();
        }
        bitcodeSize = static_cast<size_t>(st.st_size);

        // one more char for the trailing '\0'
        //
        bitcodeRawData = new uint8_t[bitcodeSize + 1];

        FILE* fp = fopen(outputFileName.c_str(), "r");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for read, errno = %d (%s)\n",
                    outputFileName.c_str(), errno, strerror(errno));
            abort();
        }

        size_t bytesRead = fread(bitcodeRawData, sizeof(uint8_t), bitcodeSize, fp);
        ReleaseAssert(bytesRead == bitcodeSize);
        bitcodeRawData[bitcodeSize] = 0;

        {
            // just to sanity check we have reached eof
            //
            uint8_t _c;
            ReleaseAssert(fread(&_c, sizeof(uint8_t), 1 /*numElements*/, fp) == 0);
            ReleaseAssert(feof(fp));
        }

        fclose(fp);
    }

    ReleaseAssert(bitcodeRawData != nullptr);
    Auto(delete [] bitcodeRawData);

    {
        std::string headerFileOutput = generatedFileDir + "/bc." + uniqueSymbolHash + ".data.h";
        FILE* fp = fopen(headerFileOutput.c_str(), "w");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    headerFileOutput.c_str(), errno, strerror(errno));
            abort();
        }

        fprintf(fp, "// GENERATED FILE, DO NOT EDIT!\n");
        fprintf(fp, "// INTERNAL FILE, DO NOT INCLUDE TO YOUR CPP!\n");
        fprintf(fp, "//\n\n");

        fprintf(fp, "#include \"pochivm/bitcode_data.h\"\n\n");

        fprintf(fp, "namespace PochiVM {\n\n");

        std::string resVarname = std::string("__pochivm_internal_bc_") + uniqueSymbolHash;

        // 'extern' declaration is required for constants to be able to export elsewhere
        //
        fprintf(fp, "extern const BitcodeData %s;\n\n", resVarname.c_str());

        std::string bitcodeDataVarname = resVarname + "_bitcode_data";
        fprintf(fp, "const uint8_t %s[%d] = {\n    ", bitcodeDataVarname.c_str(), static_cast<int>(bitcodeSize + 1));

        for (size_t i = 0; i <= bitcodeSize; i++)
        {
            uint8_t value = bitcodeRawData[i];
            fprintf(fp, "%d", value);
            if (i != bitcodeSize)
            {
                fprintf(fp, ", ");
                if (i % 16 == 15)
                {
                    fprintf(fp, "\n    ");
                }
            }
        }
        fprintf(fp, "\n};\n\n");

        std::string symbolVarname = resVarname + "_symbol";
        fprintf(fp, "const char* const %s = \"%s\";\n\n",
                symbolVarname.c_str(), functionName.c_str());

        fprintf(fp, "const BitcodeData %s = {\n", resVarname.c_str());
        fprintf(fp, "    %s,\n", symbolVarname.c_str());
        fprintf(fp, "    %s,\n", bitcodeDataVarname.c_str());
        fprintf(fp, "    %d\n};\n", static_cast<int>(bitcodeSize));

        fprintf(fp, "\n}  // namespace PochiVM\n\n");

        fclose(fp);
    }
}

int main(int argc, char** argv)
{
    InitLLVM _init_llvm_(argc, argv);

    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    // [all_needed_symbols] [filename] [generated_file_dir]
    //    Match filename.syms with all_needed_symbols, append results to filename.syms.matches,
    //    and generate the extracted bc file (extracted.[symbol hash].bc) for each symbol
    //

    ReleaseAssert(argc == 4);
    std::string allNeededSymbolsFile = argv[1];
    std::string filenameBase = argv[2];
    std::string generatedFileDir = argv[3];
    std::string symList = filenameBase + ".syms";
    std::string symListMatches = filenameBase + ".syms.matches";

    std::set<std::string> allNeededSymbols = ReadSymbolListFileOrDie(allNeededSymbolsFile);
    std::set<std::string> allSymbols = ReadSymbolListFileOrDie(symList);
    std::set<std::string> allSymbolMatches = ReadSymbolListFileOrDie(symListMatches);

    for (const std::string& symbol : allNeededSymbols)
    {
        if (allSymbols.count(symbol))
        {
            // The caller should always have purged .syms.matches from the needed symbols
            //
            ReleaseAssert(!allSymbolMatches.count(symbol));
            allSymbolMatches.insert(symbol);
            ExtractFunction(generatedFileDir, filenameBase, symbol);
        }
    }

    std::string tmpFile = symListMatches + ".tmp";
    WriteSymbolListFileOrDie(tmpFile, allSymbolMatches);

    // Give all-or-nothing guarantee by rename
    //
    int r = rename(tmpFile.c_str(), symListMatches.c_str());
    ReleaseAssert(r == 0 || r == -1);
    if (r == -1)
    {
        fprintf(stderr, "Failed to rename file '%s' into '%s', errno = %d (%s)\n",
                tmpFile.c_str(), symListMatches.c_str(), errno, strerror(errno));
        abort();
    }

    return 0;
}
