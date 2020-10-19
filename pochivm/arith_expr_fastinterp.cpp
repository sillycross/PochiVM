#include "arith_expr.h"
#include "fastinterp_ast_helper.hpp"

namespace PochiVM
{

void AstArithmeticExpr::FastInterpSetupSpillLocation()
{
    TestAssert(m_fiInlineShape == FIShape::INVALID);
    Auto(TestAssert(m_fiInlineShape != FIShape::INVALID));

    // Case 1: fully inline shape
    // FIFullyInlinedArithmeticExprImpl
    //
    {
        AstFISimpleOperandShape lhs = AstFISimpleOperandShape::TryMatch(m_lhs);
        if (lhs.MatchOK())
        {
            AstFISimpleOperandShape rhs = AstFISimpleOperandShape::TryMatch(m_rhs);
            if (rhs.MatchOK())
            {
                if (!(rhs.m_kind == FISimpleOperandShapeCategory::ZERO &&
                      (m_op == AstArithmeticExprType::MOD || m_op == AstArithmeticExprType::DIV)))
                {
                    m_fiInlineShape = FIShape::INLINE_BOTH;
                    return;
                }
            }
        }
    }

    // Case 2: partially inline shape (LHS/RHS)
    // FIPartialInlineArithmeticExprImpl
    //
    {
        AstFIOperandShape matchSide = AstFIOperandShape::TryMatch(m_lhs);
        if (matchSide.MatchOK())
        {
            m_fiInlineShape = FIShape::INLINE_LHS;
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
            m_rhs->FastInterpSetupSpillLocation();
            return;
        }

        matchSide = AstFIOperandShape::TryMatch(m_rhs);
        if (matchSide.MatchOK() && !(matchSide.m_kind == FIOperandShapeCategory::ZERO &&
                                     (m_op == AstArithmeticExprType::MOD || m_op == AstArithmeticExprType::DIV)))
        {
            m_fiInlineShape = FIShape::INLINE_RHS;
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
            m_lhs->FastInterpSetupSpillLocation();
            return;
        }
    }

    // Case default: fully outlined shape
    // FIOutlinedArithmeticExprImpl
    //
    {
        m_fiInlineShape = FIShape::OUTLINE;
        thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(GetTypeId());
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
        m_rhs->FastInterpSetupSpillLocation();
        FISpillLocation spillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(GetTypeId());
        m_fiIsLhsSpill = !spillLoc.IsNoSpill();
        m_lhs->FastInterpSetupSpillLocation();
    }
}

FastInterpSnippet WARN_UNUSED AstArithmeticExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    if (m_fiInlineShape == FIShape::INLINE_BOTH)
    {
        // Case 1: fully inline shape
        // FIFullyInlinedArithmeticExprImpl
        //
        AstFISimpleOperandShape lhs = AstFISimpleOperandShape::TryMatch(m_lhs);
        TestAssert(lhs.MatchOK());
        AstFISimpleOperandShape rhs = AstFISimpleOperandShape::TryMatch(m_rhs);
        TestAssert(rhs.MatchOK());
        TestAssert(!(rhs.m_kind == FISimpleOperandShapeCategory::ZERO &&
            (m_op == AstArithmeticExprType::MOD || m_op == AstArithmeticExprType::DIV)));

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
    else if (m_fiInlineShape == FIShape::INLINE_LHS || m_fiInlineShape == FIShape::INLINE_RHS)
    {
        bool isInlineSideLhs = (m_fiInlineShape == FIShape::INLINE_LHS);
        AstFIOperandShape matchSide;
        if (m_fiInlineShape == FIShape::INLINE_LHS)
        {
            matchSide = AstFIOperandShape::TryMatch(m_lhs);
            TestAssert(matchSide.MatchOK());
        }
        else
        {
            matchSide = AstFIOperandShape::TryMatch(m_rhs);
            TestAssert(matchSide.MatchOK() && !(matchSide.m_kind == FIOperandShapeCategory::ZERO &&
                                                (m_op == AstArithmeticExprType::MOD || m_op == AstArithmeticExprType::DIV)));
        }

        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(GetTypeId()));
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
    else
    {
        // Case default: fully outlined shape
        // FIOutlinedArithmeticExprImpl
        //
        TestAssert(m_fiInlineShape == FIShape::OUTLINE);
        thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(GetTypeId(), m_fiIsLhsSpill);
        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(GetTypeId()));
        FastInterpSnippet rhs = m_rhs->PrepareForFastInterp(x_FINoSpill);
        FISpillLocation lhsSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(GetTypeId());
        TestAssertIff(m_fiIsLhsSpill, !lhsSpillLoc.IsNoSpill());
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

void AstComparisonExpr::FastInterpSetupSpillLocation()
{
    TestAssert(m_fiInlineShape == FIShape::INVALID);
    Auto(TestAssert(m_fiInlineShape != FIShape::INVALID));

    // Case 1: fully inline shape
    // FIFullyInlinedComparisonExprImpl
    //
    {
        AstFISimpleOperandShape lhs = AstFISimpleOperandShape::TryMatch(m_lhs);
        if (lhs.MatchOK())
        {
            AstFISimpleOperandShape rhs = AstFISimpleOperandShape::TryMatch(m_rhs);
            if (rhs.MatchOK())
            {
                if (!(lhs.m_kind == FISimpleOperandShapeCategory::LITERAL_NONZERO &&
                      rhs.m_kind == FISimpleOperandShapeCategory::LITERAL_NONZERO))
                {
                    m_fiInlineShape = FIShape::INLINE_BOTH;
                    return;
                }
            }
        }
    }

    // Case 2: partially inline shape (LHS/RHS)
    // FIPartialInlinedComparisonExprImpl
    //
    {
        AstFIOperandShape matchSide = AstFIOperandShape::TryMatch(m_lhs);
        if (matchSide.MatchOK())
        {
            m_fiInlineShape = FIShape::INLINE_LHS;
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
            m_rhs->FastInterpSetupSpillLocation();
            return;
        }


        matchSide = AstFIOperandShape::TryMatch(m_rhs);
        if (matchSide.MatchOK())
        {
            m_fiInlineShape = FIShape::INLINE_RHS;
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
            m_lhs->FastInterpSetupSpillLocation();
            return;
        }
    }

    // Case default: fully outlined shape
    // FIOutlinedComparisonExprImpl
    //
    {
        m_fiInlineShape = FIShape::OUTLINE;
        thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(GetTypeId());
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
        m_rhs->FastInterpSetupSpillLocation();
        FISpillLocation lhsSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(GetTypeId());
        m_fiIsLhsSpill = !lhsSpillLoc.IsNoSpill();
        m_lhs->FastInterpSetupSpillLocation();
    }
}

FastInterpSnippet WARN_UNUSED AstComparisonExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    TestAssert(m_fiInlineShape != FIShape::INVALID);

    if (m_fiInlineShape == FIShape::INLINE_BOTH)
    {
        // Case 1: fully inline shape
        // FIFullyInlinedComparisonExprImpl
        //
        AstFISimpleOperandShape lhs = AstFISimpleOperandShape::TryMatch(m_lhs);
        TestAssert(lhs.MatchOK());
        AstFISimpleOperandShape rhs = AstFISimpleOperandShape::TryMatch(m_rhs);
        TestAssert(rhs.MatchOK());
        TestAssert(!(lhs.m_kind == FISimpleOperandShapeCategory::LITERAL_NONZERO &&
                     rhs.m_kind == FISimpleOperandShapeCategory::LITERAL_NONZERO));

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
    else if (m_fiInlineShape == FIShape::INLINE_LHS || m_fiInlineShape == FIShape::INLINE_RHS)
    {
        // Case 2: partially inline shape (LHS/RHS)
        // FIPartialInlinedComparisonExprImpl
        //
        bool isInlineSideLhs = (m_fiInlineShape == FIShape::INLINE_LHS);
        AstFIOperandShape matchSide;
        if (m_fiInlineShape == FIShape::INLINE_LHS)
        {
            matchSide = AstFIOperandShape::TryMatch(m_lhs);
            TestAssert(matchSide.MatchOK());
        }
        else
        {
            matchSide = AstFIOperandShape::TryMatch(m_rhs);
            TestAssert(matchSide.MatchOK());
        }

        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(GetTypeId()));
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
    else
    {
        // Case default: fully outlined shape
        // FIOutlinedComparisonExprImpl
        //
        TestAssert(m_fiInlineShape == FIShape::OUTLINE);

        thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(GetTypeId(), m_fiIsLhsSpill);
        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(GetTypeId()));
        FastInterpSnippet rhs = m_rhs->PrepareForFastInterp(x_FINoSpill);
        FISpillLocation lhsSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(GetTypeId());
        TestAssertIff(m_fiIsLhsSpill, !lhsSpillLoc.IsNoSpill());
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
