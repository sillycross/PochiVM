#pragma once

#include "lang_constructs.h"
#include "api_base.h"

namespace PochiVM
{

// Usage: Scope(stmt1, stmt2, ...)
//
class Scope
{
public:
    template<typename... T>
    Scope(T... args)
        : m_ptr(new AstScope())
    {
        InitScope(args...);
    }

    Scope(AstScope* ptr)
        : m_ptr(ptr)
    { }

    AstScope* GetPtr() const { return m_ptr; }

    operator Value<void>() const
    {
        return Value<void>(m_ptr);
    }

    void Append(const Value<void>& stmt)
    {
        m_ptr->Append(stmt.__pochivm_value_ptr);
    }

private:
    void InitScope()
    { }

    template<typename... T>
    void InitScope(Value<void> stmt, T... args)
    {
        m_ptr->Append(stmt.__pochivm_value_ptr);
        InitScope(args...);
    }

    AstScope* m_ptr;
};

// Usage: Block(stmt1, stmt2, ...)
//
class Block
{
public:
    template<typename... T>
    Block(T... args)
        : m_ptr(new AstBlock())
    {
        InitBlock(args...);
    }

    AstBlock* GetPtr() const { return m_ptr; }

    operator Value<void>() const
    {
        return Value<void>(m_ptr);
    }

    void Append(const Value<void>& stmt)
    {
        m_ptr->Append(stmt.__pochivm_value_ptr);
    }

private:
    void InitBlock()
    { }

    template<typename... T>
    void InitBlock(Value<void> stmt, T... args)
    {
        m_ptr->Append(stmt.__pochivm_value_ptr);
        InitBlock(args...);
    }

    AstBlock* m_ptr;
};

namespace internal
{

struct SmartWrapWithScopeHelper
{
    static AstScope* handle_one(Value<void> stmt)
    {
        AstNodeBase* p = stmt.__pochivm_value_ptr;
        if (p->GetAstNodeType() == AstNodeType::AstScope)
        {
            // If there is only one statement and it is already a scope,
            // just return it. This makes printed code look better.
            //
            return assert_cast<AstScope*>(p);
        }
        else
        {
            // Otherwise, wrap it with a scope
            //
            AstScope* r = new AstScope();
            r->Append(p);
            return r;
        }
    }

    static void insert(AstScope* /*scope*/)
    { }

    template<typename... T>
    static void insert(AstScope* scope, Value<void> stmt, T... args)
    {
        scope->Append(stmt.__pochivm_value_ptr);
        insert(scope, args...);
    }
};

template<typename... T>
AstScope* SmartWrapWithScope(T... args)
{
    AstScope* scope;
    if constexpr(sizeof...(T) == 1)
    {
        scope = SmartWrapWithScopeHelper::handle_one(args...);
    }
    else
    {
        scope = new AstScope();
        SmartWrapWithScopeHelper::insert(scope, args...);
    }
    return scope;
}

}   // namespace internal

// Syntax:
//    If(....).Then(....)
//    If(....).Then(....).Else(....)
//
class IfWithoutThen;

class IfStatement
{
public:
    friend class IfWithoutThen;

    template<typename... T>
    IfStatement Else(T... args)
    {
        TestAssert(!m_stmt->HasElseClause());
        AstScope* scope = internal::SmartWrapWithScope(args...);
        m_stmt->SetElseClause(scope);
        return *this;
    }

    operator Value<void>() const
    {
        return Value<void>(m_stmt);
    }

private:
    IfStatement(AstNodeBase* cond, AstScope* thenClause)
        : m_stmt(new AstIfStatement(cond, thenClause))
    { }

    AstIfStatement* const m_stmt;
};

class IfWithoutThen
{
public:
    friend IfWithoutThen If(const Value<bool>& cond);

    template<typename... T>
    IfStatement Then(T... args)
    {
        assert(!m_thenCalled);
#ifndef NDEBUG
        m_thenCalled = true;
#endif
        AstScope* scope = internal::SmartWrapWithScope(args...);
        return IfStatement(m_cond, scope);
    }

private:
    IfWithoutThen(AstNodeBase* cond)
        : m_cond(cond)
#ifndef NDEBUG
        , m_thenCalled(false)
#endif
    { }

    AstNodeBase* const m_cond;
#ifndef NDEBUG
    bool m_thenCalled;
#endif
};

inline IfWithoutThen If(const Value<bool>& cond)
{
    return IfWithoutThen(cond.__pochivm_value_ptr);
}

// Syntax:
//    For(init, cond, step).Do(....)
//
// init and step may be one statement or a block of statements
// loops/break/continue/return is disallowed in init/step block
//
class ForLoopWithoutBody
{
public:
    friend ForLoopWithoutBody For(const Value<void>& start,
                                  const Value<bool>& cond,
                                  const Value<void>& step);

    template<typename... T>
    Value<void> Do(T... args)
    {
        assert(!m_doCalled);
#ifndef NDEBUG
        m_doCalled = true;
#endif
        AstScope* scope = internal::SmartWrapWithScope(args...);
        AstForLoop* forLoop = new AstForLoop(new AstBlock({m_start}),
                                             m_cond,
                                             new AstBlock({m_step}),
                                             scope);
        return Value<void>(forLoop);
    }

private:
    ForLoopWithoutBody(AstNodeBase* start, AstNodeBase* cond, AstNodeBase* step)
        : m_start(start), m_cond(cond), m_step(step)
#ifndef NDEBUG
        , m_doCalled(false)
#endif
    { }

    AstNodeBase* const m_start;
    AstNodeBase* const m_cond;
    AstNodeBase* const m_step;
#ifndef NDEBUG
    bool m_doCalled;
#endif
};

inline ForLoopWithoutBody For(const Value<void>& start,
                              const Value<bool>& cond,
                              const Value<void>& step)
{
    return ForLoopWithoutBody(start.__pochivm_value_ptr, cond.__pochivm_value_ptr, step.__pochivm_value_ptr);
}

// Syntax:
//    While(cond).Do(....)
//
class WhileLoopWithoutBody
{
public:
    friend WhileLoopWithoutBody While(const Value<bool>& cond);

    template<typename... T>
    Value<void> Do(T... args)
    {
        assert(!m_doCalled);
#ifndef NDEBUG
        m_doCalled = true;
#endif
        AstScope* scope = internal::SmartWrapWithScope(args...);
        AstWhileLoop* whileLoop = new AstWhileLoop(m_cond, scope);
        return Value<void>(whileLoop);
    }

private:
    WhileLoopWithoutBody(AstNodeBase* cond)
        : m_cond(cond)
#ifndef NDEBUG
        , m_doCalled(false)
#endif
    { }

    AstNodeBase* const m_cond;
#ifndef NDEBUG
    bool m_doCalled;
#endif
};

inline WhileLoopWithoutBody While(const Value<bool>& cond)
{
    return WhileLoopWithoutBody(cond.__pochivm_value_ptr);
}

inline Value<void> Break()
{
    return Value<void>(new AstBreakOrContinueStmt(true /*isBreak*/));
}

inline Value<void> Continue()
{
    return Value<void>(new AstBreakOrContinueStmt(false /*isBreak*/));
}

}   // namespace PochiVM
