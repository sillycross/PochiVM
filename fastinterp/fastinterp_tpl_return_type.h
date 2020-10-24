#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

template<typename T>
struct FIReturnValueOrExn
{
    // T is always a fastinterp type
    //
    static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value);

    // If this struct is > 16 bytes, cdecl would use sret, breaking our expectation.
    // So just assert this is true for safety.
    //
    static_assert(sizeof(T) <= 8);

    T m_ret;

    // uint64_t is required to make sure cdecl and GHC has same ABI
    // (cdecl ABI has weird "crunching" behavior, e.g. struct s { int a; bool b; } => uint64_t)
    //
    uint64_t m_hasExn;
};

template<>
struct FIReturnValueOrExn<void>
{
    uint64_t m_hasExn;
};

template<typename T, bool isNoExcept>
using FIReturnType = typename std::conditional<isNoExcept, T, FIReturnValueOrExn<T>>::type;

struct FIReturnValueHelper
{
    template<typename T>
    static uint64_t __attribute__((__always_inline__)) HasException(FIReturnType<T, false /*isNoExcept*/> r)
    {
        return r.m_hasExn;
    }

    template<typename T, bool isNoExcept>
    static T __attribute__((__always_inline__)) GetReturnValue(FIReturnType<T, isNoExcept> r)
    {
        if constexpr(isNoExcept)
        {
            return r;
        }
        else
        {
            return r.m_ret;
        }
    }

    template<typename T>
    static FIReturnValueOrExn<T> __attribute__((__always_inline__)) GetForExn()
    {
        FIReturnValueOrExn<T> ret;
        ret.m_hasExn = true;
        return ret;
    }

    template<typename T, bool isNoExcept>
    static FIReturnType<T, isNoExcept> __attribute__((__always_inline__)) GetForRet(T retvalue)
    {
        if constexpr(isNoExcept)
        {
            static_assert(std::is_same<FIReturnType<T, isNoExcept>, T>::value);
            return retvalue;
        }
        else
        {
            FIReturnValueOrExn<T> ret;
            ret.m_ret = retvalue;
            ret.m_hasExn = false;
            return ret;
        }
    }

    template<bool isNoExcept>
    static FIReturnType<void, isNoExcept> __attribute__((__always_inline__)) GetForRetVoid()
    {
        if constexpr(isNoExcept)
        {
            static_assert(std::is_same<FIReturnType<void, isNoExcept>, void>::value);
            return;
        }
        else
        {
            FIReturnValueOrExn<void> ret;
            ret.m_hasExn = false;
            return ret;
        }
    }
};

}   // namespace PochiVM
