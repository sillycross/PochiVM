#pragma once

#include "fastinterp_tpl_condition_shape.h"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIConditionShapeHelper
{
    // Uses constant placeholder 0/1 or boilerplate placeholder 0
    //
    template<typename CondOperatorType,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape,
             FIConditionOperandShapeCategory condRhsShape>
    static bool WARN_UNUSED __attribute__((__always_inline__)) get_0_1()
    {
        if constexpr(condShape == FIConditionShapeCategory::COMPLEX)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(bool(*)() noexcept);
            return BOILERPLATE_FNPTR_PLACEHOLDER_0();
        }
        else if constexpr(condShape == FIConditionShapeCategory::VARIABLE)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            return *GetLocalVarAddress<bool>(CONSTANT_PLACEHOLDER_0);
        }
        else if constexpr(condShape == FIConditionShapeCategory::SIMPLE_COMPARISON)
        {
            CondOperatorType lhs, rhs;
            if constexpr(condLhsShape == FIConditionOperandShapeCategory::VARIABLE)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                lhs = *GetLocalVarAddress<CondOperatorType>(CONSTANT_PLACEHOLDER_0);
            }
            else if constexpr(condLhsShape == FIConditionOperandShapeCategory::LITERAL_NONZERO_OR_32BIT)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(CondOperatorType);
                lhs = CONSTANT_PLACEHOLDER_0;
            }
            else
            {
                static_assert(condLhsShape == FIConditionOperandShapeCategory::LITERAL_ZERO);
                lhs = 0;
            }

            if constexpr(condRhsShape == FIConditionOperandShapeCategory::VARIABLE)
            {
                DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
                rhs = *GetLocalVarAddress<CondOperatorType>(CONSTANT_PLACEHOLDER_1);
            }
            else if constexpr(condRhsShape == FIConditionOperandShapeCategory::LITERAL_NONZERO_OR_32BIT)
            {
                DEFINE_CONSTANT_PLACEHOLDER_1(CondOperatorType);
                rhs = CONSTANT_PLACEHOLDER_1;
            }
            else
            {
                static_assert(condRhsShape == FIConditionOperandShapeCategory::LITERAL_ZERO);
                rhs = 0;
            }

            if constexpr(condComparator == AstComparisonExprType::EQUAL)
            {
                return (lhs == rhs);
            }
            else if constexpr(condComparator == AstComparisonExprType::LESS_THAN)
            {
                return (lhs < rhs);
            }
            else
            {
                static_assert(condComparator == AstComparisonExprType::LESS_EQUAL);
                return (lhs <= rhs);
            }
        }
        else
        {
            static_assert(condShape == FIConditionShapeCategory::LITERAL_TRUE);
            return true;
        }
    }
};

}   // namespace PochiVM
