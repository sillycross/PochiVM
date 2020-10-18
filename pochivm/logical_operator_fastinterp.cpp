#include "logical_operator.h"
#include "fastinterp_ast_helper.hpp"

namespace PochiVM
{

FastInterpSnippet WARN_UNUSED AstLogicalAndOrExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    // If we have a prediction, use it to deduce the children's prediction if possible
    //
    if (m_fiPrediction != AstFiLogicalOpPrediction::NO_PREDICTION)
    {
        AstFiLogicalOpPrediction populate = AstFiLogicalOpPrediction::NO_PREDICTION;
        if (m_isAnd && m_fiPrediction == AstFiLogicalOpPrediction::PREDICT_TRUE)
        {
            populate = AstFiLogicalOpPrediction::PREDICT_TRUE;
        }
        else if (!m_isAnd && m_fiPrediction == AstFiLogicalOpPrediction::PREDICT_FALSE)
        {
            populate = AstFiLogicalOpPrediction::PREDICT_FALSE;
        }
        if (populate != AstFiLogicalOpPrediction::NO_PREDICTION)
        {
            if (m_lhs->GetAstNodeType() == AstNodeType::AstLogicalAndOrExpr)
            {
                AstLogicalAndOrExpr* expr = assert_cast<AstLogicalAndOrExpr*>(m_lhs);
                expr->m_fiPrediction = populate;
            }
            else if (m_lhs->GetAstNodeType() == AstNodeType::AstLogicalNotExpr)
            {
                AstLogicalNotExpr* expr = assert_cast<AstLogicalNotExpr*>(m_lhs);
                expr->m_fiPrediction = populate;
            }
            if (m_rhs->GetAstNodeType() == AstNodeType::AstLogicalAndOrExpr)
            {
                AstLogicalAndOrExpr* expr = assert_cast<AstLogicalAndOrExpr*>(m_rhs);
                expr->m_fiPrediction = populate;
            }
            else if (m_rhs->GetAstNodeType() == AstNodeType::AstLogicalNotExpr)
            {
                AstLogicalNotExpr* expr = assert_cast<AstLogicalNotExpr*>(m_rhs);
                expr->m_fiPrediction = populate;
            }
        }
    }

    // Evaluate two sides. The logic is a bit tricky:
    //     lhs is always no spill, and in our operator, if short-circuiting happens,
    //     we do the spilling as needed. Otherwise, we simply pass control to rhs,
    //     and rhs will do the same spilling.
    //
    thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(TypeId::Get<bool>());
    FastInterpSnippet lhs = m_lhs->PrepareForFastInterp(x_FINoSpill);
    FastInterpSnippet rhs = m_rhs->PrepareForFastInterp(spillLoc);

    FastInterpBoilerplateInstance* inst;
    if ((m_fiPrediction == AstFiLogicalOpPrediction::PREDICT_TRUE && m_isAnd) ||
        (m_fiPrediction == AstFiLogicalOpPrediction::PREDICT_FALSE && !m_isAnd))
    {
        inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIPredictableLogicalAndOrImpl>::SelectBoilerplateBluePrint(
                        m_isAnd,
                        !spillLoc.IsNoSpill(),
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
    }
    else
    {
        inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIUnpredictableLogicalAndOrImpl>::SelectBoilerplateBluePrint(
                        m_isAnd,
                        !spillLoc.IsNoSpill(),
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
    }
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);

    TestAssert(!rhs.IsEmpty() && !rhs.IsUncontinuable());
    inst->PopulateBoilerplateFnPtrPlaceholder(1, rhs.m_entry);

}

}   // namespace PochiVM
