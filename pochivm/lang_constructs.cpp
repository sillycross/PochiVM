#include "error_context.h"
#include "lang_constructs.h"
#include "function_proto.h"
#include "pochivm.hpp"

namespace PochiVM
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
    auto savedIp = thread_llvmContext->m_builder->saveIP();
    thread_llvmContext->m_builder->SetInsertPoint(thread_llvmContext->GetCurFunction()->GetEntryBlock());
    assert(GetTypeId().IsPointerType());
    Type* llvmType = AstTypeHelper::llvm_type_of(GetTypeId());
    assert(llvmType->isPointerTy());
    m_llvmValue = thread_llvmContext->m_builder->CreateAlloca(
                      llvmType->getPointerElementType(),
                      nullptr /*ArraySize*/,
                      Twine(m_varname).concat("_").concat(Twine(m_varnameSuffix)) /*name*/);
    thread_llvmContext->m_builder->restoreIP(savedIp);
    return m_llvmValue;
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
    return nullptr;
}

Value* WARN_UNUSED AstDereferenceVariableExpr::EmitIRImpl()
{
    Value* op = m_operand->EmitIR();
    return AstTypeHelper::create_load_helper(GetTypeId(), op);
}

Value* WARN_UNUSED AstBlock::EmitIRImpl()
{
    for (AstNodeBase* stmt : m_contents)
    {
        std::ignore = stmt->EmitIR();
    }
    return nullptr;
}

Value* WARN_UNUSED AstScope::EmitIRImpl()
{
    // TODO: more logic needed here when we support destructors
    //
    for (AstNodeBase* stmt : m_contents)
    {
        std::ignore = stmt->EmitIR();
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
    uint32_t labelSuffix = thread_llvmContext->GetCurFunction()->GetNextIfStmtSuffix();

    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    Value* cond = m_condClause->EmitIR();
    cond->setName(Twine("if").concat(Twine(labelSuffix)));

    BasicBlock* _afterIf = nullptr;
    auto createOrGetAfterIfBlock = [&_afterIf, labelSuffix]() -> BasicBlock*
    {
        if (_afterIf != nullptr)
        {
            return _afterIf;
        }
        _afterIf = BasicBlock::Create(*thread_llvmContext->m_llvmContext, Twine("after_if").concat(Twine(labelSuffix)));
        return _afterIf;
    };

    BasicBlock* thenBlock = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                               Twine("then").concat(Twine(labelSuffix)),
                                               thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    BasicBlock* elseBlock = nullptr;
    if (!HasElseClause())
    {
        thread_llvmContext->m_builder->CreateCondBr(cond, thenBlock /*trueBr*/, createOrGetAfterIfBlock() /*falseBr*/);
    }
    else
    {
        // Do not insert into function yet, for clarity of generated code
        //
        elseBlock = BasicBlock::Create(*thread_llvmContext->m_llvmContext, Twine("else").concat(Twine(labelSuffix)));
        thread_llvmContext->m_builder->CreateCondBr(cond, thenBlock /*trueBr*/, elseBlock /*falseBr*/);
    }

    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder->SetInsertPoint(thenBlock);
    std::ignore = m_thenClause->EmitIR();

    // then-clause control flow fallthrough
    //
    if (!thread_llvmContext->m_isCursorAtDummyBlock)
    {
        thread_llvmContext->m_builder->CreateBr(createOrGetAfterIfBlock());
    }

    if (HasElseClause())
    {
        elseBlock->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
        thread_llvmContext->m_isCursorAtDummyBlock = false;
        thread_llvmContext->m_builder->SetInsertPoint(elseBlock);
        std::ignore = m_elseClause->EmitIR();

        // else-clause control flow fallthrough
        //
        if (!thread_llvmContext->m_isCursorAtDummyBlock)
        {
            thread_llvmContext->m_builder->CreateBr(createOrGetAfterIfBlock());
        }
    }

    if (_afterIf != nullptr)
    {
        // At least one branch branches to afterIf block
        // We should insert afterIf block at the end of function, and put ip there
        //
        _afterIf->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
        thread_llvmContext->m_isCursorAtDummyBlock = false;
        thread_llvmContext->m_builder->SetInsertPoint(_afterIf);
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
    // Structure:
    //    Br(loopHead)
    // loopHead:
    //    .. evaluate condition ..
    //    BrCond(cond, loopBody, afterLoop)
    // loopBody:
    //    .. codegen loop body ..
    //    Br(loopHead)  // only emitted if reachable
    // afterLoop:
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    Auto(TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock));

    uint32_t labelSuffix = thread_llvmContext->GetCurFunction()->GetNextWhileLoopSuffix();

    BasicBlock* loopHead = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                              Twine("whileloop_head").concat(Twine(labelSuffix)),
                                              thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    BasicBlock* loopBody = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                              Twine("whileloop_body").concat(Twine(labelSuffix)));
    BasicBlock* afterLoop = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                               Twine("after_whileloop").concat(Twine(labelSuffix)));
    thread_llvmContext->m_builder->CreateBr(loopHead);

    thread_llvmContext->m_continueStmtTarget.push_back(loopHead);
    Auto(thread_llvmContext->m_continueStmtTarget.pop_back());

    thread_llvmContext->m_breakStmtTarget.push_back(afterLoop);
    Auto(thread_llvmContext->m_breakStmtTarget.pop_back());

    // Codegen loopHead block
    //
    thread_llvmContext->m_builder->SetInsertPoint(loopHead);
    Value* cond = m_condClause->EmitIR();
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder->CreateCondBr(cond, loopBody /*trueBranch*/, afterLoop /*falseBranch*/);

    // Codegen loopBody block
    //
    loopBody->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(loopBody);
    std::ignore = m_body->EmitIR();

    if (!thread_llvmContext->m_isCursorAtDummyBlock)
    {
        thread_llvmContext->m_builder->CreateBr(loopHead);
    }

    // Set ip to afterLoop
    //
    afterLoop->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_isCursorAtDummyBlock = false;
    thread_llvmContext->m_builder->SetInsertPoint(afterLoop);

    return nullptr;
}

