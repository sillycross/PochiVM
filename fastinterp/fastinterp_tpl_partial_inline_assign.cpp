#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

// Partially inlined assign expression
// Takes 1 operand (RHS), outputs 0 operand
//
struct FIPartialInlineAssignExprImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
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
             FIOperandShapeCategory lhsShapeCategory>
    static constexpr bool cond()
    {
        if (!FIOperandShapeCategoryHelper::cond<LhsIndexType, lhsShapeCategory>()) { return false; }
        if (lhsShapeCategory == FIOperandShapeCategory::LITERAL_NONZERO ||
            lhsShapeCategory == FIOperandShapeCategory::ZERO)
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
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
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
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
        return true;
    }

    // placeholder rules:
    // constant placeholder 0: RHS operand, if not quickaccess
    // constant placeholder 1/2: LHS shape
    //
    template<typename OperandType,
             typename LhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, [[maybe_unused]] OperandType qaOperand) noexcept
    {
        OperandType rhs;
        if constexpr(isQuickAccessOperand)
        {
            rhs = qaOperand;
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0);
        }

        OperandType* lhs;
        if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARIABLE)
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
            lhs = GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
        }
        else if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARPTR_DEREF)
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
            lhs = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_1);
        }
        else if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARPTR_VAR)
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_2(uint32_t);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_1);
            LhsIndexType index = *GetLocalVarAddress<LhsIndexType>(stackframe, CONSTANT_PLACEHOLDER_2);
            lhs = varPtr + index;
        }
        else if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARPTR_LIT_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_2(LhsIndexType);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_1);
            lhs = varPtr + CONSTANT_PLACEHOLDER_2;
        }
        else
        {
            static_assert(type_dependent_false<OperandType>::value, "unexpected literal category");
        }

        *lhs = rhs;

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("lhsIndexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateBoolMetaVar("isQuickAccessOperand"),
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
    RegisterBoilerplate<FIPartialInlineAssignExprImpl>();
}
