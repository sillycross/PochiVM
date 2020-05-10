#include "function_proto.h"
#include "pochivm.hpp"

namespace Ast
{

using namespace llvm;

void AstFunction::EmitDefinition()
{
    TestAssert(m_generatedPrototype == nullptr);
    std::vector<Type*> args;
    for (AstVariable* arg : m_params)
    {
        args.push_back(AstTypeHelper::llvm_type_of(arg->GetTypeId().RemovePointer()));
    }
    Type* returnType = AstTypeHelper::llvm_type_of(m_returnType);
    FunctionType* funcType = FunctionType::get(returnType, args, false /*isVarArg*/);
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
    m_llvmEntryBlock = BasicBlock::Create(thread_llvmContext->m_llvmContext,
                                          "entry", m_generatedPrototype);

    BasicBlock* body = BasicBlock::Create(thread_llvmContext->m_llvmContext,
                                          "body", m_generatedPrototype);

    m_llvmFooterBlock = BasicBlock::Create(thread_llvmContext->m_llvmContext,
                                           "footer", m_generatedPrototype);

    // Build the entry block: create alloca instruction for return value and each parameter
    // Alloca instructions for local variables will be created as they are later encountered
    //
    thread_llvmContext->m_builder.SetInsertPoint(m_llvmEntryBlock);
    if (!m_returnType.IsVoid())
    {
        m_llvmReturnValue = thread_llvmContext->m_builder.CreateAlloca(
                    AstTypeHelper::llvm_type_of(m_returnType) /*type*/,
                    nullptr /*ArraySize*/, "__ret__" /*name*/);
    }

    // Build header block: store the parameters of this function (which are RValues)
    // into the space we allocated for parameters, so we can use them as LValues later
    //
    thread_llvmContext->m_builder.SetInsertPoint(body);
    {
        size_t index = 0;
        for (auto &arg : m_generatedPrototype->args())
        {
            thread_llvmContext->m_builder.CreateStore(&arg, m_params[index]->EmitIR());
            index++;
        }
    }

    // Build footer block:
    //    %tmp = load %__ret__
    //    return %tmp
    //
    if (!m_returnType.IsVoid())
    {
        thread_llvmContext->m_builder.SetInsertPoint(m_llvmFooterBlock);
        Value* retVal = thread_llvmContext->m_builder.CreateLoad(m_llvmReturnValue);
        TestAssert(AstTypeHelper::llvm_value_has_type(m_returnType, retVal));
        thread_llvmContext->m_builder.CreateRet(retVal);
    }

    // Return insert pointer to end of body, and build the function body
    //
    thread_llvmContext->m_builder.SetInsertPoint(body);
    Value* bodyRet = m_body->EmitIR();
    TestAssert(bodyRet == nullptr);
    std::ignore = bodyRet;

    // After the body is built, connect the blocks.
    //
    // Entry block fallthroughs to body block
    //
    thread_llvmContext->m_builder.SetInsertPoint(m_llvmEntryBlock);
    thread_llvmContext->m_builder.CreateBr(body);

    // Body block fallthroughs to footer block
    //
    // TODO: call abort() in testbuild if function returns non-void
    //
    thread_llvmContext->m_builder.SetInsertPoint(body);
    thread_llvmContext->m_builder.CreateBr(m_llvmFooterBlock);

    thread_llvmContext->m_module->print(outs(), nullptr);

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
    // In debug build, user should always validate module before emitting IR
    //
    assert(!m_irEmitted && m_validated);
#ifndef NDEBUG
    m_irEmitted = true;
#endif

    // Setup the LLVM codegen context
    //
    TestAssert(m_llvmModule == nullptr);
    m_llvmModule = new Module(m_moduleName, thread_llvmContext->m_llvmContext);

    TestAssert(thread_llvmContext->m_module == nullptr);
    thread_llvmContext->m_module = m_llvmModule;

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

    thread_llvmContext->m_module = nullptr;
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
    return thread_llvmContext->m_builder.CreateCall(callee, ArrayRef<Value*>(params, params + m_params.size()));
}

Value* WARN_UNUSED AstReturnStmt::EmitIRImpl()
{
    AstFunction* function = thread_llvmContext->GetCurFunction();
    if (m_retVal != nullptr)
    {
        TestAssert(!function->GetReturnType().IsVoid());
        Value* retVal = m_retVal->EmitIR();
        TestAssert(AstTypeHelper::llvm_value_has_type(function->GetReturnType(), retVal));
        thread_llvmContext->m_builder.CreateStore(retVal, function->GetReturnValueStoreLocation());
    }
    else
    {
        TestAssert(function->GetReturnType().IsVoid());
    }
    // TODO: call all destructors
    //
    thread_llvmContext->m_builder.CreateBr(function->GetFooterBlock());
    return nullptr;
}

}   // namespace Ast
