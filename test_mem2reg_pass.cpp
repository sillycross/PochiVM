#include "gtest/gtest.h"

#include "fastinterp/fastinterp.hpp"
#include "pochivm/pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"
#include "pochivm/mem2reg_pass.h"

using namespace PochiVM;

TEST(TestMem2RegPass, Sanity_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int) noexcept;

        auto [fn, n] = NewFunction<FnPrototype>("testfn", "n");
        auto sum = fn.NewVariable<int>("sum");
        auto i = fn.NewVariable<int>("i");
        auto c = fn.NewVariable<int>("c");
        fn.SetBody(
                Declare(sum, 0),
                Declare(c, 233),
                For(Declare(i, 0), i < n, Increment(i)).Do(
                    Assign(sum, sum + i),
                    Assign(sum, sum + c)
                ),
                Return(sum)
        );


    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto checkExist = [&](const std::vector<AstRegisterCachedVariableExpr*>& list, AstVariable* var) -> bool
    {
        for (AstRegisterCachedVariableExpr* x : list)
        {
            if (x->m_variable == var) { return true; }
        }
        return false;
    };

    AstScope* s = fn.GetBody().GetPtr();
    auto sb = s->GetContents();
    ReleaseAssert(sb.size() == 1 && sb[0]->GetAstNodeType() == AstNodeType::AstBlock);
    auto l = assert_cast<AstBlock*>(sb[0])->GetContents();
    ReleaseAssert(l.size() == 4);
    ReleaseAssert(l[2]->GetAstNodeType() == AstNodeType::AstForLoop);
    AstForLoop* forLoop = assert_cast<AstForLoop*>(l[2]);
    ReleaseAssert(forLoop->m_mem2RegInitList.size() == 4);
    ReleaseAssert(checkExist(forLoop->m_mem2RegInitList, n.__pochivm_var_ptr));
    ReleaseAssert(checkExist(forLoop->m_mem2RegInitList, i.__pochivm_var_ptr));
    ReleaseAssert(checkExist(forLoop->m_mem2RegInitList, sum.__pochivm_var_ptr));
    ReleaseAssert(checkExist(forLoop->m_mem2RegInitList, c.__pochivm_var_ptr));

    ReleaseAssert(forLoop->m_mem2RegWritebackList.size() == 1);
    ReleaseAssert(forLoop->m_mem2RegWritebackList[0]->m_variable == sum.__pochivm_var_ptr);

    bool expectConverted;
    int cnt1, cnt2;
    auto traverseCheckFn = [&](AstNodeBase* cur,
                               AstNodeBase* parent,
                               FunctionRef<void(void)> Recurse)
    {
        if (cur->GetAstNodeType() == AstNodeType::AstVariable)
        {
            if (parent->GetAstNodeType() == AstNodeType::AstAssignExpr)
            {
                AstAssignExpr* expr = assert_cast<AstAssignExpr*>(parent);
                ReleaseAssert(expr->IsLhsMem2Reg() == expectConverted);
                cnt1++;
            }
            else if (parent->GetAstNodeType() != AstNodeType::AstDeclareVariable)
            {
                if (expectConverted)
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstRegisterCachedVariableExpr);
                }
                else
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr);
                }
                cnt2++;
            }
        }
        Recurse();
    };

    expectConverted = false;
    cnt1 = 0; cnt2 = 0;
    TraverseAstTree(l[0], traverseCheckFn);
    ReleaseAssert(cnt1 == 1 && cnt2 == 0);

    expectConverted = false;
    cnt1 = 0; cnt2 = 0;
    TraverseAstTree(l[1], traverseCheckFn);
    ReleaseAssert(cnt1 == 1 && cnt2 == 0);

    expectConverted = false;
    cnt1 = 0; cnt2 = 0;
    TraverseAstTree(forLoop->GetInitBlock(), traverseCheckFn);
    ReleaseAssert(cnt1 == 1 && cnt2 == 0);

    expectConverted = true;
    cnt1 = 0; cnt2 = 0;
    TraverseAstTree(forLoop->GetCondClause(), traverseCheckFn);
    ReleaseAssert(cnt1 == 0 && cnt2 == 2);

    expectConverted = true;
    cnt1 = 0; cnt2 = 0;
    TraverseAstTree(forLoop->GetBody(), traverseCheckFn);
    ReleaseAssert(cnt1 == 2 && cnt2 == 4);

    expectConverted = true;
    cnt1 = 0; cnt2 = 0;
    TraverseAstTree(forLoop->GetStepBlock(), traverseCheckFn);
    ReleaseAssert(cnt1 == 1 && cnt2 == 1);

    expectConverted = false;
    cnt1 = 0; cnt2 = 0;
    TraverseAstTree(l[3], traverseCheckFn);
    ReleaseAssert(cnt1 == 0 && cnt2 == 1);

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("testfn");
        int ret = interpFn(100);
        ReleaseAssert(ret == 50 * 99 + 233 * 100);
    }
}

