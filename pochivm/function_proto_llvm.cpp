#include "function_proto.h"
#include "destructor_helper.h"
#include "exception_helper.h"
#include "ast_catch_throw.h"
#include "llvm_ast_helper.hpp"

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Linker/Linker.h"
#include "llvm/IRReader/IRReader.h"

namespace PochiVM
{

using namespace llvm;
using namespace llvm::orc;

void AstFunction::EmitDefinition()
{
    TestAssert(m_generatedPrototype == nullptr);
    Type** args = reinterpret_cast<Type**>(alloca(sizeof(Type*) * m_params.size()));
    {
        size_t index = 0;
        for (AstVariable* arg : m_params)
        {
            args[index] = AstTypeHelper::llvm_type_of(arg->GetTypeId().RemovePointer());
            index++;
        }
    }
    Type* returnType = AstTypeHelper::llvm_type_of(m_returnType);
    FunctionType* funcType = FunctionType::get(returnType,
                                               ArrayRef<Type*>(args, args + m_params.size()),
                                               false /*isVariadic*/);
    m_generatedPrototype = Function::Create(
                funcType, Function::ExternalLinkage, m_name, thread_llvmContext->m_module);

    // LLVM craziness: bool is i1 in LLVM, but i8 in C++
    // When a function returns a bool, LLVM is free to fill whatever it wants into the first 7 bits
    // of the bool (and this actually happens), but C++ would not expect this, since the standard says the only
    // legal binary representations of a bool are 0 and 1. This actually results in random bugs in -O3.
    // Set the return value attribute to tell LLVM that it must ZeroExt the return value.
    //
    if (m_returnType.IsBool())
    {
        m_generatedPrototype->addAttribute(AttributeList::AttrIndex::ReturnIndex, Attribute::AttrKind::ZExt);
    }

    m_generatedPrototype->setPersonalityFn(thread_llvmContext->m_personalityFn);

    // Set parameter names
    //
    {
        size_t index = 0;
        for (auto &arg : m_generatedPrototype->args())
        {
            // For clarity purpose, differentiate the name with the LValue variables
            // (which has same name), so LLVM dump won't add suffix
            //
            // Twine must be constructed in the same expression,
            // otherwise we hit a use-after-free undefined behavior... such a dangerous construct..
            //
            arg.setName(Twine("_")
                        .concat(m_params[index]->GetVarNameNoSuffix())
                        .concat("_")
                        .concat(Twine(m_params[index]->GetVarSuffix())));
            index++;
        }
    }

    if (GetIsNoExcept())
    {
        m_generatedPrototype->addFnAttr(Attribute::AttrKind::NoUnwind);
    }
}

void AstFunction::EmitIR()
{
    // Set current function in context, some AST codegen relies on this.
    //
    TestAssert(thread_llvmContext->m_curFunction == nullptr && m_generatedPrototype != nullptr);
    thread_llvmContext->m_curFunction = this;
    thread_llvmContext->m_isCursorAtDummyBlock = false;
    thread_llvmContext->m_llvmLandingPadBlockOrdinal = 0;
    AutoSetScopedVarManagerOperationMode assvm(ScopedVariableManager::OperationMode::LLVM);

    // Generated code structure:
    //
    // entry:
    //   alloc space to store return value
    //   alloc space for each param
    //   alloc space for each variable
    // body:
    //   store each param RValue into param address
    //   ... codegen main body ...
    // footer:
    //   return the value stored in ret
    //
    // "return" stmt stores return value to allocated address,
    // calls each destructor (like break/continue) and branch to "footer"
    //
    m_llvmEntryBlock = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                          "entry", m_generatedPrototype);

