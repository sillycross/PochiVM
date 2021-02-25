#pragma once

#include "common.h"

namespace PochiVM
{

// If you change the value below, make sure to update the macros
// here below and in fastinterp_tpl_mem2reg_utils.hpp correspondingly!
//
constexpr size_t x_mem2reg_max_integral_vars = 5;
constexpr size_t x_mem2reg_max_floating_vars = 2;

#define DEF_MEM2REG_PARAMS  \
    uint64_t __mem2reg_i0   \
  , uint64_t __mem2reg_i1   \
  , uint64_t __mem2reg_i2   \
  , uint64_t __mem2reg_i3   \
  , uint64_t __mem2reg_i4   \
  , double __mem2reg_d0     \
  , double __mem2reg_d1

#define MEM2REG_TYPES  \
    uint64_t           \
  , uint64_t           \
  , uint64_t           \
  , uint64_t           \
  , uint64_t           \
  , double             \
  , double

#define PASS_MEM2REG_PARAMS \
    __mem2reg_i0            \
  , __mem2reg_i1            \
  , __mem2reg_i2            \
  , __mem2reg_i3            \
  , __mem2reg_i4            \
  , __mem2reg_d0            \
  , __mem2reg_d1

#define DEF_MEM2REG_DUMMYS          \
    uint64_t __mem2reg_i0_dummy;    \
    uint64_t __mem2reg_i1_dummy;    \
    uint64_t __mem2reg_i2_dummy;    \
    uint64_t __mem2reg_i3_dummy;    \
    uint64_t __mem2reg_i4_dummy;    \
    double __mem2reg_d0_dummy;      \
    double __mem2reg_d1_dummy

#define PASS_MEM2REG_DUMMYS         \
    __mem2reg_i0_dummy              \
  , __mem2reg_i1_dummy              \
  , __mem2reg_i2_dummy              \
  , __mem2reg_i3_dummy              \
  , __mem2reg_i4_dummy              \
  , __mem2reg_d0_dummy              \
  , __mem2reg_d1_dummy

enum class FIMem2RegOrdinal
{
    X_END_OF_ENUM = std::max(x_mem2reg_max_integral_vars, x_mem2reg_max_floating_vars)
};

}   // namespace PochiVM
