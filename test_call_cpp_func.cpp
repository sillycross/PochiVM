#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"
#include "codegen_context.hpp"

using namespace PochiVM;

TEST(SanityCallCppFn, Sanity_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(TestClassA*, int)>;
    {
        auto [fn, c, v] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                c.Deref().SetY(v + Literal<int>(1)),
                Return(c.Deref().GetY() + Literal<int>(2))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");
        TestClassA a;
        int ret = interpFn(&a, 123);
        ReleaseAssert(ret == 123 + 3);
    }
}

TEST(SanityCallCppFn, Sanity_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int64_t(TestClassA*, int)>;
    {
        auto [fn, c, v] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                c.Deref().PushVec(v),
                Return(c.Deref().GetVectorSum())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");
        TestClassA a;
        int expectedSum = 0;
        for (int i = 0; i < 100; i++)
        {
            int k = rand() % 1000;
            int64_t ret = interpFn(&a, k);
            expectedSum += k;
            ReleaseAssert(ret == expectedSum);
        }
    }
}

TEST(SanityCallCppFn, TypeMismatchErrorCase)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<double(TestClassA*)>;
    {
        auto [fn, c] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                Return(c.Deref().GetVectorSum())
        );
    }

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());
    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

