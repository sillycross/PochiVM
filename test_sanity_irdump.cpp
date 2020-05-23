#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"

using namespace PochiVM;

namespace {

void TestSomeRandomParams(const std::function<int(int,int)>& fn,
                          const std::function<int(int,int)>& gold,
                          const std::function<int(int, int)>& cond = nullptr)
{
    for (int i = 0; i < 1000; i++)
    {
        int a = rand() % 2000 - 1000;
        int b = rand() % 2000 - 1000;
        int r1 = fn(a, b);
        int r2 = gold(a, b);
        if (cond == nullptr || (cond != nullptr && cond(a, b)))
        {
            ReleaseAssert(r1 == r2);
        }
    }
}

}   // anonymous namespace

TEST(SanityIrCodeDump, APlusB)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, value1, value2] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            Return(value1 + value2)
        );
    }

    FnPrototype gold = [](int a, int b) {
        return a + b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}

TEST(SanityIrCodeDump, APlusB_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Assign(a, a * Literal<int>(-1))
            ),
            Return(a + b)
        );
    }

    FnPrototype gold = [](int a, int b) {
        if (a < 0) a *= -1;
        return a + b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}

TEST(SanityIrCodeDump, APlusB_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Assign(a, a * Literal<int>(-1))
            ).Else(
                Assign(a, a * Literal<int>(2))
            ),
            Return(a + b)
        );
    }

    FnPrototype gold = [](int a, int b) {
        if (a < 0) a *= -1; else a *= 2;
        return a + b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}

TEST(SanityIrCodeDump, APlusB_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Return(b - a)
            ).Else(
                Assign(a, a * Literal<int>(2))
            ),
            Return(a + b)
        );
    }

    FnPrototype gold = [](int a, int b) {
        if (a < 0) return b-a; else a *= 2;
        return a + b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}

TEST(SanityIrCodeDump, APlusB_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Return(b - a)
            ).Else(
                Return(a + b)
            )
        );
    }

    FnPrototype gold = [](int a, int b) {
        if (a < 0) return b-a; else return a + b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}

TEST(SanityIrCodeDump, APlusB_6)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
            ).Else(
                Return(a + b)
            )
        );
    }

    FnPrototype gold = [](int a, int b) {
        if (a < 0) return 0; else return a + b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold, [](int a, int /*b*/){ return a >= 0; });
}

TEST(SanityIrCodeDump, APlusB_7)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Return(a + b)
            )
        );
    }

    FnPrototype gold = [](int a, int b) {
        if (a < 0) return a + b; else return 0;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold, [](int a, int /*b*/){ return a < 0; });
}

TEST(SanityIrCodeDump, APlusB_8)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                If(b < Literal<int>(0)).Then(
                    Return(a + b)
                ).Else(
                    Increment(a)
                )
            ).Else(
                If(b < Literal<int>(0)).Then(
                    Increment(b)
                ).Else(
                    If(b > Literal<int>(5)).Then(
                        Assign(a, a + Literal<int>(2))
                    )
                )
            ),
            Return(a - b)
        );
    }

    FnPrototype gold = [](int a, int b) {
        if (a < 0)
        {
            if (b < 0)
            {
                return a+b;
            }
            else
            {
                a++;
            }
        }
        else
        {
            if (b < 0)
            {
                b++;
            }
            else
            {
                if (b > 5)
                {
                    a += 2;
                }
            }
        }
        return a-b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}

TEST(SanityIrCodeDump, APlusB_9)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            While(a < Literal<int>(0)).Do(
                Increment(a),
                Assign(b, b - Literal<int>(1))
            ),
            Return(a + b)
        );
    }

    FnPrototype gold = [](int a, int b) {
        while (a < 0)
        {
            a++;
            b--;
        }
        return a+b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}

TEST(SanityIrCodeDump, APlusB_10)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            While(a > Literal<int>(0)).Do(
                Increment(a),
                Assign(b, b - Literal<int>(1)),
                If(a % Literal<int>(11) == Literal<int>(0)).Then(
                    Assign(a, a - Literal<int>(1)),
                    Increment(b),
                    Break()
                ),
                While(b % Literal<int>(23) != Literal<int>(5)).Do(
                    Increment(b),
                    If(b % Literal<int>(17) == Literal<int>(3)).Then(
                        Assign(b, b + Literal<int>(3)),
                        Continue()
                    ).Else(
                        If(b % Literal<int>(13) == Literal<int>(2)).Then(
                            Assign(b, b + Literal<int>(2)),
                            Break()
                        )
                    ),
                    Increment(b)
                ),
                If(b % Literal<int>(19) == Literal<int>(4)).Then(
                    Continue()
                ),
                If(b % Literal<int>(19) == Literal<int>(5)).Then(
                    Break()
                ),
                While(b % Literal<int>(12) != Literal<int>(3)).Do(
                    Increment(b),
                    If(b % Literal<int>(29) == Literal<int>(3)).Then(
                        Return(a - b)
                    ),
                    If(b % Literal<int>(31) == Literal<int>(4)).Then(
                        Continue()
                    ),
                    If(b % Literal<int>(33) == Literal<int>(5)).Then(
                        Break()
                    ),
                    Increment(b)
                )
            ),
            Return(a + b)
        );
    }

    FnPrototype gold = [](int a, int b) {
        while (a > 0)
        {
            a++;
            b--;
            if (a % 11 == 0)
            {
                a--;
                b++;
                break;
            }
            while (b % 23 != 5)
            {
                b++;
                if (b % 17 == 3)
                {
                    b += 3;
                    continue;
                }
                else if (b % 13 == 2)
                {
                    b += 2;
                    break;
                }
                b++;
            }
            if (b % 19 == 4)
            {
                continue;
            }
            if (b % 19 == 5)
            {
                break;
            }
            while (b % 12 != 3)
            {
                b++;
                if (b % 29 == 3)
                {
                    return a - b;
                }
                if (b % 31 == 4)
                {
                    continue;
                }
                if (b % 33 == 5)
                {
                    break;
                }
                b++;
            }
        }
        return a+b;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_b");
    TestSomeRandomParams(jitFn, gold);
}
