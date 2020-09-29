#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"

namespace PochiVM
{

struct FIReturnArithmeticExprImpl
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

    template<typename ReturnType,
             typename LhsIndexType,
             typename RhsIndexType,
             FIOperandShapeCategory lhsShapeCategory,
             FIOperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType>
    static InterpControlSignal f() noexcept
    {
        ReturnType lhs = FIOperandShapeCategoryHelper::get_0_1<ReturnType, LhsIndexType, lhsShapeCategory>();
        ReturnType rhs = FIOperandShapeCategoryHelper::get_2_3<ReturnType, RhsIndexType, rhsShapeCategory>();

        if constexpr(arithType == AstArithmeticExprType::ADD) {
            *GetLocalVarAddress<ReturnType>(0 /*offset*/) = lhs + rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::SUB) {
            *GetLocalVarAddress<ReturnType>(0 /*offset*/) = lhs - rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MUL) {
            *GetLocalVarAddress<ReturnType>(0 /*offset*/) = lhs * rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::DIV) {
            *GetLocalVarAddress<ReturnType>(0 /*offset*/) = lhs / rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MOD) {
            *GetLocalVarAddress<ReturnType>(0 /*offset*/) = lhs % rhs;
        }
        else {
            static_assert(type_dependent_false<ReturnType>::value, "Unexpected AstArithmeticExprType");
        }
        return InterpControlSignal::Return;
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
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
    RegisterBoilerplate<FIReturnArithmeticExprImpl>();
}
