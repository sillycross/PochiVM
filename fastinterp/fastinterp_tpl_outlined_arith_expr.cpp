#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_arith_operator_helper.hpp"

namespace PochiVM
{

// Fully outlined arithmetic expr
// Takes 2 operands, outputs 1 operand
//
struct FIOutlinedArithmeticExprImpl
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
             AstArithmeticExprType arithType>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value && arithType == AstArithmeticExprType::MOD) { return false; }
        return true;
    }

    template<typename OperandType,
             AstArithmeticExprType arithType,
             FIBinaryOpNumQuickAccessParams numQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!std::is_floating_point<OperandType>::value)
        {
            // Check space for input
            //
            if (static_cast<int>(numQAP) == 0)
            {
                if (!FIOpaqueParamsHelper::IsEmpty(numOIP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::CanPush(numOIP, static_cast<int>(numQAP))) { return false; }
            }
            // Check space for output (1 operand)
            //
            if (!spillOutput)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
        }
        return true;
    }

    template<typename OperandType,
             AstArithmeticExprType arithType,
             FIBinaryOpNumQuickAccessParams numQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            // Check space for input
            //
            if (static_cast<int>(numQAP) == 0)
            {
                if (!FIOpaqueParamsHelper::IsEmpty(numOFP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP, static_cast<int>(numQAP))) { return false; }
            }
            // Check space for output (1 operand)
            //
            if (!spillOutput)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1/2 for LHS/RHS if not quickaccess
    //
    template<typename OperandType,
             AstArithmeticExprType arithType,
             FIBinaryOpNumQuickAccessParams numQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, [[maybe_unused]] OperandType qa1, [[maybe_unused]] OperandType qa2) noexcept
    {
        OperandType lhs, rhs;
        if constexpr(static_cast<int>(numQAP) == 0)
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint64_t);
            DEFINE_CONSTANT_PLACEHOLDER_2(uint64_t);
            lhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
            rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_2);
        }
        else if constexpr(static_cast<int>(numQAP) == 1)
        {
            // We always evaluate LHS before RHS, so the QAP is always RHS
            //
            DEFINE_CONSTANT_PLACEHOLDER_1(uint64_t);
            lhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
            // 'qa1' is not a typo: 'qa2' simply doesn't exist, 'qa1' is top of stack.
            //
            rhs = qa1;
        }
        else if constexpr(static_cast<int>(numQAP) == 2)
        {
            lhs = qa1;
            rhs = qa2;
        }
        else
        {
            static_assert(type_dependent_false<OperandType>::value, "Unexpected numQAP");
        }

        OperandType result = EvaluateArithmeticExpression<OperandType, arithType>(lhs, rhs);

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., OperandType) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
            *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType"),
                    CreateEnumMetaVar<FIBinaryOpNumQuickAccessParams::X_END_OF_ENUM>("numQAP"),
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
    RegisterBoilerplate<FIOutlinedArithmeticExprImpl>();
}
