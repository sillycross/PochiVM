#include "error_context.h"
#include "lang_constructs.h"
#include "function_proto.h"
#include "pochivm.hpp"

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
                      nullptr /*ArraySize*/,
                      Twine(m_varname).concat("_").concat(Twine(m_varnameSuffix)) /*name*/);
    thread_llvmContext->m_builder.restoreIP(savedIp);
    return m_llvmValue;
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
    for (AstNodeBase* stmt : m_contents)
    {
        Value* value = stmt->EmitIR();
        TestAssert(value == nullptr);
        std::ignore = value;
    }
    return nullptr;
}

Value* WARN_UNUSED AstScope::EmitIRImpl()
{
    // TODO: more logic needed here when we support destructors
    //
    for (AstNodeBase* stmt : m_contents)
    {
        Value* value = stmt->EmitIR();
        TestAssert(value == nullptr);
        std::ignore = value;
    }
    return nullptr;
}

Value* WARN_UNUSED AstIfStatement::EmitIRImpl()
{
    // Structure:
    //    .. evaluate condition ..
    //    CondBr(cond, thenBlock, hasElse ? elseBlock : afterIf)
    // thenBlock / elseBlock:
    //    .. codegen stmts ..
    //    Br(afterIf) // only emitted if !m_isCursorAtDummyBlock
    // afterIf:
    //    (new ip)    // only exists if at least one Br(afterIf) is emitted
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    Value* cond = m_condClause->EmitIR();
    BasicBlock* _afterIf = nullptr;
    auto createOrGetAfterIfBlock = [&_afterIf]() -> BasicBlock*
    {
        if (_afterIf != nullptr)
        {
            return _afterIf;
        }
        _afterIf = BasicBlock::Create(thread_llvmContext->m_llvmContext, "afterIf");
        return _afterIf;
    };

    BasicBlock* thenBlock = BasicBlock::Create(thread_llvmContext->m_llvmContext,
                                               "then",
                                               thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    BasicBlock* elseBlock = nullptr;
    if (!HasElseClause())
    {
        thread_llvmContext->m_builder.CreateCondBr(cond, thenBlock /*trueBr*/, createOrGetAfterIfBlock() /*falseBr*/);
    }
    else
    {
        elseBlock = BasicBlock::Create(thread_llvmContext->m_llvmContext,
                                       "else",
                                       thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
        thread_llvmContext->m_builder.CreateCondBr(cond, thenBlock /*trueBr*/, elseBlock /*falseBr*/);
    }

    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder.SetInsertPoint(thenBlock);
    Value* thenClauseRet = m_thenClause->EmitIR();
    TestAssert(thenClauseRet == nullptr);
    std::ignore = thenClauseRet;

    // then-clause control flow fallthrough
    //
    if (!thread_llvmContext->m_isCursorAtDummyBlock)
    {
        thread_llvmContext->m_builder.CreateBr(createOrGetAfterIfBlock());
    }

    if (HasElseClause())
    {
        thread_llvmContext->m_isCursorAtDummyBlock = false;
        thread_llvmContext->m_builder.SetInsertPoint(elseBlock);
        Value* elseClauseRet = m_elseClause->EmitIR();
        TestAssert(elseClauseRet == nullptr);
        std::ignore = elseClauseRet;

        // else-clause control flow fallthrough
        //
        if (!thread_llvmContext->m_isCursorAtDummyBlock)
        {
            thread_llvmContext->m_builder.CreateBr(createOrGetAfterIfBlock());
        }
    }

    if (_afterIf != nullptr)
    {
        // At least one branch branches to afterIf block
        // We should insert afterIf block at the end of function, and put ip there
        //
        thread_llvmContext->m_isCursorAtDummyBlock = false;
        _afterIf->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
        thread_llvmContext->m_builder.SetInsertPoint(_afterIf);
    }
    else
    {
        // No branch branches to afterIf block. The ip must be pointing at dummy block now.
        //
        TestAssert(thread_llvmContext->m_isCursorAtDummyBlock);
    }
    return nullptr;
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
