#include "logical_operator.h"
#include "error_context.h"
#include "llvm_ast_helper.hpp"
#include "function_proto.h"

namespace PochiVM
{

using namespace llvm;

Value* WARN_UNUSED AstLogicalAndOrExpr::EmitIRImpl()
{
    // Structure:
    //    %lhs = .. evaluate m_lhs ..
    //    BrCond(m_lhs, evalRhs, afterOp)   // for &&
    //    BrCond(m_lhs, afterOp, evalRhs)   // for ||
    //
    // evalRhs:
    //    %rhs = .. evaluate m_rhs ..
    //    Br(afterOp)
    //
    // afterOp:
    //    %res = phi ...
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);

    uint32_t labelSuffix = thread_llvmContext->m_curFunction->GetNextLogicalOpSuffix();

    Value* lhs = m_lhs->EmitIR();

    BasicBlock* evalRhs = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                             Twine(m_isAnd ? "logical_and_rhs" : "logical_or_rhs")
                                                     .concat(Twine(labelSuffix)));
    BasicBlock* afterOp = BasicBlock::Create(*thread_llvmContext->m_llvmContext,
                                             Twine(m_isAnd ? "logical_and_result" : "logical_or_result")
                                                     .concat(Twine(labelSuffix)));

    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    BasicBlock* lhsPhiBlock = thread_llvmContext->m_builder->GetInsertBlock();
    if (m_isAnd)
    {
        thread_llvmContext->m_builder->CreateCondBr(lhs, evalRhs /*trueBranch*/, afterOp /*falseBranch*/);
    }
    else
    {
        thread_llvmContext->m_builder->CreateCondBr(lhs, afterOp /*trueBranch*/, evalRhs /*falseBranch*/);
    }

    // codegen evalRhs block
    //
    evalRhs->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(evalRhs);
    Value* rhs = m_rhs->EmitIR();

    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    BasicBlock* rhsPhiBlock = thread_llvmContext->m_builder->GetInsertBlock();
    thread_llvmContext->m_builder->CreateBr(afterOp);

    // codegen afterOp block
    //
    afterOp->insertInto(thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_builder->SetInsertPoint(afterOp);

    PHINode *res = thread_llvmContext->m_builder->CreatePHI(AstTypeHelper::llvm_type_of(TypeId::Get<bool>()),
                                                            2 /*numIncomingValsHint*/);
    res->addIncoming(lhs, lhsPhiBlock);
    res->addIncoming(rhs, rhsPhiBlock);
    return res;
}

Value* WARN_UNUSED AstLogicalNotExpr::EmitIRImpl()
{
    Value* op = m_op->EmitIR();
    return thread_llvmContext->m_builder->CreateNot(op);
}

}   // namespace PochiVM
