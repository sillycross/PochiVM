#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

// Handles 'assign' statement in form of 'var = osc op osc'
//
struct FIAssignArithmeticExprImpl
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
        if (!FIOperandShapeCategoryHelper::cond<LhsIndexType, lhsShapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory>
    static constexpr bool cond()
    {
        if (!FIOperandShapeCategoryHelper::cond<RhsIndexType, rhsShapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value && arithType == AstArithmeticExprType::MOD) { return false; }
        // floating point division by 0 is undefined behavior, and clang generates a special relocation
        // to directly return the binary representation of NaN/Inf. We cannot support this relocation easily.
        //
        if ((arithType == AstArithmeticExprType::MOD || arithType == AstArithmeticExprType::DIV)
            && rhsShapeCategory == FIOperandShapeCategory::ZERO) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType>
    static void f() noexcept
    {
        DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
        OperandType lhs = FIOperandShapeCategoryHelper::get_1_2<OperandType, LhsIndexType, lhsShapeCategory>();
        OperandType rhs = FIOperandShapeCategoryHelper::get_3_4<OperandType, RhsIndexType, rhsShapeCategory>();

        if constexpr(arithType == AstArithmeticExprType::ADD) {
            *GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_0) = lhs + rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::SUB) {
            *GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_0) = lhs - rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MUL) {
            *GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_0) = lhs * rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::DIV) {
            *GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_0) = lhs / rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MOD) {
            *GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_0) = lhs % rhs;
        }
        else {
            static_assert(type_dependent_false<OperandType>::value, "Unexpected AstArithmeticExprType");
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("lhsIndexType"),
                    CreateTypeMetaVar("rhsIndexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory"),
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
    RegisterBoilerplate<FIAssignArithmeticExprImpl>();
}
