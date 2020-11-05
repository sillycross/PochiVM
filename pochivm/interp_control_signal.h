#pragma once

#include "common.h"

namespace PochiVM
{

// Debug interp mode control flow redirection signal
// Used for control flow redirection statements (Break/Continue/Return)
//
enum class InterpControlSignal
{
    None,
    Break,
    Continue,
    Return
};

}   // namespace PochiVM
