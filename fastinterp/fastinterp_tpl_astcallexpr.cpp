#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

// Call a generated function
//
struct FastInterpCallGeneratedFnImpl
{
    template<typename ReturnType,
             bool isNoExcept,
             AstCallExprNumParameters numParameters>
    static constexpr bool cond()
    {
        if (std::is_pointer<ReturnType>::value && !std::is_same<ReturnType, void*>::value) { return false; }
        return true;
    }

    // Constant placeholder 0: stack frame size
    // Constant placeholder 1: EH context pointer indicating current program position, if the call may throw
    // CPP placeholder 0: EH handler, if the call may throw
    // BoilerplateFn placeholder 0: the function to call
    // Constant placeholder 2 - n: the offset of each parameter in stack frame
    // BoilerplateFn placeholder 2 - n: the function evaluating each parameter
    //
    template<typename ReturnType,
             bool isNoExcept,
             AstCallExprNumParameters numParameters>
    static void f([[maybe_unused]] ReturnType* out) noexcept
    {
        // Stack frame size
        //
        DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);

        // Allocate stack frame
        //
        uintptr_t newStackFrame = reinterpret_cast<uintptr_t>(alloca(CONSTANT_PLACEHOLDER_0));

        // Populate parameters into new stack frame
        //
        constexpr uint32_t numParams = static_cast<uint32_t>(numParameters);

#define EVALUATE_PARAM(paramOrd, placeholderOrd)                                                \
    if constexpr(numParams > paramOrd)                                                          \
    {                                                                                           \
        INTERNAL_DEFINE_CONSTANT_PLACEHOLDER(placeholderOrd, uint32_t);                         \
        INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)(void*) noexcept); \
        BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd (reinterpret_cast<void*>(              \
            newStackFrame + CONSTANT_PLACEHOLDER_ ## placeholderOrd));                          \
    }

        EVALUATE_PARAM(0, 2)
        EVALUATE_PARAM(1, 3)
        EVALUATE_PARAM(2, 4)
        EVALUATE_PARAM(3, 5)
        EVALUATE_PARAM(4, 6)
        EVALUATE_PARAM(5, 7)
        EVALUATE_PARAM(6, 8)
        EVALUATE_PARAM(7, 9)
        EVALUATE_PARAM(8, 10)
        EVALUATE_PARAM(9, 11)

#undef EVALUATE_PARAM

        if constexpr(numParams > 10)
        {
            // Evaluate the remaining parameters in an outlined function
            //
            static_assert(numParameters == AstCallExprNumParameters::MORE_THAN_TEN);
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_12(void(*)(void*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_12(nullptr);
        }

        // Switch to new stack frame
        //
        uintptr_t oldStackFrame = __pochivm_thread_fastinterp_context.m_stackFrame;
        __pochivm_thread_fastinterp_context.m_stackFrame = newStackFrame;

        // Execute function call
        //
        if constexpr(isNoExcept)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(void*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(nullptr);
        }
        else
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(bool*) noexcept);
            bool exceptionThrown;
            BOILERPLATE_FNPTR_PLACEHOLDER_0(&exceptionThrown);
            if (unlikely(exceptionThrown))
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
                    CreateEnumMetaVar<AstCallExprNumParameters::X_END_OF_ENUM>("numParams")
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
    RegisterBoilerplate<FastInterpCallGeneratedFnImpl>();
}
