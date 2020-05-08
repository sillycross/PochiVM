#include "gtest/gtest.h"

#include "pochivm.h"

using namespace Ast;

TEST(Sanity, FibonacciSeq)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<uint64_t(int)>;
    auto [fn, n] = NewFunction<FnPrototype>("fib_nth");

    fn.SetBody(
        If(n <= Literal<int>(2)).Then(
                Return(Literal<uint64_t>(1))
        ).Else(
                Return(Call<FnPrototype>("fib_nth", n - Literal<int>(1))
                       + Call<FnPrototype>("fib_nth", n - Literal<int>(2)))
        )
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                           GetGeneratedFunctionInterpMode<FnPrototype>("fib_nth");
    uint64_t ret = interpFn(20);
    ReleaseAssert(ret == 6765);
}
