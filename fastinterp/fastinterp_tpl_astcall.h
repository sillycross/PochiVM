#pragma once

#include "common.h"
#include "fastinterp_tpl_abi_distinct_type_helper.h"
#include "simple_constexpr_power_helper.h"

namespace PochiVM
{

const int x_fastinterp_callexpr_num_inline_params = 3;
enum class FICallExprNumParameters
{
    MORE_THAN_THREE = x_fastinterp_callexpr_num_inline_params + 1,
    X_END_OF_ENUM
};

enum class FICallExprParamTypeMask
{
    X_END_OF_ENUM = math::power(static_cast<int>(FIABIDistinctType::X_END_OF_ENUM), x_fastinterp_callexpr_num_inline_params)
};

// In fastinterp, additional parameters to an AstCallExpr is passed in by this operator
// This operator itself inlines at most 4 additional parameters,
// and then optionally call the next operator to fill the still remaining ones.
//
const int x_fastinterp_callexpr_extra_num_inline_params = 4;
enum class FICallExprNumExtraParameters
{
    MORE_THAN_FOUR = x_fastinterp_callexpr_extra_num_inline_params + 1,
    X_END_OF_ENUM
};

enum class FICallExprExtraParamTypeMask
{
    X_END_OF_ENUM = math::power(static_cast<int>(FIABIDistinctType::X_END_OF_ENUM), x_fastinterp_callexpr_extra_num_inline_params)
};

}   // namespace PochiVM
