#pragma once

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

#define GEN_MEM2REG_EXTRACT_DEFS(isInt, ordinal, valueType)                                             \
    typename std::conditional<((isInt) && (ordinal) == 0), valueType, uint64_t>::type __mem2reg_i0    \
  , typename std::conditional<((isInt) && (ordinal) == 1), valueType, uint64_t>::type __mem2reg_i1    \
  , typename std::conditional<((isInt) && (ordinal) == 2), valueType, uint64_t>::type __mem2reg_i2    \
  , typename std::conditional<((isInt) && (ordinal) == 3), valueType, uint64_t>::type __mem2reg_i3    \
  , typename std::conditional<((isInt) && (ordinal) == 4), valueType, uint64_t>::type __mem2reg_i4    \
  , typename std::conditional<(!(isInt) && (ordinal) == 0), valueType, double>::type __mem2reg_d0     \
  , typename std::conditional<(!(isInt) && (ordinal) == 1), valueType, double>::type __mem2reg_d1

#define GEN_MEM2REG_EXTRACT_TYPES(isInt, ordinal, valueType)                                \
    typename std::conditional<((isInt) && (ordinal) == 0), valueType, uint64_t>::type     \
  , typename std::conditional<((isInt) && (ordinal) == 1), valueType, uint64_t>::type     \
  , typename std::conditional<((isInt) && (ordinal) == 2), valueType, uint64_t>::type     \
  , typename std::conditional<((isInt) && (ordinal) == 3), valueType, uint64_t>::type     \
  , typename std::conditional<((isInt) && (ordinal) == 4), valueType, uint64_t>::type     \
  , typename std::conditional<(!(isInt) && (ordinal) == 0), valueType, double>::type      \
  , typename std::conditional<(!(isInt) && (ordinal) == 1), valueType, double>::type

#define EXTRACT_MEM2REG_VALUE(isInt, ordinal)   \
    std::get<((isInt) ? 0 : x_mem2reg_max_integral_vars) + (ordinal)>(std::make_tuple(PASS_MEM2REG_PARAMS))

#define INSERT_MEM2REG_VALUE(isInt, ordinal)   \
    std::get<((isInt) ? 0 : x_mem2reg_max_integral_vars) + (ordinal)>(std::tie(PASS_MEM2REG_PARAMS))

}   // namespace PochiVM