// mem2reg only applies to inner-most loops
//
TEST(TestMem2RegPass, Sanity_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int) noexcept;

        auto [fn, n] = NewFunction<FnPrototype>("testfn", "n");
        auto sum = fn.NewVariable<int>("sum");
        auto i = fn.NewVariable<int>("i");
        auto j = fn.NewVariable<int>("j");
        fn.SetBody(
                Declare(sum, 0),
                For(Declare(i, 0), i < n, Increment(i)).Do(
                    For(Declare(j, 0), j < n, Increment(j)).Do(
                        Assign(sum, sum + 1)
                    )
                ),
                Return(sum)
        );


    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto checkExist = [&](const std::vector<AstRegisterCachedVariableExpr*>& list, AstVariable* var) -> bool
    {
        for (AstRegisterCachedVariableExpr* x : list)
        {
            if (x->m_variable == var) { return true; }
        }
        return false;
    };

    AstScope* s = fn.GetBody().GetPtr();
    auto sb = s->GetContents();
    ReleaseAssert(sb.size() == 1 && sb[0]->GetAstNodeType() == AstNodeType::AstBlock);
    auto l = assert_cast<AstBlock*>(sb[0])->GetContents();
    ReleaseAssert(l.size() == 3);
    ReleaseAssert(l[1]->GetAstNodeType() == AstNodeType::AstForLoop);
    AstForLoop* forLoop = assert_cast<AstForLoop*>(l[1]);
    ReleaseAssert(forLoop->m_mem2RegInitList.size() == 0);
    ReleaseAssert(forLoop->m_mem2RegWritebackList.size() == 0);

    ReleaseAssert(forLoop->GetBody()->GetContents().size() == 1);
    ReleaseAssert(forLoop->GetBody()->GetContents()[0]->GetAstNodeType() == AstNodeType::AstForLoop);

    AstForLoop* innerLoop = assert_cast<AstForLoop*>(forLoop->GetBody()->GetContents()[0]);
    ReleaseAssert(innerLoop->m_mem2RegInitList.size() == 3);
    ReleaseAssert(checkExist(innerLoop->m_mem2RegInitList, n.__pochivm_var_ptr));
    ReleaseAssert(checkExist(innerLoop->m_mem2RegInitList, sum.__pochivm_var_ptr));
    ReleaseAssert(checkExist(innerLoop->m_mem2RegInitList, j.__pochivm_var_ptr));

    ReleaseAssert(innerLoop->m_mem2RegWritebackList.size() == 1);
    ReleaseAssert(innerLoop->m_mem2RegWritebackList[0]->m_variable == sum.__pochivm_var_ptr);

    bool expectConverted;
    AstNodeBase* stopAt;
    int cnt1, cnt2;
    auto traverseCheckFn = [&](AstNodeBase* cur,
                               AstNodeBase* parent,
                               FunctionRef<void(void)> Recurse)
    {
        if (cur == stopAt)
        {
            return;
        }
        if (cur->GetAstNodeType() == AstNodeType::AstVariable)
        {
            if (parent->GetAstNodeType() == AstNodeType::AstAssignExpr)
            {
                AstAssignExpr* expr = assert_cast<AstAssignExpr*>(parent);
                ReleaseAssert(expr->IsLhsMem2Reg() == expectConverted);
                cnt1++;
            }
            else if (parent->GetAstNodeType() != AstNodeType::AstDeclareVariable)
            {
                if (expectConverted)
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstRegisterCachedVariableExpr);
                }
                else
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr);
                }
                cnt2++;
            }
        }
        Recurse();
    };

    expectConverted = false;
    cnt1 = 0; cnt2 = 0;
    stopAt = innerLoop;
    TraverseAstTree(s, traverseCheckFn);
    ReleaseAssert(cnt1 == 3 && cnt2 == 4);

    expectConverted = false;
    cnt1 = 0; cnt2 = 0;
    stopAt = nullptr;
    TraverseAstTree(innerLoop->GetInitBlock(), traverseCheckFn);
    ReleaseAssert(cnt1 == 1 && cnt2 == 0);

    expectConverted = true;
    cnt1 = 0; cnt2 = 0;
    stopAt = nullptr;
    TraverseAstTree(innerLoop->GetCondClause(), traverseCheckFn);
    ReleaseAssert(cnt1 == 0 && cnt2 == 2);

    expectConverted = true;
    cnt1 = 0; cnt2 = 0;
    stopAt = nullptr;
    TraverseAstTree(innerLoop->GetBody(), traverseCheckFn);
    ReleaseAssert(cnt1 == 1 && cnt2 == 1);

    expectConverted = true;
    cnt1 = 0; cnt2 = 0;
    stopAt = nullptr;
    TraverseAstTree(innerLoop->GetStepBlock(), traverseCheckFn);
    ReleaseAssert(cnt1 == 1 && cnt2 == 1);


    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("testfn");
        int ret = interpFn(100);
        ReleaseAssert(ret == 100 * 100);
    }
}

