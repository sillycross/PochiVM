#pragma once

#include "common.h"
#include "fastinterp_tpl_abi_distinct_type_helper.h"
#include "simple_constexpr_power_helper.h"

namespace PochiVM
{

// Fast interp aggregation optimization for operands.
// In interp, every operator becomes a function call.
// We want to aggregate simple and common shapes to reduce the number of function calls.
// E.g. we want to execute simple shapes like 'a[i]' in one function call,
// instead of many function calls (get addr 'a', deref 'a', get addr 'i', deref 'i',
// get addr 'a[i]', deref 'a[i]')
//
enum class FIOperandShapeCategory
{
    // A simple variable. e.g. 'a'
    //
    VARIABLE,
    // Variable dereference, e.g. '*a', 'a[0]'
    //
    VARPTR_DEREF,
    // Variable dereference with non-zero literal offset, e.g. 'a[12]'
    //
    VARPTR_LIT_NONZERO,
    // Variable dereference with variable offset, e.g. 'a[i]'
    //
    VARPTR_VAR,
    // Literal (nonzero). e.g. '12'
    //
    LITERAL_NONZERO,
    // Literal (zero)
    //
    ZERO,
    // A complex shape not belong to any simple shape above
    //
    COMPLEX,
    X_END_OF_ENUM
};

}   // namespace PochiVM
