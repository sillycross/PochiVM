#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"
#include "codegen_context.hpp"

using namespace Ast;

TEST(Sanity, FindNthPrime)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int)>;
    auto [fn, n] = NewFunction<FnPrototype>("find_nth_prime", "n");

    auto i = fn.NewVariable<int>("i");
    auto numPrimesFound = fn.NewVariable<int>("numPrimesFound");
    auto valToTest = fn.NewVariable<int>("valToTest");
    auto isPrime = fn.NewVariable<bool>("isPrime");

    fn.SetBody(
        Declare(numPrimesFound, 0),
        Declare(valToTest, 1),
        While(numPrimesFound < n).Do(
            Increment(valToTest),
            Declare(isPrime, true),
            For(
                Declare(i, 2),
                i * i <= valToTest,
                Increment(i)
            ).Do(
                If(valToTest % i == Literal<int>(0)).Then(
                    Assign(isPrime, Literal<bool>(false)),
                    Break()
                )
            ),
            If(isPrime).Then(
                Increment(numPrimesFound)
            )
        ),
        Return(valToTest)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                           GetGeneratedFunctionInterpMode<FnPrototype>("find_nth_prime");
    int ret = interpFn(1000);
    ReleaseAssert(ret == 7919);

    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}