    BasicBlock* body = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                          "body", m_generatedPrototype);


    // Build the entry block: create alloca instruction for return value and each parameter
    // Alloca instructions for local variables will be created as they are later encountered
    //
    thread_llvmContext->m_builder->SetInsertPoint(m_llvmEntryBlock);

    // Build header block: store the parameters of this function (which are RValues)
    // into the space we allocated for parameters, so we can use them as LValues later
    //
    thread_llvmContext->m_builder->SetInsertPoint(body);
    {
        size_t index = 0;
        for (auto &arg : m_generatedPrototype->args())
        {
            Value* dst = m_params[index]->EmitIR();
            AstTypeHelper::create_store_helper(m_params[index]->GetTypeId().RemovePointer(), &arg, dst);
            index++;
        }
    }

    // Return insert pointer to end of body, and build the function body
    //
    thread_llvmContext->m_builder->SetInsertPoint(body);
    std::ignore = m_body->EmitIR();

    // If the current insert point is not the dummy block, build return statement
    //
    if (!thread_llvmContext->m_isCursorAtDummyBlock)
    {
        if (!m_returnType.IsVoid())
        {
            // This is really bad. Execution reached end-of-function for a function
            // with non-void return value. We just return 0 (forcefully cast to whatever retType).
            // TODO: call abort() in testbuild
            //
            unsigned numBitsRetType = static_cast<unsigned>(m_returnType.Size()) * 8;
            Value* zeroOfSameWidth = ConstantInt::get(*thread_llvmContext->m_llvmContext,
                                                      APInt(numBitsRetType /*numBits*/,
                                                            0 /*value*/,
                                                            false /*isSigned*/));
            Value* retVal = thread_llvmContext->m_builder->CreateBitCast(
                    zeroOfSameWidth, AstTypeHelper::llvm_type_of(m_returnType));
            thread_llvmContext->m_builder->CreateRet(retVal);
        }
        else
        {
            thread_llvmContext->m_builder->CreateRetVoid();
        }
    }

    // Entry block fallthroughs to body block
    //
    thread_llvmContext->m_builder->SetInsertPoint(m_llvmEntryBlock);
    thread_llvmContext->m_builder->CreateBr(body);

    // Nothing should have been inserted into m_dummyBlock
    //
    TestAssert(thread_llvmContext->m_dummyBlock->empty());

    // In test build, validate that the function contains no errors.
    // llvm::verifyFunction returns false on success
    //
    TestAssert(llvm::verifyFunction(*m_generatedPrototype, &outs()) == false);

    thread_pochiVMContext->m_scopedVariableManager.AssertInCleanState();

    // Reset curFunction back to nullptr
    //
    thread_llvmContext->m_curFunction = nullptr;
}

