#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_comparison_operator_helper.hpp"

namespace PochiVM
{

// Fully outlined comparison expr
// Takes 2 operands, outputs 1 operand
//
struct FIOutlinedComparisonExprImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value && !std::is_same<OperandType, void*>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             AstComparisonExprType operatorType,
             bool isLhsQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (spillOutput)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        if (!std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP, 1 + (isLhsQAP ? 1 : 0))) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             AstComparisonExprType operatorType,
             bool isLhsQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOFP, 1 + (isLhsQAP ? 1 : 0))) { return false; }
        }
        else
        {
            if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1 if LHS is not QAP
    //
    template<typename OperandType,
             AstComparisonExprType operatorType,
             bool isLhsQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, OperandType qa1, [[maybe_unused]] OperandType qa2) noexcept
    {
        OperandType lhs, rhs;
        if constexpr(!isLhsQAP)
        {
            // We always evaluate LHS before RHS, so the QAP is always RHS
            //
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_1;
            lhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
            // 'qa1' is not a typo: 'qa2' simply doesn't exist, 'qa1' is top of stack.
            //
            rhs = qa1;
        }
        else
        {
            lhs = qa1;
            rhs = qa2;
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
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("operatorType"),
                    CreateBoolMetaVar("isLhsQAP"),
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
    RegisterBoilerplate<FIOutlinedComparisonExprImpl>();
}
