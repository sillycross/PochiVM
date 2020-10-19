#include "fastinterp_ast_helper.hpp"
#include "arith_expr.h"

namespace PochiVM
{

void AstDereferenceExpr::FastInterpSetupSpillLocation()
{
    thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId().AddPointer());
    m_operand->FastInterpSetupSpillLocation();
}

FastInterpSnippet WARN_UNUSED AstDereferenceExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    // TODO: support dereference with offset
    //
    TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(GetTypeId().AddPointer()));
    FastInterpSnippet snippet = m_operand->PrepareForFastInterp(x_FINoSpill);
    FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
    if (!spillLoc.IsNoSpill() || !GetTypeId().IsFloatingPoint())
    {
        numOFP = FIOpaqueParamsHelper::GetMaxOFP();
    }
    FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineRhsDereferenceImpl>::SelectBoilerplateBluePrint(
                    GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::ZERO,
                    !spillLoc.IsNoSpill(),
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                    numOFP));
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    return snippet.AddContinuation(inst);
}

FastInterpSnippet WARN_UNUSED AstLiteralExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    bool isAllBitsZero = IsAllBitsZero();
    FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
    FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
    if (!spillLoc.IsNoSpill())
    {
        numOIP = FIOpaqueParamsHelper::GetMaxOIP();
        numOFP = FIOpaqueParamsHelper::GetMaxOFP();
    }
    else if (GetTypeId().IsFloatingPoint())
    {
        numOIP = FIOpaqueParamsHelper::GetMaxOIP();
    }
    else
    {
        numOFP = FIOpaqueParamsHelper::GetMaxOFP();
    }

    FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                    isAllBitsZero,
                    !spillLoc.IsNoSpill(),
                    numOIP,
                    numOFP));
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    if (!isAllBitsZero)
    {
        inst->PopulateConstantPlaceholder<uint64_t>(1, m_as_uint64_t);
    }
    return FastInterpSnippet { inst, inst };
}

void AstAssignExpr::FastInterpSetupSpillLocation()
{
    TestAssert(m_fiInlineShape == FIShape::INVALID);
    Auto(TestAssert(m_fiInlineShape != FIShape::INVALID));

    // Case var = osc op osc
    // FIFullyInlineAssignArithExprImpl
    //
    {
        if (m_dst->GetAstNodeType() == AstNodeType::AstVariable && m_src->GetAstNodeType() == AstNodeType::AstArithmeticExpr &&
            (!m_src->GetTypeId().IsPointerType() && !m_src->GetTypeId().IsBool()))
        {
            AstArithmeticExpr* expr = assert_cast<AstArithmeticExpr*>(m_src);
            AstFIOperandShape lhs = AstFIOperandShape::TryMatch(expr->m_lhs);
            if (lhs.MatchOK())
            {
                AstFIOperandShape rhs = AstFIOperandShape::TryMatch(expr->m_rhs);
                if (rhs.MatchOK())
                {
                    m_fiInlineShape = FIShape::INLINE_ARITH;
                    return;
                }
            }
        }
    }

    // Other cases
    //
    {
        AstFIOperandShape lhs = AstFIOperandShape::TryMatch(m_dst);
        bool lhsMatchOk = lhs.MatchOK() && lhs.m_kind != FIOperandShapeCategory::ZERO && lhs.m_kind != FIOperandShapeCategory::LITERAL_NONZERO;
        AstFIOperandShape rhs = AstFIOperandShape::TryMatch(m_src);

        if (lhsMatchOk && rhs.MatchOK())
        {
            // FIFullyInlineAssignImpl
            //
            m_fiInlineShape = FIShape::INLINE_BOTH;
            return;
        }
        else if (lhsMatchOk)
        {
            // FIPartialInlineLhsAssignImpl
            //
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_src->GetTypeId());
            m_src->FastInterpSetupSpillLocation();
            m_fiInlineShape = FIShape::INLINE_LHS;
            return;
        }
        else if (rhs.MatchOK())
        {
            // FIPartialInlineRhsAssignImpl
            //
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_dst->GetTypeId());
            m_dst->FastInterpSetupSpillLocation();
            m_fiInlineShape = FIShape::INLINE_RHS;
            return;
        }
        else
        {
            // FIOutlinedAssignImpl
            //
            thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(m_dst->GetTypeId());
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_src->GetTypeId());
            m_src->FastInterpSetupSpillLocation();
            FISpillLocation lhsSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(m_dst->GetTypeId());
            m_fiIsLhsSpill = !lhsSpillLoc.IsNoSpill();
            m_dst->FastInterpSetupSpillLocation();
            m_fiInlineShape = FIShape::OUTLINE;
            return;
        }
    }
}

