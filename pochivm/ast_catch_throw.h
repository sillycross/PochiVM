#pragma once

#include "ast_expr_base.h"
#include "exception_helper.h"

namespace PochiVM
{

// Throw an exception
//
class AstThrowStmt : public AstNodeBase
{
public:
    AstThrowStmt(AstNodeBase* operand)
        : AstNodeBase(TypeId::Get<void>())
        , m_operand(operand)
    {
        TestAssert(m_operand->GetTypeId() != TypeId::Get<void>());
        TestAssert(IsTypeRegisteredForThrownFromGeneratedCode(m_operand->GetTypeId()));
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_operand);
    }

    virtual AstNodeType GetAstNodeType() const override
    {
        return AstNodeType::AstThrowStmt;
    }

    virtual void SetupInterpImpl() override
    {

    }

    AstNodeBase* m_operand;
};

}   // namespace PochiVM
