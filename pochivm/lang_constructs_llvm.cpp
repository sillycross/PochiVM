#include "error_context.h"
#include "lang_constructs.h"
#include "function_proto.h"
#include "destructor_helper.h"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

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
    // Push a new layer of variable scope, and codegen the body
    //
    AutoScopedVariableManagerScope asvms(this);
    for (AstNodeBase* stmt : m_contents)
    {
        std::ignore = stmt->EmitIR();
    }
    // Call destructors in reverse order for each local var declared in this scope
    // If the cursor is at the dummy block, it means the execution will never reach the end-of-scope
    // (due to a break/continue/return statement), so there is no need to emit destructors here.
    //
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    if (!thread_llvmContext->m_isCursorAtDummyBlock)
    {
        thread_pochiVMContext->m_scopedVariableManager.EmitIRDestructAllVariablesUntilScope(this);
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

    AutoScopedVarManagerLLVMBreakContinueTarget asvmlbct(afterLoop /*breakTarget*/,
                                                         m_body /*breakTargetScope*/,
                                                         loopHead /*continueTarget*/,
                                                         m_body /*continueTargetScope*/);

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

    AutoScopedVariableManagerScope asvms(this);
    AutoScopedVarManagerLLVMBreakContinueTarget asvmlbct(afterLoop /*breakTarget*/,
                                                         m_body /*breakTargetScope*/,
                                                         loopStep /*continueTarget*/,
                                                         m_body /*continueTargetScope*/);

    std::ignore = m_startClause->EmitIR();
    // We disallow break/continue/return in for-loop init-block
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder->CreateBr(loopHead);

#ifdef TESTBUILD
    // for-loop init-block is the only place that we allow declaring variables in the for-header
    // (the for-body is a separate scope). Although we have checked this condition in Validate(),
    // for sanity, here we assert again that no additional variables are declared since then.
    //
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    size_t numVarsInInitBlock = thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope();
#endif

    // Codegen loopHead block
    //
    loopHead->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(loopHead);
    Value* cond = m_condClause->EmitIR();
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    thread_llvmContext->m_builder->CreateCondBr(cond, loopBody /*trueBranch*/, afterLoop /*falseBranch*/);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope() == numVarsInInitBlock);

    // Codegen loopBody block
    //
    loopBody->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(loopBody);
    std::ignore = m_body->EmitIR();
    if (!thread_llvmContext->m_isCursorAtDummyBlock)
    {
        thread_llvmContext->m_builder->CreateBr(loopStep);
    }
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope() == numVarsInInitBlock);

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
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope() == numVarsInInitBlock);

    // Codegen afterLoop block, call destructors in reverse order
    //
    afterLoop->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(afterLoop);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    thread_pochiVMContext->m_scopedVariableManager.EmitIRDestructAllVariablesUntilScope(this);

    return nullptr;
}

Value* WARN_UNUSED AstBreakOrContinueStmt::EmitIRImpl()
{
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);

    // Call destructors, in reverse order of the variables being declared
    //
    std::pair<BasicBlock* /*branchTarget*/, AstNodeBase* /*scopeBoundary*/> target;
    if (IsBreakStatement())
    {
        target = thread_pochiVMContext->m_scopedVariableManager.GetLLVMBreakTarget();
    }
    else
    {
        target = thread_pochiVMContext->m_scopedVariableManager.GetLLVMContinueTarget();
    }
    thread_pochiVMContext->m_scopedVariableManager.EmitIRDestructAllVariablesUntilScope(target.second);
    thread_llvmContext->m_builder->CreateBr(target.first);
    thread_llvmContext->SetInsertPointToDummyBlock();
    return nullptr;
}

}   // namespace PochiVM
