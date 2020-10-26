#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIPointerCastImpl
{
    template<bool isNoopCast,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (isNoopCast && !spillOutput) { return false; }
        if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        return true;
    }

    template<bool isNoopCast,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1: offset modifier, if not noopcast
    // constant placeholder 0: spill location, if spilled
    //
    template<bool isNoopCast,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, uint64_t qa) noexcept
    {
        if constexpr(isNoopCast)
        {
            static_assert(spillOutput);

            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<uint64_t>(stackframe, CONSTANT_PLACEHOLDER_0) = qa;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
        else
        {
            uint64_t result;
            {
                DEFINE_INDEX_CONSTANT_PLACEHOLDER_1;
                result = qa + CONSTANT_PLACEHOLDER_1;
            }

            if constexpr(!spillOutput)
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., uint64_t) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
            }
            else
            {
                DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
                *GetLocalVarAddress<uint64_t>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
            }
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isNoopCast"),
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
    RegisterBoilerplate<FIPointerCastImpl>();
}
