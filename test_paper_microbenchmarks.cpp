#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"
#include <random>

// Uncomment to enable running paper microbenchmarks
//
//#define ENABLE_PAPER_MICROBENCHMARKS

#ifdef ENABLE_PAPER_MICROBENCHMARKS
#define PAPER_MICROBENCHMARK_TEST_PREFIX PaperMicrobenchmark
#else
#define PAPER_MICROBENCHMARK_TEST_PREFIX DISABLED_PaperMicrobenchmark
#endif

using namespace PochiVM;

namespace {

double GetBestResultOfRuns(std::function<double()> lambda, int numRuns = 10)
{
    double bestResult = 1e100;
    for (int i = 0; i < numRuns; i++)
    {
        double result = lambda();
        bestResult = std::min(bestResult, result);
    }
    return bestResult;
}

}   // anonymous namespace

namespace PaperMicrobenchmarkFibonacciSequence
{

void CreateFunction(std::string fnName)
{
    using FnPrototype = uint64_t(*)(int) noexcept;
    auto [fn, n] = NewFunction<FnPrototype>(fnName, "n");

    fn.SetBody(
        If(n <= 2).Then(
                Return(Literal<uint64_t>(1))
        ).Else(
                Return(Call<FnPrototype>(fnName, n - 1)
                       + Call<FnPrototype>(fnName, n - 2))
        )
    );
}

void SetupModuleForCodegenTiming()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    // Create 100 copies
    //
    for (int i = 0; i < 100; i++)
    {
        CreateFunction(std::string("fib_nth") + std::to_string(i));
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

void SetupModuleForExecution()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");
    CreateFunction("fib_nth");
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

double TimeFastInterpCodegenTime()
{
    double ts;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }
    return ts;
}

TestJitHelper RunLLVMCodegenForExecution(int optLevel)
{
    using FnPrototype = uint64_t(*)(int) noexcept;
    TestJitHelper jit;
    thread_pochiVMContext->m_curModule->EmitIR();
    jit.Init(optLevel);
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("fib_nth");
    std::ignore = jitFn;
    return jit;
}

std::pair<TestJitHelper, double> TimeLLVMCodegenTime(int optLevel)
{
    double ts;
    using FnPrototype = uint64_t(*)(int) noexcept;
    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        // Important to call getFunction here:
        // The actual LLVM codegen work is done in GetFunction, so must be timed
        //
        for (int i = 0; i < 100; i++)
        {
            jitFn = jit.GetFunction<FnPrototype>(std::string("fib_nth") + std::to_string(i));
            std::ignore = jitFn;
        }
    }
    return std::make_pair(std::move(jit), ts);
}

double TimeFastInterpPerformance()
{
    double ts;
    using FnPrototype = uint64_t(*)(int) noexcept;
    FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
            GetFastInterpGeneratedFunction<FnPrototype>("fib_nth");
    {
        AutoTimer t(&ts);
        uint64_t ret = interpFn(40);
        ReleaseAssert(ret == 102334155);
    }
    return ts;
}

double TimeLLVMPerformance(TestJitHelper& jit)
{
    double ts;
    using FnPrototype = uint64_t(*)(int) noexcept;
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("fib_nth");
    {
        AutoTimer t(&ts);
        uint64_t ret = jitFn(40);
        ReleaseAssert(ret == 102334155);
    }
    return ts;
}

}   // namespace PaperMicrobenchmarkFibonacciSequence