void AstModule::EmitIR()
{
    // In test build, user should always validate module before emitting IR
    //
    TestAssert(!m_irEmitted && m_validated);
#ifdef TESTBUILD
    m_irEmitted = true;
#endif

    AstTraverseColorMark::ClearAll();

    // Setup the LLVM codegen context
    // TODO: OOM error handling
    //
    TestAssert(m_llvmContext == nullptr && m_llvmModule == nullptr);
    // Must use std::new. We will later transfer ownership to LLVM using std::unique_ptr. Same for m_llvmModule
    //
    m_llvmContext = new llvm::LLVMContext();

    llvm::IRBuilder<>* llvmIrBuilder = new llvm::IRBuilder<>(*m_llvmContext);
    Auto(TestAssert(m_llvmContext != nullptr); delete llvmIrBuilder);

    m_llvmModule = new Module(m_moduleName, *m_llvmContext);

    thread_llvmContext->SetupModule(m_llvmContext, llvmIrBuilder, m_llvmModule);

    // TODO: does this leak memory?
    //
    thread_llvmContext->m_dummyBlock = BasicBlock::Create(*thread_llvmContext->m_llvmContext, "dummy");

    // Declare the __gxx_personality_v0 function for exception handling
    //     declare dso_local i32 @__gxx_personality_v0(...)
    //
    {
        Type* returnType = AstTypeHelper::llvm_type_of(TypeId::Get<int32_t>());
        FunctionType* funcType = FunctionType::get(returnType,
                                                   ArrayRef<Type*>(),
                                                   true /*isVariadic*/);
        Function* personalityFn = Function::Create(
                    funcType, Function::ExternalLinkage, "__gxx_personality_v0", m_llvmModule);
        personalityFn->setDSOLocal(true);
        thread_llvmContext->m_personalityFn = personalityFn;
    }

    // First pass: find all call-cpp-function expressions,
    // link in the bitcodes containing the implementation and necessary types
    //
    {
        auto getIrModuleFromBitcodeData = [&](const BitcodeData* bitcode) -> std::unique_ptr<Module>
        {
            TestAssert(bitcode != nullptr);
            SMDiagnostic llvmErr;
            MemoryBufferRef mb(StringRef(reinterpret_cast<const char*>(bitcode->m_bitcode), bitcode->m_length),
                               StringRef(bitcode->m_symbolName));
            std::unique_ptr<Module> bitcodeModule = parseIR(mb, llvmErr, *m_llvmContext);
            // TODO: handle error
            //
            ReleaseAssert(bitcodeModule != nullptr);
            return bitcodeModule;
        };
        // Import necessary function prototypes for exception handler
        //
        bool isCXXExceptionHandlerABISymbolsImported = false;
        auto importSymbolsForExceptionHandler = [&]()
        {
            if (!isCXXExceptionHandlerABISymbolsImported)
            {
                // Import the following symbols:
                //     declare dso_local i8* @__cxa_allocate_exception(i64) nounwind
                //     declare dso_local void @__cxa_free_exception(i8*) nounwind
                //     declare dso_local void @__cxa_throw(i8*, i8*, i8*)
                //     declare dso_local void @_ZSt9terminatev() noreturn nounwind
                //
                if (m_llvmModule->getFunction("__cxa_allocate_exception") == nullptr)
                {
                    constexpr size_t numParams = 1;
                    Type* returnType = AstTypeHelper::llvm_type_of(TypeId::Get<void*>());
                    Type* paramTypes[numParams] = { AstTypeHelper::llvm_type_of(TypeId::Get<uint64_t>()) };
                    FunctionType* funcType = FunctionType::get(returnType,
                                                               ArrayRef<Type*>(paramTypes, paramTypes + numParams),
                                                               false /*isVariadic*/);
                    Function* func = Function::Create(
                                funcType, Function::ExternalLinkage, "__cxa_allocate_exception", m_llvmModule);
                    func->setDSOLocal(true);
                    func->addFnAttr(Attribute::AttrKind::NoUnwind);
                }
                if (m_llvmModule->getFunction("__cxa_free_exception") == nullptr)
                {
                    constexpr size_t numParams = 1;
                    Type* returnType = AstTypeHelper::llvm_type_of(TypeId::Get<void>());
                    Type* paramTypes[numParams] = { AstTypeHelper::llvm_type_of(TypeId::Get<void*>()) };
                    FunctionType* funcType = FunctionType::get(returnType,
                                                               ArrayRef<Type*>(paramTypes, paramTypes + numParams),
                                                               false /*isVariadic*/);
                    Function* func = Function::Create(
                                funcType, Function::ExternalLinkage, "__cxa_free_exception", m_llvmModule);
                    func->setDSOLocal(true);
                    func->addFnAttr(Attribute::AttrKind::NoUnwind);
                }
                if (m_llvmModule->getFunction("__cxa_throw") == nullptr)
                {
                    constexpr size_t numParams = 3;
                    Type* returnType = AstTypeHelper::llvm_type_of(TypeId::Get<void>());
                    Type* paramTypes[numParams] = {
                        AstTypeHelper::llvm_type_of(TypeId::Get<void*>()),
                        AstTypeHelper::llvm_type_of(TypeId::Get<void*>()),
                        AstTypeHelper::llvm_type_of(TypeId::Get<void*>())
                    };
                    FunctionType* funcType = FunctionType::get(returnType,
                                                               ArrayRef<Type*>(paramTypes, paramTypes + numParams),
                                                               false /*isVariadic*/);
                    Function* func = Function::Create(
                                funcType, Function::ExternalLinkage, "__cxa_throw", m_llvmModule);
                    func->setDSOLocal(true);
                }
                isCXXExceptionHandlerABISymbolsImported = true;
            }
        };
        // Import the std::type_info symbol name for C++ type corresponding to typeId (needed to throw exception)
        // The C++ type must have been registered with RegisterExceptionObjectType in pochivm_register_runtime.cpp
        //
        std::unique_ptr<Module> stdTypeInfoSymbolsModule(nullptr);
        auto importStdTypeInfoSymbol = [&](TypeId typeId)
        {
            TestAssert(IsTypeRegisteredForThrownFromGeneratedCode(typeId));
            const char* symbolName = GetStdTypeInfoObjectSymbolName(typeId);
            TestAssert(symbolName != nullptr);
            {
                GlobalVariable* tmp = m_llvmModule->getGlobalVariable(symbolName);
                if (tmp != nullptr)
                {
                    TestAssert(tmp->getLinkage() == GlobalValue::LinkageTypes::ExternalLinkage);
                    TestAssert(!tmp->hasInitializer() && tmp->isConstant() && tmp->isDSOLocal());
                    return;
                }
            }
            TestAssert(m_llvmModule->getGlobalVariable(symbolName, true /*allowInternal*/) == nullptr);
            if (stdTypeInfoSymbolsModule == nullptr)
            {
                const BitcodeData* bitcode = &__pochivm_internal_bc_typeinfo_objects;
                std::unique_ptr<Module> bitcodeModule = getIrModuleFromBitcodeData(bitcode);
                stdTypeInfoSymbolsModule.swap(bitcodeModule);
            }
            // Insert the global variable into m_llvmModule.
            // It should be a declaration of constant, which has external linkage and dso_local.
            // We can directly use gv->getType() since stdTypeInfoModule and m_llvmModule share the same LLVMContext.
            //
            GlobalVariable* gv = stdTypeInfoSymbolsModule->getGlobalVariable(symbolName);
            TestAssert(gv != nullptr);
            Type* gvType = gv->getType()->getPointerElementType();
            Constant* newGvC = m_llvmModule->getOrInsertGlobal(symbolName, gvType);
            TestAssert(isa<GlobalVariable>(newGvC));
            GlobalVariable* newGv = dyn_cast<GlobalVariable>(newGvC);
            newGv->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
            newGv->setInitializer(nullptr);
            newGv->setDSOLocal(true);
            newGv->setConstant(true);
        };
        // Link in a CPP function for call from generated code
        //
        std::vector<bool> alreadyLinkedin;
        alreadyLinkedin.resize(AstTypeHelper::x_num_cpp_functions, false /*value*/);
        Linker linker(*m_llvmModule);
        auto linkinFunctionByMetadata = [&](const CppFunctionMetadata* metadata)
        {
            TestAssert(metadata != nullptr);
            assert(metadata->m_functionOrdinal < alreadyLinkedin.size());
            if (!alreadyLinkedin[metadata->m_functionOrdinal])
            {
                alreadyLinkedin[metadata->m_functionOrdinal] = true;
                const BitcodeData* bitcode = metadata->m_bitcodeData;
                std::unique_ptr<Module> bitcodeModule = getIrModuleFromBitcodeData(bitcode);
                // linkInModule returns true on error
                // TODO: handle error
                //
                ReleaseAssert(linker.linkInModule(std::move(bitcodeModule)) == false);
                // change linkage to available_externally
                //
                Function* func = m_llvmModule->getFunction(bitcode->m_symbolName);
                TestAssert(func != nullptr);
                TestAssert(func->getLinkage() == GlobalValue::LinkageTypes::ExternalLinkage);
                func->setLinkage(GlobalValue::LinkageTypes::AvailableExternallyLinkage);
            }
        };
        // Link in all bitcode stubs needed by a generated function
        //
        auto linkinBitcodeFn = [&](AstNodeBase* cur,
                                   AstNodeBase* /*parent*/,
                                   FunctionRef<void(void)> Recurse)
        {
            AstNodeType nodeType = cur->GetAstNodeType();
            if (nodeType == AstNodeType::AstCallExpr)
            {
                AstCallExpr* callExpr = assert_cast<AstCallExpr*>(cur);
                if (callExpr->IsCppFunction())
                {
                    const CppFunctionMetadata* metadata = callExpr->GetCppFunctionMetadata();
                    linkinFunctionByMetadata(metadata);
                }
            }
            else if (nodeType == AstNodeType::AstDeclareVariable)
            {
                AstDeclareVariable* declareVar = assert_cast<AstDeclareVariable*>(cur);
                AstVariable* var = declareVar->m_variable;
                if (var->GetTypeId().RemovePointer().IsCppClassType())
                {
                    const CppFunctionMetadata* metadata = GetDestructorMetadata(var->GetTypeId().RemovePointer());
                    linkinFunctionByMetadata(metadata);
                }
            }
            else if (nodeType == AstNodeType::AstThrowStmt)
            {
                // For a 'throw' statement, we need the following information:
                // (1) CXXABI Exception Handler function prototypes
                // (2) The 'std::type_info' object symbol for the exception type to be thrown
                // (3) The destructor, if the exception is a CPP class
                //
                AstThrowStmt* throwStmt = assert_cast<AstThrowStmt*>(cur);
                importSymbolsForExceptionHandler();
                importStdTypeInfoSymbol(throwStmt->m_exceptionTypeId);
                if (throwStmt->m_exceptionTypeId.IsCppClassType())
                {
                    const CppFunctionMetadata* metadata = GetDestructorMetadata(throwStmt->m_exceptionTypeId);
                    linkinFunctionByMetadata(metadata);
                }
            }
            Recurse();
        };
        for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
        {
            AstFunction* fn = iter->second;
            fn->TraverseFunctionBody(linkinBitcodeFn);
        }
    }

    // Second pass: emit all function prototype.
    //
    for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
    {
        AstFunction* fn = iter->second;
        fn->EmitDefinition();
    }

    // Third pass: emit all function bodies.
    //
    for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
    {
        AstFunction* fn = iter->second;
        fn->EmitIR();
    }

    // In test build, validate that the module contains no errors.
    // llvm::verifyModule returns false on success
    //
    TestAssert(verifyModule(*m_llvmModule, &outs()) == false);

    thread_llvmContext->m_dummyBlock = nullptr;
    thread_llvmContext->ClearModule();
}

