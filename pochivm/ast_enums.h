#pragma once

#include "common.h"

// This file is used by both pochivm and fastinterp
//

namespace PochiVM
{

class AstNodeType
{
public:
    enum _EnumType
    {
        AstArithmeticExpr,
        AstComparisonExpr,
        AstStaticCastExpr,
        AstReinterpretCastExpr,
        AstDereferenceExpr,
        AstLiteralExpr,
        AstAssignExpr,
        AstNullptrExpr,
        AstTrashPtrExpr,
        AstVariable,
        AstDeclareVariable,
        AstDereferenceVariableExpr,
        AstBlock,
        AstScope,
        AstIfStatement,
        AstWhileLoop,
        AstForLoop,
        AstBreakOrContinueStmt,
        AstCallExpr,
        AstReturnStmt,
        AstLogicalAndOrExpr,
        AstLogicalNotExpr,
        AstThrowStmt
    };

    AstNodeType() {}
    AstNodeType(AstNodeType::_EnumType e) : m_value(e) {}
    explicit operator int() const { return m_value; }
    bool operator==(const AstNodeType::_EnumType& other) const { return m_value == other; }
    bool operator!=(const AstNodeType::_EnumType& other) const { return !(*this == other); }
    bool operator==(const AstNodeType& other) const { return m_value == other.m_value; }
    bool operator!=(const AstNodeType& other) const { return !(*this == other); }

