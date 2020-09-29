#pragma once

#include "common.h"
#include "fastinterp_tpl_abi_distinct_type_helper.h"

namespace PochiVM
{

const int x_fastinterp_function_num_inline_stmts = 10;
enum class FIFunctionNumStatements
{
    X_END_OF_ENUM = x_fastinterp_function_num_inline_stmts + 1
};

enum class FIFunctionStmtsMayReturnMask
{
    X_END_OF_ENUM = (1 << x_fastinterp_function_num_inline_stmts)
};

}   // namespace PochiVM
