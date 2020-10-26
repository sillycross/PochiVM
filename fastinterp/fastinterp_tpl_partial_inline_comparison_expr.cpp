#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_comparison_operator_helper.hpp"
#include "fastinterp_tpl_conditional_jump_helper.hpp"

namespace PochiVM
{

// Partially inlined comparison expression
// % op var[var/lit] or var/[var/lit] op %
//
struct FIPartialInlinedComparisonExprImpl
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
        if (!FIOperandShapeCategoryHelper::cond<OperandType, IndexType, shapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool spillOutput,
             bool isInlinedSideLhs,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!spillOutput)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        if (!std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool spillOutput,
             bool isInlinedSideLhs,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
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
             bool spillOutput,
             bool isInlinedSideLhs,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstComparisonExprType operatorType>
    static constexpr bool cond()
    {
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1/2 for inline side
    //
    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool spillOutput,
             bool isInlinedSideLhs,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstComparisonExprType operatorType,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, OperandType qaOperand) noexcept
    {
        OperandType lhs, rhs;
        if constexpr(isInlinedSideLhs)
        {
            lhs = FIOperandShapeCategoryHelper::get_1_2<OperandType, IndexType, shapeCategory>(stackframe);
            rhs = qaOperand;
        }
        else
        {
            rhs = FIOperandShapeCategoryHelper::get_1_2<OperandType, IndexType, shapeCategory>(stackframe);
            lhs = qaOperand;
        }

        bool result = EvaluateComparisonExpression<OperandType, operatorType>(lhs, rhs);

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., bool) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<bool>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("indexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("shapeCategory"),
                    CreateBoolMetaVar("spillOutput"),
                    CreateBoolMetaVar("isInlinedSideLhs"),
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
    RegisterBoilerplate<FIPartialInlinedComparisonExprImpl>();
}
