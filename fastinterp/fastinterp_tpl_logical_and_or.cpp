#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIUnpredictableLogicalAndOrImpl
{
    template<bool isAnd,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        return true;
    }

    template<bool isAnd,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1 for LHS
    // constant placeholder 2 for RHS
    //
    template<bool isAnd,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, DEF_MEM2REG_PARAMS, OpaqueParams... opaqueParams, bool lhsCond) noexcept
    {
        if constexpr(isAnd)
        {
            if constexpr(!spillOutput)
            {
                if (lhsCond)
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams..., bool) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams..., false);
                }
            }
            else
            {
                if (lhsCond)
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
                    *GetLocalVarAddress<bool>(stackframe, CONSTANT_PLACEHOLDER_0) = false;

                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
            }
        }
        else
        {
            if constexpr(!spillOutput)
            {
                if (!lhsCond)
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams..., bool) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams..., true);
                }
            }
            else
            {
                if (!lhsCond)
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
                    *GetLocalVarAddress<bool>(stackframe, CONSTANT_PLACEHOLDER_0) = true;

                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
            }
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isAnd"),
                    CreateBoolMetaVar("spillOutput"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit()
        );
    }
};

// For 'and', the prediction is 'true'
// For 'or', the prediction is 'false'
//
struct FIPredictableLogicalAndOrImpl
{
    template<bool isAnd,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        return true;
    }

    template<bool isAnd,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1 for LHS
    // constant placeholder 2 for RHS
    //
    template<bool isAnd,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, DEF_MEM2REG_PARAMS, OpaqueParams... opaqueParams, bool lhsCond) noexcept
    {
        if constexpr(isAnd)
        {
            if constexpr(!spillOutput)
            {
                // see comments in FIConditionalJumpHelper on why this is 'unlikely', same below
                //
                if (unlikely(lhsCond))
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams..., bool) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams..., false);
                }
            }
            else
            {
                if (unlikely(lhsCond))
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
                    *GetLocalVarAddress<bool>(stackframe, CONSTANT_PLACEHOLDER_0) = false;

                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
            }
        }
        else
        {
            if constexpr(!spillOutput)
            {
                if (unlikely(!lhsCond))
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams..., bool) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams..., true);
                }
            }
            else
            {
                if (unlikely(!lhsCond))
                {
                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
                else
                {
                    DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
                    *GetLocalVarAddress<bool>(stackframe, CONSTANT_PLACEHOLDER_0) = true;

                    DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
                    BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
                }
            }
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isAnd"),
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
    RegisterBoilerplate<FIUnpredictableLogicalAndOrImpl>(FIAttribute::OptSize);
    RegisterBoilerplate<FIPredictableLogicalAndOrImpl>();
}
