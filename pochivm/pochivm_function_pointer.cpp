#include "pochivm_function_pointer.h"
#include "function_proto.h"

namespace PochiVM
{

uintptr_t GeneratedFunctionPointerImpl::GetControlValueForFastInterpFn(AstFunction* fn, uintptr_t generatedFnAddress)
{
    TestAssert(generatedFnAddress < (1ULL << 48));
    uint32_t sfSize = fn->GetFastInterpStackFrameSize();
    FIStackframeSizeCategory sfSizeCat = FIStackframeSizeCategoryHelper::SelectCategory(sfSize);
    uint64_t value = (1ULL << 62);
    if (fn->GetIsNoExcept())
    {
        value |= (1ULL << 61);
    }
    value |= static_cast<uint64_t>(sfSizeCat) << 48;
    value |= generatedFnAddress;
    return value;
}

uintptr_t GeneratedFunctionPointerImpl::GetControlValueForDebugInterpFn(AstFunction* fn)
{
    TestAssert(reinterpret_cast<uintptr_t>(fn) < (1ULL << 48));
    GeneratedFunctionPointerImpl r(0);
    uint64_t value = (2ULL << 62);
    value |= reinterpret_cast<uint64_t>(fn);
    return value;
}

void DebugInterpCallFunctionPointerFromCppImpl(AstFunction* fn, uintptr_t paramsAndRet, size_t numArgs)
{
    if (numArgs < 1) { numArgs = 1; }
    AstLiteralExpr* paramsMem = reinterpret_cast<AstLiteralExpr*>(alloca(sizeof(AstLiteralExpr) * numArgs));
    for (size_t i = 0; i < numArgs; i++)
    {
        new (paramsMem + i) AstLiteralExpr(fn->GetParamType(i), reinterpret_cast<void*>(paramsAndRet + 8 + 8 * i));
        paramsMem[i].SetupDebugInterpImpl();
    }

    AstNodeBase** params = reinterpret_cast<AstNodeBase**>(alloca(sizeof(AstNodeBase*) * numArgs));
    for (size_t i = 0; i < numArgs; i++) { params[i] = paramsMem + i; }

    // Interp the function, fill in return value at 'ret'
    //
    fn->DebugInterp(params, reinterpret_cast<void*>(paramsAndRet));
}

}   // namespace PochiVM
