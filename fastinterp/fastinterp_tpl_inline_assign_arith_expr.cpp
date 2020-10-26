#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_arith_operator_helper.hpp"

namespace PochiVM
{

// Fully inlined assign
// var = var[var/lit] op var[var/lit]
//
struct FIFullyInlineAssignArithExprImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_same<OperandType, bool>::value) { return false; }
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
             AstArithmeticExprType operatorType>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value && operatorType == AstArithmeticExprType::MOD) { return false; }
        if ((operatorType == AstArithmeticExprType::MOD || operatorType == AstArithmeticExprType::DIV)
            && rhsShapeCategory == FIOperandShapeCategory::ZERO) { return false; }
        return true;
    }

    // placeholder rules:
    // constant placeholder 0: var to be assigned
    // constant placeholder 1/2: LHS shape
    // constant placeholder 3/4: RHS shape
    //
    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstArithmeticExprType operatorType,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        OperandType lhs = FIOperandShapeCategoryHelper::get_1_2<OperandType, LhsIndexType, lhsShapeCategory>(stackframe);
        OperandType rhs = FIOperandShapeCategoryHelper::get_3_4<OperandType, RhsIndexType, rhsShapeCategory>(stackframe);
        OperandType result = EvaluateArithmeticExpression<OperandType, operatorType>(lhs, rhs);

        DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
        *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
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
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType")
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
    RegisterBoilerplate<FIFullyInlineAssignArithExprImpl>();
}
