#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP
#define FASTINTERP_TPL_USE_MEDIUM_MCMODEL

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FILiteralMcMediumImpl
{
    // Only allow primitive type and 'void*'
    //
    template<typename LiteralType,
             bool isAllUnderlyingBitsZero,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_same<LiteralType, void>::value) { return false; }
        if (std::is_pointer<LiteralType>::value && !std::is_same<LiteralType, void*>::value) { return false; }
        if (!spillOutput)
        {
            if (std::is_floating_point<LiteralType>::value)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
                if (FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
            else
            {
                if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
                if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
        }
        else
        {
            if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            if (FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        if constexpr(!std::is_same<LiteralType, void>::value)
        {
            if (IsConstantValidInSmallCodeModel<LiteralType>()) { return false; }
        }
        return true;
    }

    template<typename LiteralType,
             bool isAllUnderlyingBitsZero,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        LiteralType result;
        if constexpr(isAllUnderlyingBitsZero)
        {
            result = PochiVM::get_all_bits_zero_value<LiteralType>();
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(LiteralType);
            result = CONSTANT_PLACEHOLDER_1;
        }

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., LiteralType) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<LiteralType>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("literalType"),
                    CreateBoolMetaVar("isAllUnderlyingBitsZero"),
                    CreateBoolMetaVar("spillOutput"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit()
        );
    }
};

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    using namespace PochiVM;
    RegisterBoilerplate<FILiteralMcMediumImpl>(FIAttribute::CodeModelMedium);
}
