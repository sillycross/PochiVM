#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

template<typename T>
struct is_allowed_boilerplate_shape : std::false_type {};

template<typename R, typename... Args>
struct is_allowed_boilerplate_shape<R(Args...) noexcept> : std::true_type {};

}   // namespace PochiVM
