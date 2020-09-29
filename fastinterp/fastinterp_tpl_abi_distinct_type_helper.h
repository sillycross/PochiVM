#pragma once

#include "common.h"

namespace PochiVM
{

// Sometimes we don't need to understand the internal representation of a type,
// we just want to forward it to somewhere else.
// In that case, we don't have to enumerate every type (so we can inline more in one function).
// We just need to distinguish types that are different in ABI.
//
enum class FIABIDistinctType
{
    INT_8,
    INT_16,
    INT_32,
    INT_64,
    FLOAT,
    DOUBLE,
    X_END_OF_ENUM
};

template<FIABIDistinctType t> struct TypeForABIDistinctTypeImpl;
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_8> { using type = uint8_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_16> { using type = uint16_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_32> { using type = uint32_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_64> { using type = uint64_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::FLOAT> { using type = float; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::DOUBLE> { using type = double; };

template<FIABIDistinctType t>
using TypeForABIDistinctType = typename TypeForABIDistinctTypeImpl<t>::type;

template<typename T>
constexpr FIABIDistinctType GetFIABIDistinctType()
{
    if constexpr(std::is_pointer<T>::value)
    {
        static_assert(sizeof(T) == 8, "unexpected pointer size");
        return FIABIDistinctType::INT_64;
    }
    else if constexpr(std::is_integral<T>::value)
    {
        constexpr size_t size = sizeof(T);
        if constexpr(size == 1) {
            return FIABIDistinctType::INT_8;
        }
        else if constexpr(size == 2) {
            return FIABIDistinctType::INT_16;
        }
        else if constexpr(size == 4) {
            return FIABIDistinctType::INT_32;
        }
        else if constexpr(size == 8) {
            return FIABIDistinctType::INT_64;
        }
        else {
            static_assert(type_dependent_false<T>::value, "unexpected size of integral type");
        }
    }
    else if constexpr(std::is_same<T, float>::value)
    {
        return FIABIDistinctType::FLOAT;
    }
    else if constexpr(std::is_same<T, double>::value)
    {
        return FIABIDistinctType::DOUBLE;
    }
    else
    {
        static_assert(type_dependent_false<T>::value, "unexpected type T");
    }
}

}   // namespace PochiVM
