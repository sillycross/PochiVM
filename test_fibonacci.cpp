#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"

using namespace PochiVM;

TEST(Sanity, FibonacciSeq)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = uint64_t(*)(int) noexcept;
    auto [fn, n] = NewFunction<FnPrototype>("fib_nth", "n");

    fn.SetBody(
        If(n <= Literal<int>(2)).Then(
                Return(Literal<uint64_t>(1))
        ).Else(
                Return(Call<FnPrototype>("fib_nth", n - Literal<int>(1))
                       + Call<FnPrototype>("fib_nth", n - Literal<int>(2)))
        )
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    {
        std::function<uint64_t(int)> interpFn = thread_pochiVMContext->m_curModule->
                               GetGeneratedFunctionInterpMode<std::function<uint64_t(int)>>("fib_nth");
        uint64_t ret = interpFn(20);
        ReleaseAssert(ret == 6765);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        AutoTimer timer;
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                GetFastInterpGeneratedFunction<FnPrototype>("fib_nth");

        uint64_t ret = interpFn(20);
        ReleaseAssert(ret == 6765);
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("fib_nth");
        uint64_t ret = jitFn(20);
        ReleaseAssert(ret == 6765);
    }
}
