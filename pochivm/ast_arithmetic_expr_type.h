#pragma once

#include "common.h"

// This file is used by both pochivm and fastinterp
//

namespace PochiVM
{

enum class AstArithmeticExprType
{
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    X_END_OF_ENUM
};

}   // namespace PochiVM
