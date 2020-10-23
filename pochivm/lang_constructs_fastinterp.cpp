#include "fastinterp_ast_helper.hpp"
#include "codegen_context.hpp"
#include "arith_expr.h"
#include "logical_operator.h"
#include "destructor_helper.h"

namespace PochiVM
{

FastInterpSnippet AstDereferenceVariableExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    FINumOpaqueIntegralParams numOIP = FIOpaqueParamsHelper::GetMaxOIP();
    FINumOpaqueFloatingParams numOFP = FIOpaqueParamsHelper::GetMaxOFP();
    if (spillLoc.IsNoSpill())
    {
        if (GetTypeId().IsFloatingPoint())
        {
            numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
        }
        else
        {
            numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
        }
    }
    FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIDerefVariableImpl>::SelectBoilerplateBluePrint(
                    GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                    !spillLoc.IsNoSpill(),
                    numOIP,
                    numOFP));
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    inst->PopulateConstantPlaceholder<uint64_t>(1, m_operand->GetFastInterpOffset());
    return FastInterpSnippet {
        inst, inst
    };
}

void AstBlock::FastInterpSetupSpillLocation()
{
    for (AstNodeBase* stmt : m_contents)
    {
        stmt->FastInterpSetupSpillLocation();
    }
}

FastInterpSnippet WARN_UNUSED AstBlock::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());
    FastInterpSnippet result { nullptr, nullptr };
    for (AstNodeBase* stmt : m_contents)
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();
        FastInterpSnippet snippet = stmt->PrepareForFastInterp(x_FINoSpill);
        result = result.AddContinuation(snippet);
    }
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();
    return result;
}

void AstScope::FastInterpSetupSpillLocation()
{
    for (AstNodeBase* stmt : m_contents)
    {
        stmt->FastInterpSetupSpillLocation();
    }
}

FastInterpSnippet WARN_UNUSED AstScope::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());
    AutoScopedVariableManagerScope asvms(this);

    FastInterpSnippet result { nullptr, nullptr };
    for (AstNodeBase* stmt : m_contents)
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();
        FastInterpSnippet snippet = stmt->PrepareForFastInterp(x_FINoSpill);
        result = result.AddContinuation(snippet);
    }
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();

    if (!result.IsUncontinuable())
    {
        FastInterpSnippet dtorSequence = thread_pochiVMContext->m_scopedVariableManager.FIGenerateDestructorSequenceUntilScope(this);
        result = result.AddContinuation(dtorSequence);
    }

    const std::vector<DestructorIREmitter*>& list = thread_pochiVMContext->m_scopedVariableManager.GetObjectsInCurrentScope();
    for (auto rit = list.rbegin(); rit != list.rend(); rit++)
    {
        AstVariable* var = assert_cast<AstVariable*>(*rit);
        thread_pochiVMContext->m_fastInterpStackFrameManager->PopLocalVar(var->GetTypeId().RemovePointer());
    }
    return result;
}

