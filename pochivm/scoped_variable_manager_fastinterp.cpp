#include "fastinterp_ast_helper.hpp"
#include "codegen_context.hpp"
#include "destructor_helper.h"
#include "scoped_variable_manager.h"

namespace PochiVM
{

static void NO_RETURN fastinterp_unexpected_exception_call_terminate(uintptr_t stackframe) noexcept
{
    TestAssert(thread_pochiVMContext->m_fastInterpOutstandingExceptionPtr);
    fprintf(stderr, "[FASTINTERP RUNTIME] A C++ Exception escaped from a generated function that is marked noexcept!\n"
            "According to C++ standard, we are calling std::terminate(). The offending function's stack frame is at 0x%llx.\n"
            "The thrown exception is stored at thread_pochiVMContext->m_fastInterpOutstandingExceptionPtr.\n",
            static_cast<unsigned long long>(stackframe));
    std::terminate();
}

FastInterpSnippet WARN_UNUSED ScopedVariableManager::FIGenerateDestructorSequenceUntilScope(AstNodeBase* boundaryScope)
{
    TestAssert(m_operationMode == OperationMode::FASTINTERP);
    FastInterpSnippet result { nullptr, nullptr };
    auto rit = m_scopeStack.rbegin();
    while (rit != m_scopeStack.rend())
    {
        const std::vector<DestructorIREmitter*>& vec = rit->second;
        for (auto rit2 = vec.rbegin(); rit2 != vec.rend(); rit2++)
        {
            AstVariable* e = assert_cast<AstVariable*>(*rit2);
            FastInterpSnippet snippet = e->GetFastInterpDestructorSnippet();
            result = result.AddContinuation(snippet);
        }
        if (rit->first == boundaryScope)
        {
            break;
        }
        rit++;
    }
    TestAssertImp(boundaryScope != nullptr, rit != m_scopeStack.rend() && rit->first == boundaryScope);
    TestAssertImp(boundaryScope == nullptr, rit == m_scopeStack.rend());
    return result;
}

// We are using too many int-to-unsigned (to access vector index) here.. They are apparently safe so disable the warning.
//
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
FastInterpBoilerplateInstance* WARN_UNUSED ScopedVariableManager::FIGenerateEHEntryPointForCurrentPosition()
{
    TestAssert(m_operationMode == OperationMode::FASTINTERP);

    TestAssert(m_scopeStack.size() > 0);

    if (m_fiCurrentEHCatchBlock == nullptr)
    {
        // Generate the catch block for the case that we are not in a try-catch block
        //
        if (thread_llvmContext->m_curFunction->GetIsNoExcept())
        {
            // The caller is noexcept, we terminate program execution, as required by C++ standard.
            //
            m_fiCurrentEHCatchBlock = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FICallExprEnterCppFnImpl>::SelectBoilerplateBluePrint(
                            TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                            true /*isNoExcept*/));
            m_fiCurrentEHCatchBlock->PopulateCppFnPtrPlaceholder(0, fastinterp_unexpected_exception_call_terminate);
        }
        else
        {
            // Propagate the exception to the caller
            //
            m_fiCurrentEHCatchBlock = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                            thread_llvmContext->m_curFunction->GetReturnType().GetOneLevelPtrFastInterpTypeId(),
                            false /*isNoExcept*/,
                            true /*isExceptionThrown*/,
                            static_cast<FINumOpaqueIntegralParams>(0),
                            static_cast<FINumOpaqueFloatingParams>(0)));
        }
    }

    if (GetNumNontrivialDestructorObjects() == 0)
    {
        // No destructors to call, we can simply directly branch to m_fiCurrentEHCatchBlock
        //
        return m_fiCurrentEHCatchBlock;
    }

    int n = static_cast<int>(m_fiExceptionDtorTree.size());
    TestAssert(n <= static_cast<int>(m_scopeStack.size()));
