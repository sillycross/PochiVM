#pragma once

#include "common.h"

// This file is used by both pochivm and fastinterp
//

namespace PochiVM
{

enum class AstComparisonExprType
{
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_EQUAL,
    GREATER_THAN,
    GREATER_EQUAL,
    X_END_OF_ENUM
};

}   // namespace PochiVM