    const char* ToString() const
    {
        switch(m_value)
        {
        case AstNodeType::AstArithmeticExpr: return "AstArithmeticExpr";
        case AstNodeType::AstComparisonExpr: return "AstComparisonExpr";
        case AstNodeType::AstStaticCastExpr: return "AstStaticCastExpr";
        case AstNodeType::AstReinterpretCastExpr: return "AstReinterpretCastExpr";
        case AstNodeType::AstDereferenceExpr: return "AstDereferenceExpr";
        case AstNodeType::AstLiteralExpr: return "AstLiteralExpr";
        case AstNodeType::AstAssignExpr: return "AstAssignExpr";
        case AstNodeType::AstNullptrExpr: return "AstNullptrExpr";
        case AstNodeType::AstTrashPtrExpr: return "AstTrashPtrExpr";
        case AstNodeType::AstVariable: return "AstVariable";
        case AstNodeType::AstDeclareVariable: return "AstDeclareVariable";
        case AstNodeType::AstDereferenceVariableExpr: return "AstDereferenceVariableExpr";
        case AstNodeType::AstBlock: return "AstBlock";
        case AstNodeType::AstScope: return "AstScope";
        case AstNodeType::AstIfStatement: return "AstIfStatement";
        case AstNodeType::AstWhileLoop: return "AstWhileLoop";
        case AstNodeType::AstForLoop: return "AstForLoop";
        case AstNodeType::AstBreakOrContinueStmt: return "AstBreakOrContinueStmt";
        case AstNodeType::AstCallExpr: return "AstCallExpr";
        case AstNodeType::AstReturnStmt: return "AstReturnStmt";
        case AstNodeType::AstLogicalAndOrExpr: return "AstLogicalAndOrExpr";
        case AstNodeType::AstLogicalNotExpr: return "AstLogicalNotExpr";
        case AstNodeType::AstThrowStmt: return "AstThrowStmt";
        }
        __builtin_unreachable();
    }

private:
    _EnumType m_value;
};

// The function alignment of the generated fastinterp functions
// This must at least the function alignment used to compile fastinterp_tpl_*.cpp
// Default clang++ function alignment is 16.
//
constexpr size_t x_fastinterp_function_alignment = 16;

// Fast interp aggregation optimization for operands.
// In interp, every operator becomes a function call.
// We want to aggregate simple and common shapes to reduce the number of function calls.
// E.g. we want to execute simple shapes like 'a[i]' in one function call,
// instead of many function calls (get addr 'a', deref 'a', get addr 'i', deref 'i',
// get addr 'a[i]', deref 'a[i]')
//
enum class OperandShapeCategory
{
    // A simple variable. e.g. 'a'
    //
    VARIABLE,
    // Variable dereference, e.g. '*a', 'a[0]'
    //
    VARPTR_DEREF,
    // Variable dereference with non-zero literal offset, e.g. 'a[12]'
    //
    VARPTR_LIT_NONZERO,
    // Variable dereference with variable offset, e.g. 'a[i]'
    //
    VARPTR_VAR,
    // Literal (nonzero). e.g. '12'
    //
    LITERAL_NONZERO,
    // Literal (zero)
    //
    ZERO,
    // A complex shape not belong to any simple shape above
    //
    COMPLEX,
    X_END_OF_ENUM
};

enum class AstArithmeticExprType
{
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    X_END_OF_ENUM
};

enum class AstComparisonExprType
{
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_EQUAL,
    GREATER_THAN,
    GREATER_EQUAL,
    X_END_OF_ENUM
};

const int x_fastinterp_callexpr_num_inline_params = 3;
enum class FICallExprNumParameters
{
    MORE_THAN_THREE = x_fastinterp_callexpr_num_inline_params + 1,
    X_END_OF_ENUM
};

namespace math
{

constexpr int power(int base, int exp)
{
    int result = 1;
    for (int i = 0; i < exp; i++) result *= base;
    return result;
}

}   // namespace math

// Sometimes we don't need to understand the internal representation of a type,
// we just want to forward it to somewhere else.
// In that case, we don't have to enumerate every type (so we can inline more in one function).
// We just need to distinguish types that are different in ABI.
//
enum class FIABIDistinctType
{
    INT_8,
    INT_16,
    INT_32,
    INT_64,
    FLOAT,
    DOUBLE,
    X_END_OF_ENUM
};

template<FIABIDistinctType t> struct TypeForABIDistinctTypeImpl;
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_8> { using type = uint8_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_16> { using type = uint16_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_32> { using type = uint32_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::INT_64> { using type = uint64_t; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::FLOAT> { using type = float; };
template<> struct TypeForABIDistinctTypeImpl<FIABIDistinctType::DOUBLE> { using type = double; };

template<FIABIDistinctType t>
using TypeForABIDistinctType = typename TypeForABIDistinctTypeImpl<t>::type;

template<typename T>
constexpr FIABIDistinctType GetFIABIDistinctType()
{
    if constexpr(std::is_pointer<T>::value)
    {
        static_assert(sizeof(T) == 8, "unexpected pointer size");
        return FIABIDistinctType::INT_64;
    }
    else if constexpr(std::is_integral<T>::value)
    {
        constexpr size_t size = sizeof(T);
        if constexpr(size == 1) {
            return FIABIDistinctType::INT_8;
        }
        else if constexpr(size == 2) {
            return FIABIDistinctType::INT_16;
        }
        else if constexpr(size == 4) {
            return FIABIDistinctType::INT_32;
        }
        else if constexpr(size == 8) {
            return FIABIDistinctType::INT_64;
        }
        else {
            static_assert(type_dependent_false<T>::value, "unexpected size of integral type");
        }
    }
    else if constexpr(std::is_same<T, float>::value)
    {
        return FIABIDistinctType::FLOAT;
    }
    else if constexpr(std::is_same<T, double>::value)
    {
        return FIABIDistinctType::DOUBLE;
    }
    else
    {
        static_assert(type_dependent_false<T>::value, "unexpected type T");
    }
}

enum class FICallExprParamTypeMask
{
    X_END_OF_ENUM = math::power(static_cast<int>(FIABIDistinctType::X_END_OF_ENUM), x_fastinterp_callexpr_num_inline_params)
};

// In fastinterp, additional parameters to an AstCallExpr is passed in by this operator
// This operator itself inlines at most 4 additional parameters,
// and then optionally call the next operator to fill the still remaining ones.
//
const int x_fastinterp_callexpr_extra_num_inline_params = 4;
enum class FICallExprNumExtraParameters
{
    MORE_THAN_FOUR = x_fastinterp_callexpr_extra_num_inline_params + 1,
    X_END_OF_ENUM
};

enum class FICallExprExtraParamTypeMask
{
    X_END_OF_ENUM = math::power(static_cast<int>(FIABIDistinctType::X_END_OF_ENUM), x_fastinterp_callexpr_extra_num_inline_params)
};

class PossibleControlSignals
{
public:
    PossibleControlSignals() : m_value(0) {}
    explicit PossibleControlSignals(uint32_t value)
        : m_value(value)
    {
        TestAssert(m_value < 8);
    }

    bool MayContinue() const { return (m_value & x_MAY_CONTINUE) != 0; }
    bool MayBreak() const { return (m_value & x_MAY_BREAK) != 0; }
    bool MayReturn() const { return (m_value & x_MAY_RETURN) != 0; }

    PossibleControlSignals WARN_UNUSED operator|(const PossibleControlSignals& other) const {
        return PossibleControlSignals { m_value | other.m_value };
    }
    PossibleControlSignals WARN_UNUSED SetMayContinue() const {
        return PossibleControlSignals { m_value | x_MAY_CONTINUE };
    }
    PossibleControlSignals WARN_UNUSED SetMayBreak() const {
        return PossibleControlSignals { m_value | x_MAY_BREAK };
    }
    PossibleControlSignals WARN_UNUSED SetMayReturn() const {
        return PossibleControlSignals { m_value | x_MAY_RETURN };
    }
    PossibleControlSignals WARN_UNUSED ClearMayContinue() const {
        return PossibleControlSignals { m_value & (~x_MAY_CONTINUE) };
    }
    PossibleControlSignals WARN_UNUSED ClearMayBreak() const {
        return PossibleControlSignals { m_value & (~x_MAY_BREAK) };
    }
    PossibleControlSignals WARN_UNUSED ClearMayReturn() const {
        return PossibleControlSignals { m_value & (~x_MAY_RETURN) };
    }

private:
    const static uint32_t x_MAY_CONTINUE = 1;
    const static uint32_t x_MAY_BREAK = 2;
    const static uint32_t x_MAY_RETURN = 4;
    uint32_t m_value;
};

// Interp mode control signal
// Used for control flow redirection statements (Break/Continue/Return)
//
enum class InterpControlSignal
{
    None,
    Break,
    Continue,
    Return
};

const int x_fastinterp_function_num_inline_stmts = 10;
enum class FIFunctionNumStatements
{
    X_END_OF_ENUM = x_fastinterp_function_num_inline_stmts + 1
};

enum class FIFunctionStmtsMayReturnMask
{
    X_END_OF_ENUM = (1 << x_fastinterp_function_num_inline_stmts)
};

const int x_fastinterp_if_stmt_num_inline_stmts = 5;
enum class FIIfStmtNumStatements
{
    X_END_OF_ENUM = x_fastinterp_if_stmt_num_inline_stmts + 1
};

// Whether we need to check for control-flow redirection after every statement.
//
enum class FIIfStmtMayCFRMask
{
    X_END_OF_ENUM = (1 << x_fastinterp_if_stmt_num_inline_stmts)
};

}   // namespace PochiVM
