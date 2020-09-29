#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"

namespace PochiVM
{

struct FIArithmeticExprImpl
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
             OperandShapeCategory lhsShapeCategory>
    static constexpr bool cond()
    {
        if (!OperandShapeCategoryHelper::cond<LhsIndexType, lhsShapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory>
    static constexpr bool cond()
    {
        if (!OperandShapeCategoryHelper::cond<RhsIndexType, rhsShapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value && arithType == AstArithmeticExprType::MOD) { return false; }
        // floating point division by 0 is undefined behavior, and clang generates a special relocation
        // to directly return the binary representation of NaN/Inf. We cannot support this relocation easily.
        //
        if ((arithType == AstArithmeticExprType::MOD || arithType == AstArithmeticExprType::DIV)
            && rhsShapeCategory == OperandShapeCategory::ZERO) { return false; }
        return true;
    }

    // Placeholder rules:
    // placeholder 0/1 reserved for LHS
    // placeholder 2/3 reserved for RHS
    //
    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType>
    static OperandType f() noexcept
    {
        OperandType lhs = OperandShapeCategoryHelper::get_0_1<OperandType, LhsIndexType, lhsShapeCategory>();
        OperandType rhs = OperandShapeCategoryHelper::get_2_3<OperandType, RhsIndexType, rhsShapeCategory>();

        if constexpr(arithType == AstArithmeticExprType::ADD) {
            return lhs + rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::SUB) {
            return lhs - rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MUL) {
            return lhs * rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::DIV) {
            return lhs / rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MOD) {
            return lhs % rhs;
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
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory"),
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
    RegisterBoilerplate<FIArithmeticExprImpl>();
}
