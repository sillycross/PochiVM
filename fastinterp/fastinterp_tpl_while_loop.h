#pragma once

#include "common.h"

namespace PochiVM
{

constexpr int x_fastinterp_while_loop_num_inline_stmts = 8;
constexpr std::array<int, x_fastinterp_while_loop_num_inline_stmts + 1> x_fastinterp_while_loop_cfr_limit {
    -1, -1, -1, -1, -1, -1, 3, 2, 1
};

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
