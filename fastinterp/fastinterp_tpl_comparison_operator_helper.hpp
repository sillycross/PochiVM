#pragma once

#include "fastinterp_tpl_common.hpp"
#include "pochivm/ast_comparison_expr_type.h"

namespace PochiVM
{

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
template<typename OperandType, AstComparisonExprType operatorType>
bool WARN_UNUSED __attribute__((__always_inline__)) EvaluateComparisonExpression(OperandType lhs, OperandType rhs) noexcept
{
    if constexpr(operatorType == AstComparisonExprType::EQUAL) {
        return (lhs == rhs);
    }
    else if constexpr(operatorType == AstComparisonExprType::NOT_EQUAL) {
        return (lhs != rhs);
    }
    else if constexpr(operatorType == AstComparisonExprType::LESS_THAN) {
        return (lhs < rhs);
    }
    else if constexpr(operatorType == AstComparisonExprType::LESS_EQUAL) {
        return (lhs <= rhs);
    }
    else if constexpr(operatorType == AstComparisonExprType::GREATER_THAN) {
        return (lhs > rhs);
    }
    else if constexpr(operatorType == AstComparisonExprType::GREATER_EQUAL) {
        return (lhs >= rhs);
    }
    else
    {
        static_assert(type_dependent_false<OperandType>::value, "Unexpected AstComparisonExprType");
    }
}
#pragma clang diagnostic pop

}   // namespace PochiVM
