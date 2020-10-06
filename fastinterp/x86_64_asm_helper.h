#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

// We don't handroll assembly instructions by ourselves,
// but we still need to know some x86_64 asm opcodes for test assertion purposes,
// just to make sure that LLVM's generated code matches our expectations.
//
constexpr uint8_t x86_64_jmp_instruction_opcode = 0xe9;
constexpr uint8_t x86_64_call_instruction_opcode = 0xe8;
constexpr uint8_t x86_64_ret_instruction_opcode = 0xc3;

}   // namespace PochiVM