// Returns a snippet where m_tail is a conditional branch instance,
// with all placeholders except the two branches populated.
// placeholder 0 is for true branch, placeholder 1 is for false branch.
//
// If 'isFavourTrueBranch' is true, it generates code that would be more efficient
// if the true branch is likely to be taken.
// Otherwise, it generates code that does not bias toward true or false branch.
//
template<bool isFavourTrueBranch>
static FastInterpSnippet WARN_UNUSED FIGenerateConditionalBranchHelper(AstNodeBase* cond)
{
    TestAssert(cond->GetTypeId().IsBool());
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();

    // Evaluate condition
    //
    if (cond->GetAstNodeType() == AstNodeType::AstComparisonExpr)
    {
        // Check for simple shape
        // FIFullyInlinedComparisonFavourTrueBranchImpl
        // FIFullyInlinedComparisonUnpredictableBranchImpl
        //
        AstComparisonExpr* expr = assert_cast<AstComparisonExpr*>(cond);
        TypeId cmpType = expr->m_lhs->GetTypeId();

        {
            AstFIOperandShape lhs = AstFIOperandShape::TryMatch(expr->m_lhs);
            if (lhs.MatchOK())
            {
                {
                    AstFIOperandShape rhs = AstFIOperandShape::TryMatch(expr->m_rhs);
                    if (rhs.MatchOK())
                    {
                        if (!(lhs.m_kind == FIOperandShapeCategory::LITERAL_NONZERO &&
                              rhs.m_kind == FIOperandShapeCategory::LITERAL_NONZERO))
                        {
                            using BoilerplateName = typename std::conditional<isFavourTrueBranch,
                                    FIFullyInlinedComparisonFavourTrueBranchImpl,
                                    FIFullyInlinedComparisonUnpredictableBranchImpl>::type;
                            FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                                        FastInterpBoilerplateLibrary<BoilerplateName>::SelectBoilerplateBluePrint(
                                            cmpType.GetOneLevelPtrFastInterpTypeId(),
                                            lhs.m_indexType,
                                            rhs.m_indexType,
                                            lhs.m_kind,
                                            rhs.m_kind,
                                            FIOpaqueParamsHelper::GetMaxOIP(),
                                            FIOpaqueParamsHelper::GetMaxOFP(),
                                            expr->m_op));
                            lhs.PopulatePlaceholder(condBrInst, 0, 1);
                            rhs.PopulatePlaceholder(condBrInst, 2, 3);
                            return FastInterpSnippet { condBrInst, condBrInst };
                        }
                    }
                }

                // FIPartialInlinedComparisonFavourTrueBranchImpl
                // FIPartialInlinedComparisonUnpredictableBranchImpl
                //
                TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(cmpType));
                FastInterpSnippet snippet = expr->m_rhs->PrepareForFastInterp(x_FINoSpill);

                FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
                FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
                TestAssert(numOIP == static_cast<FINumOpaqueIntegralParams>(0));
                TestAssert(numOFP == static_cast<FINumOpaqueFloatingParams>(0));
                if (cmpType.IsFloatingPoint())
                {
                    numOIP = FIOpaqueParamsHelper::GetMaxOIP();
                }
                else
                {
                    numOFP = FIOpaqueParamsHelper::GetMaxOFP();
                }
                using BoilerplateName = typename std::conditional<isFavourTrueBranch,
                        FIPartialInlinedComparisonFavourTrueBranchImpl,
                        FIPartialInlinedComparisonUnpredictableBranchImpl>::type;
                FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                            FastInterpBoilerplateLibrary<BoilerplateName>::SelectBoilerplateBluePrint(
                                cmpType.GetOneLevelPtrFastInterpTypeId(),
                                lhs.m_indexType,
                                lhs.m_kind,
                                true /*isInlinedSideLhs*/,
                                numOIP,
                                numOFP,
                                expr->m_op));
                lhs.PopulatePlaceholder(condBrInst, 0, 1);
                return snippet.AddContinuation(condBrInst);
            }
        }

        AstFIOperandShape rhs = AstFIOperandShape::TryMatch(expr->m_rhs);
        if (rhs.MatchOK())
        {
            // FIPartialInlinedComparisonFavourTrueBranchImpl
            // FIPartialInlinedComparisonUnpredictableBranchImpl
            //
            TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(cmpType));
            FastInterpSnippet snippet = expr->m_lhs->PrepareForFastInterp(x_FINoSpill);

            FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
            FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
            TestAssert(numOIP == static_cast<FINumOpaqueIntegralParams>(0));
            TestAssert(numOFP == static_cast<FINumOpaqueFloatingParams>(0));
            if (cmpType.IsFloatingPoint())
            {
                numOIP = FIOpaqueParamsHelper::GetMaxOIP();
            }
            else
            {
                numOFP = FIOpaqueParamsHelper::GetMaxOFP();
            }
            using BoilerplateName = typename std::conditional<isFavourTrueBranch,
                    FIPartialInlinedComparisonFavourTrueBranchImpl,
                    FIPartialInlinedComparisonUnpredictableBranchImpl>::type;
            FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<BoilerplateName>::SelectBoilerplateBluePrint(
                            cmpType.GetOneLevelPtrFastInterpTypeId(),
                            rhs.m_indexType,
                            rhs.m_kind,
                            false /*isInlinedSideLhs*/,
                            numOIP,
                            numOFP,
                            expr->m_op));
            rhs.PopulatePlaceholder(condBrInst, 0, 1);
            return snippet.AddContinuation(condBrInst);
        }
    }

    // General case: outlined conditional branch
    // FIOutlinedConditionalFavourTrueBranchImpl
    // FIOutlinedConditionalUnpredictableBranchImpl
    //
    {
        // If we predict the branch to be true, and the operator is a logical operator, populate the prediction
        //
        if (isFavourTrueBranch)
        {
            if (cond->GetAstNodeType() == AstNodeType::AstLogicalAndOrExpr)
            {
                AstLogicalAndOrExpr* expr = assert_cast<AstLogicalAndOrExpr*>(cond);
                if (expr->m_isAnd)
                {
                    expr->m_fiPrediction = AstFiLogicalOpPrediction::PREDICT_TRUE;
                }
            }
            else if (cond->GetAstNodeType() == AstNodeType::AstLogicalNotExpr)
            {
                AstLogicalNotExpr* expr = assert_cast<AstLogicalNotExpr*>(cond);
                expr->m_fiPrediction = AstFiLogicalOpPrediction::PREDICT_FALSE;
            }
        }

        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(TypeId::Get<bool>()));
        FastInterpSnippet snippet = cond->PrepareForFastInterp(x_FINoSpill);

        thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();
        using BoilerplateName = typename std::conditional<isFavourTrueBranch,
                FIOutlinedConditionalFavourTrueBranchImpl,
                FIOutlinedConditionalUnpredictableBranchImpl>::type;
        FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<BoilerplateName>::SelectBoilerplateBluePrint(
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
        return snippet.AddContinuation(condBrInst);
    }
}

