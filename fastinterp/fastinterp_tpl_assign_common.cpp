#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"

namespace PochiVM
{

// Handles 'assign' statement in form of 'var = osc op osc'
//
struct FIAssignStmtImpl
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
             FIOperandShapeCategory rhsShapeCategory>
    static void f() noexcept
    {
        OperandType* lhs;
        if constexpr(lhsShapeCategory == FIOperandShapeCategory::COMPLEX)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(OperandType*(*)() noexcept);
            lhs = BOILERPLATE_FNPTR_PLACEHOLDER_0();
        }
        else if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARIABLE)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            lhs = GetLocalVarAddress<OperandType>(CONSTANT_PLACEHOLDER_0);
        }
        else if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARPTR_DEREF)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            lhs = *GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_0);
        }
        else if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARPTR_VAR)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_0);
            LhsIndexType index = *GetLocalVarAddress<LhsIndexType>(CONSTANT_PLACEHOLDER_1);
            lhs = varPtr + index;
        }
        else if constexpr(lhsShapeCategory == FIOperandShapeCategory::VARPTR_LIT_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
            DEFINE_CONSTANT_PLACEHOLDER_1(LhsIndexType);
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(CONSTANT_PLACEHOLDER_0);
            lhs = varPtr + CONSTANT_PLACEHOLDER_1;
        }
        else
        {
            static_assert(type_dependent_false<OperandType>::value, "unexpected literal category");
        }

        OperandType rhs = FIOperandShapeCategoryHelper::get_2_3<OperandType, RhsIndexType, rhsShapeCategory>();
        *lhs = rhs;
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("lhsIndexType"),
                    CreateTypeMetaVar("rhsIndexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("lhsShapeCategory"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("rhsShapeCategory")
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
    RegisterBoilerplate<FIAssignStmtImpl>();
}
