#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

template<typename T>
struct is_allowed_boilerplate_shape : std::false_type {};

template<typename R, typename... Args>
struct is_allowed_boilerplate_shape<R(Args...) noexcept> : std::true_type {};

// It seems like "attribute" prevents "Args" above from binding to the type.. Just special case it..
//
template<typename R>
struct is_allowed_boilerplate_shape<R(uintptr_t, __attribute__((__noescape__)) uint8_t*) noexcept> : std::true_type {};

}   // namespace PochiVM
