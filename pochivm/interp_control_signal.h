#pragma once

#include "common.h"

// This file is used by both pochivm and fastinterp
//

namespace PochiVM
{

// Interp mode control flow redirection signal
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
