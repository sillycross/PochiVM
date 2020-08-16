#include <iomanip>
#include <sstream>

#include "pochivm/common.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/DebugInfo.h"

using namespace llvm;
using namespace llvm::orc;

namespace {

// A fake symbol resolver that returns fake (but unique) addresses for each queried symbol
// Of course the program crashes if the symbols are called, but if they only care about the addresses
// things should be fine.
//
class FakeSymbolResolver : public JITDylib::DefinitionGenerator
{
public:
    FakeSymbolResolver()
        : m_lock()
        , m_curAddr(0)
        , m_addrEnd(0)
        , m_pastAddrs()
    { }

    ~FakeSymbolResolver() override
    {
        for (uintptr_t addr : m_pastAddrs)
        {
            int r = munmap(reinterpret_cast<void*>(addr), x_length + x_overflow_protection_buffer * 2);
            ReleaseAssert(r == 0);
        }
    }

    virtual Error tryToGenerate(LookupKind /*K*/,
                                JITDylib &JD,
                                JITDylibLookupFlags /*JDLookupFlags*/,
                                const SymbolLookupSet &symbols) override
    {
        // no idea what the thread safety requirement is... so just to be safe
        //
        std::lock_guard<std::mutex> guard(m_lock);

        SymbolMap newSymbols;

        for (auto& kv : symbols)
        {
            const SymbolStringPtr& name = kv.first;

            if ((*name).empty())
                continue;

            uintptr_t addr = GetNextAddress();
            newSymbols[name] = JITEvaluatedSymbol(
                    static_cast<JITTargetAddress>(addr),
                    JITSymbolFlags::Exported);
            // std::string strname((*name).data(), (*name).size());
            // fprintf(stderr, "Assigned symbol %s to address %llu\n",
            //         strname.c_str(), static_cast<unsigned long long>(addr));
        }

        if (newSymbols.empty())
            return Error::success();

        return JD.define(absoluteSymbols(std::move(newSymbols)));
    }

private:
    uintptr_t GetNextAddress()
    {
        if (m_curAddr == 0 || m_curAddr >= m_addrEnd)
        {
            void* r = mmap(nullptr, x_length + x_overflow_protection_buffer * 2,
                           PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1 /*fd*/, 0 /*offset*/);
            ReleaseAssert(r != reinterpret_cast<void*>(-1));
            m_curAddr = reinterpret_cast<uintptr_t>(r) + x_overflow_protection_buffer;
            m_addrEnd = m_curAddr + x_length;
            m_pastAddrs.push_back(reinterpret_cast<uintptr_t>(r));
        }

        ReleaseAssert(m_curAddr != 0 && m_curAddr < m_addrEnd);
        uintptr_t r = m_curAddr;
        m_curAddr += 32;
        return r;
    }

    static const size_t x_length = 1024 * 1024;
    static const size_t x_overflow_protection_buffer = 1024 * 1024;

    std::mutex m_lock;
    uintptr_t m_curAddr;
    uintptr_t m_addrEnd;
    std::vector<uintptr_t> m_pastAddrs;
};

}   // anonymous namespace

void AddFakeSymbolResolverGenerator(llvm::orc::LLJIT* jit)
{
    jit->getMainJITDylib().addGenerator(std::unique_ptr<FakeSymbolResolver>(new FakeSymbolResolver()));
}
