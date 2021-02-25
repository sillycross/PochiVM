#include "fastinterp_ast_helper.hpp"
#include "codegen_context.hpp"
#include "ast_mem2reg.h"

namespace PochiVM
{

FastInterpSnippet AstRegisterCachedVariableExpr::PrepareForFastInterp(FISpillLocation spillLoc)
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
                FastInterpBoilerplateLibrary<FIMem2RegExtractValue>::SelectBoilerplateBluePrint(
                    GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                    static_cast<FIMem2RegOrdinal>(m_regOrdinal),
                    !spillLoc.IsNoSpill(),
                    numOIP,
                    numOFP));
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    return FastInterpSnippet {
        inst, inst
    };
}

FastInterpSnippet WARN_UNUSED FIMem2RegGenerateInitLogic(const std::vector<AstRegisterCachedVariableExpr*>& list)
{
    FastInterpSnippet snippet;
    for (AstRegisterCachedVariableExpr* var : list)
    {
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIMem2RegInsertValue>::SelectBoilerplateBluePrint(
                        var->m_variable->GetTypeId().RemovePointer().GetOneLevelPtrFastInterpTypeId(),
                        true /*fromStack*/,
                        static_cast<FIMem2RegOrdinal>(var->m_regOrdinal),
                        FIOpaqueParamsHelper::GetMaxOIP(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
        inst->PopulateConstantPlaceholder<uint64_t>(0, var->m_variable->GetFastInterpOffset());
        snippet = snippet.AddContinuation(inst);
    }
    return snippet;
}

FastInterpSnippet WARN_UNUSED FIMem2RegGenerateWritebackLogic(const std::vector<AstRegisterCachedVariableExpr*>& list)
{
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();
    FastInterpSnippet snippet;
    for (AstRegisterCachedVariableExpr* var : list)
    {
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIMem2RegExtractValue>::SelectBoilerplateBluePrint(
                        var->m_variable->GetTypeId().RemovePointer().GetOneLevelPtrFastInterpTypeId(),
                        static_cast<FIMem2RegOrdinal>(var->m_regOrdinal),
                        true /*spillOutput*/,
                        FIOpaqueParamsHelper::GetMaxOIP(),
                        FIOpaqueParamsHelper::GetMaxOFP()));
        inst->PopulateConstantPlaceholder<uint64_t>(0, var->m_variable->GetFastInterpOffset());
        snippet = snippet.AddContinuation(inst);
    }
    return snippet;
}

}   // namespace PochiVM
