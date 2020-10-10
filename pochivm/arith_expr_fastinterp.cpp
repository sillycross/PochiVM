#include "arith_expr.h"
#include "fastinterp/fastinterp.h"
#include "fastinterp_ast_helper.h"
#include "fastinterp_snippet.hpp"

namespace PochiVM
{

FastInterpSnippet WARN_UNUSED AstArithmeticExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    // Case 1: fully inline shape
    // FIFullyInlinedArithmeticExprImpl
    //
    {
        AstFISimpleOperandShape lhs = AstFISimpleOperandShape::TryMatch(m_lhs);
        if (!lhs.MatchOK())
        {
            goto match_case2;
        }
        AstFISimpleOperandShape rhs = AstFISimpleOperandShape::TryMatch(m_rhs);
        if (!rhs.MatchOK())
        {
            goto match_case2;
        }
        if (rhs.m_kind == FISimpleOperandShapeCategory::ZERO &&
            (m_op == AstArithmeticExprType::MOD || m_op == AstArithmeticExprType::DIV))
        {
            goto match_case2;
        }

        FINumOpaqueIntegralParams numOIP = static_cast<FINumOpaqueIntegralParams>(
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral());
        FINumOpaqueFloatingParams numOFP = static_cast<FINumOpaqueFloatingParams>(
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat());
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                        GetTypeId().GetDefaultFastInterpTypeId(),
                        lhs.m_kind,
                        rhs.m_kind,
                        m_op,
                        !spillLoc.IsNoSpill(),
                        numOIP,
                        numOFP));
        lhs.PopulatePlaceholder(inst, 1);
        rhs.PopulatePlaceholder(inst, 2);
        if (!spillLoc.IsNoSpill())
        {
            inst->PopulateConstantPlaceholder<uint64_t>(0, spillLoc.GetSpillLocation());
        }
        return FastInterpSnippet {
            inst, inst
        };
    }

match_case2:;
    // Case 2: partially inline shape (LHS)
    // FIPartialInlineArithmeticExprImpl
    //
    {
        bool isInlineSideLhs;
        AstFIOperandShape matchSide = AstFIOperandShape::TryMatch(m_lhs);
        if (matchSide.MatchOK())
        {
            isInlineSideLhs = true;
        }
        else
        {
            matchSide = AstFIOperandShape::TryMatch(m_rhs);
            if (matchSide.MatchOK())
            {
                isInlineSideLhs = false;
            }
            else
            {
                goto match_case3;
            }
        }

        FINumOpaqueIntegralParams numOIP = static_cast<FINumOpaqueIntegralParams>(
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral());
        FINumOpaqueFloatingParams numOFP = static_cast<FINumOpaqueFloatingParams>(
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat());

        if (GetTypeId().IsFloatingPoint())
        {
            numOIP = FIOpaqueParamsHelper::GetMaxOIP();
        }
        else
        {
            numOFP = FIOpaqueParamsHelper::GetMaxOFP();
        }

        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
        FastInterpSnippet outlineSide;
        if (isInlineSideLhs)
        {
            outlineSide = m_rhs->PrepareForFastInterp(x_FINoSpill);
        }
        else
        {
            outlineSide = m_lhs->PrepareForFastInterp(x_FINoSpill);
        }

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIPartialInlineArithmeticExprImpl>::SelectBoilerplateBluePrint(
                        GetTypeId().GetDefaultFastInterpTypeId(),
                        matchSide.m_indexType,
                        matchSide.m_kind,
                        isInlineSideLhs,
                        !spillLoc.IsNoSpill(),
                        numOIP,
                        numOFP,
                        m_op));
        matchSide.PopulatePlaceholder(inst, 1, 2);
        if (!spillLoc.IsNoSpill())
        {
            inst->PopulateConstantPlaceholder<uint64_t>(0, spillLoc.GetSpillLocation());
        }
        return outlineSide.AddContinuation(inst);
    }

match_case3:;
    // Case default: fully outlined shape
    // FIOutlinedArithmeticExprImpl
    //
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(GetTypeId());
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
        FastInterpSnippet rhs = m_rhs->PrepareForFastInterp(x_FINoSpill);
        FISpillLocation lhsSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(GetTypeId());
        FastInterpSnippet lhs = m_lhs->PrepareForFastInterp(lhsSpillLoc);

        FINumOpaqueIntegralParams numOIP = static_cast<FINumOpaqueIntegralParams>(
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral());
        FINumOpaqueFloatingParams numOFP = static_cast<FINumOpaqueFloatingParams>(
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat());

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                        GetTypeId().GetDefaultFastInterpTypeId(),
                        m_op,
                        lhsSpillLoc.IsNoSpill(),
                        !spillLoc.IsNoSpill(),
                        numOIP,
                        numOFP));
        if (!lhsSpillLoc.IsNoSpill())
        {
            inst->PopulateConstantPlaceholder<uint64_t>(1, lhsSpillLoc.GetSpillLocation());
        }
        if (!spillLoc.IsNoSpill())
        {
            inst->PopulateConstantPlaceholder<uint64_t>(0, spillLoc.GetSpillLocation());
        }
        return lhs.AddContinuation(rhs).AddContinuation(inst);
    }
}

}   // namespace PochiVM
