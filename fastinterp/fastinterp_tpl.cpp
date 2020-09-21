#define INSIDE_FASTINTERP_TPL_CPP

#include "metavar.h"
#include "dynamic_specialization_utils.h"
#include "pochivm/ast_enums.h"
#include "fastinterp_context.h"

// fastinterp_tpl.cpp
//
//    Interp execution is slow, and one important reason is that the control flow
//    is coupled with data. To interp execute any operator, one must make an *indirect*
//    function call, hurting CPU pipelining. And the values which are constant to an operator
//    (e.g. its parameters and its input operators) still must be retrieved from memory,
//    resulting in cache misses.
//
//    FastInterp is a solution to the above issue. Each AST node still correspond
//    to a function which interp-executes this node. However, the initial function will contain
//    'placeholders', which are placeholder values that must be populated before execution.
//
//    This allows the function to refer to its operator-specific configurations as placeholders,
//    which will be directly "burned into" the binary later, getting rid of the indirect
//    function calls, or cache-missing memory accesses.
//
//    To execute such a "function boilerplate" with placeholders, we make a copy of the function
//    binary, and then populate the placeholder values, using a process similar to relocation of
//    dynamic libraries. Of course, the function binaries are pre-processed, so that at runtime
//    all we need to do is a memcpy of the function body plus a few fixed arithmetic operations.
//    This retains interp execution's advantage of extremely low setup time, while giving
//    drastically better performance than naive interpretation approach.
//
//    This file contains those "function boilerplates" for interp execution.
//
//    Since the function binary is copied to elsewhere before execution and we don't want to fix
//    the exception frames (for both engineering complexity and performance reasons), there is no
//    C++ exception support. We soft-emulate C++ exception using a combination of C++ code stubs
//    (which reside outside this file and does not use placeholders), soft-implemented destructor
//    sequence logic and C setjmp/longjmp.
//

// Communication interface with build_fast_interp_lib.cpp
// This is used to obtain the symbol names of all the boilerplates, similar to the trick we used in dump_symbols.cpp
//
void __pochivm_register_fast_interp_boilerplate__(const char* /*stringified_name*/, PochiVM::MetaVarMaterializedList* /*list*/);

namespace PochiVM
{

// Helper function to get local variable address
// this function must be always inlined FOR CORRECTNESS, otherwise it would become an
// unexpected external symbol and fire an assert in build_fast_interp_lib.cpp.
//
// Offset is uint32_t is also intentional --  we cannot pass uint64_t '0' using constant placeholders,
// so if the caller accidentally did this, it would trigger a narrowing-conversion warning.
//
template<typename LocalVarType>
static inline LocalVarType* __attribute__((__always_inline__)) GetLocalVarAddress(uint32_t offset) noexcept
{
    return reinterpret_cast<LocalVarType*>(__pochivm_thread_fastinterp_context.m_stackFrame + offset);
}

// Whether a type is suitable to be used as an array index
// To make the number of templates under control, we only inline the most common index types:
// unsigned/signed 32/64 bits
//
template<typename T>
constexpr bool is_valid_index_type()
{
    if (std::is_same<T, int32_t>::value) { return true; }
    if (std::is_same<T, int64_t>::value) { return true; }
    if (std::is_same<T, uint32_t>::value) { return true; }
    if (std::is_same<T, uint64_t>::value) { return true; }
    return false;
}

struct FastInterpArithmeticExprImpl
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
            !std::is_same<LhsIndexType, int32_t>::value) { return false; }
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
            !std::is_same<RhsIndexType, int32_t>::value) { return false; }
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
    static void f(OperandType* out) noexcept
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
            lhs = 0;
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
            rhs = 0;
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

        if constexpr(arithType == AstArithmeticExprType::ADD) {
            *out = lhs + rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::SUB) {
            *out = lhs - rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MUL) {
            *out = lhs * rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::DIV) {
            *out = lhs / rhs;
        }
        else if constexpr(arithType == AstArithmeticExprType::MOD) {
            *out = lhs % rhs;
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
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("lhsLiteralCategory"),
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("rhsLiteralCategory"),
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType")
        );
    }
};

