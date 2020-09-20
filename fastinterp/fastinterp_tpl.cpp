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

struct FastInterpArithmeticExprImpl
{
    template<typename OperandType, AstArithmeticExprType arithType, LiteralCategory lhsLiteralCategory, LiteralCategory rhsLiteralCategory>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value) { return false; }
        if (std::is_floating_point<OperandType>::value && arithType == AstArithmeticExprType::MOD) { return false; }
        return true;
    }

    template<typename OperandType, AstArithmeticExprType arithType, LiteralCategory lhsLiteralCategory, LiteralCategory rhsLiteralCategory>
    static void f(OperandType* out) noexcept
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
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType"),
                    CreateEnumMetaVar<LiteralCategory::X_END_OF_ENUM>("lhsLiteralCategory"),
                    CreateEnumMetaVar<LiteralCategory::X_END_OF_ENUM>("rhsLiteralCategory")
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
}

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    PochiVM::RegisterFastInterpImplBoilerplates();
}
