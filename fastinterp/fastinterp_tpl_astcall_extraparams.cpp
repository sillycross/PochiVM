#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_astcall.h"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

// Currently a call expression can inline at most 3 parameters
// This operator is used to pass in the remaining parameters
//
struct FICallExprExtraParamsImpl
{
    template<FICallExprNumExtraParameters numParamsEnum,
             FICallExprExtraParamTypeMask paramTypesMaskEnum>
    static constexpr bool cond()
    {
        if (static_cast<int>(paramTypesMaskEnum) >=
            math::power(static_cast<int>(FIABIDistinctType::X_END_OF_ENUM), static_cast<int>(numParamsEnum)))
        {
            return false;
        }
        return true;
    }

    // BoilerplateFn placeholder 0 - x: the function evaluating each parameter
    // BoilerplateFn placeholder n: the next FICallExprExtraParamsImpl to fill in remaining parameters
    //
    template<FICallExprNumExtraParameters numParamsEnum,
             FICallExprExtraParamTypeMask paramTypesMaskEnum>
    static void f(uintptr_t startAddr) noexcept
    {
        constexpr int paramTypesMask = static_cast<int>(paramTypesMaskEnum);
        constexpr uint32_t numParams = static_cast<uint32_t>(numParamsEnum);
        constexpr int numAbiTypes = static_cast<int>(FIABIDistinctType::X_END_OF_ENUM);

#define EVALUATE_PARAM(paramOrd, placeholderOrd)                                                    \
    if constexpr(numParams > paramOrd)                                                              \
    {                                                                                               \
        constexpr int abiType = paramTypesMask / math::power(numAbiTypes, paramOrd) % numAbiTypes;  \
        using ParamType = TypeForABIDistinctType<static_cast<FIABIDistinctType>(abiType)>;          \
        INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, ParamType(*)() noexcept);     \
        *reinterpret_cast<ParamType*>(startAddr + paramOrd * 8) =                                   \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd ();                                    \
    }

        EVALUATE_PARAM(0, 0)
        EVALUATE_PARAM(1, 1)
        EVALUATE_PARAM(2, 2)
        EVALUATE_PARAM(3, 3)

#undef EVALUATE_PARAM

        if constexpr(numParams > 4)
        {
            // Evaluate the remaining parameters in an outlined function
            //
            static_assert(numParamsEnum == FICallExprNumExtraParameters::MORE_THAN_FOUR);
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_4(void(*)(uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_4(startAddr + 8 * 4);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateEnumMetaVar<FICallExprNumExtraParameters::X_END_OF_ENUM>("numParams"),
                    CreateEnumMetaVar<FICallExprExtraParamTypeMask::X_END_OF_ENUM>("paramTypeMask")
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
    RegisterBoilerplate<FICallExprExtraParamsImpl>();
}
