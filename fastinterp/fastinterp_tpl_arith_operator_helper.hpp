#pragma once

#include "fastinterp_tpl_common.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

template<typename OperandType, AstArithmeticExprType operatorType>
OperandType WARN_UNUSED __attribute__((__always_inline__)) EvaluateArithmeticExpression(OperandType lhs, OperandType rhs) noexcept
{
    OperandType result;
    if constexpr(operatorType == AstArithmeticExprType::ADD) {
        result = lhs + rhs;
    }
    else if constexpr(operatorType == AstArithmeticExprType::SUB) {
        result = lhs - rhs;
    }
    else if constexpr(operatorType == AstArithmeticExprType::MUL) {
        result = lhs * rhs;
    }
    else if constexpr(operatorType == AstArithmeticExprType::DIV) {
        result = lhs / rhs;
    }
    else if constexpr(operatorType == AstArithmeticExprType::MOD) {
        result = lhs % rhs;
    }
    else {
        static_assert(type_dependent_false<OperandType>::value, "Unexpected AstArithmeticExprType");
    }
    return result;
}

}   // namespace PochiVM
