#pragma once

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIConditionalJumpHelper
{
    // A conditional jump, except that it allows specifying the order of true/false block.
    // 'likely' instructs the compiler to position the instruction block for true branch before false branch,
    // while 'unlikely' does the opposite.
    // The tailcall-to-fallthrough optimization can only be applied to the last instruction of function.
    // This condition allows us to control whether we want the true-branch or the false-branch to be the last instruction.
    //
    // This is not a correctness issue: if the compiler didn't do what we expect,
    // tailcall-to-fallthrough optimization cannot be applied, but the code is still correct.
    //
#define GENERATE_METHOD(meth_name, placeholder1, placeholder2)                                                               \
    template<bool placeFalseBranchAtEnd, typename... OpaqueParams>                                                           \
    static void __attribute__((__always_inline__)) meth_name(bool cond, uintptr_t sf, OpaqueParams... opaqueParams) noexcept \
    {                                                                                                                        \
        if constexpr(placeFalseBranchAtEnd)                                                                                  \
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
        else                                                                                                                 \
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
    }

    GENERATE_METHOD(execute_0_1, 0, 1)

#undef GENERATE_METHOD
};

}   // namespace PochiVM