FastInterpSnippet WARN_UNUSED AstAssignExpr::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());
    TestAssert(m_fiInlineShape != FIShape::INVALID);

    if (m_fiInlineShape == FIShape::INLINE_ARITH)
    {
        // Case var = osc op osc
        // FIFullyInlineAssignArithExprImpl
        //
        TestAssert(m_dst->GetAstNodeType() == AstNodeType::AstVariable && m_src->GetAstNodeType() == AstNodeType::AstArithmeticExpr &&
                   (!m_src->GetTypeId().IsPointerType() && !m_src->GetTypeId().IsBool()));
        AstArithmeticExpr* expr = assert_cast<AstArithmeticExpr*>(m_src);
        AstFIOperandShape lhs = AstFIOperandShape::TryMatch(expr->m_lhs);
        TestAssert(lhs.MatchOK());
        AstFIOperandShape rhs = AstFIOperandShape::TryMatch(expr->m_rhs);
        TestAssert(rhs.MatchOK());
        AstVariable* var = assert_cast<AstVariable*>(m_dst);
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIFullyInlineAssignArithExprImpl>::SelectBoilerplateBluePrint(
                        m_src->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        lhs.m_indexType,
                        rhs.m_indexType,
                        lhs.m_kind,
                        rhs.m_kind,
                        FIOpaqueParamsHelper::GetMaxOIP(),
                        FIOpaqueParamsHelper::GetMaxOFP(),
                        expr->m_op));
        inst->PopulateConstantPlaceholder<uint64_t>(0, var->GetFastInterpOffset());
        lhs.PopulatePlaceholder(inst, 1, 2);
        rhs.PopulatePlaceholder(inst, 3, 4);
        return FastInterpSnippet {
            inst, inst
        };
    }
    else if (m_fiInlineShape == FIShape::INLINE_BOTH)
    {
        AstFIOperandShape lhs = AstFIOperandShape::TryMatch(m_dst);
        TestAssert(lhs.MatchOK() && lhs.m_kind != FIOperandShapeCategory::ZERO && lhs.m_kind != FIOperandShapeCategory::LITERAL_NONZERO);
        AstFIOperandShape rhs = AstFIOperandShape::TryMatch(m_src);
        TestAssert(rhs.MatchOK());

        // FIFullyInlineAssignImpl
        //
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIFullyInlineAssignImpl>::SelectBoilerplateBluePrint(
                        m_src->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        lhs.m_indexType,
                        rhs.m_indexType,
                        lhs.m_kind,
                        rhs.m_kind,
                        FIOpaqueParamsHelper::GetMaxOIP(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
        lhs.PopulatePlaceholder(inst, 0, 1);
        rhs.PopulatePlaceholder(inst, 2, 3);
        return FastInterpSnippet {
            inst, inst
        };
    }
    else if (m_fiInlineShape == FIShape::INLINE_LHS)
    {
        // FIPartialInlineLhsAssignImpl
        //
        AstFIOperandShape lhs = AstFIOperandShape::TryMatch(m_dst);
        TestAssert(lhs.MatchOK() && lhs.m_kind != FIOperandShapeCategory::ZERO && lhs.m_kind != FIOperandShapeCategory::LITERAL_NONZERO);

        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_src->GetTypeId()));
        FastInterpSnippet snippet = m_src->PrepareForFastInterp(x_FINoSpill);

        FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
        FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
        if (m_src->GetTypeId().IsFloatingPoint())
        {
            numOIP = FIOpaqueParamsHelper::GetMaxOIP();
        }
        else
        {
            numOFP = FIOpaqueParamsHelper::GetMaxOFP();
        }
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIPartialInlineLhsAssignImpl>::SelectBoilerplateBluePrint(
                        m_src->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        lhs.m_indexType,
                        lhs.m_kind,
                        numOIP,
                        numOFP));
        lhs.PopulatePlaceholder(inst, 0, 1);
        return snippet.AddContinuation(inst);
    }
    else if (m_fiInlineShape == FIShape::INLINE_RHS)
    {
        // FIPartialInlineRhsAssignImpl
        //
        AstFIOperandShape rhs = AstFIOperandShape::TryMatch(m_src);
        TestAssert(rhs.MatchOK());
        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_dst->GetTypeId()));

        FastInterpSnippet snippet = m_dst->PrepareForFastInterp(x_FINoSpill);

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIPartialInlineRhsAssignImpl>::SelectBoilerplateBluePrint(
                        m_src->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        rhs.m_indexType,
                        rhs.m_kind,
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
        rhs.PopulatePlaceholder(inst, 0, 1);
        return snippet.AddContinuation(inst);
    }
    else
    {
        // FIOutlinedAssignImpl
        //
        TestAssert(m_fiInlineShape == FIShape::OUTLINE);
        thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(m_dst->GetTypeId(), m_fiIsLhsSpill);
        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_src->GetTypeId()));
        FastInterpSnippet rhsSnippet = m_src->PrepareForFastInterp(x_FINoSpill);
        FISpillLocation lhsSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(m_dst->GetTypeId());
        TestAssertIff(m_fiIsLhsSpill, !lhsSpillLoc.IsNoSpill());
        FastInterpSnippet lhsSnippet = m_dst->PrepareForFastInterp(lhsSpillLoc);

        FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
        if (!m_src->GetTypeId().IsFloatingPoint())
        {
            numOFP = FIOpaqueParamsHelper::GetMaxOFP();
        }
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIOutlinedAssignImpl>::SelectBoilerplateBluePrint(
                        m_src->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        lhsSpillLoc.IsNoSpill(),
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        numOFP));
        lhsSpillLoc.PopulatePlaceholderIfSpill(inst, 0);
        return lhsSnippet.AddContinuation(rhsSnippet).AddContinuation(inst);
    }
}

}   // namespace PochiVM
