#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_astcall.h"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

// Call a generated function
//
struct FICallGeneratedFnImpl
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

    // Constant placeholder 0: stack frame size
    // Constant placeholder 1: EH context pointer indicating current program position, if the call may throw
    // CPP placeholder 0: EH handler, if the call may throw
    // BoilerplateFn placeholder 0: the function to call
    // BoilerplateFn placeholder 1 - n: the function evaluating each parameter
    //
    template<bool isReturnTypeVoid,
             FIABIDistinctType returnType,
             bool isNoExcept,
             FICallExprNumParameters numParamsEnum,
             FICallExprParamTypeMask paramTypesMaskEnum>
    static typename std::conditional<isReturnTypeVoid, void, TypeForABIDistinctType<returnType>>::type f() noexcept
    {
        constexpr int paramTypesMask = static_cast<int>(paramTypesMaskEnum);
        constexpr uint32_t numParams = static_cast<uint32_t>(numParamsEnum);
        constexpr int numAbiTypes = static_cast<int>(FIABIDistinctType::X_END_OF_ENUM);

        // Stack frame size
        //
        DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);

        // Allocate stack frame
        //
        uintptr_t newStackFrame = reinterpret_cast<uintptr_t>(alloca(CONSTANT_PLACEHOLDER_0));

        // Populate parameters into new stack frame
        // Param i is always at offset 8*(i+1) byte
        //

#define EVALUATE_PARAM(paramOrd, placeholderOrd)                                                    \
    if constexpr(numParams > paramOrd)                                                              \
    {                                                                                               \
        constexpr int abiType = paramTypesMask / math::power(numAbiTypes, paramOrd) % numAbiTypes;  \
        using ParamType = TypeForABIDistinctType<static_cast<FIABIDistinctType>(abiType)>;          \
        INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, ParamType(*)() noexcept);     \
        *reinterpret_cast<ParamType*>(newStackFrame + paramOrd * 8 + 8) =                           \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd ();                                    \
    }

        EVALUATE_PARAM(0, 1)
        EVALUATE_PARAM(1, 2)
        EVALUATE_PARAM(2, 3)

#undef EVALUATE_PARAM

        if constexpr(numParams > 3)
        {
            // Evaluate the remaining parameters in an outlined function
            //
            static_assert(numParamsEnum == FICallExprNumParameters::MORE_THAN_THREE);
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_4(void(*)(uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_4(newStackFrame + 8 * 3 + 8);
        }

        // Switch to new stack frame
        //
        uintptr_t oldStackFrame = __pochivm_thread_fastinterp_context.m_stackFrame;
        __pochivm_thread_fastinterp_context.m_stackFrame = newStackFrame;

        // Execute function call
        //
        if constexpr(isNoExcept)
        {
            // noexcept function has prototype void(*)() noexcept
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)() noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0();
        }
        else
        {
            // function that may throw has prototype bool(*)() noexcept, and returns true if it throws
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(bool(*)() noexcept);
            if (unlikely(BOILERPLATE_FNPTR_PLACEHOLDER_0()))
            {
                // The function call threw a C++ exception.
                // Call the soft-emulated exception handler. It never returns.
                //
                DEFINE_CPP_FNPTR_PLACEHOLDER_0(void(*)(void*, uintptr_t) noexcept);
                DEFINE_CONSTANT_PLACEHOLDER_1(void*);
                CPP_FNPTR_PLACEHOLDER_0(CONSTANT_PLACEHOLDER_1, oldStackFrame);
                // TODO: testassert false
                __builtin_unreachable();
            }
        }

        // Restore old stack frame
        //
        __pochivm_thread_fastinterp_context.m_stackFrame = oldStackFrame;

        // Return return value
        //
        if constexpr(!isReturnTypeVoid)
        {
            using ReturnType = TypeForABIDistinctType<returnType>;
            // Return value is always stored at offset 0 in new stack frame
            //
            return *reinterpret_cast<ReturnType*>(newStackFrame);
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
    RegisterBoilerplate<FICallGeneratedFnImpl>();
}
