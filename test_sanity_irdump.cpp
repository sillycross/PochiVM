#include "gtest/gtest.h"

#include "pochivm.h"
#include "codegen_context.hpp"

using namespace Ast;

#if 0
TEST(SanityIrCodeDump, APlusB)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<int(int, int)>;
    {
        auto [fn, value1, value2] = NewFunction<FnPrototype>("a_plus_b");
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

    std::cout<<dump;
}
#endif
