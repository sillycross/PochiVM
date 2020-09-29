#pragma once

#include "pochivm/common.h"
#include "pochivm/ast_enums.h"

namespace PochiVM
{

// We inline simple and common condition shapes for if-statement, for-loop and while-loop.
// The following shapes are considered simple:
// (1) a boolean variable
// (2) literal 'true'
// (2) simple comparison in the form of 'a op b' where:
//     (a) Type of 'a' and 'b' is signed/unsigned 32/64 bit integers
//     (b) 'op' is one of '<', '<=', '==' (high-level code can do mirroring to support >, >=, !=)
//     (c) 'a' and 'b' is either a variable or a literal, and at least one of 'a' and 'b' is a variable
//
// The purpose of the above limitation is to limit the number of combinations to an reasonable amount,
// so we don't generate too many template instantiations.
//
// Under the above limitation, there is a total of 51 valid combinations.
//
enum class FIConditionShapeCategory
{
    COMPLEX,
    VARIABLE,
    LITERAL_TRUE,
    // Comparison in the shape of 'var/literal op var/literal' which are 32/64 bit integers
    //
    SIMPLE_COMPARISON,
    X_END_OF_ENUM
};

enum class FIConditionOperandShapeCategory
{
    LITERAL_ZERO,
    LITERAL_NONZERO_OR_32BIT,
    VARIABLE,
    X_END_OF_ENUM
};

struct FIConditionCombChecker
{
    template<typename CondOperatorType>
    static constexpr bool cond()
    {
        if (!std::is_same<CondOperatorType, int32_t>::value &&
            !std::is_same<CondOperatorType, uint32_t>::value &&
            !std::is_same<CondOperatorType, int64_t>::value &&
            !std::is_same<CondOperatorType, uint64_t>::value)
        {
            return false;
        }
        return true;
    }

    template<typename CondOperatorType,
             FIConditionShapeCategory condShape>
    static constexpr bool cond()
    {
        if (condShape != FIConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (!std::is_same<CondOperatorType, int32_t>::value) { return false; }
        }
        return true;
    }

    template<typename CondOperatorType,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator>
    static constexpr bool cond()
    {
        if (condShape != FIConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (condComparator != AstComparisonExprType::EQUAL) { return false; }
        }
        if (condComparator != AstComparisonExprType::EQUAL &&
            condComparator != AstComparisonExprType::LESS_THAN &&
            condComparator != AstComparisonExprType::LESS_EQUAL)
        {
            return false;
        }
        return true;
    }

    template<typename CondOperatorType,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape>
    static constexpr bool cond()
    {
        if (condShape != FIConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (condLhsShape != FIConditionOperandShapeCategory::LITERAL_ZERO) { return false; }
        }
        else
        {
            if (sizeof(CondOperatorType) != 8 && condLhsShape == FIConditionOperandShapeCategory::LITERAL_ZERO)
            {
                return false;
            }
        }
        return true;
    }

    template<typename CondOperatorType,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape,
             FIConditionOperandShapeCategory condRhsShape>
    static constexpr bool cond()
    {
        if (condShape != FIConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (condRhsShape != FIConditionOperandShapeCategory::LITERAL_ZERO) { return false; }
        }
        else
        {
            if (sizeof(CondOperatorType) != 8 && condRhsShape == FIConditionOperandShapeCategory::LITERAL_ZERO)
            {
                return false;
            }
            // At least one side of the comparison should be a variable
            //
            if (condLhsShape != FIConditionOperandShapeCategory::VARIABLE &&
                condRhsShape != FIConditionOperandShapeCategory::VARIABLE)
            {
                return false;
            }
        }
        return true;
    }
};

}   // namespace PochiVM
