#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"

using namespace Ast;

TEST(SanityError, ReturnTypeMismatch)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int()>;
    auto [fn] = NewFunction<FnPrototype>("BadFn");

    fn.SetBody(Return(Literal<double>(1.2)));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Function BadFn: return type does not match function prototype. Return statement returned double, expects int32_t";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, CallPrototypeMismatch_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int)>;
    auto [fn, param] = NewFunction<FnPrototype>("CalleeFn");
    fn.SetBody();

    using BadFnPrototype = std::function<void(double)>;

    using FnPrototype2 = std::function<void(void)>;
    auto [fn2] = NewFunction<FnPrototype2>("BadFn");

    // caller use mismatched prototype to call callee
    //
    fn2.SetBody(Call<BadFnPrototype>("CalleeFn", Literal<double>(1.2)));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Call to function CalleeFn: parameter 0 has wrong type, expects int32_t got double";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, CallPrototypeMismatch_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(int)>;
    auto [fn, param] = NewFunction<FnPrototype>("CalleeFn");
    fn.SetBody();

    using BadFnPrototype = std::function<void(int, int)>;

    using FnPrototype2 = std::function<void(void)>;
    auto [fn2] = NewFunction<FnPrototype2>("BadFn");

    // caller use mismatched prototype to call callee
    //
    fn2.SetBody(Call<BadFnPrototype>("CalleeFn", Literal<int>(1), Literal<int>(1)));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Call to function CalleeFn: wrong number of parameters, expects 1 got 2";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, CallPrototypeMismatch_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(void)>;
    auto [fn] = NewFunction<FnPrototype>("CalleeFn");
    fn.SetBody(Return(Literal<int>(1)));

    using BadFnPrototype = std::function<void(void)>;

    using FnPrototype2 = std::function<void(void)>;
    auto [fn2] = NewFunction<FnPrototype2>("BadFn");

    // caller use mismatched prototype to call callee
    //
    fn2.SetBody(Call<BadFnPrototype>("CalleeFn"));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Call to function CalleeFn: wrong return type, expects int32_t got void";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, CallPrototypeMismatch_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void(void)>;
    auto [fn] = NewFunction<FnPrototype>("BadFn");

    fn.SetBody(Call<FnPrototype>("NonexistentFn"));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Call to undefined function NonexistentFn";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, ReuseAstNode)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int)>;
    auto [fn, param] = NewFunction<FnPrototype>("BadFn");

    auto x = Call<FnPrototype>("BadFn", param - Literal<int>(1));
    fn.SetBody(Return(x * x));

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Function BadFn: illegal reuse of AST node type";
    ReleaseAssert(StartsWith(thread_errorContext->m_errorMsg, expected));
}

TEST(SanityError, UseVarInOtherFn_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(void)>;
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

    const char* expected = "Function BadFn: use of local variable var_0 belonging to another function Fn1";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, UseVarInOtherFn_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(void)>;
    auto [fn] = NewFunction<FnPrototype>("Fn1");
    auto v = fn.NewVariable<int>();
    fn.SetBody();

    auto [fn2] = NewFunction<FnPrototype>("BadFn");
    fn2.SetBody(
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Function BadFn: use of local variable var_0 belonging to another function Fn1";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, UseVarInOtherFn_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(void)>;
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

    const char* expected = "Function BadFn: use of local variable var_0 belonging to another function Fn1";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, UseUndeclaredVar)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(void)>;
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        Assign(v, Literal<int>(1)),
        Return(v)
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Function BadFn: use of undeclared variable myVarName_0";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, RedeclareVar_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(void)>;
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    auto v = fn.NewVariable<int>("myVarName");
    fn.SetBody(
        Declare(v, Literal<int>(1)),
        Declare(v, Literal<int>(2)),
        Return(v)
     );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Function BadFn: re-declaration of variable myVarName_0";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, RedeclareVar_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int)>;
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

    const char* expected = "Function BadFn: re-declaration of variable myVarName_1";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, UseOutOfScopeVar_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int)>;
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

    const char* expected = "Function BadFn: use of out-of-scope variable myVarName_1";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, UseOutOfScopeVar_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int()>;
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

    const char* expected = "Function BadFn: use of out-of-scope variable myVarName_0";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, UseOutOfScopeVar_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int()>;
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

    const char* expected = "Function BadFn: use of out-of-scope variable myVarName_0";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, UseOutOfScopeVar_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int()>;
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

    const char* expected = "Function BadFn: use of out-of-scope variable myVarName_0";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(Sanity, BlockHasNoScopeEffect)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    // 'Block' construct, unlike scope, should not destroy variables
    //
    using FnPrototype = std::function<int()>;
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
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                           GetGeneratedFunctionInterpMode<FnPrototype>("GoodFn");
    int ret = interpFn();
    ReleaseAssert(ret == 3);
}

TEST(SanityError, BreakNotInLoop)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void()>;
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        Break()
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Function BadFn: use of 'Break' statement while not in a loop";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, ContinueNotInLoop)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<void()>;
    auto [fn] = NewFunction<FnPrototype>("BadFn");
    fn.SetBody(
        Continue()
    );

    ReleaseAssert(!thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(thread_errorContext->HasError());

    const char* expected = "Function BadFn: use of 'Continue' statement while not in a loop";
    ReleaseAssert(strcmp(expected, thread_errorContext->m_errorMsg) == 0);
}

TEST(SanityError, NoReturnValue)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int)>;
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
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                           GetGeneratedFunctionInterpMode<FnPrototype>("BadFn");

#ifdef TESTBUILD
    // having no return value fires a TestAssert
    // Google test somehow triggers a bunch of warnings...
    //
    printf("Expecting a TestAssert being fired...\n");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wcovered-switch-default"

    ASSERT_DEATH(interpFn(233), "");

#pragma clang diagnostic pop
#endif
    std::ignore = interpFn;
}
