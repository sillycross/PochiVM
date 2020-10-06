#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"

namespace PochiVM
{

// Partially inlined assign expression
// % = var[var/lit]
// Takes 1 operand (LHS), outputs 0 operand
//
struct FIPartialInlineRhsAssignImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
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
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (isQuickAccessOperand)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        else
        {
            if (!FIOpaqueParamsHelper::IsEmpty(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    // placeholder rules:
    // constant placeholder 0: LHS operand, if not quickaccess
    // constant placeholder 1/2: RHS shape
    //
    template<typename OperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool isQuickAccessOperand,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, [[maybe_unused]] OperandType* qaOperand) noexcept
    {
        OperandType* lhs;
        if constexpr(isQuickAccessOperand)
        {
            lhs = qaOperand;
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
            lhs = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_0);
        }

        OperandType rhs = FIOperandShapeCategoryHelper::get_1_2<OperandType, IndexType, shapeCategory>(stackframe);
        *lhs = rhs;

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("IndexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("shapeCategory"),
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
    RegisterBoilerplate<FIPartialInlineRhsAssignImpl>();
}
