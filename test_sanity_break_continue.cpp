#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"
#include "codegen_context.hpp"

using namespace PochiVM;

TEST(Sanity, BreakAndContinue)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    {
        auto [fn, n] = NewFunction<FnPrototype>("MyFn");
        auto i = fn.NewVariable<int>("i");
        auto j = fn.NewVariable<int>("j");
        auto k = fn.NewVariable<int>("k");
        auto s = fn.NewVariable<int>("s");
        fn.SetBody(
            Declare(s, 0),
            For(Declare(i, 1), i < n, Block(Increment(i), Assign(s, s + i))).Do(
                For(Declare(j, 1), j <= i, Block(Increment(j), Increment(s))).Do(
                    If(j % Literal<int>(3) == Literal<int>(0)).Then(Continue()),
                    Assign(s, s + j * j + j),
                    For(Declare(k, j),
                        k > Literal<int>(0),
                        Block(Assign(k, k - Literal<int>(1)), Increment(s))
                    ).Do(
                        Increment(s),
                        If(s % k * Literal<int>(10) <= k).Then(Break()),
                        Assign(s, s + Literal<int>(2))
                    ),
                    Assign(s, s + j)
                ),
                If(i % Literal<int>(5) == Literal<int>(0)).Then(Continue()),
                If(s % i == Literal<int>(3)).Then(Continue()),
                Assign(s, s + i * i)
            ),
            Return(s)
        );
    }

    auto gold = [](int n) -> int
    {
        int s = 0;
        for (int i = 1; i < n; i++, s += i)
        {
            for (int j = 1; j <= i; j++, s++)
            {
                if (j % 3 == 0) { continue; }
                s += j * j + j;
                for (int k = j; k > 0; k--, s++)
                {
                    s++;
                    if (s % k * 10 <= k) { break; }
                    s += 2;
                }
                s += j;
            }
            if (i % 5 == 0) { continue; }
            if (s % i == 3) { continue; }
            s += i * i;
        }
        return s;
    };

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    thread_pochiVMContext->m_curModule->EmitIR();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");

        ReleaseAssert(gold(10) == interpFn(10));
        ReleaseAssert(gold(30) == interpFn(30));
        ReleaseAssert(gold(50) == interpFn(50));
        ReleaseAssert(gold(100) == interpFn(100));
    }

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("MyFn");

        ReleaseAssert(gold(10) == interpFn(10));
        ReleaseAssert(gold(30) == interpFn(30));
        ReleaseAssert(gold(50) == interpFn(50));
        ReleaseAssert(gold(100) == interpFn(100));
    }

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("MyFn");
        ReleaseAssert(gold(10) == jitFn(10));
        ReleaseAssert(gold(30) == jitFn(30));
        ReleaseAssert(gold(50) == jitFn(50));
        ReleaseAssert(gold(100) == jitFn(100));
    }
}