TEST(PAPER_MICROBENCHMARK_TEST_PREFIX, FibonacciSeq)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    using namespace PaperMicrobenchmarkFibonacciSequence;

    double fastinterpCodegenTime = GetBestResultOfRuns([]() {
        SetupModuleForCodegenTiming();
        return TimeFastInterpCodegenTime();
    });

    SetupModuleForExecution();
    TimeFastInterpCodegenTime();
    double fastInterpPerformance = GetBestResultOfRuns([]() {
        return TimeFastInterpPerformance();
    });

    double llvmCodegenTime[4], llvmPerformance[4];
    for (int optLevel = 0; optLevel <= 3; optLevel ++)
    {
        llvmCodegenTime[optLevel] = GetBestResultOfRuns([&]() {
            SetupModuleForCodegenTiming();
            auto result = TimeLLVMCodegenTime(optLevel);
            return result.second;
        });

        SetupModuleForExecution();
        TestJitHelper jit = RunLLVMCodegenForExecution(optLevel);
        llvmPerformance[optLevel] = GetBestResultOfRuns([&]() {
            return TimeLLVMPerformance(jit);
        });
    }

    printf("******* Fibonacci Sequence Microbenchmark *******\n");

    printf("==============================\n");
    printf("   Codegen Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastinterpCodegenTime / 100);
    printf("LLVM -O0:   %.7lf\n", llvmCodegenTime[0] / 100);
    printf("LLVM -O1:   %.7lf\n", llvmCodegenTime[1] / 100);
    printf("LLVM -O2:   %.7lf\n", llvmCodegenTime[2] / 100);
    printf("LLVM -O3:   %.7lf\n", llvmCodegenTime[3] / 100);
    printf("==============================\n\n");

    printf("==============================\n");
    printf("  Execution Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpPerformance);
    printf("LLVM -O0:   %.7lf\n", llvmPerformance[0]);
    printf("LLVM -O1:   %.7lf\n", llvmPerformance[1]);
    printf("LLVM -O2:   %.7lf\n", llvmPerformance[2]);
    printf("LLVM -O3:   %.7lf\n", llvmPerformance[3]);
    printf("==============================\n");
}

namespace PaperMicrobenchmarkEulerSieve
{

void CreateFunction(std::string fnName)
{
    using FnPrototype = int(*)(int, int*, int*) noexcept;
    auto [fn, n, lp, pr] = NewFunction<FnPrototype>(fnName);
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

void SetupModuleForCodegenTiming()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    // Create 100 copies
    //
    for (int i = 0; i < 100; i++)
    {
        CreateFunction(std::string("euler_sieve") + std::to_string(i));
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

void SetupModuleForExecution()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");
    CreateFunction("euler_sieve");
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

double TimeFastInterpCodegenTime()
{
    double ts;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }
    return ts;
}

TestJitHelper RunLLVMCodegenForExecution(int optLevel)
{
    using FnPrototype = int(*)(int, int*, int*) noexcept;
    TestJitHelper jit;
    thread_pochiVMContext->m_curModule->EmitIR();
    jit.Init(optLevel);
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("euler_sieve");
    std::ignore = jitFn;
    return jit;
}

std::pair<TestJitHelper, double> TimeLLVMCodegenTime(int optLevel)
{
    double ts;
    using FnPrototype = int(*)(int, int*, int*) noexcept;
    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        // Important to call getFunction here:
        // The actual LLVM codegen work is done in GetFunction, so must be timed
        //
        for (int i = 0; i < 100; i++)
        {
            jitFn = jit.GetFunction<FnPrototype>(std::string("euler_sieve") + std::to_string(i));
            std::ignore = jitFn;
        }
    }
    return std::make_pair(std::move(jit), ts);
}

double TimeFastInterpPerformance()
{
    double ts;
    using FnPrototype = int(*)(int, int*, int*) noexcept;
    FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
            GetFastInterpGeneratedFunction<FnPrototype>("euler_sieve");

    int n = 100000000;
    int* lp = new int[static_cast<size_t>(n + 10)];
    memset(lp, 0, sizeof(int) * static_cast<size_t>(n + 10));
    int* pr = new int[static_cast<size_t>(n + 10)];
    memset(pr, 0, sizeof(int) * static_cast<size_t>(n + 10));

    {
        AutoTimer t(&ts);
        int ret = interpFn(n, lp, pr);
        ReleaseAssert(ret == 5761455);
    }

    delete [] lp;
    delete [] pr;
    return ts;
}

double TimeLLVMPerformance(TestJitHelper& jit)
{
    double ts;
    using FnPrototype = int(*)(int, int*, int*) noexcept;
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("euler_sieve");

    int n = 100000000;
    int* lp = new int[static_cast<size_t>(n + 10)];
    memset(lp, 0, sizeof(int) * static_cast<size_t>(n + 10));
    int* pr = new int[static_cast<size_t>(n + 10)];
    memset(pr, 0, sizeof(int) * static_cast<size_t>(n + 10));

    {
        AutoTimer t(&ts);
        int ret = jitFn(n, lp, pr);
        ReleaseAssert(ret == 5761455);
    }

    delete [] lp;
    delete [] pr;
    return ts;
}

}   // namespace PaperMicrobenchmarkEulerSieve

TEST(PAPER_MICROBENCHMARK_TEST_PREFIX, EulerSieve)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    using namespace PaperMicrobenchmarkEulerSieve;

    double fastinterpCodegenTime = GetBestResultOfRuns([]() {
        SetupModuleForCodegenTiming();
        return TimeFastInterpCodegenTime();
    });

    SetupModuleForExecution();
    TimeFastInterpCodegenTime();
    double fastInterpPerformance = GetBestResultOfRuns([]() {
        return TimeFastInterpPerformance();
    });

    double llvmCodegenTime[4], llvmPerformance[4];
    for (int optLevel = 0; optLevel <= 3; optLevel ++)
    {
        llvmCodegenTime[optLevel] = GetBestResultOfRuns([&]() {
            SetupModuleForCodegenTiming();
            auto result = TimeLLVMCodegenTime(optLevel);
            return result.second;
        });

        SetupModuleForExecution();
        TestJitHelper jit = RunLLVMCodegenForExecution(optLevel);
        llvmPerformance[optLevel] = GetBestResultOfRuns([&]() {
            return TimeLLVMPerformance(jit);
        });
    }

    printf("******* Euler Sieve Microbenchmark *******\n");

    printf("==============================\n");
    printf("   Codegen Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastinterpCodegenTime / 100);
    printf("LLVM -O0:   %.7lf\n", llvmCodegenTime[0] / 100);
    printf("LLVM -O1:   %.7lf\n", llvmCodegenTime[1] / 100);
    printf("LLVM -O2:   %.7lf\n", llvmCodegenTime[2] / 100);
    printf("LLVM -O3:   %.7lf\n", llvmCodegenTime[3] / 100);
    printf("==============================\n\n");

    printf("==============================\n");
    printf("  Execution Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpPerformance);
    printf("LLVM -O0:   %.7lf\n", llvmPerformance[0]);
    printf("LLVM -O1:   %.7lf\n", llvmPerformance[1]);
    printf("LLVM -O2:   %.7lf\n", llvmPerformance[2]);
    printf("LLVM -O3:   %.7lf\n", llvmPerformance[3]);
    printf("==============================\n");
}

namespace PaperMicrobenchmarkQuickSort
{

void CreateFunction(std::string fnName)
{
    using FnPrototype = void(*)(int*, int, int) noexcept;
    auto [fn, a, lo, hi] = NewFunction<FnPrototype>(fnName);
    auto tmp = fn.NewVariable<int>();
    auto pivot = fn.NewVariable<int>();
    auto i = fn.NewVariable<int>();
    auto j = fn.NewVariable<int>();
    fn.SetBody(
        If(lo >= hi).Then(Return()),
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
        Call<FnPrototype>(fnName, a, lo, i - 1),
        Call<FnPrototype>(fnName, a, i + 1, hi)
    );
}

void SetupModuleForCodegenTiming()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    // Create 100 copies
    //
    for (int i = 0; i < 100; i++)
    {
        CreateFunction(std::string("quicksort") + std::to_string(i));
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

void SetupModuleForExecution()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");
    CreateFunction("quicksort");
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

double TimeFastInterpCodegenTime()
{
    double ts;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }
    return ts;
}

TestJitHelper RunLLVMCodegenForExecution(int optLevel)
{
    using FnPrototype = void(*)(int*, int, int) noexcept;
    TestJitHelper jit;
    thread_pochiVMContext->m_curModule->EmitIR();
    jit.Init(optLevel);
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("quicksort");
    std::ignore = jitFn;
    return jit;
}

std::pair<TestJitHelper, double> TimeLLVMCodegenTime(int optLevel)
{
    double ts;
    using FnPrototype = void(*)(int*, int, int) noexcept;
    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        // Important to call getFunction here:
        // The actual LLVM codegen work is done in GetFunction, so must be timed
        //
        for (int i = 0; i < 100; i++)
        {
            jitFn = jit.GetFunction<FnPrototype>(std::string("quicksort") + std::to_string(i));
            std::ignore = jitFn;
        }
    }
    return std::make_pair(std::move(jit), ts);
}

double TimeFastInterpPerformance()
{
    double ts;
    using FnPrototype = void(*)(int*, int, int) noexcept;
    FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
            GetFastInterpGeneratedFunction<FnPrototype>("quicksort");

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

    {
        AutoTimer t(&ts);
        interpFn(a, 0, n - 1);
    }

    for (int i = 0; i < n; i++)
    {
        ReleaseAssert(a[i] == i);
    }

    delete [] a;
    return ts;
}

double TimeLLVMPerformance(TestJitHelper& jit)
{
    double ts;
    using FnPrototype = void(*)(int*, int, int) noexcept;
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("quicksort");

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

    {
        AutoTimer t(&ts);
        jitFn(a, 0, n - 1);
    }

    for (int i = 0; i < n; i++)
    {
        ReleaseAssert(a[i] == i);
    }

    delete [] a;
    return ts;
}

}   // namespace PaperMicrobenchmarkQuickSort

TEST(PAPER_MICROBENCHMARK_TEST_PREFIX, QuickSort)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    using namespace PaperMicrobenchmarkQuickSort;

    double fastinterpCodegenTime = GetBestResultOfRuns([]() {
        SetupModuleForCodegenTiming();
        return TimeFastInterpCodegenTime();
    });

    SetupModuleForExecution();
    TimeFastInterpCodegenTime();
    double fastInterpPerformance = GetBestResultOfRuns([]() {
        return TimeFastInterpPerformance();
    });

    double llvmCodegenTime[4], llvmPerformance[4];
    for (int optLevel = 0; optLevel <= 3; optLevel ++)
    {
        llvmCodegenTime[optLevel] = GetBestResultOfRuns([&]() {
            SetupModuleForCodegenTiming();
            auto result = TimeLLVMCodegenTime(optLevel);
            return result.second;
        });

        SetupModuleForExecution();
        TestJitHelper jit = RunLLVMCodegenForExecution(optLevel);
        llvmPerformance[optLevel] = GetBestResultOfRuns([&]() {
            return TimeLLVMPerformance(jit);
        });
    }

    printf("******* Quicksort Microbenchmark *******\n");

    printf("==============================\n");
    printf("   Codegen Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastinterpCodegenTime / 100);
    printf("LLVM -O0:   %.7lf\n", llvmCodegenTime[0] / 100);
    printf("LLVM -O1:   %.7lf\n", llvmCodegenTime[1] / 100);
    printf("LLVM -O2:   %.7lf\n", llvmCodegenTime[2] / 100);
    printf("LLVM -O3:   %.7lf\n", llvmCodegenTime[3] / 100);
    printf("==============================\n\n");

    printf("==============================\n");
    printf("  Execution Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpPerformance);
    printf("LLVM -O0:   %.7lf\n", llvmPerformance[0]);
    printf("LLVM -O1:   %.7lf\n", llvmPerformance[1]);
    printf("LLVM -O2:   %.7lf\n", llvmPerformance[2]);
    printf("LLVM -O3:   %.7lf\n", llvmPerformance[3]);
    printf("==============================\n");
}

namespace PaperMicrobenchmarkBrainfxxkCompiler
{

Block Codegen(const std::string& bfCode,
              const std::vector<size_t>& match,
              size_t start, size_t end,
              const Variable<uint8_t*>& ptr,
              const Variable<uint8_t*>& input,
              const Variable<uint8_t*>& output,
              const Variable<size_t>& outputCount)
{
    if (start >= end)
    {
        return Block();
    }
    Block result = Block();
    size_t i = start;
    while (i < end)
    {
        if (bfCode[i] == '>')
        {
            size_t cnt = 0;
            while (bfCode[i] == '>')
            {
                cnt++;
                i++;
            }
            i--;
            result.Append(Assign(ptr, ptr + cnt));
        }
        else if (bfCode[i] == '<')
        {
            size_t cnt = 0;
            while (bfCode[i] == '<')
            {
                cnt++;
                i++;
            }
            i--;
            result.Append(Assign(ptr, ptr - cnt));
        }
        else if (bfCode[i] == '+')
        {
            uint8_t cnt = 0;
            while (bfCode[i] == '+')
            {
                cnt++;
                i++;
            }
            i--;
            result.Append(Assign(*ptr, *ptr + cnt));
        }
        else if (bfCode[i] == '-')
        {
            uint8_t cnt = 0;
            while (bfCode[i] == '-')
            {
                cnt++;
                i++;
            }
            i--;
            result.Append(Assign(*ptr, *ptr - cnt));
        }
        else if (bfCode[i] == ',')
        {
            result.Append(Assign(*ptr, *input));
            result.Append(Assign(input, input + 1));
        }
        else if (bfCode[i] == '.')
        {
            result.Append(Assign(output[outputCount], *ptr));
            result.Append(Assign(outputCount, outputCount + static_cast<size_t>(1)));
        }
        else if (bfCode[i] == '[')
        {
            size_t other = match[i];
            ReleaseAssert(other > i && other < end);
            Block body = Codegen(bfCode, match, i + 1, other, ptr, input, output, outputCount);
            result.Append(While(*ptr != static_cast<uint8_t>(0)).Do(body));
            i = match[i];
        }
        else
        {
            ReleaseAssert(false && "Invalid program");
        }
        i++;
    }
    return result;
}

void CreateFunction(const std::string& fnName, const std::string& bfCode)
{
    size_t length = bfCode.length();
    std::vector<size_t> match;
    match.resize(length);
    std::vector<size_t> bracketOffsetStack;
    for (size_t i = 0; i < length; i++)
    {
        if (bfCode[i] == '[')
        {
            bracketOffsetStack.push_back(i);
        }
        else if (bfCode[i] == ']')
        {
            ReleaseAssert(bracketOffsetStack.size() > 0 && "Invalid program");
            match[bracketOffsetStack.back()] = i;
            bracketOffsetStack.pop_back();
        }
    }

    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    auto [fn, pad, input, output] = NewFunction<FnPrototype>(fnName);
    auto outputCount = fn.NewVariable<size_t>();

    fn.SetBody(
        Declare(outputCount, static_cast<size_t>(0))
    );
    fn.GetBody().Append(Codegen(bfCode, match, 0, length, pad, input, output, outputCount));
    fn.GetBody().Append(Return(outputCount));
}

}   // PaperMicrobenchmarkBrainfxxkCompiler

namespace PaperMicrobenchmarkBrainfxxkPrintPrimes
{

const char* const x_program =
    ">++++++++[<++++++++>-]<++++++++++++++++.[-]>++++++++++[<++++++++++>-]"
    "<++++++++++++++.[-]>++++++++++[<++++++++++>-]<+++++.[-]>++++++++++[<+"
    "+++++++++>-]<+++++++++.[-]>++++++++++[<++++++++++>-]<+.[-]>++++++++++"
    "[<++++++++++>-]<+++++++++++++++.[-]>+++++[<+++++>-]<+++++++.[-]>+++++"
    "+++++[<++++++++++>-]<+++++++++++++++++.[-]>++++++++++[<++++++++++>-]<"
    "++++++++++++.[-]>+++++[<+++++>-]<+++++++.[-]>++++++++++[<++++++++++>-"
    "]<++++++++++++++++.[-]>++++++++++[<++++++++++>-]<+++++++++++.[-]>++++"
    "+++[<+++++++>-]<+++++++++.[-]>+++++[<+++++>-]<+++++++.[-]+[->,-------"
    "---[<+>-------------------------------------->[>+>+<<-]>>[<<+>>-]<>>>"
    "+++++++++[<<<[>+>+<<-]>>[<<+>>-]<[<<+>>-]>>-]<<<[-]<<[>+<-]]<]>>[<<+>"
    ">-]<<>+<-[>+[>+>+<<-]>>[<<+>>-]<>+<-->>>>>>>>+<<<<<<<<[>+<-<[>>>+>+<<"
    "<<-]>>>>[<<<<+>>>>-]<<<>[>>+>+<<<-]>>>[<<<+>>>-]<<<<>>>[>+>+<<-]>>[<<"
    "+>>-]<<<[>>>>>+<<<[>+>+<<-]>>[<<+>>-]<[>>[-]<<-]>>[<<<<[>+>+<<-]>>[<<"
    "+>>-]<>>>-]<<<-<<-]+>>[<<[-]>>-]<<>[-]<[>>>>>>[-]<<<<<<-]<<>>[-]>[-]<"
    "<<]>>>>>>>>[-<<<<<<<[-]<<[>>+>+<<<-]>>>[<<<+>>>-]<<<>>[>+<-]>[[>+>+<<"
    "-]>>[<<+>>-]<>+++++++++<[>>>+<<[>+>[-]<<-]>[<+>-]>[<<++++++++++>>-]<<"
    "-<-]+++++++++>[<->-]<[>+<-]<[>+<-]<[>+<-]>>>[<<<+>>>-]<>+++++++++<[>>"
    ">+<<[>+>[-]<<-]>[<+>-]>[<<++++++++++>>>+<-]<<-<-]>>>>[<<<<+>>>>-]<<<<"
    ">[-]<<+>]<[[>+<-]+++++++[<+++++++>-]<-><.[-]>>[<<+>>-]<<-]>++++[<++++"
    "++++>-]<.[-]>>>>>>>]<<<<<<<<>[-]<[-]<<-]++++++++++.[-]";

void CreateFunction(std::string fnName)
{
    PaperMicrobenchmarkBrainfxxkCompiler::CreateFunction(fnName, x_program);
}

void SetupModule()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");
    CreateFunction("bf_print_primes");
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

double TimeFastInterpCodegenTime()
{
    double ts;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }
    return ts;
}

TestJitHelper RunLLVMCodegenForExecution(int optLevel)
{
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    TestJitHelper jit;
    thread_pochiVMContext->m_curModule->EmitIR();
    jit.Init(optLevel);
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("bf_print_primes");
    std::ignore = jitFn;
    return jit;
}

std::pair<TestJitHelper, double> TimeLLVMCodegenTime(int optLevel)
{
    double ts;
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        // Important to call getFunction here:
        // The actual LLVM codegen work is done in GetFunction, so must be timed
        //
        jitFn = jit.GetFunction<FnPrototype>("bf_print_primes");
        std::ignore = jitFn;
    }
    return std::make_pair(std::move(jit), ts);
}

double TimeFastInterpPerformance()
{
    double ts;
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
            GetFastInterpGeneratedFunction<FnPrototype>("bf_print_primes");

    std::string expectedOutput =
        "Primes up to: 2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 53 59 61 67 71 73 "
        "79 83 89 97 101 103 107 109 113 127 131 137 139 149 151 157 163 167 173 "
        "179 181 191 193 197 199 211 223 227 229 233 239 241 251 \n";

    uint8_t input[] = { static_cast<uint8_t>('2'),
                        static_cast<uint8_t>('5'),
                        static_cast<uint8_t>('5'),
                        static_cast<uint8_t>(10) /* newline */ };

    uint8_t output[8192];
    uint8_t pad[30000];
    memset(pad, 0, 30000);

    size_t outputLength;
    {
        AutoTimer t(&ts);
        outputLength = interpFn(pad, input, output);
    }

    ReleaseAssert(outputLength < 8192);
    std::string result = "";
    for (size_t i = 0; i < outputLength; i++)
    {
        result += static_cast<char>(output[i]);
    }
    ReleaseAssert(result == expectedOutput);

    return ts;
}

double TimeLLVMPerformance(TestJitHelper& jit)
{
    double ts;
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("bf_print_primes");

    std::string expectedOutput =
        "Primes up to: 2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 53 59 61 67 71 73 "
        "79 83 89 97 101 103 107 109 113 127 131 137 139 149 151 157 163 167 173 "
        "179 181 191 193 197 199 211 223 227 229 233 239 241 251 \n";

    uint8_t input[] = { static_cast<uint8_t>('2'),
                        static_cast<uint8_t>('5'),
                        static_cast<uint8_t>('5'),
                        static_cast<uint8_t>(10) /* newline */ };

    uint8_t output[8192];
    uint8_t pad[30000];
    memset(pad, 0, 30000);

    size_t outputLength;
    {
        AutoTimer t(&ts);
        outputLength = jitFn(pad, input, output);
    }

    ReleaseAssert(outputLength < 8192);
    std::string result = "";
    for (size_t i = 0; i < outputLength; i++)
    {
        result += static_cast<char>(output[i]);
    }
    ReleaseAssert(result == expectedOutput);

    return ts;
}

}   // namespace PaperMicrobenchmarkBrainfxxkPrintPrimes

