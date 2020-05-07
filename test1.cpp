#include "gtest/gtest.h"

#include "pochivm.h"

using namespace Ast;

TEST(Sanity, FindNthPrime)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int)>;
    auto [fn, n] = NewFunction<FnPrototype>("find_nth_prime");

    auto i = fn.NewVariable<int>("i");
    auto numPrimesFound = fn.NewVariable<int>("numPrimesFound");
    auto valToTest = fn.NewVariable<int>("valToTest");
    auto isPrime = fn.NewVariable<bool>("isPrime");

    fn.SetBody(
      Declare(numPrimesFound, 0),
      Declare(valToTest, 1),
      While(numPrimesFound.Load() < n.Load()).Do(
        Increment(valToTest),
        Declare(isPrime, true),
        For(
          Declare(i, 2),
          i.Load() * i.Load() <= valToTest.Load(),
          Increment(i)
        ).Do(
          If(valToTest.Load() % i.Load() == Literal<int>(0)).Then(
            Assign(isPrime, Literal<bool>(false)),
            Break()
          )
        ),
        If(isPrime).Then(
          Increment(numPrimesFound)
        )
      ),
      Return(valToTest.Load())
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                GetGeneratedFunctionInterpMode<FnPrototype>("find_nth_prime");
    int ret = interpFn(1000);
    ReleaseAssert(ret == 7919);
}
