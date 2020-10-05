#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_arith_operator_helper.hpp"

namespace PochiVM
{

// Partially inlined arithmetic expression.
// One side of the expression is inlined.
// Takes 1 operand (LHS or RHS), outputs 1 operand
//
struct FIPartialInlineArithmeticExprImpl
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
             bool spillOutput,
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
            if (!spillOutput)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
        }
        else
        {
            // We won't need to bother with the # of pinned registers, just assume the max, so less templates are generated.
            //
            if (FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isInlinedSideLhs,
             bool isQuickAccessOperand,
             bool spillOutput,
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
            if (!spillOutput)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            }
        }
        else
        {
            // We won't need to bother with the # of pinned registers, just assume the max, so less templates are generated.
            //
            if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isInlinedSideLhs,
             bool isQuickAccessOperand,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstArithmeticExprType operatorType>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value && operatorType == AstArithmeticExprType::MOD) { return false; }
        if (!isInlinedSideLhs && shapeCategory == FIOperandShapeCategory::ZERO &&
            (operatorType == AstArithmeticExprType::MOD || operatorType == AstArithmeticExprType::DIV)) { return false; }
        return true;
    }

    // placeholder rules:
    // constant placeholder 0: spill position, if spillOutput
    // constant placeholder 1: outlined operand position, if not quickaccess
    // constant placeholder 2/3: inlined operand shape
    //
    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isInlinedSideLhs,
             bool isQuickAccessOperand,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             AstArithmeticExprType operatorType,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, [[maybe_unused]] OperandType qaOperand) noexcept
    {
        OperandType lhs, rhs;
        if constexpr(isInlinedSideLhs)
        {
            lhs = FIOperandShapeCategoryHelper::get_2_3<OperandType, IndexType, shapeCategory>(stackframe);
            if constexpr(isQuickAccessOperand)
            {
                rhs = qaOperand;
            }
            else
            {
                DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
                rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
            }
        }
        else
        {
            rhs = FIOperandShapeCategoryHelper::get_2_3<OperandType, IndexType, shapeCategory>(stackframe);
            if constexpr(isQuickAccessOperand)
            {
                lhs = qaOperand;
            }
            else
            {
                DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
                lhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
            }
        }

        OperandType result = EvaluateArithmeticExpression<OperandType, operatorType>(lhs, rhs);

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., OperandType) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

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
                    CreateBoolMetaVar("isInlinedSideLhs"),
                    CreateBoolMetaVar("isQuickAccessOperand"),
                    CreateBoolMetaVar("spillOutput"),
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
    RegisterBoilerplate<FIPartialInlineArithmeticExprImpl>();
}
