#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

template<typename T>
struct is_allowed_boilerplate_shape : std::false_type {};

// Normal interp fn used for almost everything
//
template<typename T>
struct is_allowed_boilerplate_shape<T() noexcept> : std::true_type {};

// A special interp fn prototype, used for populating extra parameters in a function call
//
template<>
struct is_allowed_boilerplate_shape<void(uintptr_t) noexcept> : std::true_type {};

}   // namespace PochiVM
