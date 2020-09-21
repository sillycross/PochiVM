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

}   // namespace PochiVM
