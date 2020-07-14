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
        ReleaseAssert(a.m_y == 123 + 1);
        ReleaseAssert(ret == 123 + 3);
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        if (x_isDebugBuild)
        {
            AssertIsExpectedOutput(dump, "debug_before_opt");
        }
        else
        {
            AssertIsExpectedOutput(dump, "nondebug_before_opt");
        }
    }

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "after_opt");
    }

    {
        SimpleJIT jit;
        if (x_isDebugBuild)
        {
            jit.SetAllowResolveSymbolInHostProcess(true);
        }
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        TestClassA a;
        int ret = jitFn(&a, 123);
        ReleaseAssert(a.m_y == 123 + 1);
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
        std::vector<int> expectedVec;
        for (int i = 0; i < 100; i++)
        {
            int k = rand() % 1000;
            int64_t ret = interpFn(&a, k);
            expectedVec.push_back(k);
            expectedSum += k;
            ReleaseAssert(ret == expectedSum);
            ReleaseAssert(a.m_vec == expectedVec);
        }
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "before_opt");
    }

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "after_opt");
    }

    {
        SimpleJIT jit;
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        TestClassA a;
        int expectedSum = 0;
        std::vector<int> expectedVec;
        for (int i = 0; i < 100; i++)
        {
            int k = rand() % 1000;
            int64_t ret = jitFn(&a, k);
            expectedVec.push_back(k);
            expectedSum += k;
            ReleaseAssert(ret == expectedSum);
            ReleaseAssert(a.m_vec == expectedVec);
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

TEST(SanityCallCppFn, Sanity_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(std::string*)>;
    {
        auto [fn, c] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                Return(CallFreeFn::FreeFnRecursive2(c.Deref()))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");
        std::string val = "10";
        int result = interpFn(&val);
        ReleaseAssert(result == 89);
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "before_opt");
    }

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "after_opt");
    }

    {
        SimpleJIT jit;
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        std::string val = "10";
        int result = jitFn(&val);
        ReleaseAssert(result == 89);
    }
}

TEST(SanityCallCppFn, UnusedCppTypeCornerCase)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void*(std::string*)>;
    {
        auto [fn, c] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                Return(c.ReinterpretCast<void*>())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");
        std::string val = "10";
        void* result = interpFn(&val);
        ReleaseAssert(result == reinterpret_cast<void*>(&val));
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "before_opt");
    }

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "after_opt");
    }

    {
        SimpleJIT jit;
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        std::string val = "10";
        void* result = jitFn(&val);
        ReleaseAssert(result == reinterpret_cast<void*>(&val));
    }
}
