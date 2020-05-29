#pragma once

#include <cstdlib>
#include <cstdint>

namespace PochiVM
{

struct BitcodeData
{
    const char* m_symbolName;
    const uint8_t* m_bitcode;
    size_t m_length;
};

}   // namespace PochiVM
