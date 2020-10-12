#pragma once

#include "fastinterp_codegen_helper.h"
#include "pochivm/function_proto.h"

namespace PochiVM
{

inline void FastInterpCodegenEngine::RegisterGeneratedFunctionEntryPoint(AstFunction* fn, FastInterpBoilerplateInstance* inst, bool isNoExcept)
{
    TestAssert(!m_functionEntryPoint.count(fn));
    FastInterpBoilerplateInstance* cdeclWrapper = InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICdeclInterfaceImpl>::SelectBoilerplateBluePrint(
                    fn->GetReturnType().GetDefaultFastInterpTypeId(),
                    isNoExcept));
    cdeclWrapper->PopulateBoilerplateFnPtrPlaceholder(0, inst);
    m_functionEntryPoint[fn] = std::make_pair(inst, cdeclWrapper);
}

inline void FastInterpCodegenEngine::TestOnly_RegisterUnitTestFunctionEntryPoint(
        FastInterpTypeId returnType, bool isNoExcept, uintptr_t fakeId, FastInterpBoilerplateInstance* inst)
{
    AstFunction* fn = reinterpret_cast<AstFunction*>(fakeId);
    TestAssert(!m_functionEntryPoint.count(fn));
    FastInterpBoilerplateInstance* cdeclWrapper = InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICdeclInterfaceImpl>::SelectBoilerplateBluePrint(
                    returnType, isNoExcept));
    cdeclWrapper->PopulateBoilerplateFnPtrPlaceholder(0, inst);
    m_functionEntryPoint[fn] = std::make_pair(inst, cdeclWrapper);
}

}   // namespace PochiVM
