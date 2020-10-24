#pragma once

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIConditionalJumpHelper
{
    // A conditional jump, except that it allows specifying low level details such as the order of true/false block.
    //
    // The tailcall-to-fallthrough optimization can only be applied to the last instruction of function.
    // This condition allows us to control whether we want the true-branch or the false-branch to be the last instruction.
    //
    // This is not a correctness issue: if the compiler didn't do what we expect,
    // tailcall-to-fallthrough optimization cannot be applied, but the code is still correct.
    //
    // Unfortunately this thing is very tricky:
    // When 'optsize' is enabled, the assembly code is like the following:
    //    if (cond) { fn1(); } else { fn2(); }
    // => je32 false_branch; jmp true_branch;
    // 'likely' or 'unlikely' doesn't matter.
    //
    // When 'optsize' is not enabled, 'likely' instructs the compiler to position the instruction block
    // for true branch before false branch, while 'unlikely' does the opposite. So
    //    if (likely(cond)) { fn1(); } else { fn2(); }
    // => je8 L1; jmp true_branch; L1: jmp false_branch;
    //
    //    if (unlikely(cond)) { fn1(); } else { fn2(); }
    // => jne8 L1; jmp false_branch; L1: jmp true_branch;
    //
    // We can expect last-instruction-tail-call be eliminated.
    // So for 'optsize' mode, we always execute a jcc32 branch.
    // For 'likely/unlikely' mode, we take a jcc8 branch if the condition is false/true, and take another jmp branch otherwise.
    //
    // A jcc8 branch is actually faster than jcc32 branch to take.
    // So if the condition can be predicted to be true (e.g. loop condition),
    // the 'likely/unlikely' mode fits better.
    // For unpredictable branches (e.g. if statement), the 'optsize' mode fits better.
    //
    // Note that in general 'optsize' has many side-effects other than affecting how the branch assembly is generated,
    // but since our function is pretty simple, one can expect the side effects on performance to be limited.
    //
    enum class Mode
    {
        // If these two modes are chosen, 'OptForSize' attribute shall not be used
        //
        LikelyMode,
        UnlikelyMode,
        // If this mode is chosen, it is the user's responsibility to add 'OptForSize' attribute to in RegisterBoilerplate
        //
        OptForSizeMode
    };

#define GENERATE_METHOD(meth_name, placeholder1, placeholder2)                                                               \
    template<FIConditionalJumpHelper::Mode mode, typename... OpaqueParams>                                                   \
    static void __attribute__((__always_inline__)) meth_name(bool cond, uintptr_t sf, OpaqueParams... opaqueParams) noexcept \
    {                                                                                                                        \
        if constexpr(mode == FIConditionalJumpHelper::Mode::LikelyMode)                                                      \
        {                                                                                                                    \
            if (likely(cond))                                                                                                \
            {                                                                                                                \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder1, void(*)(uintptr_t, OpaqueParams...) noexcept);   \
                BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder1(sf, opaqueParams...);                                         \
            }                                                                                                                \
            else                                                                                                             \
            {                                                                                                                \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder2, void(*)(uintptr_t, OpaqueParams...) noexcept);   \
                BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder2(sf, opaqueParams...);                                         \
            }                                                                                                                \
        }                                                                                                                    \
        else if constexpr(mode == FIConditionalJumpHelper::Mode::UnlikelyMode)                                               \
        {                                                                                                                    \
            if (unlikely(cond))                                                                                              \
            {                                                                                                                \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder1, void(*)(uintptr_t, OpaqueParams...) noexcept);   \
                BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder1(sf, opaqueParams...);                                         \
            }                                                                                                                \
            else                                                                                                             \
            {                                                                                                                \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder2, void(*)(uintptr_t, OpaqueParams...) noexcept);   \
                BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder2(sf, opaqueParams...);                                         \
            }                                                                                                                \
        }                                                                                                                    \
        else                                                                                                                 \
        {                                                                                                                    \
            static_assert(mode == FIConditionalJumpHelper::Mode::OptForSizeMode);                                            \
            if (cond)                                                                                                        \
            {                                                                                                                \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder1, void(*)(uintptr_t, OpaqueParams...) noexcept);   \
                BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder1(sf, opaqueParams...);                                         \
            }                                                                                                                \
            else                                                                                                             \
            {                                                                                                                \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder2, void(*)(uintptr_t, OpaqueParams...) noexcept);   \
                BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder2(sf, opaqueParams...);                                         \
            }                                                                                                                \
        }                                                                                                                    \
    }

    GENERATE_METHOD(execute_0_1, 0, 1)
    GENERATE_METHOD(execute_1_0, 1, 0)

#undef GENERATE_METHOD
};

}   // namespace PochiVM
