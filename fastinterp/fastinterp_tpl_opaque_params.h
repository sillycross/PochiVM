#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

// We keep a small number of temporary values directly in register, instead of on the stack, for performance reasons.
// Note that here we are taking advantage of the fact that GHC calling convention only
// differentiates integral (bit-type) and floating point parameters.
// The actual width and signness of the parameters, and the relative order *between* integer and floating point parameters
// does not affect ABI. As an example, consider the following:
//    void f(int b, float a) { ... }
//    void g(uint64_t a, double b) { reinterpret_cast<void(*)(uint64_t, double)>(f)(a, b); }
//    void h() { float a = 1.2; int b = 3; reinterpret_cast<void(*)(float, int)>(g)(a, b); }
//
// Despite the call to 'f' and 'g' completely mismatch its C prototype declaration,
// function 'f' can actually see the same value of 'a' and 'b' as in 'h', since all three functions actually have the same ABIs.
// (the 'intermediate function' 'g' having the longest possible type 'uint64_t' and 'double' is important:
// otherwise if 'g' decides to temporarily push its parameters to stack, it could push and pop a shorter value than actual).
//
// This property allows us to easily pass an opaque 'stack' of temporary values to an operator's next continuation,
// without needing to know the actual types of the parameters, since only how many of them exist, and how many of them are
// floating points matters. So we won't have an exponentially exploding number of boilerplates.
//
// The callee can retrieve the top value from the stack of temporary values by doing something like
//   template<typename... T> f(T... opaque, int value)    // get top of the integer stack, it must be actually an 'int' of course
// or 'push' new values to be passed to its continuation by doing something like
//   DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(T..., int))
//   BOILERPLATE_FNPTR_PLACEHOLDER_0(opaque..., value)
// Note that this works fine even if 'T...' contains floats, since the relative order *between* integer
// and floating point does not affect ABI. All the thing that matters is the relative order within all integers and within all floats.
//
const int x_fastinterp_max_integral_params = 3;
const int x_fastinterp_max_floating_point_params = 3;

enum class FINumOpaqueIntegralParams
{
    X_END_OF_ENUM = x_fastinterp_max_integral_params + 1
};

enum class FINumOpaqueFloatingParams
{
    X_END_OF_ENUM = x_fastinterp_max_floating_point_params + 1
};

struct FIOpaqueParamsHelper
{
    static constexpr bool CanPush(FINumOpaqueIntegralParams size, int num = 1)
    {
        return static_cast<int>(size) + num <= x_fastinterp_max_integral_params;
    }

    static constexpr bool CanPush(FINumOpaqueFloatingParams size, int num = 1)
    {
        return static_cast<int>(size) + num <= x_fastinterp_max_floating_point_params;
    }

    static constexpr bool IsEmpty(FINumOpaqueIntegralParams size)
    {
        return static_cast<int>(size) == 0;
    }

    static constexpr bool IsEmpty(FINumOpaqueFloatingParams size)
    {
        return static_cast<int>(size) == 0;
    }

    static constexpr FINumOpaqueIntegralParams GetMaxOIP()
    {
        return static_cast<FINumOpaqueIntegralParams>(x_fastinterp_max_integral_params);
    }

    static constexpr FINumOpaqueFloatingParams GetMaxOFP()
    {
        return static_cast<FINumOpaqueFloatingParams>(x_fastinterp_max_floating_point_params);
    }
};

enum FIBinaryOpNumQuickAccessParams
{
    ZERO,
    ONE,
    TWO,
    X_END_OF_ENUM
};

}   // namespace PochiVM
