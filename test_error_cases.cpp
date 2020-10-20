#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"
#include "codegen_context.hpp"

using namespace PochiVM;

TEST(SanityError, ReturnTypeMismatch)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");

    fn.SetBody(Return(Literal<double>(1.2)));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, CallPrototypeMismatch_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("CalleeFn");
    fn.SetBody();

    using BadFnPrototype = void(*)(double);

    using FnPrototype2 = void(*)();
    auto [fn2] = NewFunction<FnPrototype2>("BadFn");

    // caller use mismatched prototype to call callee
    //
    fn2.SetBody(Call<BadFnPrototype>("CalleeFn", Literal<double>(1.2)));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, CallPrototypeMismatch_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("CalleeFn");
    fn.SetBody();

    using BadFnPrototype = void(*)(int, int);

    using FnPrototype2 = void(*)();
    auto [fn2] = NewFunction<FnPrototype2>("BadFn");

    // caller use mismatched prototype to call callee
    //
    fn2.SetBody(Call<BadFnPrototype>("CalleeFn", Literal<int>(1), Literal<int>(1)));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, CallPrototypeMismatch_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("CalleeFn");
    fn.SetBody(Return(Literal<int>(1)));

    using BadFnPrototype = void(*)();

    using FnPrototype2 = void(*)();
    auto [fn2] = NewFunction<FnPrototype2>("BadFn");

    // caller use mismatched prototype to call callee
    //
    fn2.SetBody(Call<BadFnPrototype>("CalleeFn"));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, CallPrototypeMismatch_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");

    fn.SetBody(Call<FnPrototype>("NonexistentFn"));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ReuseAstNode)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");

    auto x = Call<FnPrototype>("BadFn", param - Literal<int>(1));
    fn.SetBody(Return(x * x));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseVarInOtherFn_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("Fn1");
    auto v = fn.NewVariable<int>();
    fn.SetBody(
        Declare(v, 1),
        Return(v)
    );

    auto [fn2] = NewFunction<FnPrototype>("BadFn");
    fn2.SetBody(
        Declare(v, 2),
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseVarInOtherFn_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("Fn1");
    auto v = fn.NewVariable<int>();
    fn.SetBody();

    auto [fn2] = NewFunction<FnPrototype>("BadFn");
    fn2.SetBody(
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseVarInOtherFn_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("Fn1");
    auto v = fn.NewVariable<int>();
    fn.SetBody(
        Declare(v, 1),
        Return(v)
    );

    auto [fn2] = NewFunction<FnPrototype>("BadFn");
    fn2.SetBody(
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseVarInOtherFn_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    // We validate functions in alphabetic order of their function names.
    // This time we are validating AFn1 before BadFn. The error should still be detected.
    //
    auto [fn] = NewFunction<FnPrototype>("AFn1");
    auto v = fn.NewVariable<int>();
    fn.SetBody(
        Declare(v, 1),
        Return(v)
    );

    auto [fn2] = NewFunction<FnPrototype>("BadFn");
    fn2.SetBody(
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseUndeclaredVar)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        Assign(v, Literal<int>(1)),
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, RedeclareVar_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        Declare(v, Literal<int>(1)),
        Declare(v, Literal<int>(2)),
        Return(v)
     );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, RedeclareVar_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        Declare(param, Literal<int>(1)),
        Return(param)
     );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, RedeclareVar_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        If(param > Literal<int>(0)).Then(
            Declare(v, Literal<int>(1)),
            Return(v)
        ).Else(
            Declare(v, Literal<int>(2)),
            Return(v)
        )
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityNoError, DeclareVarCornerCaseDoesNotCrash)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("TronFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        // Although we don't detect this error currently, this stupid declaration should not crash
        //
        Declare(v, v + Literal<int>(1)),
        Return(v)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("TronFn");
        std::ignore = interpFn();
    }
    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("TronFn");
        std::ignore = interpFn();
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string &dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("TronFn");
        std::ignore = jitFn();
    }
}

TEST(SanityError, UseOutOfScopeVar_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        If(param > Literal<int>(0)).Then(
            Declare(v, Literal<int>(1)),
            Return(v)
        ).Else(
            Return(v)
        )
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseOutOfScopeVar_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        Scope(
            Declare(v, 1),
            Increment(v)
        ),
        Increment(v),
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseOutOfScopeVar_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        For(Block(), Literal<bool>(true), Block()).Do(
            Declare(v, 1),
            Increment(v)
        ),
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, UseOutOfScopeVar_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        For(Declare(v, 1), Literal<bool>(true), Block()).Do(
            Increment(v)
        ),
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(Sanity, BlockHasNoScopeEffect)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    // 'Block' construct, unlike scope, should not destroy variables
    //
    using FnPrototype = int(*)();
    auto [fn] = NewFunction<FnPrototype>("GoodFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        Block(
            Declare(v, 1),
            Increment(v)
        ),
        Increment(v),
        Return(v)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("GoodFn");
        int ret = interpFn();
        ReleaseAssert(ret == 3);
    }
    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("GoodFn");
        int ret = interpFn();
        ReleaseAssert(ret == 3);
    }

    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("GoodFn");
        ReleaseAssert(jitFn() == 3);
    }
}

TEST(SanityError, BreakNotInLoop)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        Break()
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ContinueNotInLoop)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        Continue()
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, NoReturnValue)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        Block(
            Declare(v, param),
            Increment(v)
        ),
        Increment(v)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto interpFn = thread_pochiVMContext->m_curModule->
                           GetDebugInterpGeneratedFunction<FnPrototype>("BadFn");
    auto fastInterpFn = thread_pochiVMContext->m_curModule->
                           GetFastInterpGeneratedFunction<FnPrototype>("BadFn");

