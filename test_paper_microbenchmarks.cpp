#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"
#include <random>

using namespace PochiVM;

TEST(PaperMicrobenchmark, FibonacciSeq)
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

    {
        AutoTimer t;
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }

    {
        AutoTimer timer;
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                GetFastInterpGeneratedFunction<FnPrototype>("fib_nth");

        uint64_t ret = interpFn(40);
        ReleaseAssert(ret == 102334155);
    }

    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t;
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(3 /*optLevel*/);
        jitFn = jit.GetFunction<FnPrototype>("fib_nth");
    }

    {
        AutoTimer t;
        uint64_t ret = jitFn(40);
        ReleaseAssert(ret == 102334155);
    }
}

TEST(PaperMicrobenchmark, EulerSieve)
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

    TestJitHelper jit;
    jit.Init(3 /*optLevel*/);

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

TEST(PaperMicrobenchmark, QuickSort)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)(int*, int, int) noexcept;
    {
        auto [fn, a, lo, hi] = NewFunction<FnPrototype>("quicksort");
        auto tmp = fn.NewVariable<int>();
        auto pivot = fn.NewVariable<int>();
        auto i = fn.NewVariable<int>();
        auto j = fn.NewVariable<int>();
        fn.SetBody(
            If(lo < hi).Then(
                Declare(pivot, a[hi]),
                Declare(i, lo),
                For(Declare(j, lo), j <= hi, Increment(j)).Do(
                    If(a[j] < pivot).Then(
                        Declare(tmp, a[i]),
                        Assign(a[i], a[j]),
                        Assign(a[j], tmp),
                        Increment(i)
                    )
                ),
                Assign(a[hi], a[i]),
                Assign(a[i], pivot),
                Call<FnPrototype>("quicksort", a, lo, i - 1),
                Call<FnPrototype>("quicksort", a, i + 1, hi)
            )
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        int n = 5000000;
        int* a = new int[static_cast<size_t>(n)];
        for (int i = 0; i < n; i++)
        {
            a[i] = i;
        }
        std::mt19937 mt_rand(123 /*seed*/);
        for (int i = 0; i < n; i++)
        {
            std::swap(a[i], a[mt_rand() % static_cast<size_t>(i + 1)]);
        }

        FastInterpFunction<FnPrototype> fnPtr = thread_pochiVMContext->m_curModule->
                GetFastInterpGeneratedFunction<FnPrototype>("quicksort");
        {
            AutoTimer t;
            fnPtr(a, 0, n - 1);
        }

        for (int i = 0; i < n; i++)
        {
            ReleaseAssert(a[i] == i);
        }
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    TestJitHelper jit;
    jit.Init(3 /*optLevel*/);

    {
        int n = 5000000;
        int* a = new int[static_cast<size_t>(n)];
        for (int i = 0; i < n; i++)
        {
            a[i] = i;
        }
        std::mt19937 mt_rand(123 /*seed*/);
        for (int i = 0; i < n; i++)
        {
            std::swap(a[i], a[mt_rand() % static_cast<size_t>(i + 1)]);
        }

        FnPrototype jitFn = jit.GetFunction<FnPrototype>("quicksort");
        {
            AutoTimer t;
            jitFn(a, 0, n - 1);
        }

        for (int i = 0; i < n; i++)
        {
            ReleaseAssert(a[i] == i);
        }
    }
}
