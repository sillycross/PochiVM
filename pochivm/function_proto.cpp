#include "function_proto.h"
#include "pochivm.hpp"

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
}

void AstFunction::EmitIR()
{
    // Set current function in context, some AST codegen relies on this.
    //
    TestAssert(thread_llvmContext->m_curFunction == nullptr && m_generatedPrototype != nullptr);
    thread_llvmContext->m_curFunction = this;
    thread_llvmContext->m_isCursorAtDummyBlock = false;
    thread_llvmContext->m_breakStmtTarget.clear();
    thread_llvmContext->m_continueStmtTarget.clear();

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

    // First pass: find all call-cpp-function expressions,
    // link in the bitcodes containing the implementation and necessary types
    //
    {
        std::vector<bool> alreadyLinkedin;
        alreadyLinkedin.resize(AstTypeHelper::x_num_cpp_functions, false /*value*/);
        Linker linker(*m_llvmModule);
        TraverseAstTreeFn linkinBitcodeFn = [&](AstNodeBase* cur,
                                                AstNodeBase* /*parent*/,
                                                const std::function<void(void)>& Recurse)
        {
            if (cur->GetAstNodeType() == AstNodeType::AstCallExpr)
            {
                AstCallExpr* callExpr = assert_cast<AstCallExpr*>(cur);
                if (callExpr->IsCppFunction())
                {
                    const CppFunctionMetadata* metadata = callExpr->GetCppFunctionMetadata();
                    assert(metadata->m_functionOrdinal < alreadyLinkedin.size());
                    if (!alreadyLinkedin[metadata->m_functionOrdinal])
                    {
                        alreadyLinkedin[metadata->m_functionOrdinal] = true;
                        const BitcodeData* bitcode = metadata->m_bitcodeData;
                        SMDiagnostic llvmErr;
                        MemoryBufferRef mb(StringRef(reinterpret_cast<const char*>(bitcode->m_bitcode), bitcode->m_length),
                                           StringRef(bitcode->m_symbolName));
                        std::unique_ptr<Module> bitcodeModule = parseIR(mb, llvmErr, *m_llvmContext);
                        // TODO: handle error
                        //
                        ReleaseAssert(bitcodeModule != nullptr);
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

    TestAssert(thread_llvmContext->m_breakStmtTarget.size() == 0);
    TestAssert(thread_llvmContext->m_continueStmtTarget.size() == 0);
    thread_llvmContext->m_dummyBlock = nullptr;
    thread_llvmContext->ClearModule();
}

void AstModule::OptimizeIR()
{
    // According to LLVM Document, should not run optimize pass repeatedly on one module.
    //
    TestAssert(m_irEmitted && !m_irOptimized);
#ifdef TESTBUILD
    m_irOptimized = true;
#endif

    thread_llvmContext->RunOptimizationPass(m_llvmModule);

    // Just for sanity, validate that the module still contains no errors.
    // llvm::verifyModule returns false on success
    //
    TestAssert(verifyModule(*m_llvmModule, &outs()) == false);
}

void AstModule::OptimizeIRIfNotDebugMode()
{
#ifdef NDEBUG
    OptimizeIR();
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

Value* WARN_UNUSED AstCallExpr::EmitIRImpl()
{
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
        // TODO: support sret
        //
        ReleaseAssert(!m_cppFunctionMd->m_isUsingSret);
        callee = thread_llvmContext->m_module->getFunction(m_cppFunctionMd->m_bitcodeData->m_symbolName);
        TestAssert(callee != nullptr);
    }
    TestAssert(callee->arg_size() == m_params.size());
    TestAssertImp(m_isCppFunction, m_params.size() == m_cppFunctionMd->m_numParams);
    Value** params = reinterpret_cast<Value**>(alloca(sizeof(Value*) * m_params.size()));
    for (size_t index = 0; index < m_params.size(); index++)
    {
        Value* param = m_params[index]->EmitIR();
        TestAssert(param->getType() == callee->getArg(static_cast<unsigned>(index))->getType());
        TestAssertImp(!m_isCppFunction, AstTypeHelper::llvm_value_has_type(calleeAst->GetParamType(index), param));
        TestAssertImp(m_isCppFunction, AstTypeHelper::llvm_value_has_type(m_cppFunctionMd->m_paramTypes[index], param));
        params[index] = param;
    }
    Value* ret = thread_llvmContext->m_builder
                         ->CreateCall(callee, ArrayRef<Value*>(params, params + m_params.size()));
    TestAssert(AstTypeHelper::llvm_value_has_type(GetTypeId(), ret));
    if (GetTypeId().IsVoid()) { ret = nullptr; }
    return ret;
}

Value* WARN_UNUSED AstReturnStmt::EmitIRImpl()
{
    AstFunction* function = thread_llvmContext->GetCurFunction();
    if (m_retVal != nullptr)
    {
        TestAssert(!function->GetReturnType().IsVoid());
        Value* retVal = m_retVal->EmitIR();
        TestAssert(AstTypeHelper::llvm_value_has_type(function->GetReturnType(), retVal));
        // TODO: call all destructors
        //
        thread_llvmContext->m_builder->CreateRet(retVal);
    }
    else
    {
        // TODO: call all destructors
        //
        TestAssert(function->GetReturnType().IsVoid());
        thread_llvmContext->m_builder->CreateRetVoid();
    }
    thread_llvmContext->SetInsertPointToDummyBlock();
    std::ignore = function;
    return nullptr;
}

}   // namespace PochiVM
