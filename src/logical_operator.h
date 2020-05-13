#pragma once

#include "ast_expr_base.h"

namespace Ast
{

// Logical && and || operation, with short circuiting behavior as in C
//
class AstLogicalAndOrExpr : public AstNodeBase
{
public:
    AstLogicalAndOrExpr(bool isAnd, AstNodeBase* lhs, AstNodeBase* rhs)
        : AstNodeBase(TypeId::Get<bool>())
        , m_isAnd(isAnd), m_lhs(lhs), m_rhs(rhs)
    {
        TestAssert(m_lhs->GetTypeId().IsBool());
        TestAssert(m_rhs->GetTypeId().IsBool());
    }

    void InterpImplAnd(bool* out)
    {
        m_lhs->Interp(out);
        // Short circuiting
        //
        if (!*out) { return; }
        m_rhs->Interp(out);
    }

    void InterpImplOr(bool* out)
    {
        m_lhs->Interp(out);
        // Short circuiting
        //
        if (*out) { return; }
        m_rhs->Interp(out);
    }

    virtual void SetupInterpImpl() override
    {
        if (m_isAnd)
        {
            m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstLogicalAndOrExpr::InterpImplAnd);
        }
        else
        {
            m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstLogicalAndOrExpr::InterpImplOr);
        }
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        fn(m_lhs);
        fn(m_rhs);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstLogicalAndOrExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

private:
    // whether this operator is '&&' or '||'
    //
    bool m_isAnd;
    AstNodeBase* m_lhs;
    AstNodeBase* m_rhs;
};

// Logical not operation
//
class AstLogicalNotExpr : public AstNodeBase
{
public:
    AstLogicalNotExpr(AstNodeBase* op)
        : AstNodeBase(TypeId::Get<bool>())
        , m_op(op)
    {
        TestAssert(m_op->GetTypeId().IsBool());
    }

    void InterpImpl(bool* out)
    {
        m_op->Interp(out);
        *out = !*out;
    }

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstLogicalNotExpr::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        fn(m_op);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstLogicalNotExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

private:
    AstNodeBase* m_op;
};

}   // namespace Ast