// mem2reg applies to variables base on their weight
//
TEST(TestMem2RegPass, Sanity_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = double(*)(int) noexcept;

        auto [fn, n] = NewFunction<FnPrototype>("testfn", "n");
        auto sum = fn.NewVariable<double>("sum");
        auto i1 = fn.NewVariable<int>("i1");
        auto i2 = fn.NewVariable<int>("i2");
        auto i3 = fn.NewVariable<int>("i3");
        auto i4 = fn.NewVariable<int>("i4");
        auto i5 = fn.NewVariable<int>("i5");
        auto i6 = fn.NewVariable<int>("i6");
        auto i7 = fn.NewVariable<int>("i7");
        auto i8 = fn.NewVariable<int>("i8");
        auto d1 = fn.NewVariable<double>("d1");
        auto d2 = fn.NewVariable<double>("d2");
        auto d3 = fn.NewVariable<double>("d3");
        auto d4 = fn.NewVariable<double>("d4");
        auto d5 = fn.NewVariable<double>("d5");
        auto d6 = fn.NewVariable<double>("d6");
        auto d7 = fn.NewVariable<double>("d7");
        auto d8 = fn.NewVariable<double>("d8");
        fn.SetBody(
                Declare(sum, static_cast<double>(0)),
                While(n > 0).Do(
                    Declare(i6, 0),
                    Increment(i6), Increment(i6), Increment(i6), Increment(i6), Increment(i6), Increment(i6), Increment(i6), Increment(i6),
                    Declare(i8, 0),
                    Increment(i8), Increment(i8), Increment(i8), Increment(i8), Increment(i8), Increment(i8), Increment(i8), Increment(i8), Increment(i8), Increment(i8),
                    Declare(i5, 0),
                    Increment(i5), Increment(i5), Increment(i5), Increment(i5), Increment(i5), Increment(i5), Increment(i5),
                    Declare(i3, 0),
                    Increment(i3), Increment(i3), Increment(i3), Increment(i3), Increment(i3),
                    Declare(i2, 0),
                    Increment(i2), Increment(i2), Increment(i2), Increment(i2),
                    Declare(i7, 0),
                    Increment(i7), Increment(i7), Increment(i7), Increment(i7), Increment(i7), Increment(i7), Increment(i7), Increment(i7), Increment(i7),
                    Declare(i1, 0),
                    Increment(i1), Increment(i1), Increment(i1),
                    Declare(i4, 0),
                    Increment(i4), Increment(i4), Increment(i4), Increment(i4), Increment(i4), Increment(i4),
                    Declare(d2, static_cast<double>(0)),
                    Assign(d2, d2 + static_cast<double>(1)), Assign(d2, d2 + static_cast<double>(1)),
                    Declare(d4, static_cast<double>(0)),
                    Assign(d4, d4 + static_cast<double>(1)), Assign(d4, d4 + static_cast<double>(1)), Assign(d4, d4 + static_cast<double>(1)),
                    Assign(d4, d4 + static_cast<double>(1)),
                    Declare(d5, static_cast<double>(0)),
                    Assign(d5, d5 + static_cast<double>(1)), Assign(d5, d5 + static_cast<double>(1)), Assign(d5, d5 + static_cast<double>(1)),
                    Assign(d5, d5 + static_cast<double>(1)), Assign(d5, d5 + static_cast<double>(1)),
                    Declare(d6, static_cast<double>(0)),
                    Assign(d6, d6 + static_cast<double>(1)), Assign(d6, d6 + static_cast<double>(1)), Assign(d6, d6 + static_cast<double>(1)),
                    Assign(d6, d6 + static_cast<double>(1)), Assign(d6, d6 + static_cast<double>(1)), Assign(d6, d6 + static_cast<double>(1)),
                    Declare(d8, static_cast<double>(0)),
                    Assign(d8, d8 + static_cast<double>(1)), Assign(d8, d8 + static_cast<double>(1)), Assign(d8, d8 + static_cast<double>(1)),
                    Assign(d8, d8 + static_cast<double>(1)), Assign(d8, d8 + static_cast<double>(1)), Assign(d8, d8 + static_cast<double>(1)),
                    Assign(d8, d8 + static_cast<double>(1)), Assign(d8, d8 + static_cast<double>(1)),
                    Declare(d7, static_cast<double>(0)),
                    Assign(d7, d7 + static_cast<double>(1)), Assign(d7, d7 + static_cast<double>(1)), Assign(d7, d7 + static_cast<double>(1)),
                    Assign(d7, d7 + static_cast<double>(1)), Assign(d7, d7 + static_cast<double>(1)), Assign(d7, d7 + static_cast<double>(1)),
                    Assign(d7, d7 + static_cast<double>(1)),
                    Declare(d1, static_cast<double>(0)),
                    Assign(d1, d1 + static_cast<double>(1)),
                    Declare(d3, static_cast<double>(0)),
                    Assign(d3, d3 + static_cast<double>(1)), Assign(d3, d3 + static_cast<double>(1)), Assign(d3, d3 + static_cast<double>(1)),
                    Assign(sum, sum + StaticCast<double>(i1)),
                    Assign(sum, sum + StaticCast<double>(i2)),
                    Assign(sum, sum + StaticCast<double>(i3)),
                    Assign(sum, sum + StaticCast<double>(i4)),
                    Assign(sum, sum + StaticCast<double>(i5)),
                    Assign(sum, sum + StaticCast<double>(i6)),
                    Assign(sum, sum + StaticCast<double>(i7)),
                    Assign(sum, sum + StaticCast<double>(i8)),
                    Assign(sum, sum + d1),
                    Assign(sum, sum + d2),
                    Assign(sum, sum + d3),
                    Assign(sum, sum + d4),
                    Assign(sum, sum + d5),
                    Assign(sum, sum + d6),
                    Assign(sum, sum + d7),
                    Assign(sum, sum + d8),
                    Assign(n, n - 1)
                ),
                Return(sum)
        );

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto checkExist = [&](const std::vector<AstRegisterCachedVariableExpr*>& list, AstVariable* var) -> bool
    {
        for (AstRegisterCachedVariableExpr* x : list)
        {
            if (x->m_variable == var) { return true; }
        }
        return false;
    };

    AstScope* s = fn.GetBody().GetPtr();
    auto sb = s->GetContents();
    ReleaseAssert(sb.size() == 1 && sb[0]->GetAstNodeType() == AstNodeType::AstBlock);
    auto l = assert_cast<AstBlock*>(sb[0])->GetContents();
    ReleaseAssert(l.size() == 3);
    ReleaseAssert(l[1]->GetAstNodeType() == AstNodeType::AstWhileLoop);
    AstWhileLoop* whileLoop = assert_cast<AstWhileLoop*>(l[1]);
    ReleaseAssert(whileLoop->m_mem2RegInitList.size() == 1);
    ReleaseAssert(whileLoop->m_mem2RegWritebackList.size() == 1);
    ReleaseAssert(checkExist(whileLoop->m_mem2RegInitList, sum.__pochivm_var_ptr));
    ReleaseAssert(whileLoop->m_mem2RegWritebackList[0]->m_variable == sum.__pochivm_var_ptr);

    std::set<std::string> varnameSeen;
    auto traverseCheckFn = [&](AstNodeBase* cur,
                               AstNodeBase* parent,
                               FunctionRef<void(void)> Recurse)
    {
        if (cur->GetAstNodeType() == AstNodeType::AstVariable)
        {
            bool expectConverted;
            AstVariable* var = assert_cast<AstVariable*>(cur);
            std::string name = var->GetVarNameNoSuffix();
            varnameSeen.insert(name);
            if (name == "sum")
            {
                expectConverted = true;
            }
            else if (name == "n")
            {
                expectConverted = false;
            }
            else
            {
                ReleaseAssert(name.length() == 2);
                if (name[0] == 'i')
                {
                    ReleaseAssert('0' < name[1] && name[1] <= '9');
                    int i = name[1] - '0';
                    expectConverted = i > (8 - static_cast<int>(x_mem2reg_max_integral_vars));
                }
                else
                {
                    ReleaseAssert(name[0] == 'd');
                    ReleaseAssert('0' < name[1] && name[1] <= '9');
                    int i = name[1] - '0';
                    expectConverted = i > (9 - static_cast<int>(x_mem2reg_max_floating_vars));
                }
            }
            if (parent->GetAstNodeType() == AstNodeType::AstAssignExpr)
            {
                AstAssignExpr* expr = assert_cast<AstAssignExpr*>(parent);
                ReleaseAssert(expr->IsLhsMem2Reg() == expectConverted);
            }
            else if (parent->GetAstNodeType() != AstNodeType::AstDeclareVariable)
            {
                if (expectConverted)
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstRegisterCachedVariableExpr);
                }
                else
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr);
                }
            }
        }
        Recurse();
    };

    TraverseAstTree(whileLoop, traverseCheckFn);
    ReleaseAssert(varnameSeen.size() == 18);

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("testfn");
        double ret = interpFn(100);
        ReleaseAssert(fabs(ret - 8800) < 1e-9);
    }
}

