#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

const int x_fastinterp_max_integral_params = 3;
const int x_fastinterp_max_floating_point_params = 3;

enum class FINumOpaqueIntegralParams
{
    X_END_OF_ENUM = x_fastinterp_max_integral_params + 1
};

enum class FINumOpaqueFloatingParams
{
    X_END_OF_ENUM = x_fastinterp_max_floating_point_params + 1
};

}   // namespace PochiVM
