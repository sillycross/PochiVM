#pragma once

#include <cstddef>

namespace PochiVM
{

// The function alignment of the generated fastinterp functions
// This must at least the function alignment used to compile fastinterp_tpl_*.cpp
// Default clang++ function alignment is 16.
//
constexpr size_t x_fastinterp_function_alignment = 16;

}   // namespace PochiVM
