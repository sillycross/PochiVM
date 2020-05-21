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
    MemoryBufferRef mb(StringRef(reinterpret_cast<char*>(bc.m_bitcode), bc.m_length),
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

    TestClassA a;
    a.m_y = 0;

    SimpleJIT jit;
    std::unique_ptr<ThreadSafeModule> tsm1 = GetThreadSafeModuleFromBitcodeStub(setY);
    std::unique_ptr<ThreadSafeModule> tsm2 = GetThreadSafeModuleFromBitcodeStub(getY);

    using _SetFnPrototype = void(*)(TestClassA*, int);
    using _GetFnPrototype = int(*)(TestClassA*);

    jit.SetNonAstModule(std::move(tsm1));
    _SetFnPrototype setFn = jit.GetFunction<_SetFnPrototype>(std::string(setY.m_symbolName));

    setFn(&a, 233);
    ReleaseAssert(a.m_y == 233);

    jit.SetNonAstModule(std::move(tsm2));
    _GetFnPrototype getFn = jit.GetFunction<_GetFnPrototype>(std::string(getY.m_symbolName));

    int r = getFn(&a);
    ReleaseAssert(r == 233);
    ReleaseAssert(a.m_y == 233);
}
