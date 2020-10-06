#pragma once

#include <cstddef>

namespace PochiVM
{

// The function alignment of the generated fastinterp functions
// Currently eliminate-tail-call optimization is only possible when the function alignment is 1.
//
constexpr size_t x_fastinterp_function_alignment = 1;

}   // namespace PochiVM