void AstModule::OptimizeIR(int optLevel)
{
    // According to LLVM Document, should not run optimize pass repeatedly on one module.
    //
    TestAssert(m_irEmitted && !m_irOptimized);
#ifdef TESTBUILD
    m_irOptimized = true;
#endif

    thread_llvmContext->RunOptimizationPass(m_llvmModule, optLevel);

    // Just for sanity, validate that the module still contains no errors.
    // llvm::verifyModule returns false on success
    //
    TestAssert(verifyModule(*m_llvmModule, &outs()) == false);
}

void AstModule::OptimizeIRIfNotDebugMode([[maybe_unused]] int optLevel)
{
#ifdef NDEBUG
    OptimizeIR(optLevel);
#endif
}

ThreadSafeModule AstModule::GetThreadSafeModule()
{
    TestAssert(m_llvmModule != nullptr && m_llvmContext != nullptr);
    ThreadSafeModule&& r = ThreadSafeModule(std::unique_ptr<Module>(m_llvmModule),
                                            std::unique_ptr<LLVMContext>(m_llvmContext));
    m_llvmModule = nullptr;
    m_llvmContext = nullptr;
    return std::move(r);
}

void AstCallExpr::SetSretAddress(Value* address)
{
    TestAssert(m_isCppFunction && m_cppFunctionMd->m_isUsingSret);
    TestAssert(m_sretAddress == nullptr && address != nullptr);
    TestAssert(AstTypeHelper::llvm_value_has_type(m_cppFunctionMd->m_returnType.AddPointer(), address));
    m_sretAddress = address;
}

