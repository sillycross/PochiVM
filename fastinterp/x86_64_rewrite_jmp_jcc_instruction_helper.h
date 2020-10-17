#pragma once

#include "pochivm/common.h"
#include "unaligned_memaccess_helper.h"
#include "x86_64_populate_nop_instruction_helper.h"

namespace PochiVM
{

// x86_64 jmp and jcc (conditional jump) has two forms:
// one takes 8-bit operand, and one takes 32-bit operand.
// The 8-bit operand version is faster than 32-bit operand version.
// So we rewrite the 32-bit version instruction to 8-bit version, if the operand actually fits in 8-bit.
//
// Reference:
// https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-instruction-set-reference-manual-325383.pdf
//

// Rewrite 'jmp rel32' to 'jmp rel8' if possible
// jmp rel32 (5 bytes) => jmp rel8 (2 bytes) + nop3 (3 bytes)
//
inline void x86_64_try_rewrite_jmp_instruction(uint8_t* operandAddr)
{
    // Page 590.
    // jmp rel32 has opcode 0xe9, jmp rel8 has opcode 0xeb
    //
    TestAssert(operandAddr[-1] == 0xe9);
    int32_t operand = UnalignedRead<int32_t>(operandAddr);
    operand += 3;   // the instruction would be shortened by 3 bytes
    if (-128 <= operand && operand <= 127)
    {
        operandAddr[-1] = 0xeb;
        UnalignedWrite<int8_t>(operandAddr, static_cast<int8_t>(operand));
        x86_64_populate_NOP_instructions(operandAddr + 1, 3 /*length*/);
    }
}

// Rewrite 'jcc rel32' to 'jcc rel8' if possible
// jcc rel32 (6 bytes) => jcc rel8 (2 bytes) + nop4 (4 bytes)
//
inline void x86_64_try_rewrite_jcc_instruction(uint8_t* operandAddr)
{
    // Page 585 - 588.
    // jcc rel32 has opcode 0F 80 - 0F 8F
    // jcc rel8 has opcode 70 - 7F, and it one-to-one matches the rel32-version opcode
    //
    TestAssert(operandAddr[-2] == 0x0F && (0x80 <= operandAddr[-1] && operandAddr[-1] <= 0x8F));
    int32_t operand = UnalignedRead<int32_t>(operandAddr);
    operand += 4;   // the instruction would be shortened by 4 bytes
    if (-128 <= operand && operand <= 127)
    {
        operandAddr[-2] = operandAddr[-1] - 0x10;
        UnalignedWrite<int8_t>(operandAddr - 1, static_cast<int8_t>(operand));
        x86_64_populate_NOP_instructions(operandAddr, 4 /*length*/);
    }
}

}   // namespace PochiVM