// mem2reg does not apply to variables which address is taken
//
TEST(TestMem2RegPass, Sanity_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int) noexcept;

        auto [fn, n] = NewFunction<FnPrototype>("testfn", "n");
        auto sum = fn.NewVariable<int>("sum");
        auto x = fn.NewVariable<int>("x");
        auto y = fn.NewVariable<int*>("y");
        fn.SetBody(
                Declare(sum, 0),
                While(n > 0).Do(
                    Declare(x, 0),
                    Declare(y, x.Addr()),
                    Assign(*y, *y + 1),
                    Assign(sum, sum + x),
                    Assign(n, n - 1)
                ),
                Return(sum)
        );


    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto checkExist = [&](const std::vector<AstRegisterCachedVariableExpr*>& list, AstVariable* var) -> bool
    {
        for (AstRegisterCachedVariableExpr* t : list)
        {
            if (t->m_variable == var) { return true; }
        }
        return false;
    };

    AstScope* s = fn.GetBody().GetPtr();
    auto sb = s->GetContents();
    ReleaseAssert(sb.size() == 1 && sb[0]->GetAstNodeType() == AstNodeType::AstBlock);
    auto l = assert_cast<AstBlock*>(sb[0])->GetContents();
    ReleaseAssert(l.size() == 3);
    ReleaseAssert(l[1]->GetAstNodeType() == AstNodeType::AstWhileLoop);
    AstWhileLoop* whileLoop = assert_cast<AstWhileLoop*>(l[1]);
    ReleaseAssert(whileLoop->m_mem2RegInitList.size() == 2);
    ReleaseAssert(whileLoop->m_mem2RegWritebackList.size() == 2);
    ReleaseAssert(checkExist(whileLoop->m_mem2RegInitList, n.__pochivm_var_ptr));
    ReleaseAssert(checkExist(whileLoop->m_mem2RegInitList, sum.__pochivm_var_ptr));
    ReleaseAssert(checkExist(whileLoop->m_mem2RegWritebackList, n.__pochivm_var_ptr));
    ReleaseAssert(checkExist(whileLoop->m_mem2RegWritebackList, sum.__pochivm_var_ptr));

    std::set<std::string> varnameSeen;
    auto traverseCheckFn = [&](AstNodeBase* cur,
                               AstNodeBase* parent,
                               FunctionRef<void(void)> Recurse)
    {
        if (cur->GetAstNodeType() == AstNodeType::AstVariable)
        {
            bool expectConverted;
            AstVariable* var = assert_cast<AstVariable*>(cur);
            std::string name = var->GetVarNameNoSuffix();
            varnameSeen.insert(name);
            if (name == "sum" || name == "n" || name == "y")
            {
                expectConverted = true;
            }
            else if (name == "x")
            {
                expectConverted = false;
            }
            else
            {
                ReleaseAssert(false);
            }
            if (parent->GetAstNodeType() == AstNodeType::AstAssignExpr)
            {
                if (var == assert_cast<AstAssignExpr*>(parent)->GetDst())
                {
                    AstAssignExpr* expr = assert_cast<AstAssignExpr*>(parent);
                    ReleaseAssert(expr->IsLhsMem2Reg() == expectConverted);
                }
            }
            else if (parent->GetAstNodeType() != AstNodeType::AstDeclareVariable)
            {
                if (expectConverted)
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstRegisterCachedVariableExpr);
                }
                else
                {
                    ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr);
                }
            }
        }
        Recurse();
    };

    TraverseAstTree(whileLoop, traverseCheckFn);
    ReleaseAssert(varnameSeen.size() == 4);


    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("testfn");
        int ret = interpFn(100);
        ReleaseAssert(ret == 100);
    }
}

