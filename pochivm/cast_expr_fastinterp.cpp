#include "cast_expr.h"
#include "fastinterp_ast_helper.hpp"

namespace PochiVM
{

void AstReinterpretCastExpr::FastInterpSetupSpillLocation()
{
    thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(GetTypeId());
    m_operand->FastInterpSetupSpillLocation();
}

FastInterpSnippet WARN_UNUSED AstReinterpretCastExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_operand->GetTypeId()));
    FastInterpSnippet snippet = m_operand->PrepareForFastInterp(x_FINoSpill);
    TestAssert(!snippet.IsEmpty() && !snippet.IsUncontinuable());

    if (spillLoc.IsNoSpill())
    {
        // this is a no-op, since reinterpret_cast is a no-op at bit-level
        //
        return snippet;
    }
    else
    {
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIPointerCastImpl>::SelectBoilerplateBluePrint(
                        true /*isNoopCast*/,
                        true /*spillOutput*/,
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
        return snippet.AddContinuation(inst);
    }
}

}   // namespace PochiVM
