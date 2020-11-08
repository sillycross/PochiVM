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

    static AstFIOperandShape WARN_UNUSED TryMatchAddress(AstNodeBase* expr)
    {
        if (expr->GetAstNodeType() == AstNodeType::AstVariable)
        {
            AstFIOperandShape ret;
            ret.m_kind = FIOperandShapeCategory::VARIABLE;
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            ret.m_mainVariable = assert_cast<AstVariable*>(expr);
            return ret;
        }
        else
        {
            return InternalTryMatchIndexShape(expr);
        }
    }

    static AstFIOperandShape WARN_UNUSED TryMatch(AstNodeBase* expr)
    {
        AstFIOperandShape ret;
        if (expr->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
        {
            ret.m_kind = FIOperandShapeCategory::VARIABLE;
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            ret.m_mainVariable = assert_cast<AstDereferenceVariableExpr*>(expr)->GetOperand();
            return ret;
        }
        else if (expr->GetAstNodeType() == AstNodeType::AstLiteralExpr)
        {
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            AstLiteralExpr* lit = assert_cast<AstLiteralExpr*>(expr);
            if (lit->IsAllBitsZero())
            {
                ret.m_kind = FIOperandShapeCategory::ZERO;
            }
            else if (IsTypeIdConstantValidForSmallCodeModel(lit->GetTypeId()))
            {
                ret.m_kind = FIOperandShapeCategory::LITERAL_NONZERO;
                ret.m_mainLiteral = lit;
            }
            else
            {
                ret.m_kind = FIOperandShapeCategory::X_END_OF_ENUM;
            }
            return ret;
        }
        else if (expr->GetAstNodeType() == AstNodeType::AstDereferenceExpr)
        {
            AstNodeBase* derefExprOperand = assert_cast<AstDereferenceExpr*>(expr)->GetOperand();
            return InternalTryMatchIndexShape(derefExprOperand);
        }

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

    static AstFIOperandShape WARN_UNUSED InternalTryMatchIndexShape(AstNodeBase* root)
    {
        AstFIOperandShape ret;
        if (root->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
        {
            AstDereferenceVariableExpr* derefVarExpr = assert_cast<AstDereferenceVariableExpr*>(root);
            ret.m_kind = FIOperandShapeCategory::VARPTR_DEREF;
            ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
            ret.m_mainVariable = derefVarExpr->GetOperand();
            return ret;
        }
        else if (root->GetAstNodeType() == AstNodeType::AstPointerArithmeticExpr)
        {
            AstPointerArithmeticExpr* paExpr = assert_cast<AstPointerArithmeticExpr*>(root);
            TestAssert(!paExpr->GetTypeId().RemovePointer().IsCppClassType());
            if (paExpr->m_isAddition &&
                paExpr->m_base->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr &&
                (paExpr->m_index->GetTypeId() == TypeId::Get<uint32_t>() ||
                 paExpr->m_index->GetTypeId() == TypeId::Get<int32_t>() ||
                 paExpr->m_index->GetTypeId() == TypeId::Get<uint64_t>() ||
                 paExpr->m_index->GetTypeId() == TypeId::Get<int64_t>()))
            {
                if (paExpr->m_index->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
                {
                    ret.m_kind = FIOperandShapeCategory::VARPTR_VAR;
                    ret.m_indexType = paExpr->m_index->GetTypeId().GetDefaultFastInterpTypeId();
                    ret.m_mainVariable = assert_cast<AstDereferenceVariableExpr*>(paExpr->m_base)->GetOperand();
                    ret.m_indexVariable = assert_cast<AstDereferenceVariableExpr*>(paExpr->m_index)->GetOperand();
                    return ret;
                }

                if (paExpr->m_index->GetAstNodeType() == AstNodeType::AstLiteralExpr)
                {
                    AstLiteralExpr* litExpr = assert_cast<AstLiteralExpr*>(paExpr->m_index);
                    if (litExpr->IsAllBitsZero())
                    {
                        ret.m_kind = FIOperandShapeCategory::VARPTR_DEREF;
                        ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
                        ret.m_mainVariable = assert_cast<AstDereferenceVariableExpr*>(paExpr->m_base)->GetOperand();
                        return ret;
                    }
                    else if (IsTypeIdConstantValidForSmallCodeModel(litExpr->GetTypeId()))
                    {
                        ret.m_kind = FIOperandShapeCategory::VARPTR_LIT_NONZERO;
                        ret.m_indexType = litExpr->GetTypeId().GetDefaultFastInterpTypeId();
                        ret.m_mainVariable = assert_cast<AstDereferenceVariableExpr*>(paExpr->m_base)->GetOperand();
                        ret.m_indexLiteral = litExpr;
                        return ret;
                    }
                }
            }
        }
        else if (root->GetAstNodeType() == AstNodeType::AstReinterpretCastExpr)
        {
            AstReinterpretCastExpr* expr = assert_cast<AstReinterpretCastExpr*>(root);
            TestAssert(!expr->GetTypeId().RemovePointer().IsCppClassType());
            if (expr->m_operand->GetAstNodeType() == AstNodeType::AstArithmeticExpr)
            {
                AstArithmeticExpr* arithExpr = assert_cast<AstArithmeticExpr*>(expr->m_operand);
                TestAssert(arithExpr->GetTypeId() == TypeId::Get<uint64_t>() || arithExpr->GetTypeId() == TypeId::Get<int64_t>());
                if (arithExpr->m_op == AstArithmeticExprType::ADD &&
                    arithExpr->m_lhs->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr &&
                    arithExpr->m_rhs->GetAstNodeType() == AstNodeType::AstLiteralExpr)
                {
                    AstDereferenceVariableExpr* derefVar = assert_cast<AstDereferenceVariableExpr*>(arithExpr->m_lhs);
                    AstLiteralExpr* litExpr = assert_cast<AstLiteralExpr*>(arithExpr->m_rhs);
                    uint64_t value = litExpr->GetAsU64();
                    if (value == 0)
                    {
                        ret.m_kind = FIOperandShapeCategory::VARPTR_DEREF;
                        ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
                        ret.m_mainVariable = derefVar->GetOperand();
                        return ret;
                    }
                    else if (value < (1ULL << 31))
                    {
                        ret.m_kind = FIOperandShapeCategory::VARPTR_LIT_DIRECT_OFFSET;
                        ret.m_indexType = TypeId::Get<int32_t>().GetDefaultFastInterpTypeId();
                        ret.m_mainVariable = derefVar->GetOperand();
                        ret.m_indexLiteral = litExpr;
                        return ret;
                    }
                }
            }
        }

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
