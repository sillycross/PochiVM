#pragma once

#include "common.h"

namespace PochiVM
{

const int x_fastinterp_block_max_stmts = 11;
enum class FIBlockNumStatements
{
    X_END_OF_ENUM = x_fastinterp_block_max_stmts + 1
};

enum class FIBlockMayCFRMask
{
    X_END_OF_ENUM = 1 << static_cast<int>(FIBlockNumStatements::X_END_OF_ENUM)
};

}   // namespace PochiVM