TEST(PAPER_MICROBENCHMARK_TEST_PREFIX, BrainfxxkPrintPrimes)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    using namespace PaperMicrobenchmarkBrainfxxkPrintPrimes;

    double fastinterpCodegenTime = GetBestResultOfRuns([]() {
        SetupModule();
        return TimeFastInterpCodegenTime();
    });

    SetupModule();
    TimeFastInterpCodegenTime();
    double fastInterpPerformance = GetBestResultOfRuns([]() {
        return TimeFastInterpPerformance();
    });

    double llvmCodegenTime[4], llvmPerformance[4];
    for (int optLevel = 0; optLevel <= 3; optLevel ++)
    {
        llvmCodegenTime[optLevel] = GetBestResultOfRuns([&]() {
            SetupModule();
            auto result = TimeLLVMCodegenTime(optLevel);
            return result.second;
        });

        SetupModule();
        TestJitHelper jit = RunLLVMCodegenForExecution(optLevel);
        llvmPerformance[optLevel] = GetBestResultOfRuns([&]() {
            return TimeLLVMPerformance(jit);
        });
    }

    printf("******* BF Print Primes Microbenchmark *******\n");
    printf("Program Length: %d bytes\n", static_cast<int>(strlen(x_program)));

    printf("==============================\n");
    printf("   Codegen Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastinterpCodegenTime);
    printf("LLVM -O0:   %.7lf\n", llvmCodegenTime[0]);
    printf("LLVM -O1:   %.7lf\n", llvmCodegenTime[1]);
    printf("LLVM -O2:   %.7lf\n", llvmCodegenTime[2]);
    printf("LLVM -O3:   %.7lf\n", llvmCodegenTime[3]);
    printf("==============================\n\n");

    printf("==============================\n");
    printf("  Execution Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpPerformance);
    printf("LLVM -O0:   %.7lf\n", llvmPerformance[0]);
    printf("LLVM -O1:   %.7lf\n", llvmPerformance[1]);
    printf("LLVM -O2:   %.7lf\n", llvmPerformance[2]);
    printf("LLVM -O3:   %.7lf\n", llvmPerformance[3]);
    printf("==============================\n");
}

namespace PaperMicrobenchmarkBrainfxxkMandelbrot
{

// A mandelbrot set fractal viewer in brainf*** written by Erik Bosman
//
const char* const x_program =
  "+++++++++++++[->++>>>+++++>++>+<<<<<<]>>>>>++++++>--->>>>>>>>>>+++++++++++++++[["
  ">>>>>>>>>]+[<<<<<<<<<]>>>>>>>>>-]+[>>>>>>>>[-]>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>[-]+"
  "<<<<<<<+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>>>+>>>>>>>>>>>>>>>>>>>>>>>>>>"
  ">+<<<<<<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+[>>>>>>[>>>>>>>[-]>>]<<<<<<<<<[<<<<<<<<<]>>"
  ">>>>>[-]+<<<<<<++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>>+<<<<<<+++++++[-[->>>"
  ">>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>>+<<<<<<<<<<<<<<<<[<<<<<<<<<]>>>[[-]>>>>>>[>>>>>"
  ">>[-<<<<<<+>>>>>>]<<<<<<[->>>>>>+<<+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>"
  "[>>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<+<<]>>>>>>>>]<<<<<<<<<[<<<<<<<"
  "<<]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<<<]>>>>>>>>>+++++++++++++++[["
  ">>>>>>>>>]+>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+["
  ">+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>[-<<<<+>>>>]<<<<[->>>>+<<<<<[->>["
  "-<<+>>]<<[->>+>>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<"
  "<<[>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<"
  "[>[-]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+<<<<<<<<<]>>>>>"
  ">>>>[>+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+"
  "<<<<<<[->>>[-<<<+>>>]<<<[->>>+>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>"
  ">>>>>>>]<<<<<<<<<[>>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<<]>>[->>>>>>>>>+<<<<<<<<<]<<"
  "+>>>>>>>>]<<<<<<<<<[>[-]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<"
  "<]<+<<<<<<<<<]>>>>>>>>>[>>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>>"
  ">>>>>>>>>>>>>>>>>>>>>>>]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>>"
  ">>>>>]<<<<<<<<<-<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+>>>>>>>>>>>>>>>>>>>>>+<<<[<<<<<<"
  "<<<]>>>>>>>>>[>>>[-<<<->>>]+<<<[->>>->[-<<<<+>>>>]<<<<[->>>>+<<<<<<<<<<<<<[<<<<<"
  "<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>[-<<<<->>>>]+<<<<[->>>>-<[-<<<+>>>]<<<[->"
  ">>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<"
  "<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]<<<<<<<[->+>>>-<<<<]>>>>>>>>>+++++++++++++++++++"
  "+++++++>>[-<<<<+>>>>]<<<<[->>>>+<<[-]<<]>>[<<<<<<<+<[-<+>>>>+<<[-]]>[-<<[->+>>>-"
  "<<<<]>>>]>>>>>>>>>>>>>[>>[-]>[-]>[-]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-]>>>>>>[>>>>>"
  "[-<<<<+>>>>]<<<<[->>>>+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>[-<<<<<<<<"
  "<+>>>>>>>>>]>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>>>>>>>]+>[-"
  "]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+[>+>>>>>>>>]<<<"
  "<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+<<<<<<[->>[-<<+>>]<"
  "<[->>+>+<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[>[->>>>"
  ">>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<[>[-]<->>>"
  "[-<<<+>[<->-<<<<<<<+>>>>>>>]<[->+<]>>>]<<[->>+<<]<+<<<<<<<<<]>>>>>>>>>[>>>>>>[-<"
  "<<<<+>>>>>]<<<<<[->>>>>+<<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>+>>>>>>>>"
  "]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+<<<<<<[->>[-<<+"
  ">>]<<[->>+>>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[>"
  "[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<[>[-"
  "]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+<<<<<<<<<]>>>>>>>>>"
  "[>>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
  "]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>"
  ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>]>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>++++++++"
  "+++++++[[>>>>>>>>>]<<<<<<<<<-<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+[>>>>>>>>[-<<<<<<<+"
  ">>>>>>>]<<<<<<<[->>>>>>>+<<<<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>>["
  "-]>>>]<<<<<<<<<[<<<<<<<<<]>>>>+>[-<-<<<<+>>>>>]>[-<<<<<<[->>>>>+<++<<<<]>>>>>[-<"
  "<<<<+>>>>>]<->+>]<[->+<]<<<<<[->>>>>+<<<<<]>>>>>>[-]<<<<<<+>>>>[-<<<<->>>>]+<<<<"
  "[->>>>->>>>>[>>[-<<->>]+<<[->>->[-<<<+>>>]<<<[->>>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-]"
  "+>>>>>>[>>>>>>>>>]>+<]]+>>>[-<<<->>>]+<<<[->>>-<[-<<+>>]<<[->>+<<<<<<<<<<<[<<<<<"
  "<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<<<<"
  "[<<<<<<<<<]>>>>[-<<<<+>>>>]<<<<[->>>>+>>>>>[>+>>[-<<->>]<<[->>+<<]>>>>>>>>]<<<<<"
  "<<<+<[>[->>>>>+<<<<[->>>>-<<<<<<<<<<<<<<+>>>>>>>>>>>[->>>+<<<]<]>[->>>-<<<<<<<<<"
  "<<<<<+>>>>>>>>>>>]<<]>[->>>>+<<<[->>>-<<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>>+<<<]<<"
  "<<<<<<<<<<]>>>>[-]<<<<]>>>[-<<<+>>>]<<<[->>>+>>>>>>[>+>[-<->]<[->+<]>>>>>>>>]<<<"
  "<<<<<+<[>[->>>>>+<<<[->>>-<<<<<<<<<<<<<<+>>>>>>>>>>[->>>>+<<<<]>]<[->>>>-<<<<<<<"
  "<<<<<<<+>>>>>>>>>>]<]>>[->>>+<<<<[->>>>-<<<<<<<<<<<<<<+>>>>>>>>>>]>]<[->>>>+<<<<"
  "]<<<<<<<<<<<]>>>>>>+<<<<<<]]>>>>[-<<<<+>>>>]<<<<[->>>>+>>>>>[>>>>>>>>>]<<<<<<<<<"
  "[>[->>>>>+<<<<[->>>>-<<<<<<<<<<<<<<+>>>>>>>>>>>[->>>+<<<]<]>[->>>-<<<<<<<<<<<<<<"
  "+>>>>>>>>>>>]<<]>[->>>>+<<<[->>>-<<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>>+<<<]<<<<<<<"
  "<<<<<]]>[-]>>[-]>[-]>>>>>[>>[-]>[-]>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>[-<"
  "<<<+>>>>]<<<<[->>>>+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++["
  "[>>>>>>>>>]+>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+"
  "[>+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>[-<<<<+>>>>]<<<<[->>>>+<<<<<[->>"
  "[-<<+>>]<<[->>+>+<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<<"
  "<[>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<["
  ">[-]<->>>[-<<<+>[<->-<<<<<<<+>>>>>>>]<[->+<]>>>]<<[->>+<<]<+<<<<<<<<<]>>>>>>>>>["
  ">>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>]>"
  ">>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>[-]>>>>+++++++++++++++[[>>>>>>>>>]<<<<<<<<<-<<<<<"
  "<<<<[<<<<<<<<<]>>>>>>>>>-]+[>>>[-<<<->>>]+<<<[->>>->[-<<<<+>>>>]<<<<[->>>>+<<<<<"
  "<<<<<<<<[<<<<<<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>[-<<<<->>>>]+<<<<[->>>>-<[-"
  "<<<+>>>]<<<[->>>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>"
  ">>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-<<<+>>>]<<<[->>>+>>>>>>[>+>>>"
  "[-<<<->>>]<<<[->>>+<<<]>>>>>>>>]<<<<<<<<+<[>[->+>[-<-<<<<<<<<<<+>>>>>>>>>>>>[-<<"
  "+>>]<]>[-<<-<<<<<<<<<<+>>>>>>>>>>>>]<<<]>>[-<+>>[-<<-<<<<<<<<<<+>>>>>>>>>>>>]<]>"
  "[-<<+>>]<<<<<<<<<<<<<]]>>>>[-<<<<+>>>>]<<<<[->>>>+>>>>>[>+>>[-<<->>]<<[->>+<<]>>"
  ">>>>>>]<<<<<<<<+<[>[->+>>[-<<-<<<<<<<<<<+>>>>>>>>>>>[-<+>]>]<[-<-<<<<<<<<<<+>>>>"
  ">>>>>>>]<<]>>>[-<<+>[-<-<<<<<<<<<<+>>>>>>>>>>>]>]<[-<+>]<<<<<<<<<<<<]>>>>>+<<<<<"
  "]>>>>>>>>>[>>>[-]>[-]>[-]>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-]>[-]>>>>>[>>>>>>>[-<<<<<"
  "<+>>>>>>]<<<<<<[->>>>>>+<<<<+<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>+>[-<-<<<<+>>>>"
  ">]>>[-<<<<<<<[->>>>>+<++<<<<]>>>>>[-<<<<<+>>>>>]<->+>>]<<[->>+<<]<<<<<[->>>>>+<<"
  "<<<]+>>>>[-<<<<->>>>]+<<<<[->>>>->>>>>[>>>[-<<<->>>]+<<<[->>>-<[-<<+>>]<<[->>+<<"
  "<<<<<<<<<[<<<<<<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>[-<<->>]+<<[->>->[-<<<+>>>]<"
  "<<[->>>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<"
  "<<<<<<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-<<<+>>>]<<<[->>>+>>>>>>[>+>[-<->]<[->+"
  "<]>>>>>>>>]<<<<<<<<+<[>[->>>>+<<[->>-<<<<<<<<<<<<<+>>>>>>>>>>[->>>+<<<]>]<[->>>-"
  "<<<<<<<<<<<<<+>>>>>>>>>>]<]>>[->>+<<<[->>>-<<<<<<<<<<<<<+>>>>>>>>>>]>]<[->>>+<<<"
  "]<<<<<<<<<<<]>>>>>[-]>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<<<]]>>>>[-<<<<+>"
  ">>>]<<<<[->>>>+>>>>>[>+>>[-<<->>]<<[->>+<<]>>>>>>>>]<<<<<<<<+<[>[->>>>+<<<[->>>-"
  "<<<<<<<<<<<<<+>>>>>>>>>>>[->>+<<]<]>[->>-<<<<<<<<<<<<<+>>>>>>>>>>>]<<]>[->>>+<<["
  "->>-<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>+<<]<<<<<<<<<<<<]]>>>>[-]<<<<]>>>>[-<<<<+>>"
  ">>]<<<<[->>>>+>[-]>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<<<]>>>>>>>>>[>>>>>>"
  ">>>]<<<<<<<<<[>[->>>>+<<<[->>>-<<<<<<<<<<<<<+>>>>>>>>>>>[->>+<<]<]>[->>-<<<<<<<<"
  "<<<<<+>>>>>>>>>>>]<<]>[->>>+<<[->>-<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>+<<]<<<<<<<<"
  "<<<<]]>>>>>>>>>[>>[-]>[-]>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-]>[-]>>>>>[>>>>>[-<<<<+"
  ">>>>]<<<<[->>>>+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>>[-<<<<<+>>>>>"
  "]<<<<<[->>>>>+<<<+<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>>"
  ">>>>>]+>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+[>+>>"
  ">>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>[-<<<<+>>>>]<<<<[->>>>+<<<<<[->>[-<<+"
  ">>]<<[->>+>>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[>"
  "[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<[>[-"
  "]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+<<<<<<<<<]>>>>>>>>>"
  "[>+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+<<<<"
  "<<[->>>[-<<<+>>>]<<<[->>>+>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>"
  ">>>]<<<<<<<<<[>>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<<]>>[->>>>>>>>>+<<<<<<<<<]<<+>>>"
  ">>>>>]<<<<<<<<<[>[-]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+"
  "<<<<<<<<<]>>>>>>>>>[>>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>>>>>>"
  ">>>>>>>>>>>>>>>>>>>]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>>>>>>"
  ">]<<<<<<<<<-<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+>>>>>>>>>>>>>>>>>>>>>+<<<[<<<<<<<<<]"
  ">>>>>>>>>[>>>[-<<<->>>]+<<<[->>>->[-<<<<+>>>>]<<<<[->>>>+<<<<<<<<<<<<<[<<<<<<<<<"
  "]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>[-<<<<->>>>]+<<<<[->>>>-<[-<<<+>>>]<<<[->>>+<"
  "<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]>"
  ">>>>>>>]<<<<<<<<<[<<<<<<<<<]>>->>[-<<<<+>>>>]<<<<[->>>>+<<[-]<<]>>]<<+>>>>[-<<<<"
  "->>>>]+<<<<[->>>>-<<<<<<.>>]>>>>[-<<<<<<<.>>>>>>>]<<<[-]>[-]>[-]>[-]>[-]>[-]>>>["
  ">[-]>[-]>[-]>[-]>[-]>[-]>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>[-]>>>>]<<<<<<<<<"
  "[<<<<<<<<<]>+++++++++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>+>>>>>>>>>+<<<<<<<<"
  "<<<<<<[<<<<<<<<<]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+[-]>>[>>>>>>>>>]<<<<<"
  "<<<<[>>>>>>>[-<<<<<<+>>>>>>]<<<<<<[->>>>>>+<<<<<<<[<<<<<<<<<]>>>>>>>[-]+>>>]<<<<"
  "<<<<<<]]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+>>[>+>>>>[-<<<<->>>>]<<<<[->>>"
  ">+<<<<]>>>>>>>>]<<+<<<<<<<[>>>>>[->>+<<]<<<<<<<<<<<<<<]>>>>>>>>>[>>>>>>>>>]<<<<<"
  "<<<<[>[-]<->>>>>>>[-<<<<<<<+>[<->-<<<+>>>]<[->+<]>>>>>>>]<<<<<<[->>>>>>+<<<<<<]<"
  "+<<<<<<<<<]>>>>>>>-<<<<[-]+<<<]+>>>>>>>[-<<<<<<<->>>>>>>]+<<<<<<<[->>>>>>>->>[>>"
  ">>>[->>+<<]>>>>]<<<<<<<<<[>[-]<->>>>>>>[-<<<<<<<+>[<->-<<<+>>>]<[->+<]>>>>>>>]<<"
  "<<<<[->>>>>>+<<<<<<]<+<<<<<<<<<]>+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>+<<<"
  "<<[<<<<<<<<<]>>>>>>>>>[>>>>>[-<<<<<->>>>>]+<<<<<[->>>>>->>[-<<<<<<<+>>>>>>>]<<<<"
  "<<<[->>>>>>>+<<<<<<<<<<<<<<<<[<<<<<<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>>>>[-<"
  "<<<<<<->>>>>>>]+<<<<<<<[->>>>>>>-<<[-<<<<<+>>>>>]<<<<<[->>>>>+<<<<<<<<<<<<<<[<<<"
  "<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<<"
  "<<[<<<<<<<<<]>>>>[-]<<<+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>-<<<<<[<<<<<<<"
  "<<]]>>>]<<<<.>>>>>>>>>>[>>>>>>[-]>>>]<<<<<<<<<[<<<<<<<<<]>++++++++++[-[->>>>>>>>"
  ">+<<<<<<<<<]>>>>>>>>>]>>>>>+>>>>>>>>>+<<<<<<<<<<<<<<<[<<<<<<<<<]>>>>>>>>[-<<<<<<"
  "<<+>>>>>>>>]<<<<<<<<[->>>>>>>>+[-]>[>>>>>>>>>]<<<<<<<<<[>>>>>>>>[-<<<<<<<+>>>>>>"
  ">]<<<<<<<[->>>>>>>+<<<<<<<<[<<<<<<<<<]>>>>>>>>[-]+>>]<<<<<<<<<<]]>>>>>>>>[-<<<<<"
  "<<<+>>>>>>>>]<<<<<<<<[->>>>>>>>+>[>+>>>>>[-<<<<<->>>>>]<<<<<[->>>>>+<<<<<]>>>>>>"
  ">>]<+<<<<<<<<[>>>>>>[->>+<<]<<<<<<<<<<<<<<<]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[>[-]<-"
  ">>>>>>>>[-<<<<<<<<+>[<->-<<+>>]<[->+<]>>>>>>>>]<<<<<<<[->>>>>>>+<<<<<<<]<+<<<<<<"
  "<<<]>>>>>>>>-<<<<<[-]+<<<]+>>>>>>>>[-<<<<<<<<->>>>>>>>]+<<<<<<<<[->>>>>>>>->[>>>"
  ">>>[->>+<<]>>>]<<<<<<<<<[>[-]<->>>>>>>>[-<<<<<<<<+>[<->-<<+>>]<[->+<]>>>>>>>>]<<"
  "<<<<<[->>>>>>>+<<<<<<<]<+<<<<<<<<<]>+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>"
  "+>>>>>>>>>>>>>>>>>>>>>>>>>>>+<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>>[-<<<<<<->>>>>>]+<"
  "<<<<<[->>>>>>->>[-<<<<<<<<+>>>>>>>>]<<<<<<<<[->>>>>>>>+<<<<<<<<<<<<<<<<<[<<<<<<<"
  "<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>>>>>[-<<<<<<<<->>>>>>>>]+<<<<<<<<[->>>>>>>>"
  "-<<[-<<<<<<+>>>>>>]<<<<<<[->>>>>>+<<<<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>"
  ">>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>[-]<<<++++"
  "+[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>->>>>>>>>>>>>>>>>>>>>>>>>>>>-<<<<<<[<<<<"
  "<<<<<]]>>>]";

const char* const x_expectedOutput =
  "AAAAAAAAAAAAAAAABBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDEGFFEEEEDDDDDDCCCCCCCCCBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAAAAAABBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDEEEFGIIGFFEEEDDDDDDDDCCCCCCCCCBBBBBBBBBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAAAABBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDEEEEFFFI KHGGGHGEDDDDDDDDDCCCCCCCCCBBBBBBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAAABBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDEEEEEFFGHIMTKLZOGFEEDDDDDDDDDCCCCCCCCCBBBBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAABBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDEEEEEEFGGHHIKPPKIHGFFEEEDDDDDDDDDCCCCCCCCCCBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAABBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDEEEEEEFFGHIJKS  X KHHGFEEEEEDDDDDDDDDCCCCCCCCCCBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAABBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDEEEEEEFFGQPUVOTY   ZQL[MHFEEEEEEEDDDDDDDCCCCCCCCCCCBBBBBBBBBBBBBB\n"
  "AAAAAAAABBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDEEEEEFFFFFGGHJLZ         UKHGFFEEEEEEEEDDDDDCCCCCCCCCCCCBBBBBBBBBBBB\n"
  "AAAAAAABBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDEEEEFFFFFFGGGGHIKP           KHHGGFFFFEEEEEEDDDDDCCCCCCCCCCCBBBBBBBBBBB\n"
  "AAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDEEEEEFGGHIIHHHHHIIIJKMR        VMKJIHHHGFFFFFFGSGEDDDDCCCCCCCCCCCCBBBBBBBBB\n"
  "AAAAAABBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDEEEEEEFFGHK   MKJIJO  N R  X      YUSR PLV LHHHGGHIOJGFEDDDCCCCCCCCCCCCBBBBBBBB\n"
  "AAAAABBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDEEEEEEEEEFFFFGH O    TN S                       NKJKR LLQMNHEEDDDCCCCCCCCCCCCBBBBBBB\n"
  "AAAAABBCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDEEEEEEEEEEEEFFFFFGHHIN                                 Q     UMWGEEEDDDCCCCCCCCCCCCBBBBBB\n"
  "AAAABBCCCCCCCCCCCCCCCCCCCCCCCCCDDDDEEEEEEEEEEEEEEEFFFFFFGHIJKLOT                                     [JGFFEEEDDCCCCCCCCCCCCCBBBBB\n"
  "AAAABCCCCCCCCCCCCCCCCCCCCCCDDDDEEEEEEEEEEEEEEEEFFFFFFGGHYV RQU                                     QMJHGGFEEEDDDCCCCCCCCCCCCCBBBB\n"
  "AAABCCCCCCCCCCCCCCCCCDDDDDDDEEFJIHFFFFFFFFFFFFFFGGGGGGHIJN                                            JHHGFEEDDDDCCCCCCCCCCCCCBBB\n"
  "AAABCCCCCCCCCCCDDDDDDDDDDEEEEFFHLKHHGGGGHHMJHGGGGGGHHHIKRR                                           UQ L HFEDDDDCCCCCCCCCCCCCCBB\n"
  "AABCCCCCCCCDDDDDDDDDDDEEEEEEFFFHKQMRKNJIJLVS JJKIIIIIIJLR                                               YNHFEDDDDDCCCCCCCCCCCCCBB\n"
  "AABCCCCCDDDDDDDDDDDDEEEEEEEFFGGHIJKOU  O O   PR LLJJJKL                                                OIHFFEDDDDDCCCCCCCCCCCCCCB\n"
  "AACCCDDDDDDDDDDDDDEEEEEEEEEFGGGHIJMR              RMLMN                                                 NTFEEDDDDDDCCCCCCCCCCCCCB\n"
  "AACCDDDDDDDDDDDDEEEEEEEEEFGGGHHKONSZ                QPR                                                NJGFEEDDDDDDCCCCCCCCCCCCCC\n"
  "ABCDDDDDDDDDDDEEEEEFFFFFGIPJIIJKMQ                   VX                                                 HFFEEDDDDDDCCCCCCCCCCCCCC\n"
  "ACDDDDDDDDDDEFFFFFFFGGGGHIKZOOPPS                                                                      HGFEEEDDDDDDCCCCCCCCCCCCCC\n"
  "ADEEEEFFFGHIGGGGGGHHHHIJJLNY                                                                        TJHGFFEEEDDDDDDDCCCCCCCCCCCCC\n"
  "A                                                                                                 PLJHGGFFEEEDDDDDDDCCCCCCCCCCCCC\n"
  "ADEEEEFFFGHIGGGGGGHHHHIJJLNY                                                                        TJHGFFEEEDDDDDDDCCCCCCCCCCCCC\n"
  "ACDDDDDDDDDDEFFFFFFFGGGGHIKZOOPPS                                                                      HGFEEEDDDDDDCCCCCCCCCCCCCC\n"
  "ABCDDDDDDDDDDDEEEEEFFFFFGIPJIIJKMQ                   VX                                                 HFFEEDDDDDDCCCCCCCCCCCCCC\n"
  "AACCDDDDDDDDDDDDEEEEEEEEEFGGGHHKONSZ                QPR                                                NJGFEEDDDDDDCCCCCCCCCCCCCC\n"
  "AACCCDDDDDDDDDDDDDEEEEEEEEEFGGGHIJMR              RMLMN                                                 NTFEEDDDDDDCCCCCCCCCCCCCB\n"
  "AABCCCCCDDDDDDDDDDDDEEEEEEEFFGGHIJKOU  O O   PR LLJJJKL                                                OIHFFEDDDDDCCCCCCCCCCCCCCB\n"
  "AABCCCCCCCCDDDDDDDDDDDEEEEEEFFFHKQMRKNJIJLVS JJKIIIIIIJLR                                               YNHFEDDDDDCCCCCCCCCCCCCBB\n"
  "AAABCCCCCCCCCCCDDDDDDDDDDEEEEFFHLKHHGGGGHHMJHGGGGGGHHHIKRR                                           UQ L HFEDDDDCCCCCCCCCCCCCCBB\n"
  "AAABCCCCCCCCCCCCCCCCCDDDDDDDEEFJIHFFFFFFFFFFFFFFGGGGGGHIJN                                            JHHGFEEDDDDCCCCCCCCCCCCCBBB\n"
  "AAAABCCCCCCCCCCCCCCCCCCCCCCDDDDEEEEEEEEEEEEEEEEFFFFFFGGHYV RQU                                     QMJHGGFEEEDDDCCCCCCCCCCCCCBBBB\n"
  "AAAABBCCCCCCCCCCCCCCCCCCCCCCCCCDDDDEEEEEEEEEEEEEEEFFFFFFGHIJKLOT                                     [JGFFEEEDDCCCCCCCCCCCCCBBBBB\n"
  "AAAAABBCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDEEEEEEEEEEEEFFFFFGHHIN                                 Q     UMWGEEEDDDCCCCCCCCCCCCBBBBBB\n"
  "AAAAABBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDEEEEEEEEEFFFFGH O    TN S                       NKJKR LLQMNHEEDDDCCCCCCCCCCCCBBBBBBB\n"
  "AAAAAABBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDEEEEEEFFGHK   MKJIJO  N R  X      YUSR PLV LHHHGGHIOJGFEDDDCCCCCCCCCCCCBBBBBBBB\n"
  "AAAAAAABBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDEEEEEFGGHIIHHHHHIIIJKMR        VMKJIHHHGFFFFFFGSGEDDDDCCCCCCCCCCCCBBBBBBBBB\n"
  "AAAAAAABBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDEEEEFFFFFFGGGGHIKP           KHHGGFFFFEEEEEEDDDDDCCCCCCCCCCCBBBBBBBBBBB\n"
  "AAAAAAAABBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDEEEEEFFFFFGGHJLZ         UKHGFFEEEEEEEEDDDDDCCCCCCCCCCCCBBBBBBBBBBBB\n"
  "AAAAAAAAABBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDEEEEEEFFGQPUVOTY   ZQL[MHFEEEEEEEDDDDDDDCCCCCCCCCCCBBBBBBBBBBBBBB\n"
  "AAAAAAAAAABBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDEEEEEEFFGHIJKS  X KHHGFEEEEEDDDDDDDDDCCCCCCCCCCBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAABBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDEEEEEEFGGHHIKPPKIHGFFEEEDDDDDDDDDCCCCCCCCCCBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAAABBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDEEEEEFFGHIMTKLZOGFEEDDDDDDDDDCCCCCCCCCBBBBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAAAABBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDEEEEFFFI KHGGGHGEDDDDDDDDDCCCCCCCCCBBBBBBBBBBBBBBBBBBBBBBB\n"
  "AAAAAAAAAAAAAAABBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDEEEFGIIGFFEEEDDDDDDDDCCCCCCCCCBBBBBBBBBBBBBBBBBBBBBBBBBB\n";

void CreateFunction(std::string fnName)
{
    PaperMicrobenchmarkBrainfxxkCompiler::CreateFunction(fnName, x_program);
}

void SetupModule()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");
    CreateFunction("bf_mandelbrot");
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

double TimeFastInterpCodegenTime()
{
    double ts;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }
    return ts;
}

