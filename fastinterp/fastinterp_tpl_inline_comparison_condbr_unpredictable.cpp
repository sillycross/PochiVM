#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_comparison_operator_helper.hpp"
#include "fastinterp_tpl_conditional_jump_helper.hpp"

namespace PochiVM
{

// Fully inlined conditional branch based to comparison
// if (var[var/lit] op var[var/lit]) ....
//
struct FIFullyInlinedComparisonUnpredictableBranchImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value && !std::is_same<OperandType, void*>::value) { return false; }
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
             FIOperandShapeCategory lhsShapeCategory>
    static constexpr bool cond()
    {
        if (!FIOperandShapeCategoryHelper::cond<OperandType, LhsIndexType, lhsShapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory>
    static constexpr bool cond()
    {
        if (!FIOperandShapeCategoryHelper::cond<OperandType, RhsIndexType, rhsShapeCategory>()) { return false; }
        // LHS and RHS cannot be both literal:
        // We cannot compare equality between two placeholders if they are 64 bits.
        // It is weird for users to write such expressions anyway, so it's OK to lose some performance in this case.
        //
        if (lhsShapeCategory == FIOperandShapeCategory::LITERAL_NONZERO &&
            rhsShapeCategory == FIOperandShapeCategory::LITERAL_NONZERO)
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstComparisonExprType operatorType>
    static constexpr bool cond()
    {
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0/1 for LHS
    // constant placeholder 2/3 for RHS
    //
    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstComparisonExprType operatorType,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        OperandType lhs = FIOperandShapeCategoryHelper::get_0_1<OperandType, LhsIndexType, lhsShapeCategory>(stackframe);
        OperandType rhs = FIOperandShapeCategoryHelper::get_2_3<OperandType, RhsIndexType, rhsShapeCategory>(stackframe);
        bool result = EvaluateComparisonExpression<OperandType, operatorType>(lhs, rhs);
        FIConditionalJumpHelper::execute_0_1<FIConditionalJumpHelper::Mode::OptForSizeMode, OpaqueParams...>(result, stackframe, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("lhsIndexType"),
                    CreateTypeMetaVar("rhsIndexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit(),
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
    RegisterBoilerplate<FIFullyInlinedComparisonUnpredictableBranchImpl>(FIAttribute::OptSize);
}
