#include "gtest/gtest.h"

#include "fastinterp/fastinterp.hpp"
#include "pochivm/pochivm.h"
#include "codegen_context.hpp"
#include "test_util_helper.h"

using namespace PochiVM;

TEST(TestFastInterp, Sanity_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = int(*)(int, int) noexcept;
    {
        auto [fn, a, b] = NewFunction<FnPrototype>("testfn");

        fn.SetBody(
                Return(a + b)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("testfn");
        int ret = interpFn(123, 456);
        ReleaseAssert(ret == 123 + 456);
    }
}
