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

constexpr uint32_t x86_64_jmp_opcode_num_bytes = 1;

// The length of a rip-relative jmp instruction '0xe9 imm32'
//
constexpr uint32_t x86_64_rip_relative_jmp_instruction_len = x86_64_jmp_opcode_num_bytes + sizeof(uint32_t);

}   // namespace PochiVM
