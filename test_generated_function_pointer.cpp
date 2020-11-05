#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"

using namespace PochiVM;

TEST(SanityGeneratedFunctionPointer, Sanity_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int, int);
    auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b");

    fn.SetBody(Return(a + b));

    using FnPrototype2 = uintptr_t(*)();
    auto [fn2] = NewFunction<FnPrototype2>("get_ptr");

    fn2.SetBody(Return(GetGeneratedFunctionPointer("a_plus_b")));

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                GetDebugInterpGeneratedFunction<FnPrototype2>("get_ptr");

        uintptr_t p = interpFn();
        ReleaseAssert((p >> 62) == 2);
        ReleaseAssert(GeneratedFunctionPointer<FnPrototype>(p)(123, 456) == 123 + 456);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    {
        FastInterpFunction<FnPrototype2> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype2>("get_ptr");
        uintptr_t p = interpFn();
        ReleaseAssert((p >> 62) == 1);
        ReleaseAssert(GeneratedFunctionPointer<FnPrototype>(p)(123, 456) == 123 + 456);
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

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "after_opt");
    }

    SimpleJIT jit;
    jit.SetAllowResolveSymbolInHostProcess(true);
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype2 jitFn = jit.GetFunction<FnPrototype2>("get_ptr");
        uintptr_t p = jitFn();
        ReleaseAssert((p >> 62) == 0);
        ReleaseAssert(GeneratedFunctionPointer<FnPrototype>(p)(123, 456) == 123 + 456);
    }
}

TEST(SanityGeneratedFunctionPointer, Sanity_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int, int) noexcept;
    auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b");

    fn.SetBody(Return(a + b));

    using FnPrototype2 = uintptr_t(*)();
    auto [fn2] = NewFunction<FnPrototype2>("get_ptr");

    fn2.SetBody(Return(GetGeneratedFunctionPointer("a_plus_b")));

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                GetDebugInterpGeneratedFunction<FnPrototype2>("get_ptr");

        uintptr_t p = interpFn();
        ReleaseAssert((p >> 62) == 2);
        ReleaseAssert(GeneratedFunctionPointer<FnPrototype>(p)(123, 456) == 123 + 456);
        ReleaseAssert(GeneratedFunctionPointer<int(*)(int, int)>(p)(123, 456) == 123 + 456);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    {
        FastInterpFunction<FnPrototype2> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype2>("get_ptr");
        uintptr_t p = interpFn();
        ReleaseAssert((p >> 62) == 1);
        ReleaseAssert(GeneratedFunctionPointer<FnPrototype>(p)(123, 456) == 123 + 456);
        ReleaseAssert(GeneratedFunctionPointer<int(*)(int, int)>(p)(123, 456) == 123 + 456);
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

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "after_opt");
    }

    SimpleJIT jit;
    jit.SetAllowResolveSymbolInHostProcess(true);
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype2 jitFn = jit.GetFunction<FnPrototype2>("get_ptr");
        uintptr_t p = jitFn();
        ReleaseAssert((p >> 62) == 0);
        ReleaseAssert(GeneratedFunctionPointer<FnPrototype>(p)(123, 456) == 123 + 456);
        ReleaseAssert(GeneratedFunctionPointer<int(*)(int, int)>(p)(123, 456) == 123 + 456);
    }
}

TEST(SanityGeneratedFunctionPointer, Sanity_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int, int);
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b");
        fn.SetBody(Return(a + b));
    }

    {
        auto [fn, a, b] = NewFunction<FnPrototype>("testfn");
        auto v = fn.NewVariable<TestGeneratedFnPtr>();
        fn.SetBody(
            Declare(v, Constructor<TestGeneratedFnPtr>(GetGeneratedFunctionPointer("a_plus_b"))),
            Return(v.execute(a, b))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                GetDebugInterpGeneratedFunction<FnPrototype>("testfn");

        ReleaseAssert(interpFn(123, 456) == 123 + 456);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("testfn");
        ReleaseAssert(interpFn(123, 456) == 123 + 456);
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

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);

    if (!x_isDebugBuild)
    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump, "after_opt");
    }

    SimpleJIT jit;
    jit.SetAllowResolveSymbolInHostProcess(true);
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");
        ReleaseAssert(jitFn(123, 456) == 123 + 456);
    }
}
