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
struct FIPartialInlinedComparisonBranchImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value && !std::is_same<OperandType, void*>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             typename IndexType>
    static constexpr bool cond()
    {
        if (!is_valid_index_type<IndexType>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory>
    static constexpr bool cond()
    {
        if (!FIOperandShapeCategoryHelper::cond<IndexType, shapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isInlinedSideLhs,
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!std::is_floating_point<OperandType>::value)
        {
            if (isQuickAccessOperand)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::IsEmpty(numOIP)) { return false; }
            }
        }
        else
        {
            if (FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isInlinedSideLhs,
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            if (isQuickAccessOperand)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::IsEmpty(numOFP)) { return false; }
            }
        }
        else
        {
            if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isInlinedSideLhs,
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstComparisonExprType operatorType,
             bool putFalseBranchAtEnd>
    static constexpr bool cond()
    {
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0 for outlined side, if not quickaccess
    // constant placeholder 1/2 for inline side
    //
    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isInlinedSideLhs,
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstComparisonExprType operatorType,
             bool putFalseBranchAtEnd,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, [[maybe_unused]] OperandType qaOperand) noexcept
    {
        OperandType lhs, rhs;
        if constexpr(isInlinedSideLhs)
        {
            lhs = FIOperandShapeCategoryHelper::get_1_2<OperandType, IndexType, shapeCategory>(stackframe);
            if constexpr(isQuickAccessOperand)
            {
                rhs = qaOperand;
            }
            else
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
                rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0);
            }
        }
        else
        {
            rhs = FIOperandShapeCategoryHelper::get_1_2<OperandType, IndexType, shapeCategory>(stackframe);
            if constexpr(isQuickAccessOperand)
            {
                lhs = qaOperand;
            }
            else
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
                lhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0);
            }
        }

        bool result = EvaluateComparisonExpression<OperandType, operatorType>(lhs, rhs);
        FIConditionalJumpHelper::execute_0_1<putFalseBranchAtEnd, OpaqueParams...>(result, stackframe, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("indexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("shapeCategory"),
                    CreateBoolMetaVar("isInlinedSideLhs"),
                    CreateBoolMetaVar("isQAP"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit(),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("operatorType"),
                    CreateBoolMetaVar("putFalseBranchAtEnd")
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
    RegisterBoilerplate<FIPartialInlinedComparisonBranchImpl>();
}
