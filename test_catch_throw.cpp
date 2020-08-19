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
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        try {
            jitFn(123);
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
                Throw(Variable<TestNonTrivialConstructor>::Create3(v))
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
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        try {
            jitFn(123);
            ReleaseAssert(false);
        } catch(TestNonTrivialConstructor& v) {
            ReleaseAssert(v.m_value == 123);
        }
    }
}

TEST(SanityCatchThrow, ThrowSanity_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int)>;
    {
        auto [fn, v] = NewFunction<FnPrototype>("testfn");
        fn.SetBody(
                Throw(Variable<TestNonTrivialConstructor>::Create2(v))
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

        try {
            interpFn(12345);
            ReleaseAssert(false);
        } catch(std::bad_function_call& w) {
            ReleaseAssert(std::string(w.what()) == std::string("bad_function_call"));
        } catch(...) {
            ReleaseAssert(false);
        }
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
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        try {
            jitFn(123);
            ReleaseAssert(false);
        } catch(TestNonTrivialConstructor& v) {
            ReleaseAssert(v.m_value == 123);
        }

        try {
            jitFn(12345);
            ReleaseAssert(false);
        } catch(std::bad_function_call& w) {
            ReleaseAssert(std::string(w.what()) == std::string("bad_function_call"));
        } catch(...) {
            ReleaseAssert(false);
        }
    }
}

TEST(SanityCatchThrow, ThrowSanity_6)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int, CtorDtorOrderRecorder*)>;
    {
        auto [fn, v, r] = NewFunction<FnPrototype>("testfn");
        auto x = fn.NewVariable<TestDestructor2>();
        fn.SetBody(
                Declare(x, Constructor<TestDestructor2>(r, Literal<int>(233))),
                Throw(v)
        );
    }

    std::vector<int> expectedAns {233, -233};

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");
        CtorDtorOrderRecorder r;
        try {
            interpFn(123, &r);
            ReleaseAssert(false);
        } catch(int& v) {
            ReleaseAssert(v == 123);
            ReleaseAssert(r.order == expectedAns);
        }
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
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        CtorDtorOrderRecorder r;
        try {
            jitFn(123, &r);
            ReleaseAssert(false);
        } catch(int& v) {
            ReleaseAssert(v == 123);
            ReleaseAssert(r.order == expectedAns);
        }
    }
}

TEST(SanityCatchThrow, ThrowSanity_7)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int, CtorDtorOrderRecorder*)>;
    {
        auto [fn, v, r] = NewFunction<FnPrototype>("testfn");
        auto x = fn.NewVariable<TestDestructor2>();
        fn.SetBody(
                Declare(x, Constructor<TestDestructor2>(r, Literal<int>(233))),
                Throw(Variable<TestNonTrivialConstructor>::Create2(v))
        );
    }

    std::vector<int> expectedAns {233, -233};

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        {
            CtorDtorOrderRecorder r;
            try {
                interpFn(123, &r);
                ReleaseAssert(false);
            } catch(TestNonTrivialConstructor& v) {
                ReleaseAssert(v.m_value == 123);
                ReleaseAssert(r.order == expectedAns);
            }
        }
        {
            CtorDtorOrderRecorder r;
            try {
                interpFn(12345, &r);
                ReleaseAssert(false);
            } catch(std::bad_function_call& w) {
                ReleaseAssert(std::string(w.what()) == std::string("bad_function_call"));
                ReleaseAssert(r.order == expectedAns);
            } catch(...) {
                ReleaseAssert(false);
            }
        }
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
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        {
            CtorDtorOrderRecorder r;
            try {
                jitFn(123, &r);
                ReleaseAssert(false);
            } catch(TestNonTrivialConstructor& v) {
                ReleaseAssert(v.m_value == 123);
                ReleaseAssert(r.order == expectedAns);
            }
        }
        {
            CtorDtorOrderRecorder r;
            try {
                jitFn(12345, &r);
                ReleaseAssert(false);
            } catch(std::bad_function_call& w) {
                ReleaseAssert(std::string(w.what()) == std::string("bad_function_call"));
                ReleaseAssert(r.order == expectedAns);
            } catch(...) {
                ReleaseAssert(false);
            }
        }
    }
}

TEST(SanityCatchThrow, ThrowSanity_8)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int, CtorDtorOrderRecorder*)>;
    {
        auto [fn, v, r] = NewFunction<FnPrototype>("testfn");
        fn.SetBody(
                Throw(Variable<TestDestructor2>::Create(r, v))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        CtorDtorOrderRecorder r;
        std::vector<int> tmp;
        try {
            interpFn(123, &r);
            ReleaseAssert(false);
        } catch(TestDestructor2& v) {
            ReleaseAssert(v.m_value == 123);
            ReleaseAssert(r.order == (std::vector<int>{123, 123, -123}));
            tmp = r.order;
        }
        tmp.push_back(-123);
        ReleaseAssert(r.order == tmp);
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
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        CtorDtorOrderRecorder r;
        std::vector<int> tmp;
        try {
            jitFn(123, &r);
            ReleaseAssert(false);
        } catch(TestDestructor2& v) {
            ReleaseAssert(v.m_value == 123);
            ReleaseAssert(r.order == (std::vector<int>{123}));
            tmp = r.order;
        }
        tmp.push_back(-123);
        ReleaseAssert(r.order == tmp);
    }
}
