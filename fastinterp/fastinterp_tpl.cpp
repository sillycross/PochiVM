#include "metavar.h"
#include "pochivm/ast_enums.h"

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
void __pochivm_register_fast_interp_boilerplate__(PochiVM::AstNodeType, PochiVM::MetaVarMaterializedList*);

// DEFINE_PLACEHOLDER_N(type):
//   Define a 'placeholder' value of specified type, which will be "dynamically specialized" at runtime.
//   After declaration, one can use to the placeholder by PLACEHOLDER_N
//   'type' must be either uint64_t, or a pointer or function pointer type.
//
// Example:
//   DEFINE_PLACEHOLDER_1(uint64_t);
//   *out = PLACEHOLDER_1;      // suppose 'out' is a uint64_t*
//

// These are just dummy declarations, the prototype does not matter, and no implementations need to exist
// (the implementation must not exist actually, to make sure all accesses to them become relocation records)
// Make more of them if necessary. 'extern "C"' is to allow easy distinguish of those symbols in object file.
//
extern "C" void __pochivm_fast_interp_dynamic_specialization_placeholder_1();
extern "C" void __pochivm_fast_interp_dynamic_specialization_placeholder_2();
extern "C" void __pochivm_fast_interp_dynamic_specialization_placeholder_3();
extern "C" void __pochivm_fast_interp_dynamic_specialization_placeholder_4();
extern "C" void __pochivm_fast_interp_dynamic_specialization_placeholder_5();
extern "C" void __pochivm_fast_interp_dynamic_specialization_placeholder_6();

#define DEFINE_PLACEHOLDER_INTERNAL(ordinal, ...)                                 \
    using _PLACEHOLDER_TYPE_ ## ordinal = __VA_ARGS__;                            \
    const _PLACEHOLDER_TYPE_ ## ordinal PLACEHOLDER_ ## ordinal =                 \
        reinterpret_cast<_PLACEHOLDER_TYPE_ ## ordinal>(                          \
            __pochivm_fast_interp_dynamic_specialization_placeholder_ ## ordinal)

#define DEFINE_PLACEHOLDER_1(...) DEFINE_PLACEHOLDER_INTERNAL(1, __VA_ARGS__)
#define DEFINE_PLACEHOLDER_2(...) DEFINE_PLACEHOLDER_INTERNAL(2, __VA_ARGS__)
#define DEFINE_PLACEHOLDER_3(...) DEFINE_PLACEHOLDER_INTERNAL(3, __VA_ARGS__)
#define DEFINE_PLACEHOLDER_4(...) DEFINE_PLACEHOLDER_INTERNAL(4, __VA_ARGS__)
#define DEFINE_PLACEHOLDER_5(...) DEFINE_PLACEHOLDER_INTERNAL(5, __VA_ARGS__)
#define DEFINE_PLACEHOLDER_6(...) DEFINE_PLACEHOLDER_INTERNAL(6, __VA_ARGS__)

namespace PochiVM
{

struct AstArithmeticExprImpl
{
    template<typename OperandType, AstArithmeticExprType arithType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value) { return false; }
        if (std::is_floating_point<OperandType>::value && arithType == AstArithmeticExprType::MOD) { return false; }
        return true;
    }

    template<typename OperandType, AstArithmeticExprType arithType>
    static void f(OperandType* out) noexcept
    {
        DEFINE_PLACEHOLDER_1(void(*)(OperandType*) noexcept);    // lhs
        DEFINE_PLACEHOLDER_2(void(*)(OperandType*) noexcept);    // rhs
        OperandType lhs;
        PLACEHOLDER_1(&lhs /*out*/);
        OperandType rhs;
        PLACEHOLDER_2(&rhs /*out*/);
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

    static void Register()
    {
        MetaVarMaterializedList list = CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType")
        ).Materialize<AstArithmeticExprImpl>();
        __pochivm_register_fast_interp_boilerplate__(AstNodeType::AstArithmeticExpr, &list);
    }
};

static void RegisterFastInterpImplBoilerplates()
{
    AstArithmeticExprImpl::Register();
}

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    PochiVM::RegisterFastInterpImplBoilerplates();
}
