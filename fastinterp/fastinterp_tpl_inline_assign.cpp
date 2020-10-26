#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

// Fully inlined assign
// var[var/lit] = var[var/lit]
//
struct FIFullyInlineAssignImpl
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
        if (lhsShapeCategory == FIOperandShapeCategory::LITERAL_NONZERO ||
            lhsShapeCategory == FIOperandShapeCategory::ZERO)
        {
            return false;
        }
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

    // placeholder rules:
    // constant placeholder 0/1: LHS shape
    // constant placeholder 2/3: RHS shape
    //
    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        OperandType* lhs = FIOperandShapeCategoryHelper::get_address_0_1<OperandType, LhsIndexType, lhsShapeCategory>(stackframe);
        OperandType rhs = FIOperandShapeCategoryHelper::get_2_3<OperandType, RhsIndexType, rhsShapeCategory>(stackframe);
        *lhs = rhs;

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
    RegisterBoilerplate<FIFullyInlineAssignImpl>();
}
