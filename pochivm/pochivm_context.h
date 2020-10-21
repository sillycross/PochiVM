#pragma once

#include "common.h"

namespace PochiVM
{

class AstScope;
class AstVariable;
class AstModule;
class FIStackFrameManager;
class FastInterpCodegenEngine;
class AstFunction;
class AstCallExpr;
class FastInterpGeneratedProgram;

struct PochiVMContext
{
    PochiVMContext()
        : m_astTraverseColorMark(1)
        , m_debugInterpStackFrameBase(0)
        , m_fastInterpStackFrameManager(nullptr)
        , m_fastInterpEngine(nullptr)
        , m_fastInterpGeneratedProgram(nullptr)
        , m_curModule(nullptr)
    { }

    // The current value used to mark an AST node in traversal
    //
    uint64_t m_astTraverseColorMark;

    // The current stack of scopes, used to figure out the variable destructors to call
    // when leaving a scope naturally or by break/continue/return statements
    //
    // Each element in m_debugInterpScopeStack is a variable scope,
    // which is a std::vector holding all the variables that have been declared in the scope
    //
    std::vector<std::vector<AstVariable*>> m_debugInterpScopeStack;

    // Current interp stack frame base
    //
    uintptr_t m_debugInterpStackFrameBase;

    FIStackFrameManager* m_fastInterpStackFrameManager;
    FastInterpCodegenEngine* m_fastInterpEngine;
    std::vector<std::pair<AstFunction*, AstCallExpr*>> m_fastInterpFnCallFixList;
    FastInterpGeneratedProgram* m_fastInterpGeneratedProgram;
    std::exception_ptr m_fastInterpOutstandingExceptionPtr;

    // Current module
    //
    AstModule* m_curModule;
};

inline thread_local PochiVMContext* thread_pochiVMContext = nullptr;

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