struct FastInterpVariableImpl
{
    template<typename VarTypePtr>
    static constexpr bool cond()
    {
        if (!std::is_pointer<VarTypePtr>::value) { return false; }
        return true;
    }

    template<typename VarTypePtr>
    static void f(VarTypePtr* out) noexcept
    {
        // Must not use uint64_t, since it may be zero
        //
        DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
        uint32_t offset = CONSTANT_PLACEHOLDER_0;
        *out = reinterpret_cast<VarTypePtr>(__pochivm_thread_fastinterp_context.m_stackFrame + offset);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("varTypePtr")
        );
    }
};

#if 0
struct FastInterpComparisonExprImpl
{
    template<typename OperandType, AstComparisonExprType comparatorType, LiteralCategory lhsLiteralCategory, LiteralCategory rhsLiteralCategory>
    static constexpr bool cond()
    {
        if constexpr(std::is_same<OperandType, void>::value)
        {
            return false;
        }
        else
        {
            if (std::is_pointer<OperandType>::value) { return false; }
            if (sizeof(OperandType) == 8 &&
                lhsLiteralCategory == LiteralCategory::LITERAL_NONZERO &&
                rhsLiteralCategory == LiteralCategory::LITERAL_NONZERO) { return false; }
            return true;
        }
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    template<typename OperandType, AstComparisonExprType comparatorType, LiteralCategory lhsLiteralCategory, LiteralCategory rhsLiteralCategory>
    static void f(bool* out) noexcept
    {
        OperandType lhs;
        if constexpr(lhsLiteralCategory == LiteralCategory::NOT_LITERAL)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(OperandType*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(&lhs /*out*/);
        }
        else if constexpr(lhsLiteralCategory == LiteralCategory::LITERAL_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(OperandType);
            lhs = CONSTANT_PLACEHOLDER_0;
        }
        else
        {
            static_assert(lhsLiteralCategory == LiteralCategory::ZERO, "unexpected literal category");
            lhs = 0;
        }

        OperandType rhs;
        if constexpr(rhsLiteralCategory == LiteralCategory::NOT_LITERAL)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(OperandType*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_1(&rhs /*out*/);
        }
        else if constexpr(rhsLiteralCategory == LiteralCategory::LITERAL_NONZERO)
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(OperandType);
            rhs = CONSTANT_PLACEHOLDER_1;
        }
        else
        {
            static_assert(rhsLiteralCategory == LiteralCategory::ZERO, "unexpected literal category");
            rhs = 0;
        }

        if constexpr(comparatorType == AstComparisonExprType::EQUAL) {
            *out = (lhs == rhs);
        }
        else if constexpr(comparatorType == AstComparisonExprType::NOT_EQUAL) {
            *out = (lhs != rhs);
        }
        else if constexpr(comparatorType == AstComparisonExprType::LESS_THAN) {
            *out = (lhs < rhs);
        }
        else if constexpr(comparatorType == AstComparisonExprType::LESS_EQUAL) {
            *out = (lhs <= rhs);
        }
        else if constexpr(comparatorType == AstComparisonExprType::GREATER_THAN) {
            *out = (lhs > rhs);
        }
        else if constexpr(comparatorType == AstComparisonExprType::GREATER_EQUAL) {
            *out = (lhs >= rhs);
        }
        else {
            static_assert(type_dependent_false<OperandType>::value, "Unexpected AstComparisonExprType");
        }
    }
#pragma clang diagnostic pop

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("operatorType"),
                    CreateEnumMetaVar<LiteralCategory::X_END_OF_ENUM>("lhsLiteralCategory"),
                    CreateEnumMetaVar<LiteralCategory::X_END_OF_ENUM>("rhsLiteralCategory")
        );
    }
};

#endif

template<typename T>
static void RegisterBoilerplate()
{
    MetaVarMaterializedList list = T::metavars().template Materialize<T>();
    __pochivm_register_fast_interp_boilerplate__(__pochivm_stringify_type__<T>(), &list);
}

static void RegisterFastInterpImplBoilerplates()
{
    RegisterBoilerplate<FastInterpArithmeticExprImpl>();
    RegisterBoilerplate<FastInterpVariableImpl>();
    // RegisterBoilerplate<FastInterpComparisonExprImpl>();
}

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    PochiVM::RegisterFastInterpImplBoilerplates();
}
