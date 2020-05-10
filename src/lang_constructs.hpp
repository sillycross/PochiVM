#pragma once

#include "error_context.h"
#include "lang_constructs.h"
#include "codegen_context.hpp"
#include "ast_type_helper.hpp"
#include "function_proto.h"

namespace Ast
{

using namespace llvm;

Value* WARN_UNUSED AstVariable::EmitIRImpl()
{
    // If a storage location has been assigned, just return it
    //
    if (m_llvmValue != nullptr)
    {
        return m_llvmValue;
    }
    // Otherwise, assign a storage location for this variable
    // Save insertion point, insert the alloca into end of entry block, and restore insertion point
    //
    auto savedIp = thread_llvmContext->m_builder.saveIP();
    thread_llvmContext->m_builder.SetInsertPoint(thread_llvmContext->GetCurFunction()->GetEntryBlock());
    m_llvmValue = thread_llvmContext->m_builder.CreateAlloca(
                      AstTypeHelper::llvm_type_of(GetTypeId().RemovePointer()),
                      nullptr /*ArraySize*/, GetVarNameTwine() /*name*/);
    thread_llvmContext->m_builder.restoreIP(savedIp);
    return m_llvmValue;
}

llvm::Twine AstVariable::GetVarNameTwine() const
{
    return Twine(m_varname).concat("_").concat(Twine(m_varnameSuffix));
}

Value* WARN_UNUSED AstDeclareVariable::EmitIRImpl()
{
    // If there is an initial value, alloc the var and assign it. Otherwise this is a no-op,
    // the variable will be automatically alloca'ed later when it is first used later.
    //
    if (m_assignExpr != nullptr)
    {
        Value* ptr = m_variable->EmitIR();
        Value* val = m_assignExpr->EmitIR();
        thread_llvmContext->m_builder.CreateStore(val, ptr);
    }
    return nullptr;
}

Value* WARN_UNUSED AstDereferenceVariableExpr::EmitIRImpl()
{
    Value* op = m_operand->EmitIR();
    Value* inst = thread_llvmContext->m_builder.CreateLoad(op);
    return inst;
}

Value* WARN_UNUSED AstBlock::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstScope::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstIfStatement::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstWhileLoop::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstForLoop::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstBreakOrContinueStmt::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

}   // namespace Ast