void AstIfStatement::FastInterpSetupSpillLocation()
{
    m_condClause->FastInterpSetupSpillLocation();
    m_thenClause->FastInterpSetupSpillLocation();
    if (HasElseClause())
    {
        m_elseClause->FastInterpSetupSpillLocation();
    }
}

FastInterpSnippet WARN_UNUSED AstIfStatement::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());

    FastInterpSnippet condBrSnippet = FIGenerateConditionalBranchHelper<false /*favourTrueBranch*/>(m_condClause);
    TestAssert(!condBrSnippet.IsEmpty());
    FastInterpBoilerplateInstance* condBrInst = condBrSnippet.m_tail;
    TestAssert(condBrInst != nullptr);

    FastInterpSnippet thenClause = m_thenClause->PrepareForFastInterp(x_FINoSpill);
    FastInterpSnippet elseClause;
    if (HasElseClause())
    {
        elseClause = m_elseClause->PrepareForFastInterp(x_FINoSpill);
    }

    FastInterpBoilerplateInstance* join;
    bool needNoop = !thenClause.IsUncontinuable() && !elseClause.IsUncontinuable();
    needNoop |= thenClause.IsEmpty() || elseClause.IsEmpty();
    if (needNoop)
    {
        FastInterpBoilerplateInstance* noop = FIGetNoopBoilerplate();
        if (!thenClause.IsUncontinuable())
        {
            thenClause = thenClause.AddContinuation(noop);
        }
        if (!elseClause.IsUncontinuable())
        {
            elseClause = elseClause.AddContinuation(noop);
        }
        join = noop;
    }
    else
    {
        if (thenClause.IsUncontinuable())
        {
            TestAssert(!elseClause.IsEmpty());
            join = elseClause.m_tail;
        }
        else
        {
            TestAssert(elseClause.IsUncontinuable() && !thenClause.IsEmpty());
            join = thenClause.m_tail;
        }
    }

    TestAssert(!thenClause.IsEmpty() && !elseClause.IsEmpty());
    condBrInst->PopulateBoilerplateFnPtrPlaceholder(0, thenClause.m_entry);
    condBrInst->PopulateBoilerplateFnPtrPlaceholder(1, elseClause.m_entry);
    return FastInterpSnippet {
        condBrSnippet.m_entry, join
    };
}

void AstWhileLoop::FastInterpSetupSpillLocation()
{
    m_condClause->FastInterpSetupSpillLocation();
    m_body->FastInterpSetupSpillLocation();
}

FastInterpSnippet WARN_UNUSED AstWhileLoop::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());

    FastInterpSnippet condBrSnippet = FIGenerateConditionalBranchHelper<true /*favourTrueBranch*/>(m_condClause);
    TestAssert(condBrSnippet.m_entry != nullptr);

    FastInterpBoilerplateInstance* afterLoop = FIGetNoopBoilerplate();

    AutoScopedVarManagerFIBreakContinueTarget asvmfbct(afterLoop /*breakTarget*/,
                                                       m_body /*breakTargetScope*/,
                                                       condBrSnippet.m_entry /*continueTarget*/,
                                                       m_body /*continueTargetScope*/);

    FastInterpSnippet loopBody = m_body->PrepareForFastInterp(x_FINoSpill);
    if (loopBody.IsEmpty())
    {
        loopBody = loopBody.AddContinuation(FIGetNoopBoilerplate());
    }

    condBrSnippet.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, loopBody.m_entry);
    condBrSnippet.m_tail->PopulateBoilerplateFnPtrPlaceholder(1, afterLoop);
    if (!loopBody.IsUncontinuable())
    {
        loopBody.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, condBrSnippet.m_entry);
    }

    return FastInterpSnippet {
        condBrSnippet.m_entry, afterLoop
    };
}

void AstForLoop::FastInterpSetupSpillLocation()
{
    m_startClause->FastInterpSetupSpillLocation();
    m_condClause->FastInterpSetupSpillLocation();
    m_body->FastInterpSetupSpillLocation();
    m_stepClause->FastInterpSetupSpillLocation();
}

