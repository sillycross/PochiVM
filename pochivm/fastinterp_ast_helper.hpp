#pragma once

#include "fastinterp/fastinterp.hpp"
#include "ast_expr_base.h"
#include "ast_variable.h"
#include "common_expr.h"
#include "lang_constructs.h"

namespace PochiVM
{

struct AstFISimpleOperandShape
{
    bool MatchOK() const
    {
        return m_kind != FISimpleOperandShapeCategory::X_END_OF_ENUM;
    }

    static AstFISimpleOperandShape WARN_UNUSED TryMatch(AstNodeBase* expr)
    {
        AstFISimpleOperandShape ret;
        if (expr->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
        {
            ret.m_kind = FISimpleOperandShapeCategory::VARIABLE;
            ret.m_variable = assert_cast<AstDereferenceVariableExpr*>(expr)->GetOperand();
        }
        else if (expr->GetAstNodeType() == AstNodeType::AstLiteralExpr)
        {
            AstLiteralExpr* lit = assert_cast<AstLiteralExpr*>(expr);
            if (lit->IsAllBitsZero())
            {
                ret.m_kind = FISimpleOperandShapeCategory::ZERO;
            }
            else
            {
                ret.m_kind = FISimpleOperandShapeCategory::LITERAL_NONZERO;
                ret.m_literal = lit;
            }
        }
        else
        {
            ret.m_kind = FISimpleOperandShapeCategory::X_END_OF_ENUM;
        }
        return ret;
    }

    void PopulatePlaceholder(FastInterpBoilerplateInstance* inst, uint32_t ph1)
    {
        if (m_kind == FISimpleOperandShapeCategory::VARIABLE)
        {
            inst->PopulateConstantPlaceholder<uint64_t>(ph1, m_variable->GetFastInterpOffset());
        }
        else if (m_kind == FISimpleOperandShapeCategory::LITERAL_NONZERO)
        {
            inst->PopulateConstantPlaceholder<uint64_t>(ph1, m_literal->GetAsU64());
        }
    }

    FISimpleOperandShapeCategory m_kind;
    AstVariable* m_variable;
    AstLiteralExpr* m_literal;
};

struct AstFIOperandShape
{
    bool MatchOK() const
    {
        return m_kind != FIOperandShapeCategory::X_END_OF_ENUM;
    }

    static AstFIOperandShape WARN_UNUSED TryMatchAddress(AstNodeBase* expr)
    {
        AstFIOperandShape ret;
        if (expr->GetAstNodeType() == AstNodeType::AstVariable)
        {
            ret.m_kind = FIOperandShapeCategory::VARIABLE;
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            ret.m_mainVariable = assert_cast<AstVariable*>(expr);
        }
        else if (expr->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
        {
            ret.m_kind = FIOperandShapeCategory::VARPTR_DEREF;
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            ret.m_mainVariable = assert_cast<AstDereferenceVariableExpr*>(expr)->GetOperand();
        }
        else
        {
            // TODO: handle index type
            //
            ret.m_kind = FIOperandShapeCategory::X_END_OF_ENUM;
        }
        return ret;
    }

    static AstFIOperandShape WARN_UNUSED TryMatch(AstNodeBase* expr)
    {
        AstFIOperandShape ret;
        if (expr->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
        {
            ret.m_kind = FIOperandShapeCategory::VARIABLE;
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            ret.m_mainVariable = assert_cast<AstDereferenceVariableExpr*>(expr)->GetOperand();
        }
        else if (expr->GetAstNodeType() == AstNodeType::AstLiteralExpr)
        {
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            AstLiteralExpr* lit = assert_cast<AstLiteralExpr*>(expr);
            if (lit->IsAllBitsZero())
            {
                ret.m_kind = FIOperandShapeCategory::ZERO;
            }
            else
            {
                ret.m_kind = FIOperandShapeCategory::LITERAL_NONZERO;
                ret.m_mainLiteral = lit;
            }
        }
        else if (expr->GetAstNodeType() == AstNodeType::AstDereferenceExpr &&
                 assert_cast<AstDereferenceExpr*>(expr)->GetOperand()->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
        {
            AstDereferenceVariableExpr* derefExpr = assert_cast<AstDereferenceVariableExpr*>(
                        assert_cast<AstDereferenceExpr*>(expr)->GetOperand());
            ret.m_kind = FIOperandShapeCategory::VARPTR_DEREF;
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            ret.m_mainVariable = derefExpr->GetOperand();
        }
        else
        {
            // TODO: handle index type
            //
            ret.m_kind = FIOperandShapeCategory::X_END_OF_ENUM;
        }
        return ret;
    }

    void PopulatePlaceholder(FastInterpBoilerplateInstance* inst, uint32_t ph1, uint32_t ph2)
    {
        if (m_kind == FIOperandShapeCategory::VARIABLE)
        {
            inst->PopulateConstantPlaceholder<uint64_t>(ph1, m_mainVariable->GetFastInterpOffset());
        }
        else if (m_kind == FIOperandShapeCategory::VARPTR_VAR)
        {
            inst->PopulateConstantPlaceholder<uint64_t>(ph1, m_mainVariable->GetFastInterpOffset());
            inst->PopulateConstantPlaceholder<uint64_t>(ph2, m_indexVariable->GetFastInterpOffset());
        }
        else if (m_kind == FIOperandShapeCategory::VARPTR_DEREF)
        {
            inst->PopulateConstantPlaceholder<uint64_t>(ph1, m_mainVariable->GetFastInterpOffset());
        }
        else if (m_kind == FIOperandShapeCategory::LITERAL_NONZERO)
        {
            inst->PopulateConstantPlaceholder<uint64_t>(ph1, m_mainLiteral->GetAsU64());
        }
        else if (m_kind == FIOperandShapeCategory::VARPTR_LIT_NONZERO)
        {
            inst->PopulateConstantPlaceholder<uint64_t>(ph1, m_mainVariable->GetFastInterpOffset());
            inst->PopulateConstantPlaceholder<uint64_t>(ph2, m_indexLiteral->GetAsU64());
        }
        else if (m_kind == FIOperandShapeCategory::ZERO)
        {
            /* nothing */
        }
        else
        {
            TestAssert(false);
        }
    }

    FIOperandShapeCategory m_kind;
    FastInterpTypeId m_indexType;
    AstVariable* m_mainVariable;
    AstLiteralExpr* m_mainLiteral;
    AstVariable* m_indexVariable;
    AstLiteralExpr* m_indexLiteral;
};

inline FastInterpBoilerplateInstance* WARN_UNUSED FIGetNoopBoilerplate()
{
    return thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FINoopImpl>::SelectBoilerplateBluePrint(
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
}

}   // namespace PochiVM
