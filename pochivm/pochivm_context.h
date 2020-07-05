#pragma once

#include "common.h"

namespace PochiVM
{

class AstScope;
class AstVariable;
class AstModule;

// The list of variables with non-trivial destructor declared in a scope
//
struct VarDtorInScope
{
    VarDtorInScope(AstScope* scope)
        : m_scope(scope)
        , m_variables()
    { }

    AstScope* m_scope;
    std::vector<AstVariable*> m_variables;
};

struct PochiVMContext
{
    PochiVMContext()
        : m_astTraverseColorMark(1)
        , m_interpStackFrameBase(0)
        , m_curModule(nullptr)
        , m_variableBeingDeclared(nullptr)
    { }

    // The current value used to mark an AST node in traversal
    //
    uint64_t m_astTraverseColorMark;

    // The current stack of scopes, used to figure out the variable destructors to call
    // when leaving a scope naturally or by break/continue/return statements
    //
    std::vector<VarDtorInScope> m_scopeStack;

    // Current interp stack frame base
    //
    uintptr_t m_interpStackFrameBase;

    // Current module
    //
    AstModule* m_curModule;

    // For Declare(var, value), the 'var' being declared right now
    //
    AstVariable* m_variableBeingDeclared;
};

}   // namespace PochiVM

extern thread_local PochiVM::PochiVMContext* thread_pochiVMContext;

namespace PochiVM
{

class AutoThreadPochiVMContext
{
public:
    AutoThreadPochiVMContext()
    {
        TestAssert(thread_pochiVMContext == nullptr);
        m_contextPtr = new PochiVMContext();
        ReleaseAssert(m_contextPtr != nullptr);
        thread_pochiVMContext = m_contextPtr;
    }

    ~AutoThreadPochiVMContext()
    {
        TestAssert(thread_pochiVMContext == m_contextPtr);
        delete m_contextPtr;
        thread_pochiVMContext = nullptr;
    }

private:
    PochiVMContext* m_contextPtr;
};
#define AutoThreadPochiVMContext(...) static_assert(false, "Wrong use of 'auto'-pattern!");

}   // namespace PochiVM

