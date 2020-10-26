#include "cast_expr.h"
#include "fastinterp_ast_helper.hpp"

namespace PochiVM
{

// See comment in fastinterp_tpl_static_cast.cpp
//
constexpr uint64_t x_x86_64_uint64_to_double_magic_csts[3] = {
    4985484788626358272ULL, 4841369599423283200ULL, 4985484787499139072ULL
};

void AstReinterpretCastExpr::FastInterpSetupSpillLocation()
{
    thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_operand->GetTypeId());
    m_operand->FastInterpSetupSpillLocation();
}

static FastInterpBoilerplateInstance* WARN_UNUSED FIGetNoopBitCastButSpill(FISpillLocation spillLoc)
{
    TestAssert(!spillLoc.IsNoSpill());
    FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPointerCastImpl>::SelectBoilerplateBluePrint(
                    true /*isNoopCast*/,
                    true /*spillOutput*/,
                    thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    return inst;
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
        FastInterpBoilerplateInstance* inst = FIGetNoopBitCastButSpill(spillLoc);
        return snippet.AddContinuation(inst);
    }
}

void AstStaticCastExpr::FastInterpSetupSpillLocation()
{
    thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_operand->GetTypeId());
    m_operand->FastInterpSetupSpillLocation();
}

FastInterpSnippet WARN_UNUSED AstStaticCastExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_operand->GetTypeId()));
    FastInterpSnippet snippet = m_operand->PrepareForFastInterp(x_FINoSpill);
    TestAssert(!snippet.IsEmpty() && !snippet.IsUncontinuable());

    TypeId srcType = m_operand->GetTypeId();
    TypeId dstType = GetTypeId();

    TestAssert(!srcType.IsVoid() && !dstType.IsVoid());
    TestAssertIff(srcType.IsPointerType(), dstType.IsPointerType());

    if (!srcType.IsPointerType())
    {
        // Case 1: static_cast between non-pointer types
        // FIStaticCastImpl / FIStaticCastU64DoubleImpl
        //
        TestAssertImp(srcType.IsFloatingPoint(), dstType.IsFloatingPoint());
        FastInterpBoilerplateInstance* inst;

        // Special case: uint64_t -> double
        // We currently need magic constant table support for uint64_t -> double cast,
        // see comment in fastinterp_tpl_static_cast.cpp
        //
        //
        if (srcType == TypeId::Get<uint64_t>() && dstType == TypeId::Get<double>())
        {
            inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIStaticCastU64DoubleImpl>::SelectBoilerplateBluePrint(
                            !spillLoc.IsNoSpill(),
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
            spillLoc.PopulatePlaceholderIfSpill(inst, 0);
            inst->PopulateConstantPlaceholder<uintptr_t>(1, reinterpret_cast<uintptr_t>(x_x86_64_uint64_to_double_magic_csts));
        }
        else
        {
            inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIStaticCastImpl>::SelectBoilerplateBluePrint(
                            srcType.GetDefaultFastInterpTypeId(),
                            dstType.GetDefaultFastInterpTypeId(),
                            !spillLoc.IsNoSpill(),
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
            spillLoc.PopulatePlaceholderIfSpill(inst, 0);
        }

        return snippet.AddContinuation(inst);
    }
    else
    {
        // Case 2: static_cast between pointer types
        // FIPointerCastImpl
        //
        ssize_t offset = AstTypeHelper::get_static_cast_offset(srcType, dstType);
        if (offset == 0)
        {
            if (spillLoc.IsNoSpill())
            {
                // This cast is a no-op, since it doesn't change the pointer value, and does not spill the result
                //
                return snippet;
            }
            else
            {
                FastInterpBoilerplateInstance* inst = FIGetNoopBitCastButSpill(spillLoc);
                return snippet.AddContinuation(inst);
            }
        }
        else
        {
            // A pointer-cast with offset
            //
            FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIPointerCastImpl>::SelectBoilerplateBluePrint(
                            false /*isNoopCast*/,
                            !spillLoc.IsNoSpill(),
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                            FIOpaqueParamsHelper::GetMaxOFP()));
            spillLoc.PopulatePlaceholderIfSpill(inst, 0);
            inst->PopulateConstantPlaceholder<uint64_t>(1, static_cast<uint64_t>(offset));
            return snippet.AddContinuation(inst);
        }
    }
}

}   // namespace PochiVM
