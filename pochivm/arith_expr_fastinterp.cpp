#include "arith_expr.h"
#include "fastinterp_ast_helper.hpp"

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

        FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
        FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
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
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
        return FastInterpSnippet {
            inst, inst
        };
    }

match_case2:;
    // Case 2: partially inline shape (LHS/RHS)
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
            if (matchSide.MatchOK() && !(matchSide.m_kind == FIOperandShapeCategory::ZERO &&
                                         (m_op == AstArithmeticExprType::MOD || m_op == AstArithmeticExprType::DIV)))
            {
                isInlineSideLhs = false;
            }
            else
            {
                goto match_case3;
            }
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

        FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
        FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();

        if (GetTypeId().IsFloatingPoint())
        {
            numOIP = FIOpaqueParamsHelper::GetMaxOIP();
        }
        else
        {
            numOFP = FIOpaqueParamsHelper::GetMaxOFP();
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
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
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

        FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
        FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                        GetTypeId().GetDefaultFastInterpTypeId(),
                        m_op,
                        lhsSpillLoc.IsNoSpill(),
                        !spillLoc.IsNoSpill(),
                        numOIP,
                        numOFP));
        lhsSpillLoc.PopulatePlaceholderIfSpill(inst, 1);
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
        return lhs.AddContinuation(rhs).AddContinuation(inst);
    }
}

FastInterpSnippet WARN_UNUSED AstComparisonExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    // Case 1: fully inline shape
    // FIFullyInlinedComparisonExprImpl
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
        if (lhs.m_kind == FISimpleOperandShapeCategory::LITERAL_NONZERO &&
            rhs.m_kind == FISimpleOperandShapeCategory::LITERAL_NONZERO)
        {
            goto match_case2;
        }

        FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIFullyInlinedComparisonExprImpl>::SelectBoilerplateBluePrint(
                        GetTypeId().GetDefaultFastInterpTypeId(),
                        lhs.m_kind,
                        rhs.m_kind,
                        m_op,
                        !spillLoc.IsNoSpill(),
                        numOIP,
                        FIOpaqueParamsHelper::GetMaxOFP()));
        lhs.PopulatePlaceholder(inst, 1);
        rhs.PopulatePlaceholder(inst, 2);
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
        return FastInterpSnippet {
            inst, inst
        };
    }

match_case2:;
    // Case 2: partially inline shape (LHS/RHS)
    // FIPartialInlinedComparisonExprImpl
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

        FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
        FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
        if (!GetTypeId().IsFloatingPoint())
        {
            numOFP = FIOpaqueParamsHelper::GetMaxOFP();
        }

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIPartialInlinedComparisonExprImpl>::SelectBoilerplateBluePrint(
                        GetTypeId().GetDefaultFastInterpTypeId(),
                        matchSide.m_indexType,
                        matchSide.m_kind,
                        !spillLoc.IsNoSpill(),
                        isInlineSideLhs,
                        numOIP,
                        numOFP,
                        m_op));
       matchSide.PopulatePlaceholder(inst, 1, 2);
       spillLoc.PopulatePlaceholderIfSpill(inst, 0);
       return outlineSide.AddContinuation(inst);
    }

match_case3:;
    // Case default: fully outlined shape
    // FIOutlinedComparisonExprImpl
    //
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(GetTypeId());
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
        FastInterpSnippet rhs = m_rhs->PrepareForFastInterp(x_FINoSpill);
        FISpillLocation lhsSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(GetTypeId());
        FastInterpSnippet lhs = m_lhs->PrepareForFastInterp(lhsSpillLoc);

        FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
        FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
        if (!GetTypeId().IsFloatingPoint())
        {
            numOFP = FIOpaqueParamsHelper::GetMaxOFP();
        }

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIOutlinedComparisonExprImpl>::SelectBoilerplateBluePrint(
                        GetTypeId().GetDefaultFastInterpTypeId(),
                        m_op,
                        lhsSpillLoc.IsNoSpill(),
                        !spillLoc.IsNoSpill(),
                        numOIP,
                        numOFP));
        lhsSpillLoc.PopulatePlaceholderIfSpill(inst, 1);
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
        return lhs.AddContinuation(rhs).AddContinuation(inst);
    }
}

}   // namespace PochiVM
