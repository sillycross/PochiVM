#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

// Populate multi-byte NOP instruction to an address range
//
inline void x86_64_populate_NOP_instructions(uint8_t* addr, size_t length)
{
    // From Intel's Manual:
    //    https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2b-manual.pdf
    //    Page 165, table 4-12, "Recommended Multi-Byte Sequence of NOP Instruction"
    //
    // AMD Manual recommends the same byte sequence.
    //
    static constexpr uint8_t nop1[] = { 0x90 };
    static constexpr uint8_t nop2[] = { 0x66, 0x90 };
    static constexpr uint8_t nop3[] = { 0x0F, 0x1F, 0x00 };
    static constexpr uint8_t nop4[] = { 0x0F, 0x1F, 0x40, 0x00 };
    static constexpr uint8_t nop5[] = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    static constexpr uint8_t nop6[] = { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    static constexpr uint8_t nop7[] = { 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00 };
    static constexpr uint8_t nop8[] = { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static constexpr uint8_t nop9[] = { 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static constexpr const uint8_t* nops[10] = {
        nullptr, nop1, nop2, nop3, nop4, nop5, nop6, nop7, nop8, nop9
    };
    while (length > 0)
    {
        size_t choice = 9;
        choice = std::min(choice, length);
        memcpy(addr, nops[choice], choice);
        length -= choice;
        addr += choice;
    }
}

}   // namespace PochiVM
