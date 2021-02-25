#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_mem2reg_util.hpp"

namespace PochiVM
{

struct FIMem2RegInsertValue
{
    template<typename T,
             bool fromStack,
             FIMem2RegOrdinal ordinal,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_same<T, void>::value) { return false; }
        if (std::is_pointer<T>::value && !std::is_same<T, void*>::value) { return false; }
        if constexpr(fromStack)
        {
            if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            if (FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        else
        {
            if (std::is_floating_point<T>::value)
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
        if (std::is_floating_point<T>::value)
        {
            if (static_cast<size_t>(ordinal) >= x_mem2reg_max_floating_vars) { return false; }
        }
        else
        {
            if (static_cast<size_t>(ordinal) >= x_mem2reg_max_integral_vars) { return false; }
        }
        return true;
    }

    template<typename T,
             bool fromStack,
             FIMem2RegOrdinal ordinal,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe,
                  GEN_MEM2REG_EXTRACT_DEFS(!std::is_floating_point<T>::value, static_cast<size_t>(ordinal), T),
                  OpaqueParams... opaqueParams,
                  [[maybe_unused]] T value) noexcept
    {
        constexpr bool isInt = !std::is_floating_point<T>::value;
        constexpr size_t regOrd = static_cast<size_t>(ordinal);
        if constexpr(fromStack)
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            INSERT_MEM2REG_VALUE(isInt, regOrd) = *GetLocalVarAddress<T>(stackframe, CONSTANT_PLACEHOLDER_0);
        }
        else
        {
            INSERT_MEM2REG_VALUE(isInt, regOrd) = value;
        }
        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, GEN_MEM2REG_EXTRACT_TYPES(isInt, regOrd, T), OpaqueParams...) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("valueType"),
                    CreateBoolMetaVar("fromStack"),
                    CreateEnumMetaVar<FIMem2RegOrdinal::X_END_OF_ENUM>("ordinal"),
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
    RegisterBoilerplate<FIMem2RegInsertValue>();
}
