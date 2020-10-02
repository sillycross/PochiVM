#pragma once

#include "ast_expr_base.h"

namespace PochiVM
{

// A basic arithmetic expression.
// T op T -> T (T must be primitive type)
//
class AstArithmeticExpr : public AstNodeBase
{
public:
    AstArithmeticExpr(char op, AstNodeBase* lhs, AstNodeBase* rhs)
        : AstNodeBase(lhs->GetTypeId())
        , m_op(op), m_lhs(lhs), m_rhs(rhs)
    {
        TestAssert(m_lhs->GetTypeId() == m_rhs->GetTypeId());
        TestAssert(m_lhs->GetTypeId().IsPrimitiveType());
        TestAssert(!m_lhs->GetTypeId().IsBool());
    }

    // Interp implementations
    // TODO: signed overflow is undefined behavior in C++
    //
    template<typename T>
    void AddImpl(T* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = lhs + rhs;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectAddImpl, AstArithmeticExpr, AddImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void SubImpl(T* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = lhs - rhs;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectSubImpl, AstArithmeticExpr, SubImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void MulImpl(T* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = lhs * rhs;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectMulImpl, AstArithmeticExpr, MulImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void DivImpl(T* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = lhs / rhs;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectDivImpl, AstArithmeticExpr, DivImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void ModImpl(T* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = lhs % rhs;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectModImpl, AstArithmeticExpr, ModImpl, AstTypeHelper::is_primitive_int_type)

    virtual void SetupDebugInterpImpl() override
    {
        if (m_op == '+') {
            m_debugInterpFn = SelectAddImpl(m_lhs->GetTypeId());
        }
        else if (m_op == '-') {
            m_debugInterpFn = SelectSubImpl(m_lhs->GetTypeId());
        }
        else if (m_op == '*') {
            m_debugInterpFn = SelectMulImpl(m_lhs->GetTypeId());
        }
        else if (m_op == '/') {
            m_debugInterpFn = SelectDivImpl(m_lhs->GetTypeId());
        }
        else if (m_op == '%') {
            m_debugInterpFn = SelectModImpl(m_lhs->GetTypeId());
        }
        else {
            TestAssert(false);
        }
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_lhs);
        fn(m_rhs);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstArithmeticExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

private:
    char m_op;
    AstNodeBase* m_lhs;
    AstNodeBase* m_rhs;
};

// A comparison expression.
// T op T -> bool (T must be primitive type)
//
class AstComparisonExpr : public AstNodeBase
{
public:
    AstComparisonExpr(const char* op, AstNodeBase* lhs, AstNodeBase* rhs)
        : AstNodeBase(TypeId::Get<bool>())
        , m_lhs(lhs), m_rhs(rhs)
    {
        assert(op[0] != '\0');
        m_op[0] = op[0];
        m_op[1] = op[1];
        TestAssert(m_lhs->GetTypeId() == m_rhs->GetTypeId());
        TestAssert(m_lhs->GetTypeId().IsPrimitiveType());
    }

    // Interp implementations
    //

    // If user wants to do a float equal for whatever reason, we should let them do it
    //
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    template<typename T>
    void EqImpl(bool* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = (lhs == rhs);
    }
#pragma clang diagnostic pop

    GEN_CLASS_METHOD_SELECTOR(SelectEqImpl, AstComparisonExpr, EqImpl, AstTypeHelper::is_primitive_type)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    template<typename T>
    void NeqImpl(bool* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = (lhs != rhs);
    }
#pragma clang diagnostic pop

    GEN_CLASS_METHOD_SELECTOR(SelectNeqImpl, AstComparisonExpr, NeqImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void GtImpl(bool* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = (lhs > rhs);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectGtImpl, AstComparisonExpr, GtImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void GEqImpl(bool* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = (lhs >= rhs);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectGEqImpl, AstComparisonExpr, GEqImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void LtImpl(bool* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = (lhs < rhs);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectLtImpl, AstComparisonExpr, LtImpl, AstTypeHelper::is_primitive_type)

    template<typename T>
    void LEqImpl(bool* out)
    {
        T lhs, rhs;
        m_lhs->DebugInterp(&lhs);
        m_rhs->DebugInterp(&rhs);
        *out = (lhs <= rhs);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectLEqImpl, AstComparisonExpr, LEqImpl, AstTypeHelper::is_primitive_type)

    virtual void SetupDebugInterpImpl() override
    {
        if (m_op[0] == '=')
        {
            TestAssert(m_op[1] == '=');
            m_debugInterpFn = SelectEqImpl(m_lhs->GetTypeId());
        }
        else if (m_op[0] == '!')
        {
            TestAssert(m_op[1] == '=');
            m_debugInterpFn = SelectNeqImpl(m_lhs->GetTypeId());
        }
        else if (m_op[0] == '<')
        {
            if (m_op[1] == '\0')
            {
                m_debugInterpFn = SelectLtImpl(m_lhs->GetTypeId());
            }
            else
            {
                TestAssert(m_op[1] == '=');
                m_debugInterpFn = SelectLEqImpl(m_lhs->GetTypeId());
            }
        }
        else if (m_op[0] == '>')
        {
            if (m_op[1] == '\0')
            {
                m_debugInterpFn = SelectGtImpl(m_lhs->GetTypeId());
            }
            else
            {
                TestAssert(m_op[1] == '=');
                m_debugInterpFn = SelectGEqImpl(m_lhs->GetTypeId());
            }
        }
        else
        {
            TestAssert(false);
        }
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_lhs);
        fn(m_rhs);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstComparisonExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

private:
    char m_op[2];
    AstNodeBase* m_lhs;
    AstNodeBase* m_rhs;
};

// TODO: logical operations
//

}   // namespace PochiVM
