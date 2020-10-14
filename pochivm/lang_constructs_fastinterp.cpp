#include "fastinterp_ast_helper.hpp"
#include "codegen_context.hpp"

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

}   // namespace PochiVM
