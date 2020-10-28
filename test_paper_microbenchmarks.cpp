#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"

using namespace PochiVM;

TEST(DISABLED_PaperMicrobenchmark, FibonacciSeq)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = uint64_t(*)(int) noexcept;
    auto [fn, n] = NewFunction<FnPrototype>("fib_nth", "n");

    fn.SetBody(
        If(n <= 2).Then(
                Return(Literal<uint64_t>(1))
        ).Else(
                Return(Call<FnPrototype>("fib_nth", n - 1)
                       + Call<FnPrototype>("fib_nth", n - 2))
        )
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        AutoTimer timer;
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                GetFastInterpGeneratedFunction<FnPrototype>("fib_nth");

        uint64_t ret = interpFn(40);
        ReleaseAssert(ret == 102334155);
    }

    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        AutoTimer t;
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("fib_nth");
        uint64_t ret = jitFn(40);
        ReleaseAssert(ret == 102334155);
    }
}

TEST(DISABLED_PaperMicrobenchmark, EulerSieve)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int, int*, int*) noexcept;
    {
        auto [fn, n, lp, pr] = NewFunction<FnPrototype>("euler_sieve");
        auto cnt = fn.NewVariable<int>();
        auto i = fn.NewVariable<int>();
        auto j = fn.NewVariable<int>();
        fn.SetBody(
            Declare(cnt, 0),
            For(Declare(i, 2), i <= n, Increment(i)).Do(
                If(lp[i] == 0).Then(
                    Assign(lp[i], i),
                    Assign(pr[cnt], i),
                    Increment(cnt)
                ),
                For(Declare(j, 0), j < cnt && pr[j] <= lp[i] && i * pr[j] <= n, Increment(j)).Do(
                    Assign(lp[i * pr[j]], pr[j])
                )
            ),
            Return(cnt)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        int n = 100000000;
        int* lp = new int[static_cast<size_t>(n + 10)];
        memset(lp, 0, sizeof(int) * static_cast<size_t>(n + 10));
        int* pr = new int[static_cast<size_t>(n + 10)];
        memset(pr, 0, sizeof(int) * static_cast<size_t>(n + 10));

        FastInterpFunction<FnPrototype> fnPtr = thread_pochiVMContext->m_curModule->
                GetFastInterpGeneratedFunction<FnPrototype>("euler_sieve");
        {
            AutoTimer t;
            int result = fnPtr(n, lp, pr);
            ReleaseAssert(result == 5761455);
        }
    }

    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        int n = 100000000;
        int* lp = new int[static_cast<size_t>(n + 10)];
        memset(lp, 0, sizeof(int) * static_cast<size_t>(n + 10));
        int* pr = new int[static_cast<size_t>(n + 10)];
        memset(pr, 0, sizeof(int) * static_cast<size_t>(n + 10));

        FnPrototype jitFn = jit.GetFunction<FnPrototype>("euler_sieve");
        {
            AutoTimer t;
            int result = jitFn(n, lp, pr);
            ReleaseAssert(result == 5761455);
        }
    }
}