TestJitHelper RunLLVMCodegenForExecution(int optLevel)
{
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    TestJitHelper jit;
    thread_pochiVMContext->m_curModule->EmitIR();
    jit.Init(optLevel);
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("bf_mandelbrot");
    std::ignore = jitFn;
    return jit;
}

std::pair<TestJitHelper, double> TimeLLVMCodegenTime(int optLevel)
{
    double ts;
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        // Important to call getFunction here:
        // The actual LLVM codegen work is done in GetFunction, so must be timed
        //
        jitFn = jit.GetFunction<FnPrototype>("bf_mandelbrot");
        std::ignore = jitFn;
    }
    return std::make_pair(std::move(jit), ts);
}

double TimeFastInterpPerformance()
{
    double ts;
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
            GetFastInterpGeneratedFunction<FnPrototype>("bf_mandelbrot");

    uint8_t input[] = { static_cast<uint8_t>(255) };

    uint8_t output[8192];
    uint8_t pad[30000];
    memset(pad, 0, 30000);

    size_t outputLength;
    {
        AutoTimer t(&ts);
        outputLength = interpFn(pad, input, output);
    }

    ReleaseAssert(outputLength < 8192);
    std::string result = "";
    for (size_t i = 0; i < outputLength; i++)
    {
        result += static_cast<char>(output[i]);
    }
    ReleaseAssert(result == x_expectedOutput);

    return ts;
}

