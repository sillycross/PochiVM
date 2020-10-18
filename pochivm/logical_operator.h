#pragma once

#include "ast_expr_base.h"

namespace PochiVM
{

enum class AstFiLogicalOpPrediction
{
    PREDICT_TRUE,
    PREDICT_FALSE,
    NO_PREDICTION
};

// Logical && and || operation, with short circuiting behavior as in C
//
class AstLogicalAndOrExpr : public AstNodeBase
{
public:
    AstLogicalAndOrExpr(bool isAnd, AstNodeBase* lhs, AstNodeBase* rhs)
        : AstNodeBase(AstNodeType::AstLogicalAndOrExpr, TypeId::Get<bool>())
        , m_isAnd(isAnd), m_fiPrediction(AstFiLogicalOpPrediction::NO_PREDICTION), m_lhs(lhs), m_rhs(rhs)
    {
        TestAssert(m_lhs->GetTypeId().IsBool());
        TestAssert(m_rhs->GetTypeId().IsBool());
    }

    void InterpImplAnd(bool* out)
    {
        m_lhs->DebugInterp(out);
        // Short circuiting
        //
        if (!*out) { return; }
        m_rhs->DebugInterp(out);
    }

    void InterpImplOr(bool* out)
    {
        m_lhs->DebugInterp(out);
        // Short circuiting
        //
        if (*out) { return; }
        m_rhs->DebugInterp(out);
    }

    virtual void SetupDebugInterpImpl() override
    {
        if (m_isAnd)
        {
            m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstLogicalAndOrExpr::InterpImplAnd);
        }
        else
        {
            m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstLogicalAndOrExpr::InterpImplOr);
        }
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_lhs);
        fn(m_rhs);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    // virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override;

    // whether this operator is '&&' or '||'
    //
    bool m_isAnd;
    AstFiLogicalOpPrediction m_fiPrediction;
    AstNodeBase* m_lhs;
    AstNodeBase* m_rhs;
};

// Logical not operation
//
class AstLogicalNotExpr : public AstNodeBase
{
public:
    AstLogicalNotExpr(AstNodeBase* op)
        : AstNodeBase(AstNodeType::AstLogicalNotExpr, TypeId::Get<bool>())
        , m_fiPrediction(AstFiLogicalOpPrediction::NO_PREDICTION)
        , m_op(op)
    {
        TestAssert(m_op->GetTypeId().IsBool());
    }

    void InterpImpl(bool* out)
    {
        m_op->DebugInterp(out);
        *out = !*out;
    }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstLogicalNotExpr::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_op);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    AstFiLogicalOpPrediction m_fiPrediction;
    AstNodeBase* m_op;
};

}   // namespace PochiVM
