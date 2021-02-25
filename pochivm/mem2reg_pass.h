#pragma once

#include "common.h"
#include "fastinterp/fastinterp_mem2reg_helper.h"

namespace PochiVM
{

class AstFunction;

// Apply a simple mem2reg transform pass
//
void ApplyMem2RegPass(AstFunction* func);

}   // namespace PochiVM
