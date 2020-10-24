#include "fastinterp_ast_helper.hpp"

namespace PochiVM
{

FastInterpSnippet WARN_UNUSED AstVariable::PrepareForFastInterp(FISpillLocation spillLoc)
{
    FINumOpaqueIntegralParams numOIP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral();
    FINumOpaqueFloatingParams numOFP = thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat();
    FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIVariableImpl>::SelectBoilerplateBluePrint(
                    GetTypeId().GetDefaultFastInterpTypeId(),
                    !spillLoc.IsNoSpill(),
                    numOIP,
                    numOFP));
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    inst->PopulateConstantPlaceholder<uint64_t>(1, GetFastInterpOffset());
    return FastInterpSnippet {
        inst, inst
    };
}

FastInterpSnippet WARN_UNUSED AstVariable::GetFastInterpDestructorSnippet()
{
    if (!GetTypeId().RemovePointer().IsCppClassType())
    {
        return FastInterpSnippet { nullptr, nullptr };
    }

    if (m_fastInterpDtorCallOp == nullptr)
    {
        const CppFunctionMetadata* md = GetDestructorMetadata(GetTypeId().RemovePointer());
        TestAssert(md != nullptr);

        FastInterpCppFunctionInfo info = md->m_getFastInterpFn();
        TestAssert(info.m_isNoExcept);
        TestAssert(info.m_returnType.IsVoid());

        m_fastInterpDtorCallOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprCallDestructorOpImpl>::SelectBoilerplateBluePrint(true /*dummy*/));
        m_fastInterpDtorCallOp->PopulateConstantPlaceholder<uint64_t>(0, GetFastInterpOffset());
        m_fastInterpDtorCallOp->PopulateCppFnPtrPlaceholder(0, info.m_interpImpl);
        m_fastInterpDtorCallOp->SetAlignmentLog2(4);
    }

    FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
            FastInterpBoilerplateLibrary<FICallExprCallDestructorCallerImpl>::SelectBoilerplateBluePrint(true /*dummy*/));
    inst->PopulateBoilerplateFnPtrPlaceholder(1, m_fastInterpDtorCallOp);

    return FastInterpSnippet {
        inst, inst
    };
}

}   // namespace PochiVM
