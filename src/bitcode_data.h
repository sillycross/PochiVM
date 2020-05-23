#pragma once

#include <cstdlib>
#include <cstdint>

struct BitcodeData
{
    const char* m_symbolName;
    const uint8_t* m_bitcode;
    size_t m_length;
};

struct CppFunctionMetadata
{
    const BitcodeData* m_bitcodeData;
    bool m_isUsingSret;
};
