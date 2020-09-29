#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_astcall.h"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

// Call a CPP function
//
struct FICallCppFnImpl
{
    template<bool isReturnTypeVoid,
             FIABIDistinctType returnType>
    static constexpr bool cond()
    {
        if (isReturnTypeVoid && returnType != FIABIDistinctType::INT_8) { return false; }
        return true;
    }

    template<bool isReturnTypeVoid,
             FIABIDistinctType returnType,
             bool isNoExcept,
             FICallExprNumParameters numParamsEnum,
             FICallExprParamTypeMask paramTypesMaskEnum>
    static constexpr bool cond()
    {
        if (static_cast<int>(paramTypesMaskEnum) >=
            math::power(static_cast<int>(FIABIDistinctType::X_END_OF_ENUM), static_cast<int>(numParamsEnum)))
        {
            return false;
        }
        return true;
    }

    // Constant placeholder 0: true number of parameters
    // Constant placeholder 1: EH context pointer indicating current program position, if the call may throw
    // CPP placeholder 0: the function to call
    // CPP placeholder 1: EH handler, if the call may throw
    // BoilerplateFn placeholder 0 - n: the function evaluating each parameter
    //
    template<bool isReturnTypeVoid,
             FIABIDistinctType returnType,
             bool isNoExcept,
             FICallExprNumParameters numParamsEnum,
             FICallExprParamTypeMask paramTypesMaskEnum>
    static typename std::conditional<isReturnTypeVoid, void, TypeForABIDistinctType<returnType>>::type f() noexcept
    {
        // CPP function has an interface of 'void* /*ret*/, void** /*params*/' where
        // 'params' is an array holding pointers to all parameters.
        //

        constexpr int paramTypesMask = static_cast<int>(paramTypesMaskEnum);
        constexpr uint32_t numParams = static_cast<uint32_t>(numParamsEnum);
        constexpr int numAbiTypes = static_cast<int>(FIABIDistinctType::X_END_OF_ENUM);

        DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
        const uint32_t trueNumParams = CONSTANT_PLACEHOLDER_0;
        uint8_t* params = reinterpret_cast<uint8_t*>(alloca(trueNumParams * 8));
        void** paramsPtr = reinterpret_cast<void**>(alloca(trueNumParams * 8));

        // TODO: unfortunately clang thinks this loop is huge, and generates vectorized instructions
        // Even worse, it would reference constants in a constant table, causing relocations that we don't currently support.
        // Disable vectorization for now as a workaround.
        // This whole 'paramsPtr' thing is just a historical debt of the old DebugInterp API. We should fix it at some time.
        //
#pragma clang loop vectorize(disable)
        for (uint32_t i = 0; i < trueNumParams; i++)
        {
            paramsPtr[i] = &params[i * 8];
        }

        // Populate parameters into 'params'
        //

#define EVALUATE_PARAM(paramOrd, placeholderOrd)                                                    \
    if constexpr(numParams > paramOrd)                                                              \
    {                                                                                               \
        constexpr int abiType = paramTypesMask / math::power(numAbiTypes, paramOrd) % numAbiTypes;  \
        using ParamType = TypeForABIDistinctType<static_cast<FIABIDistinctType>(abiType)>;          \
        INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, ParamType(*)() noexcept);     \
        *reinterpret_cast<ParamType*>(params + paramOrd * 8) =                                      \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd ();                                    \
    }

        EVALUATE_PARAM(0, 0)
        EVALUATE_PARAM(1, 1)
        EVALUATE_PARAM(2, 2)

#undef EVALUATE_PARAM

        if constexpr(numParams > 3)
        {
            // Evaluate the remaining parameters in an outlined function
            //
            static_assert(numParamsEnum == FICallExprNumParameters::MORE_THAN_THREE);
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_3(void(*)(uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_3(reinterpret_cast<uintptr_t>(params + 8 * 3));
        }

        // Execute function call
        //
        if constexpr(isNoExcept)
        {
            // noexcept function has prototype ReturnType(*)(void**) noexcept
            //
            if constexpr(isReturnTypeVoid)
            {
                DEFINE_CPP_FNPTR_PLACEHOLDER_0(void(*)(void**) noexcept);
                CPP_FNPTR_PLACEHOLDER_0(paramsPtr);
            }
            else
            {
                using ReturnType = TypeForABIDistinctType<returnType>;
                DEFINE_CPP_FNPTR_PLACEHOLDER_0(ReturnType(*)(void**) noexcept);
                return CPP_FNPTR_PLACEHOLDER_0(paramsPtr);
            }
        }
        else
        {
            // function that may throw has prototype bool(*)(ReturnType*, void**) noexcept (for non-void return)
            // or bool(*)(void**) noexcept (for void return), and returns true if it throws
            //
            if constexpr(isReturnTypeVoid)
            {
                DEFINE_CPP_FNPTR_PLACEHOLDER_0(bool(*)(void**) noexcept);
                if (likely(!CPP_FNPTR_PLACEHOLDER_0(paramsPtr)))
                {
                    return;
                }
            }
            else
            {
                using ReturnType = TypeForABIDistinctType<returnType>;
                ReturnType ret;
                DEFINE_CPP_FNPTR_PLACEHOLDER_0(bool(*)(ReturnType*, void**) noexcept);
                if (likely(!CPP_FNPTR_PLACEHOLDER_0(&ret, paramsPtr)))
                {
                    return ret;
                }
            }

            // The function call threw a C++ exception.
            // Call the soft-emulated exception handler. It never returns.
            //
            DEFINE_CPP_FNPTR_PLACEHOLDER_1(void(*)(void*, uintptr_t) noexcept);
            DEFINE_CONSTANT_PLACEHOLDER_1(void*);
            CPP_FNPTR_PLACEHOLDER_1(CONSTANT_PLACEHOLDER_1, __pochivm_thread_fastinterp_context.m_stackFrame);
            // TODO: testassert false
            __builtin_unreachable();
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isReturnTypeVoid"),
                    CreateEnumMetaVar<FIABIDistinctType::X_END_OF_ENUM>("returnType"),
                    CreateBoolMetaVar("isNoExcept"),
                    CreateEnumMetaVar<FICallExprNumParameters::X_END_OF_ENUM>("numParams"),
                    CreateEnumMetaVar<FICallExprParamTypeMask::X_END_OF_ENUM>("paramTypeMask")
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
    RegisterBoilerplate<FICallCppFnImpl>();
}
