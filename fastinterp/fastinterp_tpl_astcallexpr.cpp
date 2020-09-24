#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

// Call a generated function
//
struct FICallGeneratedFnImpl
{
    // TODO: support specialization of parameter shapes
    // maybe consider varptr / var / literal? but perf impact should be very small
    //
    template<typename ReturnType,
             bool isNoExcept,
             FICallExprNumParameters numParameters>
    static constexpr bool cond()
    {
        if (std::is_pointer<ReturnType>::value && !std::is_same<ReturnType, void*>::value) { return false; }
        return true;
    }

    // Constant placeholder 0: stack frame size
    // Constant placeholder 1: EH context pointer indicating current program position, if the call may throw
    // CPP placeholder 0: EH handler, if the call may throw
    // BoilerplateFn placeholder 0: the function to call
    // BoilerplateFn placeholder 1 - n: the function evaluating each parameter
    //
    template<typename ReturnType,
             bool isNoExcept,
             FICallExprNumParameters numParameters>
    static void f([[maybe_unused]] ReturnType* out) noexcept
    {
        // Stack frame size
        //
        DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);

        // Allocate stack frame
        //
        uintptr_t newStackFrame = reinterpret_cast<uintptr_t>(alloca(CONSTANT_PLACEHOLDER_0));

        // Populate parameters into new stack frame
        // Param i is always at offset 8*(i+1) byte
        //
        constexpr uint32_t numParams = static_cast<uint32_t>(numParameters);

#define EVALUATE_PARAM(paramOrd, placeholderOrd)                                                \
    if constexpr(numParams > paramOrd)                                                          \
    {                                                                                           \
        INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)(void*) noexcept); \
        BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd (reinterpret_cast<void*>(              \
            newStackFrame + paramOrd * 8 + 8));                                                 \
    }

        EVALUATE_PARAM(0, 1)
        EVALUATE_PARAM(1, 2)
        EVALUATE_PARAM(2, 3)
        EVALUATE_PARAM(3, 4)
        EVALUATE_PARAM(4, 5)
        EVALUATE_PARAM(5, 6)
        EVALUATE_PARAM(6, 7)
        EVALUATE_PARAM(7, 8)
        EVALUATE_PARAM(8, 9)
        EVALUATE_PARAM(9, 10)

#undef EVALUATE_PARAM

        if constexpr(numParams > 10)
        {
            // Evaluate the remaining parameters in an outlined function
            //
            static_assert(numParameters == FICallExprNumParameters::MORE_THAN_TEN);
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_11(void(*)(void*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_11(nullptr);
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
                DEFINE_CPP_FNTPR_PLACEHOLDER_0(void(*)(void*, uintptr_t) noexcept);
                DEFINE_CONSTANT_PLACEHOLDER_1(void*);
                CPP_FNTPR_PLACEHOLDER_0(CONSTANT_PLACEHOLDER_1, oldStackFrame);
                // TODO: testassert false
                __builtin_unreachable();
            }
        }

        // Store return value
        //
        if constexpr(!std::is_same<ReturnType, void>::value)
        {
            // Return value is always stored at offset 0 in new stack frame
            //
            ReturnType* retValAddr = GetLocalVarAddress<ReturnType>(0 /*offset*/);
            *out = *retValAddr;
        }

        // Restore old stack frame
        //
        __pochivm_thread_fastinterp_context.m_stackFrame = oldStackFrame;
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateBoolMetaVar("isNoExcept"),
                    CreateEnumMetaVar<FICallExprNumParameters::X_END_OF_ENUM>("numParams")
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
