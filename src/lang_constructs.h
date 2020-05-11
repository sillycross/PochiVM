#pragma once

#include "ast_expr_base.h"
#include "common_expr.h"
#include "pochivm_context.h"

namespace llvm
{

class Twine;
class AllocaInst;

}   // namespace llvm

namespace Ast
{

class AstFunction;
class AstScope;

class AstVariable : public AstNodeBase
{
public:
    AstVariable(TypeId typeId, AstFunction* owner, uint32_t varnameSuffix, const char* name = "var")
        : AstNodeBase(typeId)
        , m_varname(name)
        , m_functionOwner(owner)
        , m_llvmValue(nullptr)
        , m_varnameSuffix(varnameSuffix)
        , m_storageSize(static_cast<uint32_t>(typeId.RemovePointer().Size()))
        , m_interpOffset(static_cast<uint32_t>(-1))
    {
        TestAssert(GetTypeId().IsPointerType());
        TestAssert(GetTypeId().RemovePointer().IsPrimitiveType() ||
                   GetTypeId().RemovePointer().IsPointerType());
    }

    void SetVarName(const char* newName)
    {
        m_varname = newName;
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    template<typename T>
    void InterpImpl(T* out)
    {
        *out = reinterpret_cast<T>(thread_pochiVMContext->m_interpStackFrameBase + m_interpOffset);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstVariable, InterpImpl, std::is_pointer)

    virtual void SetupInterpImpl() override
    {
        m_interpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& /*fn*/) override { }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstVariable; }

    const char* GetVarNameNoSuffix() const { return m_varname; }
    uint32_t GetVarSuffix() const { return m_varnameSuffix; }
    AstFunction* GetFunctionOwner() const { return m_functionOwner; }
    uint32_t GetStorageSize() const { return m_storageSize; }

    void SetInterpOffset(uint32_t offset)
    {
        assert(m_interpOffset == static_cast<uint32_t>(-1) && offset != static_cast<uint32_t>(-1));
        m_interpOffset = offset;
    }

    uint32_t GetInterpOffset() const
    {
        assert(m_interpOffset != static_cast<uint32_t>(-1));
        return m_interpOffset;
    }

private:
    // name of the variable
    //
    const char* m_varname;
    // The function in which this variable is declared
    //
    AstFunction* m_functionOwner;
    // The llvm AllocaInst corresponding to this variable,
    // populated by AstDeclareVariable if it is a local var, or by AstFunction if it is a parameter
    //
    llvm::AllocaInst* m_llvmValue;
    // The variable name suffix for printing
    //
    uint32_t m_varnameSuffix;
    // The storage size of this variable
    // This is useful for C++ class variables
    //
    uint32_t m_storageSize;
    // The offset in stackframe that stores the value of this variable, in interp mode
    //
    uint32_t m_interpOffset;
};

class AstDeclareVariable : public AstNodeBase
{
public:
    AstDeclareVariable(AstVariable* variable)
        : AstNodeBase(TypeId::Get<void>())
        , m_assignExpr(nullptr)
        , m_variable(variable)
    { }

    AstDeclareVariable(AstVariable* variable, AstAssignExpr* assignExpr)
        : AstNodeBase(TypeId::Get<void>())
        , m_assignExpr(assignExpr)
        , m_variable(variable)
    {
        TestAssert(m_assignExpr->GetValueType().AddPointer() == m_variable->GetTypeId());
        TestAssert(assert_cast<AstVariable*>(m_assignExpr->GetDst()) == m_variable);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void InterpImpl(void* /*out*/)
    {
        if (m_assignExpr != nullptr)
        {
            m_assignExpr->Interp(nullptr /*out*/);
        }
    }

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstDeclareVariable::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        if (m_assignExpr != nullptr) { fn(m_assignExpr); }
        fn(m_variable);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstDeclareVariable; }

    // An assign statement for primitive type variable initialization.
    // nullptr if no initial value.
    // TODO: support constructor
    //
    AstAssignExpr* m_assignExpr;
    AstVariable* m_variable;
};

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
        : AstNodeBase(operand->GetTypeId().RemovePointer())
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
        m_operand->Interp(&src);
        *out = *src;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl,
                              AstDereferenceVariableExpr,
                              InterpImpl,
                              AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupInterpImpl() override
    {
        m_interpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        fn(m_operand);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstDereferenceVariableExpr; }

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
        : AstNodeBase(TypeId::Get<void>())
        , m_contents()
    { }

    AstBlock(const std::vector<AstNodeBase*>& contents)
        : AstNodeBase(TypeId::Get<void>())
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
            stmt->Interp(ics);
            if (unlikely(*ics != InterpControlSignal::None))
            {
                break;
            }
        }
    }

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstBlock::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        for (AstNodeBase* stmt : m_contents)
        {
            fn(stmt);
        }
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstBlock; }

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
        : AstNodeBase(TypeId::Get<void>())
        , m_contents()
    { }

    AstScope(const std::vector<AstNodeBase*>& contents)
        : AstNodeBase(TypeId::Get<void>())
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
        assert(ics != nullptr && *ics == InterpControlSignal::None);
        for (AstNodeBase* stmt : m_contents)
        {
            stmt->Interp(ics);
            if (unlikely(*ics != InterpControlSignal::None))
            {
                break;
            }
        }
        // TODO: call destructors after they are supported
    }

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstScope::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        for (AstNodeBase* stmt : m_contents)
        {
            fn(stmt);
        }
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstScope; }

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
        : AstNodeBase(TypeId::Get<void>())
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
        m_condClause->Interp(&cond);
        if (cond)
        {
            m_thenClause->Interp(ics);
        }
        else if (m_elseClause != nullptr)
        {
            m_elseClause->Interp(ics);
        }
    }

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstIfStatement::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        // The order is important: reachability analysis relies on this order
        //
        fn(m_condClause);
        fn(m_thenClause);
        if (m_elseClause != nullptr) { fn(m_elseClause); }
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstIfStatement; }

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
        : AstNodeBase(TypeId::Get<void>())
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
            m_condClause->Interp(&cond);
            if (!cond) { break; }

            m_body->Interp(ics);
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

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstWhileLoop::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        fn(m_condClause);
        fn(m_body);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstWhileLoop; }

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
        : AstNodeBase(TypeId::Get<void>())
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

        m_startClause->Interp(ics);
        // Break/Continue/Return statements not allowed in for-loop init block
        //
        TestAssert(*ics == InterpControlSignal::None);

        while (true)
        {
            bool cond;
            m_condClause->Interp(&cond);
            if (!cond) { break; }

            m_body->Interp(ics);
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

            m_stepClause->Interp(ics);
            // Break/Continue/Return statements not allowed in for-loop step block
            //
            TestAssert(*ics == InterpControlSignal::None);
        }
    }

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstForLoop::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        // The order is important: reachability analysis relies on this order
        //
        fn(m_startClause);
        fn(m_condClause);
        fn(m_body);
        fn(m_stepClause);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstForLoop; }

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
        : AstNodeBase(TypeId::Get<void>())
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

    virtual void SetupInterpImpl() override
    {
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstBreakOrContinueStmt::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& /*fn*/) override { }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstBreakOrContinueStmt; }

private:
    // whether it is a break statement or a continue statement
    //
    bool m_isBreak;
};

}   // namespace Ast