FastInterpSnippet WARN_UNUSED AstForLoop::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());

    AutoScopedVariableManagerScope asvms(this);

    FastInterpBoilerplateInstance* loopStepHead = FIGetNoopBoilerplate();
    FastInterpBoilerplateInstance* afterLoopHead = FIGetNoopBoilerplate();

    AutoScopedVarManagerFIBreakContinueTarget asvmfbct(afterLoopHead /*breakTarget*/,
                                                       m_body /*breakTargetScope*/,
                                                       loopStepHead /*continueTarget*/,
                                                       m_body /*continueTargetScope*/);

    FastInterpSnippet startClause = m_startClause->PrepareForFastInterp(x_FINoSpill);
    // We disallow break/continue/return in for-loop init-block
    //
    TestAssert(!startClause.IsUncontinuable());

#ifdef TESTBUILD
    // for-loop init-block is the only place that we allow declaring variables in the for-header
    // (the for-body is a separate scope). Although we have checked this condition in Validate(),
    // for sanity, here we assert again that no additional variables are declared since then.
    //
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    size_t numVarsInInitBlock = thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope();
#endif

    FastInterpSnippet condClause = FIGenerateConditionalBranchHelper<true /*favourTrueBranch*/>(m_condClause);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope() == numVarsInInitBlock);

    FastInterpSnippet loopBody = m_body->PrepareForFastInterp(x_FINoSpill);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope() == numVarsInInitBlock);
    if (loopBody.IsEmpty())
    {
        loopBody = loopBody.AddContinuation(FIGetNoopBoilerplate());
    }

    FastInterpSnippet loopStep = m_stepClause->PrepareForFastInterp(x_FINoSpill);
    // We disallow break/continue/return in for-loop step-block
    //
    TestAssert(!loopStep.IsUncontinuable());
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope() == numVarsInInitBlock);
    loopStep = FastInterpSnippet(loopStepHead, loopStepHead).AddContinuation(loopStep);

    // Call destructors for variables declared in for-loop init-block
    //
    TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetCurrentScope() == this);
    FastInterpSnippet afterLoop = thread_pochiVMContext->m_scopedVariableManager.FIGenerateDestructorSequenceUntilScope(this);
    afterLoop = FastInterpSnippet(afterLoopHead, afterLoopHead).AddContinuation(afterLoop);

    // Pop off the variable scope
    //
    const std::vector<DestructorIREmitter*>& list = thread_pochiVMContext->m_scopedVariableManager.GetObjectsInCurrentScope();
    for (auto rit = list.rbegin(); rit != list.rend(); rit++)
    {
        AstVariable* var = assert_cast<AstVariable*>(*rit);
        thread_pochiVMContext->m_fastInterpStackFrameManager->PopLocalVar(var->GetTypeId().RemovePointer());
    }

    // Now link everything together
    //
    TestAssert(!startClause.IsUncontinuable());
    TestAssert(!condClause.IsEmpty() && !condClause.IsUncontinuable());
    TestAssert(!loopBody.IsEmpty());
    TestAssert(!loopStep.IsEmpty() && !loopStep.IsUncontinuable());
    TestAssert(!afterLoop.IsEmpty() && !afterLoop.IsUncontinuable());
    condClause.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, loopBody.m_entry);
    condClause.m_tail->PopulateBoilerplateFnPtrPlaceholder(1, afterLoop.m_entry);
    if (!loopBody.IsUncontinuable())
    {
        loopBody.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, loopStep.m_entry);
    }
    loopStep.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, condClause.m_entry);

    if (startClause.IsEmpty())
    {
        return FastInterpSnippet {
            condClause.m_entry, afterLoop.m_tail
        };
    }
    else
    {
        startClause.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, condClause.m_entry);
        return FastInterpSnippet {
            startClause.m_entry, afterLoop.m_tail
        };
    }
}

FastInterpSnippet WARN_UNUSED AstBreakOrContinueStmt::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());

    std::pair<FastInterpBoilerplateInstance* /*branchTarget*/, AstNodeBase* /*scopeBoundary*/> target;
    if (IsBreakStatement())
    {
        target = thread_pochiVMContext->m_scopedVariableManager.GetFIBreakTarget();
    }
    else
    {
        target = thread_pochiVMContext->m_scopedVariableManager.GetFIContinueTarget();
    }

    FastInterpSnippet snippet = thread_pochiVMContext->m_scopedVariableManager.FIGenerateDestructorSequenceUntilScope(target.second);
    TestAssert(!snippet.IsUncontinuable());
    if (snippet.IsEmpty())
    {
        snippet = snippet.AddContinuation(FIGetNoopBoilerplate());
    }
    snippet.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, target.first);
    return FastInterpSnippet {
        snippet.m_entry, nullptr
    };
}

}   // namespace PochiVM
