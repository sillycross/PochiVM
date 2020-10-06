#pragma once

#include <cstdint>

namespace PochiVM
{

// In ELF x86_64, thread_local variables are implemented using the FS segmentation register
//
// This function returns the base address of the FS segmentation register,
// which allows us to compute the relocation offset of a thread_local variable.
//
uint64_t x86_64_get_fs_segmentation_register_base();

}   // namespace PochiVM
