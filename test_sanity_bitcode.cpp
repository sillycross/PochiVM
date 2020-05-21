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

TEST(SanityBitcode, SanityExecution_1)
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

TEST(SanityBitcode, SanityExecution_2)
{
    const BitcodeData& pushVec = __pochivm_internal_bc_9d40e07b8417ba7543c9b4cd;
    const BitcodeData& sortVector = __pochivm_internal_bc_9211d742c8c7612d69d11c5b;
    const BitcodeData& getVectorSize = __pochivm_internal_bc_ece726fdd4130f1f904091ef;
    const BitcodeData& getVectorSum = __pochivm_internal_bc_2d9dcbc4a90b11f2fd8ca7d6;

    TestClassA a;
    a.m_vec.clear();

    SimpleJIT jit;
    std::unique_ptr<ThreadSafeModule> tsm1 = GetThreadSafeModuleFromBitcodeStub(pushVec);
    std::unique_ptr<ThreadSafeModule> tsm2 = GetThreadSafeModuleFromBitcodeStub(sortVector);
    std::unique_ptr<ThreadSafeModule> tsm3 = GetThreadSafeModuleFromBitcodeStub(getVectorSize);
    std::unique_ptr<ThreadSafeModule> tsm4 = GetThreadSafeModuleFromBitcodeStub(getVectorSum);

    using _PushFnPrototype = void(*)(TestClassA*, int);
    using _SortFnPrototype = int(*)(TestClassA*);
    using _GetSizeFnPrototype = size_t(*)(TestClassA*);
    using _GetSumFnPrototype = int64_t(*)(TestClassA*);

    jit.SetAllowResolveSymbolInHostProcess(true);
    jit.SetNonAstModule(std::move(tsm1));
    _PushFnPrototype pushFn = jit.GetFunctionNonAst<_PushFnPrototype>(std::string(pushVec.m_symbolName));

    for (size_t i = 0; i < 97; i++)
    {
        pushFn(&a, i * 37 % 97);
        ReleaseAssert(a.m_vec.size() == i + 1);
        for (size_t j = 0; j < i; j++)
        {
            ReleaseAssert(a.m_vec[j] == j * 37 % 97);
        }
    }

    jit.SetNonAstModule(std::move(tsm2));
    _SortFnPrototype sortFn = jit.GetFunctionNonAst<_SortFnPrototype>(std::string(sortVector.m_symbolName));

    for (int k = 0; k < 2; k++)
    {
        sortFn(&a);
        ReleaseAssert(a.m_vec.size() == 97);
        for (size_t i = 0; i < 97; i++)
        {
            ReleaseAssert(a.m_vec[i] == static_cast<int>(i));
        }
    }

    jit.SetNonAstModule(std::move(tsm3));
    _GetSizeFnPrototype getSizeFn = jit.GetFunctionNonAst<_GetSizeFnPrototype>(std::string(getVectorSize.m_symbolName));

    for (int k = 0; k < 2; k++)
    {
        ReleaseAssert(a.m_vec.size() == 97);
        size_t r = getSizeFn(&a);
        ReleaseAssert(r == 97);
    }

    jit.SetNonAstModule(std::move(tsm4));
    _GetSumFnPrototype getSumFn = jit.GetFunctionNonAst<_GetSumFnPrototype>(std::string(getVectorSum.m_symbolName));

    for (int k = 0; k < 2; k++)
    {
        ReleaseAssert(a.m_vec.size() == 97);
        int64_t r = getSumFn(&a);
        ReleaseAssert(r == 97 * 96 / 2);
    }
}

TEST(SanityBitcode, SanityExecution_3)
{
    // int FreeFunctionOverloaded<double, double, int>(int, int)
    const BitcodeData& sig1 = __pochivm_internal_bc_bd614a9c9c6a469d1aa5a33c;
    // int FreeFunctionOverloaded<double, double, double>(int, int)
    const BitcodeData& sig2 = __pochivm_internal_bc_a9b7de4eb4a4edee827deb07;
    // int FreeFunctionOverloaded<int, int, int>(int)
    const BitcodeData& sig3 = __pochivm_internal_bc_55fe00ceaaa5ab7945195db3;
    // int FreeFunctionOverloaded<int, int, double>(int)
    const BitcodeData& sig4 = __pochivm_internal_bc_6d29f5afd61bc9b4312e126b;
    // double FreeFunctionOverloaded(double)
    const BitcodeData& sig5 = __pochivm_internal_bc_0c6d6b096143d90aa39723f9;

    TestClassA a;
    a.m_vec.clear();

    SimpleJIT jit;
    std::unique_ptr<ThreadSafeModule> tsm1 = GetThreadSafeModuleFromBitcodeStub(sig1);
    std::unique_ptr<ThreadSafeModule> tsm2 = GetThreadSafeModuleFromBitcodeStub(sig2);
    std::unique_ptr<ThreadSafeModule> tsm3 = GetThreadSafeModuleFromBitcodeStub(sig3);
    std::unique_ptr<ThreadSafeModule> tsm4 = GetThreadSafeModuleFromBitcodeStub(sig4);
    std::unique_ptr<ThreadSafeModule> tsm5 = GetThreadSafeModuleFromBitcodeStub(sig5);

    using _Sig12Proto = int(*)(int, int);
    using _Sig34Proto = int(*)(int);
    using _Sig5Proto = double(*)(double);

    jit.SetNonAstModule(std::move(tsm1));
    _Sig12Proto fn1 = jit.GetFunctionNonAst<_Sig12Proto>(std::string(sig1.m_symbolName));
    for (int i = 0; i < 100; i++)
    {
        int x = rand(), y = rand();
        ReleaseAssert(fn1(x, y) == x + y + 4);
    }

    jit.SetNonAstModule(std::move(tsm2));
    _Sig12Proto fn2 = jit.GetFunctionNonAst<_Sig12Proto>(std::string(sig2.m_symbolName));
    for (int i = 0; i < 100; i++)
    {
        int x = rand(), y = rand();
        ReleaseAssert(fn2(x, y) == x + y);
    }

    jit.SetNonAstModule(std::move(tsm3));
    _Sig34Proto fn3 = jit.GetFunctionNonAst<_Sig34Proto>(std::string(sig3.m_symbolName));
    for (int i = 0; i < 100; i++)
    {
        int x = rand();
        ReleaseAssert(fn3(x) == x + 7);
    }

    jit.SetNonAstModule(std::move(tsm4));
    _Sig34Proto fn4 = jit.GetFunctionNonAst<_Sig34Proto>(std::string(sig4.m_symbolName));
    for (int i = 0; i < 100; i++)
    {
        int x = rand();
        ReleaseAssert(fn4(x) == x + 3);
    }

    jit.SetNonAstModule(std::move(tsm5));
    _Sig5Proto fn5 = jit.GetFunctionNonAst<_Sig5Proto>(std::string(sig5.m_symbolName));
    for (int i = 0; i < 100; i++)
    {
        int x = rand() % 1000000;
        double dx = double(x) / 1000;
        double out = fn5(dx);
        ReleaseAssert(fabs(dx - out + 2.3) < 1e-8);
    }
}