Value* WARN_UNUSED AstCallExpr::EmitIRImpl()
{
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    Function* callee = nullptr;
    AstFunction* calleeAst = nullptr;
    if (!m_isCppFunction)
    {
        calleeAst = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
        TestAssertIff(calleeAst == nullptr, thread_llvmContext->m_module->getFunction(m_fnName) == nullptr);
        TestAssert(calleeAst != nullptr);
        callee = calleeAst->GetGeneratedPrototype();
        TestAssert(callee != nullptr && callee == thread_llvmContext->m_module->getFunction(m_fnName));
    }
    else
    {
        TestAssertIff(m_cppFunctionMd->m_isUsingSret, m_sretAddress != nullptr);
        callee = thread_llvmContext->m_module->getFunction(m_cppFunctionMd->m_bitcodeData->m_symbolName);
        TestAssert(callee != nullptr);
        TestAssertImp(m_cppFunctionMd->m_isNoExcept, callee->hasFnAttribute(Attribute::AttrKind::NoUnwind));
    }
    TestAssert(callee->arg_size() == m_params.size() + ((m_isCppFunction && m_cppFunctionMd->m_isUsingSret) ? 1 : 0));
    TestAssertImp(m_isCppFunction, m_params.size() == m_cppFunctionMd->m_numParams);
    Value** params = reinterpret_cast<Value**>(alloca(sizeof(Value*) * callee->arg_size()));
    size_t curParamIndex = 0;
    if (m_isCppFunction && m_cppFunctionMd->m_isUsingSret)
    {
        TestAssert(AstTypeHelper::llvm_value_has_type(m_cppFunctionMd->m_returnType.AddPointer(), m_sretAddress));
        TestAssert(m_sretAddress->getType() == callee->getArg(static_cast<unsigned>(curParamIndex))->getType());
        params[curParamIndex] = m_sretAddress;
        curParamIndex++;
    }
    for (size_t index = 0; index < m_params.size(); index++)
    {
        Value* param = m_params[index]->EmitIR();
        TestAssert(param->getType() == callee->getArg(static_cast<unsigned>(curParamIndex))->getType());
        TestAssertImp(!m_isCppFunction, AstTypeHelper::llvm_value_has_type(calleeAst->GetParamType(index), param));
        TestAssertImp(m_isCppFunction, AstTypeHelper::llvm_value_has_type(m_cppFunctionMd->m_paramTypes[index], param));
        params[curParamIndex] = param;
        curParamIndex++;
    }
    TestAssert(curParamIndex == callee->arg_size());

    Value* ret;
    // If the function is nothrow, or there is no need to set up a landing pad since there are no destructors or catch,
    // just use normal 'Call' instruction. Otherwise we need to use Invoke.
    //
    if (callee->hasFnAttribute(Attribute::AttrKind::NoUnwind) || IsNoLandingPadNeeded())
    {
        ret = thread_llvmContext->m_builder
                ->CreateCall(callee, ArrayRef<Value*>(params, params + callee->arg_size()));
    }
    else
    {
        BasicBlock* unwindDest = EmitEHLandingPadForCurrentPosition();
        BasicBlock* normalDest = BasicBlock::Create(
                    *thread_llvmContext->m_llvmContext,
                    Twine("") /*name*/,
                    thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
        ret = thread_llvmContext->m_builder
                ->CreateInvoke(callee, normalDest, unwindDest, ArrayRef<Value*>(params, params + callee->arg_size()));
        thread_llvmContext->m_builder->SetInsertPoint(normalDest);
    }
    if (GetTypeId().IsVoid() || GetTypeId().IsCppClassType())
    {
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void>(), ret));
        ret = nullptr;
    }
    else
    {
        TestAssert(AstTypeHelper::llvm_value_has_type(GetTypeId(), ret));
    }
    return ret;
}

