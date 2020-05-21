#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"
#include "codegen_context.hpp"
#include "generated/pochivm_runtime_library_bitcodes.generated.h"
#include "runtime/pochivm_runtime_headers.h"

#include "llvm/Support/MemoryBuffer.h"

using namespace Ast;

using namespace llvm;
using namespace llvm::orc;

namespace {

std::unique_ptr<ThreadSafeModule> GetThreadSafeModuleFromBitcodeStub(const BitcodeData& bc)
{
    SMDiagnostic llvmErr;
    std::unique_ptr<LLVMContext> context(new LLVMContext);
    MemoryBufferRef mb(StringRef(reinterpret_cast<const char*>(bc.m_bitcode), bc.m_length),
                       StringRef(bc.m_symbolName));
    std::unique_ptr<Module> module = parseIR(mb, llvmErr, *context.get());
    ReleaseAssert(module != nullptr);
    return std::unique_ptr<ThreadSafeModule>(new ThreadSafeModule(std::move(module), std::move(context)));
}

}   // anonymous namespace

TEST(SanityBitcode, SanityExecution)
{
    const BitcodeData& getY = __pochivm_internal_bc_fb2d7a235b0bee90ed026467;
    const BitcodeData& setY = __pochivm_internal_bc_9e962f83bf07fe71b8a356ec;
    const BitcodeData& getXPlusY = __pochivm_internal_bc_0168482cbf35dd60c2677b26;
    const BitcodeData& getStringY = __pochivm_internal_bc_888a3d0065471b9b797cc710;

    TestClassA a;
    a.m_y = 0;

    SimpleJIT jit;
    std::unique_ptr<ThreadSafeModule> tsm1 = GetThreadSafeModuleFromBitcodeStub(setY);
    std::unique_ptr<ThreadSafeModule> tsm2 = GetThreadSafeModuleFromBitcodeStub(getY);
    std::unique_ptr<ThreadSafeModule> tsm3 = GetThreadSafeModuleFromBitcodeStub(getXPlusY);
    std::unique_ptr<ThreadSafeModule> tsm4 = GetThreadSafeModuleFromBitcodeStub(getStringY);

    using _SetFnPrototype = void(*)(TestClassA*, int);
    using _GetFnPrototype = int(*)(TestClassA*);
    using _GetXPlusYFnPrototype = int(*)(TestClassA*, int);
    using _GetStringYFnPrototype = std::string(*)(TestClassA*);

    jit.SetNonAstModule(std::move(tsm1));
    _SetFnPrototype setFn = jit.GetFunctionNonAst<_SetFnPrototype>(std::string(setY.m_symbolName));

    setFn(&a, 233);
    ReleaseAssert(a.m_y == 233);

    jit.SetNonAstModule(std::move(tsm2));
    _GetFnPrototype getFn = jit.GetFunctionNonAst<_GetFnPrototype>(std::string(getY.m_symbolName));

    {
        int r = getFn(&a);
        ReleaseAssert(r == 233);
        ReleaseAssert(a.m_y == 233);
    }

    jit.SetNonAstModule(std::move(tsm3));
    _GetXPlusYFnPrototype getXPlusYFn = jit.GetFunctionNonAst<_GetXPlusYFnPrototype>(std::string(getXPlusY.m_symbolName));

    {
        int r = getXPlusYFn(&a, 456);
        ReleaseAssert(r == 233 + 456);
        ReleaseAssert(a.m_y == 233);
    }

    jit.SetAllowResolveSymbolInHostProcess(true);
    jit.SetNonAstModule(std::move(tsm4));
    _GetStringYFnPrototype getStringYFn = jit.GetFunctionNonAst<_GetStringYFnPrototype>(std::string(getStringY.m_symbolName));

    {
        std::string r = getStringYFn(&a);
        ReleaseAssert(r == "mystr_233");
    }
}
