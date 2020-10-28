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
