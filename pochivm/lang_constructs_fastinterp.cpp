#include "fastinterp_ast_helper.hpp"
#include "codegen_context.hpp"
#include "arith_expr.h"

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

FastInterpSnippet WARN_UNUSED AstBlock::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());
    FastInterpSnippet result { nullptr, nullptr };
    for (AstNodeBase* stmt : m_contents)
    {
        FastInterpSnippet snippet = stmt->PrepareForFastInterp(x_FINoSpill);
        result = result.AddContinuation(snippet);
    }
    return result;
}

FastInterpSnippet WARN_UNUSED AstScope::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());
    thread_llvmContext->m_scopeStack.push_back(std::make_pair(this, std::vector<DestructorIREmitter*>()));

    FastInterpSnippet result { nullptr, nullptr };
    for (AstNodeBase* stmt : m_contents)
    {
        FastInterpSnippet snippet = stmt->PrepareForFastInterp(x_FINoSpill);
        result = result.AddContinuation(snippet);
    }

    // TODO: call destructors
    //

    thread_llvmContext->PopVariableScope(this);
    return result;
}

FastInterpSnippet WARN_UNUSED AstIfStatement::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());

    // After evaluating condition (at label 'after_eval_cond'), the m_tail should be a conditional branch instance,
    // with all placeholders except the two branches populated
    //
    FastInterpSnippet condBrSnippet;

    // Evaluate condition
    //
    if (m_condClause->GetAstNodeType() == AstNodeType::AstComparisonExpr)
    {
        // Check for simple shape
        // FIFullyInlinedComparisonBranchImpl
        //
        AstComparisonExpr* expr = assert_cast<AstComparisonExpr*>(m_condClause);
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
                            FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                                        FastInterpBoilerplateLibrary<FIFullyInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
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
                            condBrSnippet = FastInterpSnippet { condBrInst, condBrInst };
                            goto after_eval_cond;
                        }
                    }
                }

                // FIPartialInlinedComparisonBranchImpl
                //
                thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(cmpType);
                FastInterpSnippet snippet = expr->m_rhs->PrepareForFastInterp(x_FINoSpill);

                FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
                FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
                if (cmpType.IsFloatingPoint())
                {
                    numOIP = FIOpaqueParamsHelper::GetMaxOIP();
                }
                else
                {
                    numOFP = FIOpaqueParamsHelper::GetMaxOFP();
                }
                FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                            FastInterpBoilerplateLibrary<FIPartialInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                                cmpType.GetOneLevelPtrFastInterpTypeId(),
                                lhs.m_indexType,
                                lhs.m_kind,
                                true /*isInlinedSideLhs*/,
                                numOIP,
                                numOFP,
                                expr->m_op));
                lhs.PopulatePlaceholder(condBrInst, 0, 1);
                condBrSnippet = snippet.AddContinuation(condBrInst);
                goto after_eval_cond;
            }
        }

        AstFIOperandShape rhs = AstFIOperandShape::TryMatch(expr->m_rhs);
        if (rhs.MatchOK())
        {
            // FIPartialInlinedComparisonBranchImpl
            //
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(cmpType);
            FastInterpSnippet snippet = expr->m_lhs->PrepareForFastInterp(x_FINoSpill);

            FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
            FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
            if (cmpType.IsFloatingPoint())
            {
                numOIP = FIOpaqueParamsHelper::GetMaxOIP();
            }
            else
            {
                numOFP = FIOpaqueParamsHelper::GetMaxOFP();
            }
            FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIPartialInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                            cmpType.GetOneLevelPtrFastInterpTypeId(),
                            rhs.m_indexType,
                            rhs.m_kind,
                            false /*isInlinedSideLhs*/,
                            numOIP,
                            numOFP,
                            expr->m_op));
            rhs.PopulatePlaceholder(condBrInst, 0, 1);
            condBrSnippet = snippet.AddContinuation(condBrInst);
            goto after_eval_cond;
        }
    }

    // General case: outlined conditional branch
    // FIOutlinedConditionalBranchImpl
    //
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(TypeId::Get<bool>());
        FastInterpSnippet snippet = m_condClause->PrepareForFastInterp(x_FINoSpill);
        FastInterpBoilerplateInstance* condBrInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIOutlinedConditionalBranchImpl>::SelectBoilerplateBluePrint(
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
        condBrSnippet = snippet.AddContinuation(condBrInst);
    }

after_eval_cond:;

    // placeholder 0 is true branch, 1 is false branch
    //
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
    bool needNoop = !thenClause.IsMustReturn() && !elseClause.IsMustReturn();
    needNoop |= thenClause.IsEmpty() || elseClause.IsEmpty();
    if (needNoop)
    {
        FastInterpBoilerplateInstance* noop = FIGetNoopBoilerplate();
        if (!thenClause.IsMustReturn())
        {
            thenClause = thenClause.AddContinuation(noop);
        }
        if (!elseClause.IsMustReturn())
        {
            elseClause = elseClause.AddContinuation(noop);
        }
        join = noop;
    }
    else
    {
        if (thenClause.IsMustReturn())
        {
            TestAssert(!elseClause.IsEmpty());
            join = elseClause.m_tail;
        }
        else
        {
            TestAssert(elseClause.IsMustReturn() && !thenClause.IsEmpty());
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

}   // namespace PochiVM
