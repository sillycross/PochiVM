#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

struct FIComparisonExprImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
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
        // If LHS is not an array-element shape, we should always pass in the fake LhsIndexType of int32_t
        //
        if (!(lhsShapeCategory == OperandShapeCategory::VARPTR_VAR ||
            lhsShapeCategory == OperandShapeCategory::VARPTR_LIT_NONZERO) &&
            !std::is_same<LhsIndexType, int32_t>::value)
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory>
    static constexpr bool cond()
    {
        // If RHS is not an array-element shape, we should always pass in the fake RhsIndexType of int32_t
        //
        if (!(rhsShapeCategory == OperandShapeCategory::VARPTR_VAR ||
            rhsShapeCategory == OperandShapeCategory::VARPTR_LIT_NONZERO) &&
            !std::is_same<RhsIndexType, int32_t>::value)
        {
            return false;
        }
        // LHS and RHS cannot be both literal:
        // We cannot compare equality between two placeholders if they are 64 bits.
        // It is weird for users to write such expressions anyway, so it's OK to lose some performance in this case.
        //
        if (lhsShapeCategory == OperandShapeCategory::LITERAL_NONZERO &&
            rhsShapeCategory == OperandShapeCategory::LITERAL_NONZERO)
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory,
             AstComparisonExprType comparisonType>
    static constexpr bool cond()
    {
        return true;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    template<typename OperandType,
             typename LhsIndexType,
             typename RhsIndexType,
             OperandShapeCategory lhsShapeCategory,
             OperandShapeCategory rhsShapeCategory,
             AstComparisonExprType comparisonType>
    static void f(bool* out) noexcept
    {
        OperandType lhs;
        if constexpr(lhsShapeCategory == OperandShapeCategory::COMPLEX)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(OperandType*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(&lhs /*out*/);
        }
        else if constexpr(lhsShapeCategory == OperandShapeCategory::LITERAL_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(OperandType);
            lhs = CONSTANT_PLACEHOLDER_0;
        }
        else if constexpr(lhsShapeCategory == OperandShapeCategory::ZERO)
        {
            constexpr OperandType v = PochiVM::get_all_bits_zero_value<OperandType>();
            lhs = v;
        }
        else if constexpr(lhsShapeCategory == OperandShapeCategory::VARIABLE)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            lhs = *GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_0);
        }
        else if constexpr(lhsShapeCategory == OperandShapeCategory::VARPTR_DEREF)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            lhs = **GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_0);
        }
        else if constexpr(lhsShapeCategory == OperandShapeCategory::VARPTR_VAR)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_0);
            LhsIndexType index = *GetLocalVarAddress<LhsIndexType>(CONSTANT_PLACEHOLDER_1);
            lhs = varPtr[index];
        }
        else if constexpr(lhsShapeCategory == OperandShapeCategory::VARPTR_LIT_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_1(LhsIndexType);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_0);
            lhs = varPtr[CONSTANT_PLACEHOLDER_1];
        }
        else
        {
            static_assert(type_dependent_false<OperandType>::value, "unexpected literal category");
        }

        OperandType rhs;
        if constexpr(rhsShapeCategory == OperandShapeCategory::COMPLEX)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_2(void(*)(OperandType*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_2(&rhs /*out*/);
        }
        else if constexpr(rhsShapeCategory == OperandShapeCategory::LITERAL_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_2(OperandType);
            rhs = CONSTANT_PLACEHOLDER_2;
        }
        else if constexpr(rhsShapeCategory == OperandShapeCategory::ZERO)
        {
            constexpr OperandType v = PochiVM::get_all_bits_zero_value<OperandType>();
            rhs = v;
        }
        else if constexpr(rhsShapeCategory == OperandShapeCategory::VARIABLE)
        {
            DEFINE_CONSTANT_PLACEHOLDER_2(uint32_t);
            rhs = *GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_2);
        }
        else if constexpr(rhsShapeCategory == OperandShapeCategory::VARPTR_DEREF)
        {
            DEFINE_CONSTANT_PLACEHOLDER_2(uint32_t);
            rhs = **GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_2);
        }
        else if constexpr(rhsShapeCategory == OperandShapeCategory::VARPTR_VAR)
        {
            DEFINE_CONSTANT_PLACEHOLDER_2(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_3(uint32_t);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_2);
            RhsIndexType index = *GetLocalVarAddress<RhsIndexType>(CONSTANT_PLACEHOLDER_3);
            rhs = varPtr[index];
        }
        else if constexpr(rhsShapeCategory == OperandShapeCategory::VARPTR_LIT_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_2(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_3(RhsIndexType);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_2);
            rhs = varPtr[CONSTANT_PLACEHOLDER_3];
        }
        else
        {
            static_assert(type_dependent_false<OperandType>::value, "unexpected literal category");
        }

        if constexpr(comparisonType == AstComparisonExprType::EQUAL) {
            *out = (lhs == rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::NOT_EQUAL) {
            *out = (lhs != rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::LESS_THAN) {
            *out = (lhs < rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::LESS_EQUAL) {
            *out = (lhs <= rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::GREATER_THAN) {
            *out = (lhs > rhs);
        }
        else if constexpr(comparisonType == AstComparisonExprType::GREATER_EQUAL) {
            *out = (lhs >= rhs);
        }
        else
        {
            static_assert(type_dependent_false<OperandType>::value, "Unexpected AstComparisonExprType");
        }
    }
#pragma clang diagnostic pop

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("lhsIndexType"),
                    CreateTypeMetaVar("rhsIndexType"),
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory"),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("operatorType")
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
    RegisterBoilerplate<FIComparisonExprImpl>();
}
