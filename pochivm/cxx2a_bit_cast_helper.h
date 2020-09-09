#pragma once

#include <type_traits>

namespace PochiVM
{

// std::bit_cast is only supported in C++20, which is still officially in experimental state.
//
// Fortunately the compiler magic '__builtin_bit_cast' (internally used by clang++ STL
// to implement std::bit_cast) is available regardless of C++-standard-version.
//
// This helper imports the std::bit_cast utility. Code directly stolen from libc++ source code.
//
template<class _ToType, class _FromType>
constexpr _ToType cxx2a_bit_cast(_FromType const& __from) noexcept
{
    static_assert(sizeof(_ToType) == sizeof(_FromType));
    static_assert(std::is_trivially_copyable_v<_ToType>);
    static_assert(std::is_trivially_copyable_v<_FromType>);
    return __builtin_bit_cast(_ToType, __from);
}

}   // namespace PochiVM