Value* WARN_UNUSED AstForLoop::EmitIRImpl()
{
    // Structure:
    //    .. evaluate init block ..
    //    Br(loopHead)
    // loopHead:
    //    .. evaluate cond ..
    //    BrCond(cond, loopBody, afterLoop)
    // loopBody:
    //    .. codegen loop body ..
    //    Br(loopStep)  // only emitted if reachable
    // loopStep:    (this is target for continue stmt)
    //    .. codegen loop step ..
    //    Br(loopHead)
    // afterLoop:   (this is target for break stmt)
    //    .. call destructors for vars in init block ..
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    Auto(TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock));

    uint32_t labelSuffix = thread_llvmContext->GetCurFunction()->GetNextForLoopSuffix();

    BasicBlock* loopHead = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                              Twine("forloop_head").concat(Twine(labelSuffix)));
    BasicBlock* loopBody = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                              Twine("forloop_body").concat(Twine(labelSuffix)));
    BasicBlock* loopStep = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                              Twine("forloop_step").concat(Twine(labelSuffix)));
    BasicBlock* afterLoop = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                               Twine("after_forloop").concat(Twine(labelSuffix)));

    thread_llvmContext->m_continueStmtTarget.push_back(loopStep);
    Auto(thread_llvmContext->m_continueStmtTarget.pop_back());

    thread_llvmContext->m_breakStmtTarget.push_back(afterLoop);
    Auto(thread_llvmContext->m_breakStmtTarget.pop_back());

    std::ignore = m_startClause->EmitIR();
    // We disallow break/continue/return in for-loop init-block
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder->CreateBr(loopHead);

    // Codegen loopHead block
    //
    loopHead->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(loopHead);
    Value* cond = m_condClause->EmitIR();
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder->CreateCondBr(cond, loopBody /*trueBranch*/, afterLoop /*falseBranch*/);

    // Codegen loopBody block
    //
    loopBody->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(loopBody);
    std::ignore = m_body->EmitIR();
    if (!thread_llvmContext->m_isCursorAtDummyBlock)
    {
        thread_llvmContext->m_builder->CreateBr(loopStep);
    }

    // Codegen loopStep block
    //
    loopStep->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_isCursorAtDummyBlock = false;
    thread_llvmContext->m_builder->SetInsertPoint(loopStep);
    std::ignore = m_stepClause->EmitIR();
    // We disallow break/continue/return in for-loop step-block
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder->CreateBr(loopHead);

    // Codegen afterLoop block
    // TODO: call destructors for vars in init-block
    //
    afterLoop->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(afterLoop);
    return nullptr;
}

Value* WARN_UNUSED AstBreakOrContinueStmt::EmitIRImpl()
{
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    TestAssert(thread_llvmContext->m_breakStmtTarget.size() > 0);
    TestAssert(thread_llvmContext->m_continueStmtTarget.size() > 0);
    // TODO: call destructors
    //
    if (IsBreakStatement())
    {
        thread_llvmContext->m_builder->CreateBr(thread_llvmContext->m_breakStmtTarget.back());
    }
    else
    {
        thread_llvmContext->m_builder->CreateBr(thread_llvmContext->m_continueStmtTarget.back());
    }
    thread_llvmContext->SetInsertPointToDummyBlock();
    return nullptr;
}

}   // namespace PochiVM
