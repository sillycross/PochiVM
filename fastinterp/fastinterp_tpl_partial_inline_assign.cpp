#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"

namespace PochiVM
{

// Partially inlined assign expression
// Takes 1 operand (RHS), outputs 0 operand
//
struct FIPartialInlineAssignImpl
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
            DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
            rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0);
        }

        OperandType* lhs = FIOperandShapeCategoryHelper::get_address_1_2<OperandType, LhsIndexType, lhsShapeCategory>(stackframe);

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
    RegisterBoilerplate<FIPartialInlineAssignImpl>();
}
