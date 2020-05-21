#pragma once

#include <cstdlib>
#include <cstdint>

struct BitcodeData
{
    const char* m_symbolName;
    uint8_t* m_bitcode;
    size_t m_length;
};
