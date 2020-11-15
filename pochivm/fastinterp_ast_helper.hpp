#pragma once

#include "fastinterp/fastinterp.hpp"
#include "ast_expr_base.h"
#include "ast_variable.h"
#include "common_expr.h"
#include "lang_constructs.h"
#include "cast_expr.h"
#include "arith_expr.h"

namespace PochiVM
{

inline bool IsTypeIdConstantValidForSmallCodeModel(TypeId typeId)
{
    TestAssert(!typeId.IsCppClassType() && !typeId.IsVoid());
    if (typeId.Size() > 4)
    {
        return false;
    }
    if (typeId == TypeId::Get<uint32_t>())
    {
        return false;
    }
    return true;
}

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
                if (IsTypeIdConstantValidForSmallCodeModel(lit->GetTypeId()))
                {
                    ret.m_kind = FISimpleOperandShapeCategory::LITERAL_NONZERO;
                    ret.m_literal = lit;
                }
                else
                {
                    ret.m_kind = FISimpleOperandShapeCategory::X_END_OF_ENUM;
                }
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

    static AstFIOperandShape WARN_UNUSED TryMatchAddress(AstNodeBase* /*expr*/)
    {
        AstFIOperandShape ret;
        ret.m_kind = FIOperandShapeCategory::X_END_OF_ENUM;
        return ret;
    }

    static AstFIOperandShape WARN_UNUSED TryMatch(AstNodeBase* /*expr*/)
    {
        AstFIOperandShape ret;
        ret.m_kind = FIOperandShapeCategory::X_END_OF_ENUM;
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
        else if (m_kind == FIOperandShapeCategory::VARPTR_LIT_DIRECT_OFFSET)
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

    static AstFIOperandShape WARN_UNUSED InternalTryMatchIndexShape(AstNodeBase* /*root*/)
    {
        AstFIOperandShape ret;
        ret.m_kind = FIOperandShapeCategory::X_END_OF_ENUM;
        return ret;
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
