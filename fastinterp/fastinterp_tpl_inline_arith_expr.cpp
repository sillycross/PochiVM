#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

// Fully inlined arithmetic expr
// Takes 0 operands, outputs 1 operand
//
struct FIFullyInlinedArithmeticExprImpl
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
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value && arithType == AstArithmeticExprType::MOD) { return false; }
        // floating point division by 0 is undefined behavior, and clang generates a special relocation
        // to directly return the binary representation of NaN/Inf. We cannot support this relocation easily.
        //
        if ((arithType == AstArithmeticExprType::MOD || arithType == AstArithmeticExprType::DIV)
            && rhsShapeCategory == FISimpleOperandShapeCategory::ZERO) { return false; }
        return true;
    }

    template<typename OperandType,
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0 for LHS
    // constant placeholder 1 for RHS
    //
    template<typename OperandType,
             FISimpleOperandShapeCategory lhsShapeCategory,
             FISimpleOperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        OperandType lhs = FISimpleOperandShapeCategoryHelper::get_0<OperandType, lhsShapeCategory>(stackframe);
        OperandType rhs = FISimpleOperandShapeCategoryHelper::get_1<OperandType, rhsShapeCategory>(stackframe);

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
                    CreateEnumMetaVar<FISimpleOperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<FISimpleOperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory"),
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType"),
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
    RegisterBoilerplate<FIFullyInlinedArithmeticExprImpl>();
}
