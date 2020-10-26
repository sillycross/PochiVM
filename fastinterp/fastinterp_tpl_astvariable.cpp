#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIVariableImpl
{
    template<typename VarTypePtr>
    static constexpr bool cond()
    {
        if (!std::is_pointer<VarTypePtr>::value) { return false; }
        return true;
    }

    template<typename VarTypePtr,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!spillOutput)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename VarTypePtr,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        return true;
    }

    // placeholder rules:
    // placeholder 1 for var offset
    // placeholder 0 for output, if spilled
    //
    template<typename VarTypePtr,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        static_assert(std::is_pointer<VarTypePtr>::value, "unexpected VarTypePtr");

        DEFINE_INDEX_CONSTANT_PLACEHOLDER_1;
        VarTypePtr result = GetLocalVarAddress<typename std::remove_pointer<VarTypePtr>::type>(stackframe, CONSTANT_PLACEHOLDER_1);

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., VarTypePtr) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<VarTypePtr>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("varTypePtr"),
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
    RegisterBoilerplate<FIVariableImpl>();
}
