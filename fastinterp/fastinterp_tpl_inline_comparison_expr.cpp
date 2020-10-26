#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_comparison_operator_helper.hpp"

namespace PochiVM
{

// Fully inlined comparison expr
// Takes 0 operands, outputs 1 operand
//
struct FIFullyInlinedComparisonExprImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value && !std::is_same<OperandType, void*>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstComparisonExprType operatorType>
    static constexpr bool cond()
    {
        if (!FISimpleOperandShapeCategoryHelper::cond<OperandType, lhsShapeCategory>()) { return false; }
        if (!FISimpleOperandShapeCategoryHelper::cond<OperandType, rhsShapeCategory>()) { return false; }
        // LHS and RHS cannot be both literal:
        // We cannot compare equality between two placeholders if they are 64 bits.
        // It is weird for users to write such expressions anyway, so it's OK to lose some performance in this case.
        //
        if (lhsShapeCategory == FISimpleOperandShapeCategory::LITERAL_NONZERO &&
            rhsShapeCategory == FISimpleOperandShapeCategory::LITERAL_NONZERO)
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstComparisonExprType operatorType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!spillOutput)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstComparisonExprType operatorType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1 for LHS
    // constant placeholder 2 for RHS
    //
    template<typename OperandType,
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstComparisonExprType operatorType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        OperandType lhs = FISimpleOperandShapeCategoryHelper::get_1<OperandType, lhsShapeCategory>(stackframe);
        OperandType rhs = FISimpleOperandShapeCategoryHelper::get_2<OperandType, rhsShapeCategory>(stackframe);

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
                    CreateEnumMetaVar<FISimpleOperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<FISimpleOperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory"),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("operatorType"),
                    CreateBoolMetaVar("spillOutput"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit()
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
    RegisterBoilerplate<FIFullyInlinedComparisonExprImpl>();
}
