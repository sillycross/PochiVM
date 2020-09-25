#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_helper.h"
#include "fastinterp_tpl_operandshape_helper.h"

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
        return true;
    }

    template<typename ReturnType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory,
             AstArithmeticExprType arithType>
    static void f(InterpControlSignal* out) noexcept
    {
        ReturnType lhs = OperandShapeCategoryHelper::get_0_1<ReturnType, LhsIndexType, lhsShapeCategory>();
        ReturnType rhs = OperandShapeCategoryHelper::get_2_3<ReturnType, RhsIndexType, rhsShapeCategory>();

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
        *out = InterpControlSignal::Return;
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
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
    RegisterBoilerplate<FIReturnArithmeticExprImpl>();
}
