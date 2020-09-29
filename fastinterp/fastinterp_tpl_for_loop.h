#pragma once

#include "common.h"
#include "fastinterp_tpl_loop_condition_shape.h"

namespace PochiVM
{

const int x_fastinterp_for_loop_body_num_inline_stmts = 5;
enum class FIForLoopBodyNumStatements
{
    X_END_OF_ENUM = x_fastinterp_for_loop_body_num_inline_stmts + 1
};

// Whether we need to check for control-flow redirection after every statement.
//
enum class FIForLoopBodyMayCFRMask
{
    X_END_OF_ENUM = (1 << x_fastinterp_for_loop_body_num_inline_stmts)
};

// We disallow break/continue/return in for-loop step block, so no control flow redirection may happen
//
const int x_fastinterp_for_loop_step_num_inline_stmts = 4;
enum class FIForLoopStepNumStatements
{
    X_END_OF_ENUM = x_fastinterp_for_loop_step_num_inline_stmts + 1
};

}   // namespace PochiVM