double TimeLLVMPerformance(TestJitHelper& jit)
{
    double ts;
    using FnPrototype = size_t(*)(uint8_t* /*pad*/, uint8_t* /*input*/, uint8_t* /*output*/) noexcept;
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("bf_mandelbrot");

    uint8_t input[] = { static_cast<uint8_t>(255) };

    uint8_t output[8192];
    uint8_t pad[30000];
    memset(pad, 0, 30000);

    size_t outputLength;
    {
        AutoTimer t(&ts);
        outputLength = jitFn(pad, input, output);
    }

    ReleaseAssert(outputLength < 8192);
    std::string result = "";
    for (size_t i = 0; i < outputLength; i++)
    {
        result += static_cast<char>(output[i]);
    }
    ReleaseAssert(result == x_expectedOutput);

    return ts;
}

}   // namespace PaperMicrobenchmarkBrainfxxkMandelbrot

TEST(PAPER_MICROBENCHMARK_TEST_PREFIX, BrainfxxkMandelbrot)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    using namespace PaperMicrobenchmarkBrainfxxkMandelbrot;

    double fastinterpCodegenTime = GetBestResultOfRuns([]() {
        SetupModule();
        return TimeFastInterpCodegenTime();
    });

    SetupModule();
    TimeFastInterpCodegenTime();
    double fastInterpPerformance = GetBestResultOfRuns([]() {
        return TimeFastInterpPerformance();
    });

    double llvmCodegenTime[4], llvmPerformance[4];
    for (int optLevel = 0; optLevel <= 3; optLevel ++)
    {
        llvmCodegenTime[optLevel] = GetBestResultOfRuns([&]() {
            SetupModule();
            auto result = TimeLLVMCodegenTime(optLevel);
            return result.second;
        });

        SetupModule();
        TestJitHelper jit = RunLLVMCodegenForExecution(optLevel);
        llvmPerformance[optLevel] = GetBestResultOfRuns([&]() {
            return TimeLLVMPerformance(jit);
        }, 5 /*numRuns*/);
    }

    printf("******* BF Mandelbrot Microbenchmark *******\n");
    printf("Program Length: %d bytes\n", static_cast<int>(strlen(x_program)));

    printf("==============================\n");
    printf("   Codegen Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastinterpCodegenTime);
    printf("LLVM -O0:   %.7lf\n", llvmCodegenTime[0]);
    printf("LLVM -O1:   %.7lf\n", llvmCodegenTime[1]);
    printf("LLVM -O2:   %.7lf\n", llvmCodegenTime[2]);
    printf("LLVM -O3:   %.7lf\n", llvmCodegenTime[3]);
    printf("==============================\n\n");

    printf("==============================\n");
    printf("  Execution Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpPerformance);
    printf("LLVM -O0:   %.7lf\n", llvmPerformance[0]);
    printf("LLVM -O1:   %.7lf\n", llvmPerformance[1]);
    printf("LLVM -O2:   %.7lf\n", llvmPerformance[2]);
    printf("LLVM -O3:   %.7lf\n", llvmPerformance[3]);
    printf("==============================\n");
}

namespace PaperMicrobenchmarkAMillionIncrement
{

void CreateFunction(std::string fnName)
{
    using FnPrototype = int(*)(int, int) noexcept;
    auto [fn, a, b] = NewFunction<FnPrototype>(fnName);

    fn.SetBody();
    for (int i = 0; i < 1000000; i++)
    {
        fn.GetBody().Append(Assign(a, a + b));
    }
    fn.GetBody().Append(Return(a));
}

void SetupModule()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");
    CreateFunction("largefn");
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

double TimeFastInterpCodegenTime()
{
    double ts;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }
    return ts;
}

TestJitHelper RunLLVMCodegenForExecution(int optLevel)
{
    using FnPrototype = int(*)(int, int) noexcept;
    TestJitHelper jit;
    thread_pochiVMContext->m_curModule->EmitIR();
    jit.Init(optLevel);
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("largefn");
    std::ignore = jitFn;
    return jit;
}

std::pair<TestJitHelper, double> TimeLLVMCodegenTime(int optLevel)
{
    double ts;
    using FnPrototype = int(*)(int, int) noexcept;
    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        // Important to call getFunction here:
        // The actual LLVM codegen work is done in GetFunction, so must be timed
        //
        jitFn = jit.GetFunction<FnPrototype>("largefn");
        std::ignore = jitFn;
    }
    return std::make_pair(std::move(jit), ts);
}

