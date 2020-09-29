#pragma once

#include "common.h"
#include "fastinterp_tpl_loop_condition_shape.h"

namespace PochiVM
{

const int x_fastinterp_while_loop_num_inline_stmts = 7;
enum class FIWhileLoopNumStatements
{
    X_END_OF_ENUM = x_fastinterp_while_loop_num_inline_stmts + 1
};

// Whether we need to check for control-flow redirection after every statement.
//
enum class FIWhileLoopMayCFRMask
{
    X_END_OF_ENUM = (1 << x_fastinterp_while_loop_num_inline_stmts)
};

}   // namespace PochiVM
