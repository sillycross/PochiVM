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

TEST(SanityCallCppFn, BooleanTypeCornerCase_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<bool(int, bool*, bool**)>;
    {
        auto [fn, a, b, c] = NewFunction<FnPrototype>("testfn");
        auto d = fn.NewVariable<bool>();

        fn.SetBody(
                Declare(d, a == Literal<int>(233)),
                Return(CallFreeFn::TestCornerCases::BoolParamTest1(d, b, c))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        bool x[2];
        bool* px = x;
        bool** ppx = &px;
        bool b[3];
        uint8_t* _b = reinterpret_cast<uint8_t*>(b);

        memset(b, 233, 3);
        x[0] = true; x[1] = false;
        ReleaseAssert(interpFn(233, b, ppx) == false);
        ReleaseAssert(_b[0] == 1);
        ReleaseAssert(_b[1] == 1);
        ReleaseAssert(_b[2] == 1);

        memset(b, 233, 3);
        x[0] = false; x[1] = true;
        ReleaseAssert(interpFn(100, b, ppx) == true);
        ReleaseAssert(_b[0] == 1);
        ReleaseAssert(_b[1] == 0);
        ReleaseAssert(_b[2] == 0);
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

        bool x[2];
        bool* px = x;
        bool** ppx = &px;
        bool b[3];
        uint8_t* _b = reinterpret_cast<uint8_t*>(b);

        memset(b, 233, 3);
        x[0] = true; x[1] = false;
        ReleaseAssert(jitFn(233, b, ppx) == false);
        ReleaseAssert(_b[0] == 1);
        ReleaseAssert(_b[1] == 1);
        ReleaseAssert(_b[2] == 1);

        memset(b, 233, 3);
        x[0] = false; x[1] = true;
        ReleaseAssert(jitFn(100, b, ppx) == true);
        ReleaseAssert(_b[0] == 1);
        ReleaseAssert(_b[1] == 0);
        ReleaseAssert(_b[2] == 0);
    }
}

TEST(SanityCallCppFn, BooleanTypeCornerCase_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<bool(int, bool**, bool**)>;
    {
        auto [fn, a, b, c] = NewFunction<FnPrototype>("testfn");
        auto d = fn.NewVariable<bool>();

        fn.SetBody(
                Declare(d, a == Literal<int>(233)),
                CallFreeFn::TestCornerCases::BoolParamTest2(d, b.Deref(), c),
                Return(d)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        {
            bool x[2];
            bool* px = x;
            bool** ppx = &px;
            bool b[4];
            bool* pb = b;
            bool** ppb = &pb;
            uint8_t* _b = reinterpret_cast<uint8_t*>(b);

            memset(x, 233, 1); x[1] = false;
            memset(b, 233, 4);
            ReleaseAssert(interpFn(233, ppx, ppb) == false);
            ReleaseAssert(*reinterpret_cast<uint8_t*>(x) == 1);
            ReleaseAssert(_b[2] == 1);
            ReleaseAssert(_b[3] == 0);
            ReleaseAssert(*ppx = pb);
        }
        {
            bool x[2];
            bool* px = x;
            bool** ppx = &px;
            bool b[4];
            bool* pb = b;
            bool** ppb = &pb;
            uint8_t* _b = reinterpret_cast<uint8_t*>(b);
            memset(x, 233, 1); x[1] = true;
            memset(b, 233, 4); b[1] = false;
            ReleaseAssert(interpFn(100, ppx, ppb) == true);
            ReleaseAssert(*reinterpret_cast<uint8_t*>(x) == 0);
            ReleaseAssert(_b[2] == 1);
            ReleaseAssert(_b[3] == 0);
            ReleaseAssert(*ppx = pb);
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
            bool x[2];
            bool* px = x;
            bool** ppx = &px;
            bool b[4];
            bool* pb = b;
            bool** ppb = &pb;
            uint8_t* _b = reinterpret_cast<uint8_t*>(b);

            memset(x, 233, 1); x[1] = false;
            memset(b, 233, 4);
            ReleaseAssert(jitFn(233, ppx, ppb) == false);
            ReleaseAssert(*reinterpret_cast<uint8_t*>(x) == 1);
            ReleaseAssert(_b[2] == 1);
            ReleaseAssert(_b[3] == 0);
            ReleaseAssert(*ppx = pb);
        }
        {
            bool x[2];
            bool* px = x;
            bool** ppx = &px;
            bool b[4];
            bool* pb = b;
            bool** ppb = &pb;
            uint8_t* _b = reinterpret_cast<uint8_t*>(b);
            memset(x, 233, 1); x[1] = true;
            memset(b, 233, 4); b[1] = false;
            ReleaseAssert(jitFn(100, ppx, ppb) == true);
            ReleaseAssert(*reinterpret_cast<uint8_t*>(x) == 0);
            ReleaseAssert(_b[2] == 1);
            ReleaseAssert(_b[3] == 0);
            ReleaseAssert(*ppx = pb);
        }
    }
}

TEST(SanityCallCppFn, VoidStarCornerCase_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void*(void*, void**)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("testfn");
        fn.SetBody(
                Return(CallFreeFn::TestCornerCases::VoidStarParamTest1(a, b))
       );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        void* a = reinterpret_cast<void*>(233);
        void* c;
        void** b = &c;
        ReleaseAssert(interpFn(a, b) == reinterpret_cast<void*>(233 + 8));
        ReleaseAssert(c == reinterpret_cast<void*>(233));
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

        void* a = reinterpret_cast<void*>(233);
        void* c;
        void** b = &c;
        ReleaseAssert(jitFn(a, b) == reinterpret_cast<void*>(233 + 8));
        ReleaseAssert(c == reinterpret_cast<void*>(233));
    }
}

TEST(SanityCallCppFn, VoidStarCornerCase_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void*(void*, void**)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("testfn");
        fn.SetBody(
                CallFreeFn::TestCornerCases::VoidStarParamTest2(a, b),
                Return(a)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        void* a = reinterpret_cast<void*>(233);
        void* b[2];
        b[0] = nullptr; b[1] = reinterpret_cast<void*>(345);
        ReleaseAssert(interpFn(a, b) == reinterpret_cast<void*>(345));
        ReleaseAssert(b[0] == reinterpret_cast<void*>(233));
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

        void* a = reinterpret_cast<void*>(233);
        void* b[2];
        b[0] = nullptr; b[1] = reinterpret_cast<void*>(345);
        ReleaseAssert(jitFn(a, b) == reinterpret_cast<void*>(345));
        ReleaseAssert(b[0] == reinterpret_cast<void*>(233));
    }
}

TEST(SanityCallCppFn, ReturnsNonPrimitiveType)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<bool(int)>;
    {
        auto [fn, param] = NewFunction<FnPrototype>("testfn");
        auto v = fn.NewVariable<TestNonTrivialConstructor>();
        fn.SetBody(
                Declare(v, Variable<TestNonTrivialConstructor>::Create(param)),
                Return(v.GetValue() == Literal<int>(233))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        ReleaseAssert(interpFn(233) == true);
        ReleaseAssert(interpFn(234) == false);
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
        // in non-debug build, the whole thing should have been inlined and optimized out
        //
        jit.SetAllowResolveSymbolInHostProcess(x_isDebugBuild);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("testfn");

        ReleaseAssert(jitFn(233) == true);
        ReleaseAssert(jitFn(234) == false);
    }
}

TEST(SanityCallCppFn, NonTrivialCopyConstructor)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(TestNonTrivialCopyConstructor*)>;
    {
        auto [fn, param] = NewFunction<FnPrototype>("testfn");
        fn.SetBody(
                Return(Variable<TestNonTrivialCopyConstructor>::Fn(param.Deref()))
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        TestNonTrivialCopyConstructor::counter = 0;
        TestNonTrivialCopyConstructor x(5);
        ReleaseAssert(interpFn(&x) == 5);
        ReleaseAssert(TestNonTrivialCopyConstructor::counter == 1);
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

        TestNonTrivialCopyConstructor::counter = 0;
        TestNonTrivialCopyConstructor x(5);
        ReleaseAssert(jitFn(&x) == 5);
        ReleaseAssert(TestNonTrivialCopyConstructor::counter == 1);
    }
}

TEST(SanityCallCppFn, Constructor_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(std::vector<int>*)>;
    {
        auto [fn, param] = NewFunction<FnPrototype>("testfn");
        auto v = fn.NewVariable<std::vector<int>>();
        fn.SetBody(
                Declare(v),
                CallFreeFn::CopyVectorInt(param, v.Addr())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        std::vector<int> a;
        a.push_back(233);
        interpFn(&a);
        ReleaseAssert(a.size() == 0);
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

        std::vector<int> a;
        a.push_back(233);
        jitFn(&a);
        ReleaseAssert(a.size() == 0);
    }
}

TEST(SanityCallCppFn, Constructor_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(std::vector<int>*, size_t)>;
    {
        auto [fn, param, num] = NewFunction<FnPrototype>("testfn");
        auto v = fn.NewVariable<std::vector<int>>();
        fn.SetBody(
                Declare(v, Constructor<std::vector<int>>(num)),
                CallFreeFn::CopyVectorInt(param, v.Addr())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        std::vector<int> a;
        a.push_back(233);
        interpFn(&a, 100);
        ReleaseAssert(a.size() == 100);
        for (size_t i = 0; i < 100; i++) { ReleaseAssert(a[i] == 0); }
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

        std::vector<int> a;
        a.push_back(233);
        jitFn(&a, 100);
        ReleaseAssert(a.size() == 100);
        for (size_t i = 0; i < 100; i++) { ReleaseAssert(a[i] == 0); }
    }
}

TEST(SanityCallCppFn, Constructor_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(std::vector<int>*, size_t, int)>;
    {
        auto [fn, param, num, val] = NewFunction<FnPrototype>("testfn");
        auto v = fn.NewVariable<std::vector<int>>();
        fn.SetBody(
                Declare(v, Constructor<std::vector<int>>(num, val)),
                CallFreeFn::CopyVectorInt(param, v.Addr())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        std::vector<int> a;
        a.push_back(233);
        interpFn(&a, 100, 34567);
        ReleaseAssert(a.size() == 100);
        for (size_t i = 0; i < 100; i++) { ReleaseAssert(a[i] == 34567); }
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

        std::vector<int> a;
        a.push_back(233);
        jitFn(&a, 100, 45678);
        ReleaseAssert(a.size() == 100);
        for (size_t i = 0; i < 100; i++) { ReleaseAssert(a[i] == 45678); }
    }
}

TEST(SanityCallCppFn, Constructor_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(std::vector<int>*, std::vector<int>*)>;
    {
        auto [fn, param, param2] = NewFunction<FnPrototype>("testfn");
        auto v = fn.NewVariable<std::vector<int>>();
        fn.SetBody(
                Declare(v, Constructor<std::vector<int>>(param2.Deref())),
                CallFreeFn::CopyVectorInt(param, v.Addr())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        std::vector<int> a, b;
        a.push_back(233);
        b.push_back(456); b.push_back(567);
        interpFn(&a, &b);
        ReleaseAssert(a.size() == 2);
        ReleaseAssert(a[0] == 456 && a[1] == 567);
        ReleaseAssert(b.size() == 2);
        ReleaseAssert(b[0] == 456 && b[1] == 567);
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

        std::vector<int> a, b;
        a.push_back(233);
        b.push_back(456); b.push_back(567);
        jitFn(&a, &b);
        ReleaseAssert(a.size() == 2);
        ReleaseAssert(a[0] == 456 && a[1] == 567);
        ReleaseAssert(b.size() == 2);
        ReleaseAssert(b[0] == 456 && b[1] == 567);
    }
}

TEST(SanityCallCppFn, Constructor_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int()>;
    {
        auto [fn] = NewFunction<FnPrototype>("testfn");
        auto v = fn.NewVariable<TestConstructor1>();
        fn.SetBody(
                Declare(v),
                Return(v.GetValue())
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        ReleaseAssert(interpFn() == 233);
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

        ReleaseAssert(jitFn() == 233);
    }
}

TEST(SanityCallCppFn, ManuallyCallDestructor)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(TestDestructor1*)>;
    {
        auto [fn, v] = NewFunction<FnPrototype>("testfn");
        fn.SetBody(
                CallDestructor(v)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<FnPrototype>("testfn");

        int out = 0;
        TestDestructor1* obj = reinterpret_cast<TestDestructor1*>(alloca(sizeof(TestDestructor1)));
        new (obj) TestDestructor1(233, &out);
        ReleaseAssert(out == 0);
        interpFn(obj);
        ReleaseAssert(out == 233);
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

        int out = 0;
        TestDestructor1* obj = reinterpret_cast<TestDestructor1*>(alloca(sizeof(TestDestructor1)));
        new (obj) TestDestructor1(233, &out);
        ReleaseAssert(out == 0);
        jitFn(obj);
        ReleaseAssert(out == 233);
    }
}
