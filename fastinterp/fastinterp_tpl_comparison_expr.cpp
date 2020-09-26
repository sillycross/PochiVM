#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_helper.h"
#include "fastinterp_tpl_operandshape_helper.h"

namespace PochiVM
{

struct FIComparisonExprImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType>
    static constexpr bool cond()
    {
        if (!is_valid_index_type<LhsIndexType>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType>
    static constexpr bool cond()
    {
        if (!is_valid_index_type<RhsIndexType>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory>
    static constexpr bool cond()
    {
        if (!OperandShapeCategoryHelper::cond<LhsIndexType, lhsShapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory>
    static constexpr bool cond()
    {
        if (!OperandShapeCategoryHelper::cond<RhsIndexType, rhsShapeCategory>()) { return false; }

        // LHS and RHS cannot be both literal:
        // We cannot compare equality between two placeholders if they are 64 bits.
        // It is weird for users to write such expressions anyway, so it's OK to lose some performance in this case.
        //
        if (lhsShapeCategory == OperandShapeCategory::LITERAL_NONZERO &&
            rhsShapeCategory == OperandShapeCategory::LITERAL_NONZERO)
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory,
             AstComparisonExprType comparisonType>
    static constexpr bool cond()
    {
        return true;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory,
             AstComparisonExprType comparisonType>
    static bool f() noexcept
    {
        OperandType lhs = OperandShapeCategoryHelper::get_0_1<OperandType, LhsIndexType, lhsShapeCategory>();
        OperandType rhs = OperandShapeCategoryHelper::get_2_3<OperandType, RhsIndexType, rhsShapeCategory>();

        if constexpr(comparisonType == AstComparisonExprType::EQUAL) {
            return (lhs == rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::NOT_EQUAL) {
            return (lhs != rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::LESS_THAN) {
            return (lhs < rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::LESS_EQUAL) {
            return (lhs <= rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::GREATER_THAN) {
            return (lhs > rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::GREATER_EQUAL) {
            return (lhs >= rhs);
        }
        else
        {
            static_assert(type_dependent_false<OperandType>::value, "Unexpected AstComparisonExprType");
        }
    }
#pragma clang diagnostic pop

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("lhsIndexType"),
                    CreateTypeMetaVar("rhsIndexType"),
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory"),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("operatorType")
        );
    }
};

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    using namespace PochiVM;
    RegisterBoilerplate<FIComparisonExprImpl>();
}
