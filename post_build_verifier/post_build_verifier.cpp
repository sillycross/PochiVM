// Verify that all the symbols used in bitcode exists in the executable and is public.
// This makes sure we will never hit undefined symbol issue at runtime.
// Although we should have locked down if any symbol has internal linkage,
// it's good to do a double-check here to make sure nothing unexpected happened
// during the build and linking process.
//

#include "pochivm/common.h"
#include "generated/pochivm_runtime_library_bitcodes.generated.h"

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
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Linker/Linker.h"

using namespace llvm;

static void FindAllSymbols(const PochiVM::BitcodeData* bitcode,
                           std::map<std::string, std::string>& allSymbols /*out*/)
{
    // find all symbols which is not a definition.
    // these are symbols that might potentially be resolved to the host process.
    //
    SMDiagnostic llvmErr;
    MemoryBufferRef mb(StringRef(reinterpret_cast<const char*>(bitcode->m_bitcode), bitcode->m_length),
                       StringRef(bitcode->m_symbolName));
    std::unique_ptr<LLVMContext> context(new LLVMContext);
    std::unique_ptr<Module> module = parseIR(mb, llvmErr, *context.get());
    if (module == nullptr)
    {
        fprintf(stderr, "[INTERNAL ERROR] Bitcode stub for symbol %s cannot be parsed. Please report a bug.\n",
                bitcode->m_symbolName);
        abort();
    }

    for (GlobalVariable& gv : module->globals())
    {
        if (gv.isDeclaration())
        {
            ReleaseAssert(gv.hasExternalLinkage());
            std::string symbol = gv.getGlobalIdentifier();
            allSymbols[symbol] = bitcode->m_symbolName;
        }
        else
        {
            // All global variables which is a definition in bitcode stub must be a constant.
            //
            ReleaseAssert(gv.isConstant());
        }
    }

    bool found = false;
    for (Function& fn : module->functions())
    {
        std::string fnName = fn.getName().str();
        if (fnName != std::string(bitcode->m_symbolName))
        {
            if (x_isDebugBuild)
            {
                ReleaseAssert((fn.empty() && fn.hasExternalLinkage()) ||
                              (!fn.empty() && fn.hasAvailableExternallyLinkage()));
            }
            else
            {
                ReleaseAssert(fn.empty() && fn.hasExternalLinkage());
            }
        }
        else
        {
            ReleaseAssert(!found);
            found = true;
            ReleaseAssert(!fn.empty());
            ReleaseAssert(fn.hasExternalLinkage());
        }
        // Ignore LLVM intrinsics, those are not symbols and will go away after compilation
        //
        if (fn.getIntrinsicID() == Intrinsic::not_intrinsic)
        {
            allSymbols[fnName] = bitcode->m_symbolName;
        }
    }
    ReleaseAssert(found);
}

int main(int argc, char** argv)
{
    // [main executable bin path]
    //
    ReleaseAssert(argc == 2);
    std::string binPath = argv[1];

    std::map<std::string, std::string> allNeededSymbols;
    size_t n = std::extent<decltype(PochiVM::x_all_bitcode_data)>::value;
    for (size_t i = 0; i < n; i++)
    {
        FindAllSymbols(PochiVM::x_all_bitcode_data[i], allNeededSymbols /*out*/);
    }

    std::string outputFile = binPath + ".all_nm_symbols.txt";
    std::string cmd = std::string("nm --extern-only ") + binPath + " > " + outputFile;
    int r = system(cmd.c_str());
    if (r != 0)
    {
        fprintf(stderr, "[ERROR] Command %s failed with return code %d.\n",
                cmd.c_str(), r);
        abort();
    }

    std::map<std::string /*symbol*/, char /*type*/> binSymbols;
    {
        FILE* fp = fopen(outputFile.c_str(), "r");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for read, errno = %d (%s)\n",
                    outputFile.c_str(), errno, strerror(errno));
            abort();
        }

        char* buf = nullptr;
        size_t line_buf_size = 0;
        while (true)
        {
            ssize_t line_size = getline(&buf, &line_buf_size, fp);
            if (line_size < 0)
            {
                break;
            }

            if (buf[line_size - 1] == '\n')
            {
                buf[line_size - 1] = '\0';
                line_size--;
            }

            std::string line(buf, static_cast<size_t>(line_size));

            // Each line looks like this:
            // 0000000000c4bd60 t __cxx_global_var_init
            //
            ReleaseAssert(line.length() >= 20);
            // First column should either be 16 spaces or a 16-digit hex value
            //
            bool isFirstColumnSpace = true;
            for (size_t i = 0; i < 16; i++)
            {
                isFirstColumnSpace = isFirstColumnSpace && (line[i] == ' ');
            }
            if (!isFirstColumnSpace)
            {
                for (size_t i = 0; i < 16; i++)
                {
                    ReleaseAssert(('0' <= line[i] && line[i] <= '9') ||
                                  ('a' <= line[i] && line[i] <= 'f'));
                }
            }
            // Second column is the type, and everything else is the symbol
            //
            ReleaseAssert(line[16] == ' ' && line[18] == ' ');
            char symbolType = line[17];
            std::string symbol = line.substr(19);
            std::string symbolBak = symbol;

            // For undefined symbol, strip the '@@library' part
            //
            if (symbolType == 'U')
            {
                size_t pos = symbol.find("@@");
                if (pos == 0 || pos == std::string::npos)
                {
                    fprintf(stderr, "[INTERNAL ERROR] Unexpected nm output: undefined symbol %s does "
                                    "not have the expectedformat of 'symbolname@@library'.",
                            symbol.c_str());
                    abort();
                }
                std::string library = symbol.substr(pos + 2);
                ReleaseAssert(library.find("@@") == std::string::npos);
                symbol = symbol.substr(0, pos);
            }
            if (binSymbols.count(symbol))
            {
                fprintf(stderr, "[INTERNAL ERROR] Unexpected nm output: symbol '%s' showed up "
                                "multiple times in nm output.",
                        symbolBak.c_str());
                abort();
            }
            binSymbols[symbol] = symbolType;
        }
        free(buf);

        fclose(fp);
    }

    for (std::pair<std::string, std::string> symbol : allNeededSymbols)
    {
        if (!binSymbols.count(symbol.first))
        {
            fprintf(stderr, "[INTERNAL ERROR] Symbol '%s' is required by bitcode stub '%s', "
                            "but does not present in the binary executable, "
                            "or is not a global (aka external) symbol. Please report a bug.\n",
                    symbol.first.c_str(), symbol.second.c_str());
            abort();
        }
    }

    return 0;
}
