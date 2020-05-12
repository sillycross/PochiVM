#include "function_proto.h"
#include "pochivm.hpp"

namespace Ast
{

using namespace llvm;

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
            thread_llvmContext->m_builder->CreateStore(&arg, m_params[index]->EmitIR());
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
    m_llvmContext = new llvm::LLVMContext();

    llvm::IRBuilder<>* llvmIrBuilder = new llvm::IRBuilder<>(*m_llvmContext);
    Auto(TestAssert(m_llvmContext != nullptr); delete llvmIrBuilder);

    m_llvmModule = new Module(m_moduleName, *m_llvmContext);

    thread_llvmContext->SetupModule(m_llvmContext, llvmIrBuilder, m_llvmModule);

    // TODO: does this leak memory?
    //
    thread_llvmContext->m_dummyBlock = BasicBlock::Create(*thread_llvmContext->m_llvmContext, "dummy");

    // First pass: emit all function prototype.
    //
    for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
    {
        AstFunction* fn = iter->second;
        fn->EmitDefinition();
    }
    // Second pass: emit all function bodies.
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

Value* WARN_UNUSED AstCallExpr::EmitIRImpl()
{
    AstFunction* calleeAst = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
    TestAssertIff(calleeAst == nullptr, thread_llvmContext->m_module->getFunction(m_fnName) == nullptr);
    TestAssert(calleeAst != nullptr);
    Function* callee = calleeAst->GetGeneratedPrototype();
    TestAssert(callee != nullptr && callee == thread_llvmContext->m_module->getFunction(m_fnName));
    TestAssert(callee->arg_size() == m_params.size());
    Value** params = reinterpret_cast<Value**>(alloca(sizeof(Value*) * m_params.size()));
    {
        size_t index = 0;
        for (auto& arg : callee->args())
        {
            Value* param = m_params[index]->EmitIR();
            TestAssert(param->getType() == arg.getType());
            TestAssert(AstTypeHelper::llvm_value_has_type(calleeAst->GetParamType(index), param));
            params[index] = param;
            index++;
        }
    }
    Value* ret = thread_llvmContext->m_builder->CreateCall(callee, ArrayRef<Value*>(params, params + m_params.size()));
    TestAssert(AstTypeHelper::llvm_value_has_type(GetTypeId(), ret));
    if (GetTypeId().IsVoid())
    {
        ret = nullptr;
    }
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
    return nullptr;
}

}   // namespace Ast