#ifdef TESTBUILD
    // having no return value fires a TestAssert
    // Google test somehow triggers a bunch of warnings...
    //
    printf("Expecting a TestAssert being fired...\n");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wcovered-switch-default"

    ASSERT_DEATH(interpFn(233), "");

    printf("Expecting a Illegal Instruction trap being triggered...\n");
    ASSERT_DEATH(fastInterpFn(233), "");

#pragma clang diagnostic pop
#endif
    std::ignore = interpFn;
    std::ignore = fastInterpFn;
}

TEST(SanityError, Unreachable_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)();
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        Return(),
        Call<FnPrototype>("BadFn")
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, Unreachable_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        If(param == Literal<int>(1)).Then(
            Return(Literal<int>(1))
        ).Else(
            Return(Literal<int>(2))
        ),
        Return(Call<FnPrototype>("BadFn", param))
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, Unreachable_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto i = fn.NewVariable<int>();
    fn.SetBody(
        For(Declare(i, 0), i < param, Block()).Do(
            Break(),
            Assign(i, i + Literal<int>(2))
        ),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, Unreachable_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto i = fn.NewVariable<int>();
    fn.SetBody(
        For(Declare(i, 0), i < param, Block()).Do(
            If(i % Literal<int>(2) == Literal<int>(0)).Then(
                Break()
            ).Else(
                Continue()
            ),
            Assign(i, i + Literal<int>(2))
        ),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, Unreachable_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto i = fn.NewVariable<int>();
    fn.SetBody(
        // the Increment(i) statement is unreachable
        For(Declare(i, 0), i < param, Increment(i)).Do(
            Break()
        ),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, Unreachable_6)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto i = fn.NewVariable<int>();
    fn.SetBody(
        // the Increment(i) statement is unreachable
        For(Declare(i, 0), i < param, Increment(i)).Do(
            If(i % Literal<int>(2) == Literal<int>(0)).Then(
                If(i % Literal<int>(3) == Literal<int>(0)).Then(
                    Break()
                ).Else(
                    Return(param)
                )
            ).Else(
                Break()
            )
        ),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, Unreachable_7)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        If(param == Literal<int>(1)).Then(
            Return(Literal<int>(1)),
            Assign(param, param + Literal<int>(1))
        ),
        Return(Call<FnPrototype>("BadFn", param))
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ForLoopInitStepBlockLimitation_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        For(If(param > Literal<int>(0)).Then(Break()), param > Literal<int>(0), Block()).Do(),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ForLoopInitStepBlockLimitation_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        For(Block(), param > Literal<int>(0), If(param > Literal<int>(0)).Then(Continue())).Do(),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ForLoopInitStepBlockLimitation_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        For(Block(), param > Literal<int>(0), If(param > Literal<int>(0)).Then(Return(param))).Do(),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ForLoopInitStepBlockLimitation_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        For(For(Block(), Literal<bool>(true), Block()).Do(), param > Literal<int>(0), Block()).Do(),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ForLoopInitStepBlockLimitation_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        For(Block(), param > Literal<int>(0), While(Literal<bool>(false)).Do()).Do(),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityError, ForLoopStepBlockLimitation)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");
    auto i = fn.NewVariable<int>();
    fn.SetBody(
        For(Block(), param > Literal<int>(0), Block(Declare(i, Literal<int>(1)), Increment(i))).Do(),
        Return(param)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    AssertIsExpectedOutput(thread_errorContext->m_errorMsg);
}

TEST(SanityNoError, NoUnreachable_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("GoodFn");
    fn.SetBody(
        If(param == Literal<int>(1)).Then(
            Return(Literal<int>(1))
        ),
        Return(Call<FnPrototype>("GoodFn", param))
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityNoError, NoUnreachable_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("GoodFn");
    fn.SetBody(
        If(param == Literal<int>(1)).Then(
            Return(Literal<int>(1))
        ).Else(
            Increment(param)
        ),
        Return(Call<FnPrototype>("GoodFn", param))
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityNoError, NoUnreachable_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("GoodFn");
    auto i = fn.NewVariable<int>();
    fn.SetBody(
        For(Declare(i, 0), i < param, Increment(i)).Do(
            If(i % Literal<int>(2) == Literal<int>(0)).Then(
                Break()
            ).Else(
                Continue()
            )
        ),
        Return(param)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityNoError, NoUnreachable_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("GoodFn");
    auto i = fn.NewVariable<int>();
    fn.SetBody(
        For(Declare(i, 0), i < param, Block()).Do(
            If(i % Literal<int>(2) == Literal<int>(0)).Then(
                Break()
            ).Else(
                Block(
                    Scope(
                        Break()
                    ),
                    Block(),
                    Scope()
                )
            )
        ),
        Return(param)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityNoError, NoUnreachable_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("GoodFn");
    fn.SetBody(
        While(param > Literal<int>(0)).Do(
            Increment(param),
            If(param % Literal<int>(2) == Literal<int>(0)).Then(
                Continue()
            ).Else(
                Return(param)
            )
        ),
        Return(param)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityNoError, NoUnreachable_6)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("GoodFn");
    fn.SetBody(
        While(param > Literal<int>(0)).Do(
            Increment(param),
            If(param % Literal<int>(2) == Literal<int>(0)).Then(
                Return(param)
            )
        ),
        Return(param)
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityNoError, NoUnreachable_7)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int);
    auto [fn, param] = NewFunction<FnPrototype>("GoodFn");
    fn.SetBody(
        While(param > Literal<int>(0)).Do(
            If(param % Literal<int>(2) == Literal<int>(0)).Then(
                 Return(param)
            ).Else(
                 Return(param + Literal<int>(1))
            )
        ),
        Return(param + Literal<int>(2))
    );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}
