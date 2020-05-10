#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"

using namespace Ast;

TEST(SanityIrCodeDump, APlusB)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, value1, value2] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            Return(value1 + value2)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityIrCodeDump, APlusB_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Assign(a, a * Literal<int>(-1))
            ),
            Return(a + b)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityIrCodeDump, APlusB_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Assign(a, a * Literal<int>(-1))
            ).Else(
                Assign(a, a * Literal<int>(2))
            ),
            Return(a + b)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityIrCodeDump, APlusB_4)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Return(b - a)
            ).Else(
                Assign(a, a * Literal<int>(2))
            ),
            Return(a + b)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityIrCodeDump, APlusB_5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Return(b - a)
            ).Else(
                Return(a + b)
            )
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityIrCodeDump, APlusB_6)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
            ).Else(
                Return(a + b)
            )
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}

TEST(SanityIrCodeDump, APlusB_7)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b", "a", "b");
        fn.SetBody(
            If(a < Literal<int>(0)).Then(
                Return(a + b)
            )
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);
}
