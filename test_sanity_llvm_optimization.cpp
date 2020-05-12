#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"

using namespace Ast;

TEST(Sanity, LLVMOptimizationPassEffective)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    // Sanity check that LLVM function-level and module-level optimization are working.
    // Specifically, we want to see expression gets simplified, and function gets inlined
    //
    using FnPrototype = std::function<int(int)>;
    {
        auto [fn, a] = NewFunction<FnPrototype>("a_plus_10", "a");
        fn.SetBody(
                Increment(a),
                Increment(a),
                Increment(a),
                Increment(a),
                Increment(a),
                Increment(a),
                Increment(a),
                Increment(a),
                Increment(a),
                Increment(a),
                Return(a)
        );
    }

    {
        auto [fn, a] = NewFunction<FnPrototype>("a_plus_50", "a");
        fn.SetBody(
                Assign(a, Call<FnPrototype>("a_plus_10", a)),
                Assign(a, Call<FnPrototype>("a_plus_10", a)),
                Assign(a, Call<FnPrototype>("a_plus_10", a)),
                Assign(a, Call<FnPrototype>("a_plus_10", a)),
                Assign(a, Call<FnPrototype>("a_plus_10", a)),
                Return(a)
         );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_10");
        ReleaseAssert(jitFn(233) == 233 + 10);
    }

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("a_plus_50");
        ReleaseAssert(jitFn(233) == 233 + 50);
    }
}
