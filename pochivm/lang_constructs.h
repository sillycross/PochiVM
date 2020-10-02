#pragma once

#include "ast_expr_base.h"
#include "common_expr.h"
#include "pochivm_context.h"
#include "ast_variable.h"
#include "destructor_helper.h"

namespace PochiVM
{

// Functionality is identical to AstDereferenceExpr, except that
// (1) This works only directly on an AstVariable
// (2) This node may be reused in AST tree
// In metaprogramming frontend, Variable class inherit Value class,
// and whenever we create a Variable, we create a AstDereferenceVariableExpr node
// of this variable as its Value base class.
//
class AstDereferenceVariableExpr : public AstNodeBase
{
public:
    AstDereferenceVariableExpr(AstVariable* operand)
        : AstNodeBase(AstNodeType::AstDereferenceVariableExpr, operand->GetTypeId().RemovePointer())
        , m_operand(operand)
    {
        TestAssert(m_operand->GetTypeId().IsPointerType());
        // cannot dereference a cpp_class* or void*, those do not make sense
        //
        TestAssert(!GetTypeId().IsCppClassType() && !GetTypeId().IsVoid());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    template<typename T>
    void InterpImpl(T* out)
    {
        T* src;
        m_operand->DebugInterp(&src);
        *out = *src;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl,
                              AstDereferenceVariableExpr,
                              InterpImpl,
                              AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_operand);
    }

private:
    AstVariable* m_operand;
};

// A syntax sugar holding a list of statements.
// It has no effect on program behavior.
//
class AstBlock : public AstNodeBase
{
public:
    AstBlock()
        : AstNodeBase(AstNodeType::AstBlock, TypeId::Get<void>())
        , m_contents()
    { }

    AstBlock(const std::vector<AstNodeBase*>& contents)
        : AstNodeBase(AstNodeType::AstBlock, TypeId::Get<void>())
        , m_contents(contents)
    {
#ifdef TESTBUILD
        for (AstNodeBase* stmt : m_contents)
        {
            TestAssert(stmt->GetTypeId().IsVoid());
        }
#endif
    }

    void InterpImpl(InterpControlSignal* ics)
    {
        assert(ics != nullptr && *ics == InterpControlSignal::None);
        for (AstNodeBase* stmt : m_contents)
        {
            stmt->DebugInterp(ics);
            if (unlikely(*ics != InterpControlSignal::None))
            {
                break;
            }
        }
    }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstBlock::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        for (AstNodeBase* stmt : m_contents)
        {
            fn(stmt);
        }
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void Append(AstNodeBase* stmt)
    {
        TestAssert(stmt->GetTypeId().IsVoid());
        m_contents.push_back(stmt);
    }

private:
    std::vector<AstNodeBase*> m_contents;
};

// The concept of a scope, holding a list of statements.
// Variables declared in the scope are destructed when leaving the scope.
//
class AstScope : public AstNodeBase
{
public:
    AstScope()
        : AstNodeBase(AstNodeType::AstScope, TypeId::Get<void>())
        , m_contents()
    { }

    AstScope(const std::vector<AstNodeBase*>& contents)
        : AstNodeBase(AstNodeType::AstScope, TypeId::Get<void>())
        , m_contents(contents)
    {
#ifdef TESTBUILD
        for (AstNodeBase* stmt : m_contents)
        {
            TestAssert(stmt->GetTypeId().IsVoid());
        }
#endif
    }

    bool IsEmpty() const
    {
        return m_contents.size() == 0;
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void InterpImpl(InterpControlSignal* ics)
    {
        AutoInterpExecutionScope aies;
        assert(ics != nullptr && *ics == InterpControlSignal::None);
        for (AstNodeBase* stmt : m_contents)
        {
            stmt->DebugInterp(ics);
            if (unlikely(*ics != InterpControlSignal::None))
            {
                break;
            }
        }
    }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstScope::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        for (AstNodeBase* stmt : m_contents)
        {
            fn(stmt);
        }
    }

    void Append(AstNodeBase* stmt)
    {
        TestAssert(stmt->GetTypeId().IsVoid());
        m_contents.push_back(stmt);
    }

private:
    std::vector<AstNodeBase*> m_contents;
};

// if-then-else construct
//
class AstIfStatement : public AstNodeBase
{
public:
    AstIfStatement(AstNodeBase* condClause, AstScope* thenClause)
        : AstNodeBase(AstNodeType::AstIfStatement, TypeId::Get<void>())
        , m_condClause(condClause)
        , m_thenClause(thenClause)
        , m_elseClause(nullptr)
    {
        TestAssert(m_condClause->GetTypeId().IsBool());
    }

    bool HasElseClause() const { return m_elseClause != nullptr; }

    void SetElseClause(AstScope* elseClause)
    {
        TestAssert(m_elseClause == nullptr && elseClause != nullptr);
        m_elseClause = elseClause;
    }