#ifdef TESTBUILD
    for (int i = 0; i < n - 1; i++)
    {
        TestAssert(m_fiExceptionDtorTree[i].first == static_cast<int>(m_scopeStack[i].second.size()));
    }
#endif
    // Create the root dtor tree block
    //
    if (n == 0)
    {
        FastInterpSnippet snippet;
        int allInLevel = static_cast<int>(m_scopeStack[0].second.size());
        for (int i = allInLevel - 1; i >= 0; i--)
        {
            DestructorIREmitter* e = m_scopeStack[0].second[i];
            AstVariable* var = assert_cast<AstVariable*>(e);
            snippet = snippet.AddContinuation(var->GetFastInterpDestructorSnippet());
        }
        if (snippet.IsEmpty())
        {
            snippet = snippet.AddContinuation(FIGetNoopBoilerplate());
        }
        TestAssert(!snippet.IsEmpty() && !snippet.IsUncontinuable());
        snippet.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, m_fiCurrentEHCatchBlock);
        m_fiExceptionDtorTree.push_back(std::make_pair(allInLevel, snippet.m_entry));
        n++;
    }

    // Add destructors for the current scope for the newly constructed local vars after the previous generated block
    //
    {
        assert(n > 0 && m_fiExceptionDtorTree.size() == static_cast<size_t>(n));
        int numInCurLevel = m_fiExceptionDtorTree[n - 1].first;
        int allInCurLevel = static_cast<int>(m_scopeStack[n - 1].second.size());
        TestAssert(numInCurLevel <= allInCurLevel);
        if (numInCurLevel < allInCurLevel)
        {
            FastInterpSnippet snippet;
            for (int i = allInCurLevel - 1; i >= numInCurLevel; i--)
            {
                DestructorIREmitter* e = m_scopeStack[n - 1].second[i];
                AstVariable* var = assert_cast<AstVariable*>(e);
                snippet = snippet.AddContinuation(var->GetFastInterpDestructorSnippet());
            }
            if (snippet.IsEmpty())
            {
                snippet = snippet.AddContinuation(FIGetNoopBoilerplate());
            }
            TestAssert(!snippet.IsEmpty() && !snippet.IsUncontinuable());
            snippet.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, m_fiExceptionDtorTree[n - 1].second);
            m_fiExceptionDtorTree[n - 1] = std::make_pair(allInCurLevel, snippet.m_entry);
        }
    }

    while (static_cast<size_t>(n) < m_scopeStack.size())
    {
        FastInterpSnippet snippet;
        int allInLevel = static_cast<int>(m_scopeStack[n].second.size());
        for (int i = allInLevel - 1; i >= 0; i--)
        {
            DestructorIREmitter* e = m_scopeStack[n].second[i];
            AstVariable* var = assert_cast<AstVariable*>(e);
            snippet = snippet.AddContinuation(var->GetFastInterpDestructorSnippet());
        }
        if (snippet.IsEmpty())
        {
            snippet = snippet.AddContinuation(FIGetNoopBoilerplate());
        }
        TestAssert(!snippet.IsEmpty() && !snippet.IsUncontinuable());
        snippet.m_tail->PopulateBoilerplateFnPtrPlaceholder(0, m_fiExceptionDtorTree[n - 1].second);
        m_fiExceptionDtorTree.push_back(std::make_pair(allInLevel, snippet.m_entry));
        n++;
    }
    TestAssert(static_cast<size_t>(n) == m_fiExceptionDtorTree.size());
    TestAssert(m_fiExceptionDtorTree.size() == m_scopeStack.size());
#ifdef TESTBUILD
    for (size_t i = 0; i < m_fiExceptionDtorTree.size() - 1; i++)
    {
        TestAssert(m_fiExceptionDtorTree[i].first == static_cast<int>(m_scopeStack[i].second.size()));
    }
#endif
    return m_fiExceptionDtorTree.back().second;
}
#pragma clang diagnostic pop    // for "-Wno-sign-conversion"

}   // namespace PochiVM