Value* WARN_UNUSED AstDeclareVariable::EmitIRImpl()
{
    // If there is an initial value, alloc the var and assign it. Otherwise this is a no-op,
    // the variable will be automatically alloca'ed later when it is first used later.
    //
    if (m_assignExpr != nullptr)
    {
        std::ignore = m_variable->EmitIR();
        std::ignore = m_assignExpr->EmitIR();
    }
    else if (m_callExpr != nullptr)
    {
        if (!m_isCtor)
        {
            Value* pos = m_variable->EmitIR();
            m_callExpr->SetSretAddress(pos);
            std::ignore = m_callExpr->EmitIR();
        }
        else
        {
            std::ignore = m_callExpr->EmitIR();
        }
    }
    // If it is a CPP class type variable, push it into the variable scope,
    // so the destructor will be called when this variable goes out of scope.
    //
    // The order is important: first call constructor, then push it into scope,
    // since the constructor may throw (and in that case we should not destruct the variable again)
    //
    if (m_variable->GetTypeId().RemovePointer().IsCppClassType())
    {
        thread_pochiVMContext->m_scopedVariableManager.PushObject(m_variable);
    }
    return nullptr;
}

Value* WARN_UNUSED AstReturnStmt::EmitIRImpl()
{
    AstFunction* function = thread_llvmContext->GetCurFunction();
    if (m_retVal != nullptr)
    {
        TestAssert(!function->GetReturnType().IsVoid());
        Value* retVal = m_retVal->EmitIR();
        TestAssert(AstTypeHelper::llvm_value_has_type(function->GetReturnType(), retVal));
        thread_pochiVMContext->m_scopedVariableManager.EmitIRDestructAllVariablesUntilScope(nullptr /*scopeBoundary*/);
        thread_llvmContext->m_builder->CreateRet(retVal);
    }
    else
    {
        TestAssert(function->GetReturnType().IsVoid());
        thread_pochiVMContext->m_scopedVariableManager.EmitIRDestructAllVariablesUntilScope(nullptr /*scopeBoundary*/);
        thread_llvmContext->m_builder->CreateRetVoid();
    }
    thread_llvmContext->SetInsertPointToDummyBlock();
    std::ignore = function;
    return nullptr;
}

}   // namespace PochiVM