double TimeFastInterpPerformance()
{
    double ts;
    using FnPrototype = int(*)(int, int) noexcept;
    FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
            GetFastInterpGeneratedFunction<FnPrototype>("largefn");

    {
        AutoTimer t(&ts);
        ReleaseAssert(interpFn(1, 2) == 2000001);
    }

    return ts;
}

double TimeLLVMPerformance(TestJitHelper& jit)
{
    double ts;
    using FnPrototype = int(*)(int, int) noexcept;
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("largefn");

    {
        AutoTimer t(&ts);
        ReleaseAssert(jitFn(1, 2) == 2000001);
    }

    return ts;
}

}   // PaperMicrobenchmarkAMillionIncrement

TEST(PAPER_MICROBENCHMARK_TEST_PREFIX, AMillionIncrement)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    using namespace PaperMicrobenchmarkAMillionIncrement;

    double fastinterpCodegenTime = GetBestResultOfRuns([]() {
        SetupModule();
        return TimeFastInterpCodegenTime();
    });

    SetupModule();
    TimeFastInterpCodegenTime();
    double fastInterpPerformance = GetBestResultOfRuns([]() {
        return TimeFastInterpPerformance();
    });

    double llvmCodegenTime[4], llvmPerformance[4];
    for (int optLevel = 0; optLevel <= 3; optLevel ++)
    {
        llvmCodegenTime[optLevel] = GetBestResultOfRuns([&]() {
            SetupModule();
            auto result = TimeLLVMCodegenTime(optLevel);
            return result.second;
        }, 1 /*numRuns*/);

        SetupModule();
        TestJitHelper jit = RunLLVMCodegenForExecution(optLevel);
        llvmPerformance[optLevel] = GetBestResultOfRuns([&]() {
            return TimeLLVMPerformance(jit);
        });
    }

    printf("******* A million increment Microbenchmark *******\n");

    printf("==============================\n");
    printf("   Codegen Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastinterpCodegenTime);
    printf("LLVM -O0:   %.7lf\n", llvmCodegenTime[0]);
    printf("LLVM -O1:   %.7lf\n", llvmCodegenTime[1]);
    printf("LLVM -O2:   %.7lf\n", llvmCodegenTime[2]);
    printf("LLVM -O3:   %.7lf\n", llvmCodegenTime[3]);
    printf("==============================\n\n");

    printf("==============================\n");
    printf("  Execution Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpPerformance);
    printf("LLVM -O0:   %.7lf\n", llvmPerformance[0]);
    printf("LLVM -O1:   %.7lf\n", llvmPerformance[1]);
    printf("LLVM -O2:   %.7lf\n", llvmPerformance[2]);
    printf("LLVM -O3:   %.7lf\n", llvmPerformance[3]);
    printf("==============================\n");
}

namespace PaperRegexMiniExample
{

static int g_ordinal = 0;
using RegexFn = bool(*)(char* /*input*/);

Function codegen(const char* regex)
{
    auto [regexfn, input] = NewFunction<RegexFn>(std::string("match") + std::to_string(g_ordinal++));
    if (regex[0] == '\0')
    {
        regexfn.SetBody(
            Return(*input == '\0')
        );
    }
    else if (regex[1] == '+')
    {
        regexfn.SetBody(
            While(*input == regex[0]).Do(
                Assign(input, input + 1),
                If (Call<RegexFn>(codegen(regex+2), input)).Then(
                    Return(true)
                )
            ),
            Return(false)
        );
    }
    else if (regex[0] == '.')
    {
        regexfn.SetBody(
            Return(*input != '\0' && Call<RegexFn>(codegen(regex+1), input+1))
        );
    }
    else
    {
        regexfn.SetBody(
            Return(*input == *regex && Call<RegexFn>(codegen(regex+1), input+1))
        );
    }
    return regexfn;
}

void CreateFunction(const char* regex)
{
    Function regexfn = codegen(regex);
    using Regexs = int(*)(std::vector<std::string>*);
    auto [regexs, inputs] = NewFunction<Regexs>("regexs");
    auto result = regexs.NewVariable<int>();
    auto it = regexs.NewVariable<std::vector<std::string>::iterator>();
    auto end = regexs.NewVariable<std::vector<std::string>::iterator>();
    regexs.SetBody(
            Declare(result, 0),
            Declare(end, inputs->end()),
            For(Declare(it, inputs->begin()), it != end, it++).Do(
                Assign(result, result + StaticCast<int>(
                    Call<RegexFn>(regexfn, it->c_str())
                ))
            ),
            Return(result)
    );
}

void SetupModule(const char* regex)
{
    thread_pochiVMContext->m_curModule = new AstModule("test");
    CreateFunction(regex);
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

double TimeFastInterpCodegenTime()
{
    double ts;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    }
    return ts;
}

std::pair<TestJitHelper, double> TimeLLVMCodegenTime(int optLevel)
{
    double ts;
    using FnPrototype = int(*)(int, int) noexcept;
    TestJitHelper jit;
    FnPrototype jitFn;
    {
        AutoTimer t(&ts);
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        // Important to call getFunction here:
        // The actual LLVM codegen work is done in GetFunction, so must be timed
        //
        jitFn = jit.GetFunction<FnPrototype>("largefn");
        std::ignore = jitFn;
    }
    return std::make_pair(std::move(jit), ts);
}

}   // namespace PaperRegexMiniExample

TEST(PaperRegexMiniExample, Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    using namespace PaperRegexMiniExample;
    SetupModule("ab.d+e");

    using FnPrototype = int(*)(std::vector<std::string>*);

    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("regexs");

        std::vector<std::string> input { "abcde", "abcdde", "abde", "abcdef" };
        int out = interpFn(&input);
        ReleaseAssert(out == 2);
    }

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("regexs");

        std::vector<std::string> input { "abcde", "abcdde", "abde", "abcdef" };
        int out = interpFn(&input);
        ReleaseAssert(out == 2);
    }

    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);

    {
        SimpleJIT jit;
        jit.SetAllowResolveSymbolInHostProcess(true);
        jit.SetModule(thread_pochiVMContext->m_curModule);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("regexs");

        std::vector<std::string> input { "abcde", "abcdde", "abde", "abcdef" };
        int out = jitFn(&input);
        ReleaseAssert(out == 2);
    }
}
