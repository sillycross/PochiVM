#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"
#include "codegen_context.hpp"

using namespace PochiVM;

TEST(SanityCatchThrow, ThrowSanity_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void()>;
    {
        auto [fn] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                Throw(Constructor<std::bad_alloc>())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");
        try {
            interpFn();
            ReleaseAssert(false);
        } catch(std::bad_alloc& w) {
            ReleaseAssert(std::string(w.what()) == "std::bad_alloc");
        }

        try {
            interpFn();
            ReleaseAssert(false);
        } catch(std::exception& w) {
            ReleaseAssert(std::string(w.what()) == "std::bad_alloc");
        }
    }
}

TEST(SanityCatchThrow, ThrowSanity_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int)>;
    {
        auto [fn, v] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                Throw(v)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");
        try {
            interpFn(123);
            ReleaseAssert(false);
        } catch(int& v) {
            ReleaseAssert(v == 123);
        }
    }
}

TEST(SanityCatchThrow, ThrowSanity_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int)>;
    {
        auto [fn, v] = NewFunction<FnPrototype>("testfn");
        auto x = fn.NewVariable<TestNonTrivialCopyConstructor>();
        fn.SetBody(
                Declare(x, Constructor<TestNonTrivialCopyConstructor>(v)),
                Throw(x)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        TestNonTrivialCopyConstructor::counter = 0;
        try {
            interpFn(123);
            ReleaseAssert(false);
        } catch(TestNonTrivialCopyConstructor& v) {
            ReleaseAssert(v.value == 123);
            ReleaseAssert(TestNonTrivialCopyConstructor::counter == 1);
        }
    }
}

TEST(SanityCatchThrow, ThrowSanity_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int)>;
    {
        auto [fn, v] = NewFunction<FnPrototype>("testfn");
        fn.SetBody(
                Throw(Variable<TestNonTrivialConstructor>::Create(v))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        try {
            interpFn(123);
            ReleaseAssert(false);
        } catch(TestNonTrivialConstructor& v) {
            ReleaseAssert(v.m_value == 123);
        }
    }
}