// mem2reg does not apply to innermost loop if there is a C++ function call within it
//
TEST(TestMem2RegPass, Sanity_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int) noexcept;

        auto [fn, n] = NewFunction<FnPrototype>("testfn", "n");
        auto sum = fn.NewVariable<int>("sum");
        auto t = fn.NewVariable<TestDestructor1>("t");
        auto x = fn.NewVariable<int>("x");
        auto y = fn.NewVariable<int*>("y");
        fn.SetBody(
                Declare(sum, 0),
                While(n > 0).Do(
                    Declare(x, 0),
                    Scope(
                        Declare(y, x.Addr()),
                        Declare(t, Constructor<TestDestructor1>(Literal<int>(1), y))
                    ),
                    Assign(sum, sum + x),
                    Assign(n, n - 1)
                ),
                Return(sum)
        );


    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    AstScope* s = fn.GetBody().GetPtr();
    auto sb = s->GetContents();
    ReleaseAssert(sb.size() == 1 && sb[0]->GetAstNodeType() == AstNodeType::AstBlock);
    auto l = assert_cast<AstBlock*>(sb[0])->GetContents();
    ReleaseAssert(l.size() == 3);
    ReleaseAssert(l[1]->GetAstNodeType() == AstNodeType::AstWhileLoop);
    AstWhileLoop* whileLoop = assert_cast<AstWhileLoop*>(l[1]);
    ReleaseAssert(whileLoop->m_mem2RegInitList.size() == 0);
    ReleaseAssert(whileLoop->m_mem2RegWritebackList.size() == 0);

    auto traverseCheckFn = [&](AstNodeBase* cur,
                               AstNodeBase* parent,
                               FunctionRef<void(void)> Recurse)
    {
        if (cur->GetAstNodeType() == AstNodeType::AstVariable)
        {
            if (parent->GetAstNodeType() == AstNodeType::AstAssignExpr)
            {
                AstAssignExpr* expr = assert_cast<AstAssignExpr*>(parent);
                ReleaseAssert(expr->IsLhsMem2Reg() == false);
            }
            else if (parent->GetAstNodeType() != AstNodeType::AstDeclareVariable &&
                     parent->GetAstNodeType() != AstNodeType::AstCallExpr)
            {
                ReleaseAssert(parent->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr);
            }
        }
        Recurse();
    };

    TraverseAstTree(s, traverseCheckFn);

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("testfn");
        int ret = interpFn(100);
        ReleaseAssert(ret == 100);
    }
}