    AstScope* GetThenClause() const { return m_thenClause; }
    AstScope* GetElseClause() const { return m_elseClause; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void InterpImpl(InterpControlSignal* ics)
    {
        assert(ics != nullptr && *ics == InterpControlSignal::None);
        bool cond;
        m_condClause->DebugInterp(&cond);
        if (cond)
        {
            m_thenClause->DebugInterp(ics);
        }
        else if (m_elseClause != nullptr)
        {
            m_elseClause->DebugInterp(ics);
        }
    }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstIfStatement::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        // The order is important: reachability analysis relies on this order
        //
        fn(m_condClause);
        fn(m_thenClause);
        if (m_elseClause != nullptr) { fn(m_elseClause); }
    }

private:
    AstNodeBase* m_condClause;
    AstScope* m_thenClause;
    AstScope* m_elseClause;
};

// while-loop construct
//
class AstWhileLoop : public AstNodeBase
{
public:
    AstWhileLoop(AstNodeBase* condClause, AstScope* body)
        : AstNodeBase(AstNodeType::AstWhileLoop, TypeId::Get<void>())
        , m_condClause(condClause)
        , m_body(body)
    {
        TestAssert(m_condClause->GetTypeId().IsBool());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void InterpImpl(InterpControlSignal* ics)
    {
        assert(ics != nullptr && *ics == InterpControlSignal::None);
        while (true)
        {
            bool cond;
            m_condClause->DebugInterp(&cond);
            if (!cond) { break; }

            m_body->DebugInterp(ics);
            if (unlikely(*ics != InterpControlSignal::None))
            {
                if (*ics == InterpControlSignal::Break)
                {
                    // A 'break' statement is executed in body.
                    // We should reset the signal back to 'None' (since it only takes effect till here)
                    // and break out.
                    //
                    *ics = InterpControlSignal::None;
                    break;
                }
                else if (*ics == InterpControlSignal::Continue)
                {
                    // A 'continue' statement is executed in body.
                    // We should reset the signal back to 'None' and continue.
                    //
                    *ics = InterpControlSignal::None;
                }
                else
                {
                    // A 'return' statement is executed in body.
                    // We should break out and continue propogating this signal to parent
                    //
                    TestAssert(*ics == InterpControlSignal::Return);
                    break;
                }
            }
        }
    }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstWhileLoop::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_condClause);
        fn(m_body);
    }

private:
    AstNodeBase* m_condClause;
    AstScope* m_body;
};

// For-loop construct
//
class AstForLoop : public AstNodeBase
{
public:
    AstForLoop(AstBlock* startClause,
               AstNodeBase* condClause,
               AstBlock* stepClause,
               AstScope* body)
        : AstNodeBase(AstNodeType::AstForLoop, TypeId::Get<void>())
        , m_startClause(startClause)
        , m_condClause(condClause)
        , m_stepClause(stepClause)
        , m_body(body)
    {
        TestAssert(m_condClause->GetTypeId().IsBool());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void InterpImpl(InterpControlSignal* ics)
    {
        assert(ics != nullptr && *ics == InterpControlSignal::None);

        // The for-loop itself is implicitly a variable scope (in addition to the loop body which is also
        // a variable scope), since variables declared in init-block is destructed when the for-loop ends,
        // so we need to declare a AutoInterpExecutionScope here.
        //
        AutoInterpExecutionScope aies;

        m_startClause->DebugInterp(ics);
        // Break/Continue/Return statements not allowed in for-loop init block
        //
        TestAssert(*ics == InterpControlSignal::None);

        while (true)
        {
            bool cond;
            m_condClause->DebugInterp(&cond);
            if (!cond) { break; }

            m_body->DebugInterp(ics);
            // Logic similar to while-loop, see while-loop for comments
            //
            if (unlikely(*ics != InterpControlSignal::None))
            {
                if (*ics == InterpControlSignal::Break)
                {
                    *ics = InterpControlSignal::None;
                    break;
                }
                else if (*ics == InterpControlSignal::Continue)
                {
                    // When a 'continue' is executed in for-loop, only the rest of loop
                    // body is skipped. The step block should still be executed
                    //
                    *ics = InterpControlSignal::None;
                }
                else
                {
                    TestAssert(*ics == InterpControlSignal::Return);
                    break;
                }
            }

            m_stepClause->DebugInterp(ics);
            // Break/Continue/Return statements not allowed in for-loop step block
            //
            TestAssert(*ics == InterpControlSignal::None);
        }
    }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstForLoop::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        // The order is important: reachability analysis relies on this order
        //
        fn(m_startClause);
        fn(m_condClause);
        fn(m_body);
        fn(m_stepClause);
    }

    AstBlock* GetInitBlock() const { return m_startClause; }
    AstScope* GetBody() const { return m_body; }
    AstBlock* GetStepBlock() const { return m_stepClause; }

private:
    AstBlock* m_startClause;
    AstNodeBase* m_condClause;
    AstBlock* m_stepClause;
    AstScope* m_body;
};

// break/continue statement
//
class AstBreakOrContinueStmt : public AstNodeBase
{
public:
    AstBreakOrContinueStmt(bool isBreak)
        : AstNodeBase(AstNodeType::AstBreakOrContinueStmt, TypeId::Get<void>())
        , m_isBreak(isBreak)
    { }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void InterpImpl(InterpControlSignal* ics)
    {
        assert(ics != nullptr && *ics == InterpControlSignal::None);
        if (m_isBreak)
        {
            *ics = InterpControlSignal::Break;
        }
        else
        {
            *ics = InterpControlSignal::Continue;
        }
    }

    bool IsBreakStatement() const { return m_isBreak; }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstBreakOrContinueStmt::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> /*fn*/) override { }

private:
    // whether it is a break statement or a continue statement
    //
    bool m_isBreak;
};

}   // namespace PochiVM
