#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

// Safe unaligned memory read/write.
//
template<typename T>
T WARN_UNUSED UnalignedRead(uint8_t* src)
{
    T ret;
    memcpy(&ret, src, sizeof(T));
    return ret;
}

template<typename T>
void UnalignedWrite(uint8_t* dst, T value)
{
    memcpy(dst, &value, sizeof(T));
}

template<typename T>
void UnalignedAddAndWriteback(uint8_t* addr, T value)
{
    T old = UnalignedRead<T>(addr);
    UnalignedWrite<T>(addr, old + value);
}

}   // namespace PochiVM
