#pragma once

#include "common.h"

namespace PochiVM
{

const int x_fastinterp_if_stmt_num_inline_stmts = 5;
constexpr std::array<int, x_fastinterp_if_stmt_num_inline_stmts + 1> x_fastinterp_if_stmt_cfr_limit {
    -1, -1, -1, 1, 0, 0
};
enum class FIIfStmtNumStatements
{
    X_END_OF_ENUM = x_fastinterp_if_stmt_num_inline_stmts + 1
};

// Whether we need to check for control-flow redirection after every statement.
//
enum class FIIfStmtMayCFRMask
{
    X_END_OF_ENUM = (1 << x_fastinterp_if_stmt_num_inline_stmts)
};

}   // namespace PochiVM
