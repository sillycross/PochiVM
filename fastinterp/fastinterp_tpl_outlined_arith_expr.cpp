#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

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
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             AstArithmeticExprType arithType,
             FIBinaryOpNumQuickAccessParams numQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (!std::is_floating_point<OperandType>::value)
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
            if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0/1 for LHS/RHS if not quickaccess
    //
    template<typename OperandType,
             AstArithmeticExprType arithType,
             FIBinaryOpNumQuickAccessParams numQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, [[maybe_unused]] OperandType qa1, [[maybe_unused]] OperandType qa2) noexcept
    {
        OperandType lhs, rhs;
        if constexpr(static_cast<int>(numQAP) == 0)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
            lhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0);
            rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
        }
        else if constexpr(static_cast<int>(numQAP) == 1)
        {
            // We always evaluate LHS before RHS, so the QAP is always RHS
            //
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            lhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0);
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

        OperandType result;
        if constexpr(arithType == AstArithmeticExprType::ADD) {
            result = lhs + rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::SUB) {
            result = lhs - rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MUL) {
            result = lhs * rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::DIV) {
            result = lhs / rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MOD) {
            result = lhs % rhs;
        }
        else {
            static_assert(type_dependent_false<OperandType>::value, "Unexpected AstArithmeticExprType");
        }

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., OperandType) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType"),
                    CreateEnumMetaVar<FIBinaryOpNumQuickAccessParams::X_END_OF_ENUM>("numQAP"),
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
