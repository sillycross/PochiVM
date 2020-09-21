#include <iomanip>
#include <sstream>

#include "pochivm/common.h"

#include "runtime_lib_builder/fake_symbol_resolver.h"
#include "runtime_lib_builder/reflective_stringify_parser.h"

#include "pochivm/ast_enums.h"
#include "metavar.h"

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
#include "llvm/IR/TypeFinder.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"

using namespace llvm;
using namespace llvm::orc;
using namespace llvm::object;

namespace {

struct RelocationInfo
{
    uint64_t offset;
    uint64_t type;
    std::string typeHumanReadableName;
    std::string symbol;
    int64_t addend;
};

struct BoilerplateParam
{
    PochiVM::MetaVarType m_type;
    std::string m_name;
    std::string m_typename;
};

struct BoilerplateInstance
{
    std::vector<uint64_t> m_paramValues;
    void* m_ptr;
    std::string m_symbolName;
    SectionRef m_llvmSectionRef;
    std::vector<RelocationInfo> m_relocationInfo;
};

struct BoilerplatePack
{
    std::vector<BoilerplateParam> m_params;
    std::vector<BoilerplateInstance> m_instances;
};

static std::vector<std::pair<std::string, BoilerplatePack>> g_allBoilerplatePacks;

}   // anonymous namespace

void __pochivm_register_fast_interp_boilerplate__(const char* stringified_typename, PochiVM::MetaVarMaterializedList* list)
{
    BoilerplatePack p;
    for (const PochiVM::MetaVar& var : list->m_metavars)
    {
        BoilerplateParam bvar;
        bvar.m_type = var.m_type;
        bvar.m_name = std::string(var.m_name);
        if (bvar.m_type == PochiVM::MetaVarType::ENUM)
        {
            bvar.m_typename = ReflectiveStringifyParser::ParseTypeName(var.m_enum_typename);
        }
        else if (bvar.m_type == PochiVM::MetaVarType::BOOL)
        {
            bvar.m_typename = "bool";
        }
        else
        {
            ReleaseAssert(bvar.m_type == PochiVM::MetaVarType::PRIMITIVE_TYPE);
            bvar.m_typename = "FastInterpTypeId";
        }
        p.m_params.push_back(bvar);
    }

    for (const PochiVM::MetaVarMaterializedInstance& inst : list->m_instances)
    {
        BoilerplateInstance binst;
        binst.m_paramValues = inst.m_values;
        binst.m_ptr = inst.m_fnPtr;
        binst.m_symbolName = "";
        p.m_instances.push_back(binst);
    }

    std::string indexType = ReflectiveStringifyParser::ParseTypeName(stringified_typename);
    std::string pref = "PochiVM::";
    ReleaseAssert(indexType.length() > pref.length() && indexType.substr(0, pref.length()) == pref);
    indexType = indexType.substr(pref.length());
    g_allBoilerplatePacks.push_back(std::make_pair(indexType, p));
}

namespace {

bool IsRelocAddressLess(RelocationRef A, RelocationRef B) {
  return A.getOffset() < B.getOffset();
}

// Logic stolen from llvm/tools/llvm-objdump/llvm-objdump.cpp
// Get a map from section to all its relocations
// tbh I have no idea why we have to call the 'getRelocatedSection'..
//
std::map<SectionRef, std::vector<RelocationRef>> GetRelocationMap(ObjectFile const &Obj)
{
    std::map<SectionRef, std::vector<RelocationRef>> Ret;
    for (SectionRef Sec : Obj.sections())
    {
        Expected<section_iterator> RelocatedOrErr = Sec.getRelocatedSection();
        if (!RelocatedOrErr)
        {
            fprintf(stderr, "[INTERNAL ERROR] Failed to get a relocated section. Please report a bug.\n");
            abort();
        }

        section_iterator Relocated = *RelocatedOrErr;
        if (Relocated == Obj.section_end()) continue;
        std::vector<RelocationRef> &V = Ret[*Relocated];
        for (const RelocationRef &R : Sec.relocations())
        {
            V.push_back(R);
        }
        // Sort relocations by address.
        //
        llvm::stable_sort(V, IsRelocAddressLess);
    }

    for (SectionRef Sec : Obj.sections())
    {
        if (!Ret.count(Sec))
        {
            Ret[Sec] = std::vector<RelocationRef>();
        }
    }
    return Ret;
}

// Logic stolen from llvm/tools/llvm-objdump/ELFDump.cpp, slightly modified
//
template <class ELFT>
static RelocationInfo GetRelocationInfoImpl(const ELFObjectFile<ELFT>* Obj,
                                            const RelocationRef& RelRef)
{
    RelocationInfo ret;
    ret.offset = RelRef.getOffset();
    ret.type = RelRef.getType();
    SmallString<16> tmp;
    RelRef.getTypeName(tmp);
    ret.typeHumanReadableName = std::string(tmp.c_str());

    const ELFFile<ELFT> &EF = *Obj->getELFFile();
    DataRefImpl Rel = RelRef.getRawDataRefImpl();
    auto SecOrErr = EF.getSection(Rel.d.a);
    if (!SecOrErr)
    {
        fprintf(stderr, "[INTERNAL ERROR] Fail to get section from a relocation. Please report a bug.\n");
        abort();
    }

    if ((*SecOrErr)->sh_type != ELF::SHT_RELA)
    {
        fprintf(stderr, "[INTERNAL ERROR] Unexpected ELF SH_TYPE. Please report a bug.\n");
        abort();
    }

    {
        const typename ELFT::Rela *ERela = Obj->getRela(Rel);
        if (ERela->getSymbol(false /*isMips64EL*/) == 0)
        {
            fprintf(stderr, "[INTERNAL ERROR] Encountered a relocation record not associated with a symbol. Please report a bug.\n");
            abort();
        }
        ret.addend = ERela->r_addend;
    }

    symbol_iterator SI = RelRef.getSymbol();
    const typename ELFT::Sym *Sym = Obj->getSymbol(SI->getRawDataRefImpl());
    if (Sym->getType() == ELF::STT_SECTION)
    {
        // In our use case, we should never have relocations that shall be populated by section address
        //
        fprintf(stderr, "[INTERNAL ERROR] Unexpected relocation: a relocation associated with a section. Please report a bug.\n");
        abort();
    }

    {
        Expected<StringRef> SymName = SI->getName();
        if (!SymName)
        {
            fprintf(stderr, "[INTERNAL ERROR] Unable to get name of symbol associated with a relocation. Please report a bug.\n");
            abort();
        }
        ret.symbol = SymName.get().str();
    }

    return ret;
}

RelocationInfo GetRelocationInfo(const ELFObjectFileBase* Obj,
                                 const RelocationRef& Rel)
{
    if (auto* ELF32LE = dyn_cast<ELF32LEObjectFile>(Obj))
    {
        return GetRelocationInfoImpl(ELF32LE, Rel);
    }
    if (auto* ELF64LE = dyn_cast<ELF64LEObjectFile>(Obj))
    {
        return GetRelocationInfoImpl(ELF64LE, Rel);
    }
    if (auto* ELF32BE = dyn_cast<ELF32BEObjectFile>(Obj))
    {
        return GetRelocationInfoImpl(ELF32BE, Rel);
    }
    auto* ELF64BE = dyn_cast<ELF64BEObjectFile>(Obj);
    ReleaseAssert(ELF64BE != nullptr);
    return GetRelocationInfoImpl(ELF64BE, Rel);
}

uint32_t StringToIntHelper(const std::string& s)
{
    uint32_t res = 0;
    for (size_t i = 0; i < s.length(); i++)
    {
        ReleaseAssert('0' <= s[i] && s[i] <= '9');
        res = res * 10 + static_cast<uint32_t>(s[i] - '0');
        ReleaseAssert(res < 100000000);
    }
    return res;
}

bool IsBoilerplateFnPtrPlaceholder(const std::string symbol, uint32_t& placeholderOrd /*out*/)
{
    std::string pref = "__pochivm_fast_interp_dynamic_specialization_boilerplate_function_placeholder_";
    if (symbol.length() > pref.length() && symbol.substr(0, pref.length()) == pref)
    {
        placeholderOrd = StringToIntHelper(symbol.substr(pref.length()));
        return true;
    }
    return false;
}

bool IsCppFnPtrPlaceholder(const std::string symbol, uint32_t& placeholderOrd /*out*/)
{
    std::string pref = "__pochivm_fast_interp_dynamic_specialization_aotc_cpp_function_placeholder_";
    if (symbol.length() > pref.length() && symbol.substr(0, pref.length()) == pref)
    {
        placeholderOrd = StringToIntHelper(symbol.substr(pref.length()));
        return true;
    }
    return false;
}

bool IsU64Placeholder(const std::string symbol, uint32_t& placeholderOrd /*out*/)
{
    std::string pref = "__pochivm_fast_interp_dynamic_specialization_data_placeholder_";
    if (symbol.length() > pref.length() && symbol.substr(0, pref.length()) == pref)
    {
        placeholderOrd = StringToIntHelper(symbol.substr(pref.length()));
        return true;
    }
    return false;
}

template<typename T>
T WARN_UNUSED UnalignedRead(uint8_t* src)
{
    T ret;
    memcpy(&ret, src, sizeof(T));
    return ret;
}

template<typename T>
void UnalignedWrite(uint8_t* dst, T value)
{
    memcpy(dst, &value, sizeof(T));
}

template<typename T>
void UnalignedAddAndWriteback(uint8_t* addr, T value)
{
    T old = UnalignedRead<T>(addr);
    UnalignedWrite<T>(addr, old + value);
}

struct CuckooHashTable
{
    std::vector<uint32_t> h1;
    std::vector<uint32_t> h2;
    std::vector<uint32_t> h3;
    std::vector<std::pair<uint32_t, uint64_t>> ht;
    std::vector<std::vector<uint64_t>> trueEntries;
};

bool is_prime(uint32_t value)
{
    if (value == 1) { return false; }
    for (uint32_t i = 2; static_cast<uint64_t>(i) * i <= value; i++)
    {
        if (value % i == 0)
        {
            return false;
        }
    }
    return true;
}

uint32_t find_next_prime(uint32_t x)
{
    while (!is_prime(x)) x++;
    return x;
}

uint32_t find_random_prime()
{
    while (true)
    {
        uint32_t value = 0;
        for (int i = 0; i < 2; i++)
        {
            value = (value << 16) + static_cast<uint32_t>(rand() & ((1<<16)-1));
        }
        if (is_prime(value)) {
            return value;
        }
    }
}

void build_random_hash_function(std::vector<uint32_t>& out)
{
    for (size_t i = 0; i < out.size(); i++)
    {
        out[i] = find_random_prime();
    }
}

uint64_t compute_hash_value(const std::vector<uint32_t>& hashfn, const std::vector<uint64_t>& input)
{
    ReleaseAssert(input.size() == hashfn.size());
    uint64_t ret = 0;
    for (size_t i = 0; i < input.size(); i++)
    {
        ret += input[i] * hashfn[i];
    }
    return ret;
}

bool try_displace(CuckooHashTable& h, const std::vector<std::pair<uint32_t, uint32_t>>& twoslots, uint32_t displace, uint32_t depth)
{
    if (depth > 1000)
    {
        return false;
    }
    if (h.ht[displace].first == static_cast<uint32_t>(-1))
    {
        return true;
    }
    uint32_t victim = h.ht[displace].first;
    ReleaseAssert(displace == twoslots[victim].first || displace == twoslots[victim].second);
    uint32_t otherslot = twoslots[victim].first + twoslots[victim].second - displace;
    if (otherslot == displace)
    {
        return false;
    }
    if (!try_displace(h, twoslots, otherslot, depth + 1))
    {
        return false;
    }
    ReleaseAssert(h.ht[otherslot].first == static_cast<uint32_t>(-1));
    h.ht[otherslot] = h.ht[displace];
    h.ht[displace] = std::make_pair(static_cast<uint32_t>(-1), static_cast<uint64_t>(-1));
    return true;
}

bool try_populate_fingerprint(CuckooHashTable& h, const std::vector<BoilerplateInstance>& list)
{
    std::set<uint64_t> s;
    for (size_t i = 0; i < list.size(); i++)
    {
        uint64_t x = compute_hash_value(h.h3, list[i].m_paramValues);
        if (x == static_cast<uint64_t>(-1))
        {
            return false;
        }
        if (s.count(x))
        {
            return false;
        }
        s.insert(x);
    }
    return true;
}

bool try_build_ht(CuckooHashTable& h, const std::vector<BoilerplateInstance>& list)
{
    uint32_t M = static_cast<uint32_t>(h.ht.size());
    std::vector<std::pair<uint32_t, uint32_t>> twoslots;
    twoslots.resize(list.size());
    for (size_t i = 0; i < list.size(); i++)
    {
        uint32_t h1 = static_cast<uint32_t>(compute_hash_value(h.h1, list[i].m_paramValues) % M);
        uint32_t h2 = static_cast<uint32_t>(compute_hash_value(h.h2, list[i].m_paramValues) % M);
        twoslots[i] = std::make_pair(h1, h2);
    }
    for (uint32_t i = 0; i < M; i++)
    {
        h.ht[i] = std::make_pair(static_cast<uint32_t>(-1), static_cast<uint64_t>(-1));
    }
    for (size_t i = 0; i < list.size(); i++)
    {
        if (h.ht[twoslots[i].first].first == static_cast<uint32_t>(-1))
        {
            h.ht[twoslots[i].first] = std::make_pair(static_cast<uint32_t>(i), static_cast<uint64_t>(-1));
        }
        else if (h.ht[twoslots[i].second].first == static_cast<uint32_t>(-1))
        {
            h.ht[twoslots[i].second] = std::make_pair(static_cast<uint32_t>(i), static_cast<uint64_t>(-1));
        }
        else
        {
            uint32_t displace;
            if (rand() % 2 == 0)
            {
                displace = twoslots[i].first;
            }
            else
            {
                displace = twoslots[i].second;
            }
            if (!try_displace(h, twoslots, displace, 0))
            {
                return false;
            }
            ReleaseAssert(h.ht[displace].first == static_cast<uint32_t>(-1));
            h.ht[displace] = std::make_pair(static_cast<uint32_t>(i), static_cast<uint64_t>(-1));
        }
    }

    while (true)
    {
        build_random_hash_function(h.h3);
        if (try_populate_fingerprint(h, list))
        {
            break;
        }
    }

    for (size_t i = 0; i < list.size(); i++)
    {
        uint32_t p1 = twoslots[i].first;
        uint32_t p2 = twoslots[i].second;
        ReleaseAssert(h.ht[p1].first == static_cast<uint32_t>(i) || h.ht[p2].first == static_cast<uint32_t>(i));
        if (h.ht[p1].first == static_cast<uint32_t>(i))
        {
            h.ht[p1].second = compute_hash_value(h.h3, list[i].m_paramValues);
            h.trueEntries[p1] = list[i].m_paramValues;
        }
        else
        {
            h.ht[p2].second = compute_hash_value(h.h3, list[i].m_paramValues);
            h.trueEntries[p2] = list[i].m_paramValues;
        }
    }

    // Just sanity check everything is correct
    //
    for (size_t i = 0; i < list.size(); i++)
    {
        uint32_t p1 = static_cast<uint32_t>(compute_hash_value(h.h1, list[i].m_paramValues) % M);
        uint32_t p2 = static_cast<uint32_t>(compute_hash_value(h.h2, list[i].m_paramValues) % M);
        uint64_t h3 = compute_hash_value(h.h3, list[i].m_paramValues);
        ReleaseAssert(h3 != static_cast<uint64_t>(-1));
        if (h.ht[p1].second == h3)
        {
            ReleaseAssert(h.ht[p1].first == static_cast<uint32_t>(i));
            ReleaseAssert(h.trueEntries[p1] == list[i].m_paramValues);
            ReleaseAssert(p1 == p2 || h.ht[p2].second != h3);
        }
        else
        {
            ReleaseAssert(h.ht[p2].second == h3);
            ReleaseAssert(h.ht[p2].first == static_cast<uint32_t>(i));
            ReleaseAssert(h.trueEntries[p2] == list[i].m_paramValues);
        }
    }

    std::set<uint32_t> s;
    for (uint32_t i = 0; i < M; i++)
    {
        if (h.ht[i].first == static_cast<uint32_t>(-1))
        {
            ReleaseAssert(h.ht[i].second == static_cast<uint64_t>(-1));
        }
        else
        {
            ReleaseAssert(h.ht[i].second != static_cast<uint64_t>(-1));
            ReleaseAssert(!s.count(h.ht[i].first));
            ReleaseAssert(h.ht[i].first < list.size());
            s.insert(h.ht[i].first);
            ReleaseAssert(h.ht[i].second == compute_hash_value(h.h3, list[h.ht[i].first].m_paramValues));
        }
    }
    ReleaseAssert(s.size() == list.size());

    return true;
}

CuckooHashTable GetCuckooHashTable(size_t n, const std::vector<BoilerplateInstance>& list)
{
    ReleaseAssert(n > 0);
    ReleaseAssert(list.size() > 0);
    {
        std::set<std::vector<uint64_t>> s;
        for (const BoilerplateInstance& inst : list)
        {
            ReleaseAssert(inst.m_paramValues.size() == n);
            ReleaseAssert(!s.count(inst.m_paramValues));
            s.insert(inst.m_paramValues);
        }
    }

    CuckooHashTable ret;
    ret.h1.resize(n);
    ret.h2.resize(n);
    ret.h3.resize(n);
    uint32_t htSize = find_next_prime(static_cast<uint32_t>(static_cast<double>(list.size()) / 0.4));
    if (htSize < 5) { htSize = 5; }
    ret.ht.resize(htSize);
    ret.trueEntries.resize(htSize);
    for (uint32_t i = 0; i < htSize; i++)
    {
        ret.trueEntries[i].resize(n);
    }

    while (true)
    {
        build_random_hash_function(ret.h1);
        build_random_hash_function(ret.h2);
        if (try_build_ht(ret, list))
        {
            return ret;
        }
    }
}

struct SourceFileInfo
{
    // multi-thread-safe prepare phase
    //
    void MTSafePrepare(const std::string& bc_file)
    {
        SMDiagnostic llvmErr;
        std::unique_ptr<LLVMContext> context(new LLVMContext);
        std::unique_ptr<Module> module = parseIRFile(bc_file, llvmErr, *context.get());

        if (module == nullptr)
        {
            fprintf(stderr, "[ERROR] An error occurred while parsing IR file '%s', detail:\n", bc_file.c_str());
            llvmErr.print("build_fast_interp_lib" /*programName*/, errs());
            abort();
        }

        // Iterate the IR file and pick up all the function name symbols
        //
        for (Function& f : module->functions())
        {
            if (f.getLinkage() == GlobalValue::LinkageTypes::LinkOnceODRLinkage ||
                f.getLinkage() == GlobalValue::LinkageTypes::WeakODRLinkage ||
                f.getLinkage() == GlobalValue::LinkageTypes::ExternalLinkage ||
                f.getLinkage() == GlobalValue::LinkageTypes::ExternalWeakLinkage ||
                f.getLinkage() == GlobalValue::LinkageTypes::AvailableExternallyLinkage)
            {
                std::string name = f.getName();
                // Empty functions are declarations
                //
                if (!f.empty())
                {
                    ReleaseAssert(!allDefinitions.count(name));
                    allDefinitions.insert(name);
                }
                ReleaseAssert(!allDeclarations.count(name));
                allDeclarations.insert(name);
            }
        }

        // Beyond this point the IR module is gone: we move it to the JIT
        //
        ExitOnError exitOnError;
        ThreadSafeModule tsm(std::move(module), std::move(context));

        // Prepare for JIT execution of function __pochivm_register_runtime_library__ to figure out
        // which functions are needed by the runtime library, and their detailed type information
        //
        J = exitOnError(LLJITBuilder().create());

        char Prefix = EngineBuilder().selectTarget()->createDataLayout().getGlobalPrefix();
        std::unique_ptr<DynamicLibrarySearchGenerator> R = exitOnError(DynamicLibrarySearchGenerator::GetForCurrentProcess(Prefix));
        ReleaseAssert(R != nullptr);

        // For undefined symbol in the IR file, first try to resolve in host process (it
        // contains symbols from system library and our hook __pochivm_report_info__).
        // If failed, it should be a symbol implemented in other CPP files,
        // and we should only care about its address (and never actually need to invoke it),
        // so just resolve it to an unique address using our FakeSymbolResolver.
        //
        J->getMainJITDylib().addGenerator(std::move(R));
        AddFakeSymbolResolverGenerator(J.get());

        exitOnError(J->addIRModule(std::move(tsm)));

        auto entryPointSym = exitOnError(J->lookup("__pochivm_build_fast_interp_library__"));
        entryPoint = reinterpret_cast<FnPrototype>(entryPointSym.getAddress());
    }

    // Not MT safe
    //
    void ExecuteAndInsertSymbols(std::map<uintptr_t, std::pair<SourceFileInfo*, std::string> >& addrToSymbol,
                                 std::set<uintptr_t>& hasMultipleSymbolsMappingToThisAddress)
    {
        entryPoint();
        ExitOnError exitOnError;
        for (const std::string& symbol : allDeclarations)
        {
            auto sym = exitOnError(J->lookup(symbol));
            uintptr_t addr = sym.getAddress();
            if (addrToSymbol.count(addr))
            {
                hasMultipleSymbolsMappingToThisAddress.insert(addr);
            }
            addrToSymbol[addr] = std::make_pair(this, symbol);
        }
    }

    std::set<std::string> allDefinitions, allDeclarations;
    std::unique_ptr<LLJIT> J;
    using FnPrototype = void(*)(void);
    FnPrototype entryPoint;
};

}   // anonymous namespace

int main(int argc, char** argv)
{
    InitLLVM _init_llvm_(argc, argv);

    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    // params: [bc_file] [obj_file] [generated_header_dir]

    ReleaseAssert(argc == 4);
    std::string bc_file = argv[1];
    std::string obj_file = argv[2];
    std::string output_dir = argv[3];

    ReleaseAssert(bc_file.find(";") == std::string::npos);
    ReleaseAssert(obj_file.find(";") == std::string::npos);

    SourceFileInfo sfi;
    sfi.MTSafePrepare(bc_file);

    // Match the function address with the symbol name
    //
    std::map<uintptr_t, std::pair<SourceFileInfo*, std::string> > addrToSymbol;
    std::set<uintptr_t> hasMultipleSymbolsMappingToThisAddress;

    sfi.ExecuteAndInsertSymbols(addrToSymbol, hasMultipleSymbolsMappingToThisAddress);

    for (auto it = g_allBoilerplatePacks.begin(); it != g_allBoilerplatePacks.end(); it++)
    {
        BoilerplatePack& bp = it->second;
        for (BoilerplateInstance& inst : bp.m_instances)
        {
            uintptr_t addr = reinterpret_cast<uintptr_t>(inst.m_ptr);
            if (hasMultipleSymbolsMappingToThisAddress.count(addr))
            {
                fprintf(stderr, "[INTERNAL ERROR] A fastinterp boilerplate is resolved to an ambiguous address, "
                                "in boilerplate for %s. Please report a bug.\n",
                        it->first.c_str());
                abort();
            }
            ReleaseAssert(addrToSymbol.count(addr));
            inst.m_symbolName = addrToSymbol[addr].second;
            ReleaseAssert(addrToSymbol[addr].first->allDefinitions.count(inst.m_symbolName));
        }
    }

    Expected<OwningBinary<Binary>> expectedBinary = createBinary(obj_file);
    if (!expectedBinary)
    {
        fprintf(stderr, "[INTERNAL ERROR] Failed to parse object file %s. Please report a bug.\n",
                obj_file.c_str());
        abort();
    }
    Binary* binary = expectedBinary.get().getBinary();

    if (!isa<ObjectFile>(binary))
    {
        fprintf(stderr, "[INTERNAL ERROR] Object file %s does not seem to be an object file. Please report a bug.\n",
                obj_file.c_str());
        abort();
    }
    ObjectFile* obj = dyn_cast<ObjectFile>(binary);
    ReleaseAssert(obj != nullptr);

    ELFObjectFileBase* elf = dyn_cast<ELFObjectFileBase>(obj);
    if (elf == nullptr)
    {
        fprintf(stderr, "[INTERNAL ERROR] Object file %s is not an ELF object file. Currently only ELF is supported.\n",
                obj_file.c_str());
        abort();
    }

    std::map<SectionRef, std::vector<RelocationRef>> relocMap = GetRelocationMap(*obj);

    {
        std::map<std::string, BoilerplateInstance*> symbolToInstance;
        for (auto it = g_allBoilerplatePacks.begin(); it != g_allBoilerplatePacks.end(); it++)
        {
            BoilerplatePack& bp = it->second;
            for (BoilerplateInstance& inst : bp.m_instances)
            {
                ReleaseAssert(!symbolToInstance.count(inst.m_symbolName));
                symbolToInstance[inst.m_symbolName] = &inst;
            }
        }

        for (auto it = relocMap.begin(); it != relocMap.end(); it++)
        {
            SectionRef sec = it->first;
            if (sec.isText())
            {
                Expected<StringRef> expectedName = sec.getName();
                if (!expectedName)
                {
                    fprintf(stderr, "[INTERNAL ERROR] Failed to get a name for a section. Please report a bug.\n");
                    abort();
                }
                std::string name = expectedName.get().str();
                std::string pref = ".text.";
                if (name.length() >= pref.length() && name.substr(0, pref.length()) == pref)
                {
                    std::string symbol = name.substr(pref.length());
                    if (symbolToInstance.count(symbol))
                    {
                        BoilerplateInstance* inst = symbolToInstance[symbol];
                        if (inst == nullptr)
                        {
                            fprintf(stderr, "[INTERNAL ERROR] Unexpected: encountered two sections with same symbol name %s. "
                                            "Please report a bug.\n", name.c_str());
                            abort();
                        }
                        symbolToInstance[symbol] = nullptr;
                        inst->m_llvmSectionRef = sec;
                    }
                }
            }
        }

        for (auto it = symbolToInstance.begin(); it != symbolToInstance.end(); it++)
        {
            if (it->second != nullptr)
            {
                fprintf(stderr, "[INTERNAL ERROR] Unable to locate text section for symbol %s in object file. "
                                "Please report a bug.\n", it->first.c_str());
                abort();
            }
        }
    }

    for (auto it = g_allBoilerplatePacks.begin(); it != g_allBoilerplatePacks.end(); it++)
    {
        BoilerplatePack& bp = it->second;
        for (BoilerplateInstance& inst : bp.m_instances)
        {
            SectionRef sec = inst.m_llvmSectionRef;
            std::vector<RelocationRef>& relocs = relocMap[sec];
            for (const RelocationRef& rel : relocs)
            {
                RelocationInfo info = GetRelocationInfo(elf, rel);
                inst.m_relocationInfo.push_back(info);
            }
        }
    }

    std::string output1_name = output_dir + "/fastinterp_fwd_declarations.generated.h";
    FILE* fp1 = fopen(output1_name.c_str(), "w");
    if (fp1 == nullptr)
    {
        fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                output1_name.c_str(), errno, strerror(errno));
        abort();
    }

    std::string output2_name = output_dir + "/fastinterp_library.generated.h";
    FILE* fp2 = fopen(output2_name.c_str(), "w");
    if (fp2 == nullptr)
    {
        fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                output2_name.c_str(), errno, strerror(errno));
        abort();
    }

    std::string output3_name = output_dir + "/fastinterp_library.generated.cpp";
    FILE* fp3 = fopen(output3_name.c_str(), "w");
    if (fp3 == nullptr)
    {
        fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                output3_name.c_str(), errno, strerror(errno));
        abort();
    }

    fprintf(fp1, "// GENERATED FILE, DO NOT EDIT!\n//\n\n#pragma once\n");
    fprintf(fp1, "#include \"fastinterp/fast_interp_generated_header_helper.h\"\n\n");
    fprintf(fp1, "namespace PochiVM {\n\n");

    fprintf(fp2, "// GENERATED FILE, DO NOT EDIT!\n//\n\n#pragma once\n");
    fprintf(fp2, "#include \"fastinterp_fwd_declarations.generated.h\"\n\n");
    fprintf(fp2, "namespace PochiVM {\n\n");

    fprintf(fp2, "template<typename T>\n");
    fprintf(fp2, "struct FastInterpBoilerplateLibrary;    // unspecialized class intentionally undefined\n\n");

    fprintf(fp3, "// GENERATED FILE, DO NOT EDIT!\n//\n\n");
    fprintf(fp3, "#include \"fastinterp_library.generated.h\"\n\n");
    fprintf(fp3, "namespace PochiVM {\n\n");

    const std::string lib_varname_prefix = "__pochivm_fastinterp_boilerplate_lib_";
    const std::string blueprint_varname_prefix = "__pochivm_fastinterp_boilerplate_blueprint_";
    int blueprint_varname_suffix = 0;
    int lib_varname_suffix = 0;

    fprintf(fp2, "// Declarations of the library index types. We only need the typename as an index into the library, no definitions needed\n//\n");
    for (auto it = g_allBoilerplatePacks.begin(); it != g_allBoilerplatePacks.end(); it++)
    {
        fprintf(fp2, "struct %s;\n", it->first.c_str());
    }
    fprintf(fp2, "\n");
    for (auto it = g_allBoilerplatePacks.begin(); it != g_allBoilerplatePacks.end(); it++)
    {
        BoilerplatePack& bp = it->second;
        ReleaseAssert(bp.m_instances.size() > 0);
        int begin_suffix = blueprint_varname_suffix;
        for (BoilerplateInstance& inst : bp.m_instances)
        {
            SectionRef section = inst.m_llvmSectionRef;
            uint64_t size = section.getSize();
            Expected<StringRef> content = section.getContents();
            if (!content)
            {
                fprintf(stderr, "[INTERNAL ERROR] Failed to get content for section for symbol %s. Please report a bug.\n",
                        inst.m_symbolName.c_str());
                abort();
            }
            StringRef& sr = content.get();
            ReleaseAssert(sr.size() == size);
            uint8_t* data = new uint8_t[size];
            Auto(delete [] data);
            memcpy(data, sr.data(), size);

            for (const RelocationInfo& rinfo : inst.m_relocationInfo)
            {
                // TODO: support more types as we see them.
                // Reference: https://refspecs.linuxbase.org/elf/x86_64-abi-0.98.pdf Page 69
                //
                if (!(rinfo.type == ELF::R_X86_64_64 || rinfo.type == ELF::R_X86_64_PLT32 || rinfo.type == ELF::R_X86_64_TPOFF32))
                {
                    fprintf(stderr, "[INTERNAL] Unhandled relocation type %s(%d) in symbol %s. "
                                    "We haven't taken care of this case. Please report a bug.\n",
                            rinfo.typeHumanReadableName.c_str(), static_cast<int>(rinfo.type), inst.m_symbolName.c_str());
                    abort();
                }

                if (rinfo.type == ELF::R_X86_64_PLT32)
                {
                    ReleaseAssert(UnalignedRead<uint32_t>(data + rinfo.offset) == 0);
                }
                else if (rinfo.type == ELF::R_X86_64_64)
                {
                    ReleaseAssert(UnalignedRead<uint64_t>(data + rinfo.offset) == 0);
                }
                else if (rinfo.type == ELF::R_X86_64_TPOFF32)
                {
                    if (rinfo.symbol != "__pochivm_thread_fastinterp_context")
                    {
                        fprintf(stderr, "Unknown use of thread-local variable in symbol %s, "
                                        "thread-local variable symbol name = %s\n", inst.m_symbolName.c_str(), rinfo.symbol.c_str());
                        abort();
                    }
                }
                else
                {
                    ReleaseAssert(false);
                }
            }

            std::vector<uint32_t> minusAddrOffsets32;
            std::vector<std::pair<uint32_t /*offset*/, uint32_t /*ordinal*/>> bpfpList32, bpfpList64, cfpList64, u64List64, allList32, allList64, tpoff32List;
            uint32_t highestBPFPOrdinal = 0;
            uint32_t highestCFPOrdinal = 0;
            uint32_t highestU64Ordinal = 0;
            uint64_t usedBpfpMask = 0;
            uint64_t usedCfpMask = 0;
            uint64_t usedU64Mask = 0;
            for (const RelocationInfo& rinfo : inst.m_relocationInfo)
            {
                ReleaseAssert(rinfo.offset < size);
                if (rinfo.type == ELF::R_X86_64_PLT32)
                {
                    ReleaseAssert(rinfo.offset + 4 <= size);
                    uint32_t addend = static_cast<uint32_t>(-static_cast<int32_t>(static_cast<uint32_t>(rinfo.offset)));
                    uint32_t addend2 = static_cast<uint32_t>(static_cast<uint64_t>(rinfo.addend));
                    addend += addend2;
                    UnalignedAddAndWriteback<uint32_t>(data + rinfo.offset, addend);
                    minusAddrOffsets32.push_back(static_cast<uint32_t>(rinfo.offset));
                }
                else if (rinfo.type == ELF::R_X86_64_64)
                {
                    ReleaseAssert(rinfo.offset + 8 <= size);
                    uint64_t addend = static_cast<uint64_t>(rinfo.addend);
                    UnalignedAddAndWriteback<uint64_t>(data + rinfo.offset, addend);
                }
                else if (rinfo.type == ELF::R_X86_64_TPOFF32)
                {
                    tpoff32List.push_back(std::make_pair(rinfo.offset, 0 /*ordinal*/));
                    continue;
                }
                else
                {
                    ReleaseAssert(false);
                }

                uint32_t ord;
                if (IsBoilerplateFnPtrPlaceholder(rinfo.symbol, ord /*out*/))
                {
                    ReleaseAssert(ord < 64);
                    highestBPFPOrdinal = std::max(highestBPFPOrdinal, ord + 1);
                    if (rinfo.type == ELF::R_X86_64_PLT32)
                    {
                        bpfpList32.push_back(std::make_pair(rinfo.offset, ord));
                    }
                    else if (rinfo.type == ELF::R_X86_64_64)
                    {
                        bpfpList64.push_back(std::make_pair(rinfo.offset, ord));
                    }
                    else
                    {
                        ReleaseAssert(false);
                    }
                    usedBpfpMask |= (1ULL << ord);
                }
                else if (IsCppFnPtrPlaceholder(rinfo.symbol, ord /*out*/))
                {
                    ReleaseAssert(ord < 64);
                    highestCFPOrdinal = std::max(highestCFPOrdinal, ord + 1);
                    ReleaseAssert(rinfo.type == ELF::R_X86_64_64);
                    cfpList64.push_back(std::make_pair(rinfo.offset, ord));
                    usedCfpMask |= (1ULL << ord);
                }
                else if (IsU64Placeholder(rinfo.symbol, ord /*out*/))
                {
                    ReleaseAssert(ord < 64);
                    highestU64Ordinal = std::max(highestU64Ordinal, ord + 1);
                    ReleaseAssert(rinfo.type == ELF::R_X86_64_64);
                    u64List64.push_back(std::make_pair(rinfo.offset, ord));
                    usedU64Mask |= (1ULL << ord);
                }
                else
                {
                    // TODO: handle external symbols
                    fprintf(stderr, "%s %s\n", rinfo.typeHumanReadableName.c_str(), rinfo.symbol.c_str());
                    ReleaseAssert(false);
                }
            }

            for (std::pair<uint32_t, uint32_t>& p : cfpList64)
            {
                p.second += highestBPFPOrdinal;
            }

            for (std::pair<uint32_t, uint32_t>& p : u64List64)
            {
                p.second += highestBPFPOrdinal + highestCFPOrdinal;
            }

            for (std::pair<uint32_t, uint32_t>& p : bpfpList32)
            {
                allList32.push_back(p);
            }

            for (std::pair<uint32_t, uint32_t>& p : bpfpList64)
            {
                allList64.push_back(p);
            }

            for (std::pair<uint32_t, uint32_t>& p : cfpList64)
            {
                allList64.push_back(p);
            }

            for (std::pair<uint32_t, uint32_t>& p : u64List64)
            {
                allList64.push_back(p);
            }

            std::sort(allList32.begin(), allList32.end());
            std::sort(allList64.begin(), allList64.end());
            std::sort(minusAddrOffsets32.begin(), minusAddrOffsets32.end());

            int usedCfpOrdinalCnt = 0;
            std::vector<int> cfpOrdinalMap;
            cfpOrdinalMap.resize(highestCFPOrdinal);
            for (uint32_t i = 0; i < highestCFPOrdinal; i++)
            {
                if ((usedCfpMask & (1ULL <<i)) > 0)
                {
                    cfpOrdinalMap[i] = usedCfpOrdinalCnt;
                    usedCfpOrdinalCnt++;
                }
                else
                {
                    cfpOrdinalMap[i] = -1;
                }
            }

            fprintf(fp3, "static uint8_t %s%d_contents[%d] = {\n", blueprint_varname_prefix.c_str(), blueprint_varname_suffix,
                    static_cast<int>(size));
            for (size_t i = 0; i < size; i++)
            {
                fprintf(fp3, "%d", static_cast<int>(data[i]));
                if (i + 1 < size) {
                    fprintf(fp3, ", ");
                }
                if (i % 16 == 15) {
                    fprintf(fp3, "\n");
                }
            }
            fprintf(fp3, "\n};\n");

            ReleaseAssert(size < (1ULL << 31));
            fprintf(fp3, "constexpr uint32_t %s%d_contentLength = %d;\n",
                    blueprint_varname_prefix.c_str(), blueprint_varname_suffix, static_cast<int>(size));

            // Do startup-time fixup of external symbols
            // In the same translational unit, global initializers are executed in the same order they are declared.
            // (across translational unit there is no guarantee in initialization order)
            // So it is important to put this initializer in the same CPP file as the "contents" array, and after the array,
            // so at the time this global initializer is executed, the array is already initialized.
            //
            if (tpoff32List.size() > 0)
            {
                fprintf(fp3, "static FastInterpInitFixupThreadLocalHelper __pochivm_fastinterp_internal_fixup_thread_local_%d(\n", blueprint_varname_suffix);
                fprintf(fp3, "    %s%d_contents /*contentArray*/,\n", blueprint_varname_prefix.c_str(), blueprint_varname_suffix);
                fprintf(fp3, "    %s%d_contentLength /*contentArrayLength*/,\n", blueprint_varname_prefix.c_str(), blueprint_varname_suffix);
                fprintf(fp3, "    std::array<size_t, %d>{\n", static_cast<int>(tpoff32List.size()));
                fprintf(fp3, "        ");
                for (size_t i = 0; i < tpoff32List.size(); i++)
                {
                    fprintf(fp3, "%d", static_cast<int>(tpoff32List[i].first));
                    if (i + 1 < tpoff32List.size()) {
                        fprintf(fp3, ", ");
                        if (i % 8 == 0) {
                            fprintf(fp3, "\n        ");
                        }
                    }
                }
                fprintf(fp3, "\n    } /*fixupSites*/);\n");
            }

            fprintf(fp3, "constexpr FastInterpBoilerplateBluePrintWrapper<%d, %d, %d, %d> %s%d(\n",
                    static_cast<int>(minusAddrOffsets32.size()),
                    static_cast<int>(allList32.size()),
                    static_cast<int>(allList64.size()),
                    static_cast<int>(highestCFPOrdinal),
                    blueprint_varname_prefix.c_str(),
                    blueprint_varname_suffix);
            fprintf(fp3, "%s%d_contents /*contents*/,\n", blueprint_varname_prefix.c_str(), blueprint_varname_suffix);
            fprintf(fp3, "%s%d_contentLength /*contentLength*/,\n", blueprint_varname_prefix.c_str(), blueprint_varname_suffix);

            fprintf(fp3, "std::array<uint32_t, %d>{", static_cast<int>(minusAddrOffsets32.size()));
            for (size_t i = 0; i < minusAddrOffsets32.size(); i++)
            {
                fprintf(fp3, "%d", static_cast<int>(minusAddrOffsets32[i]));
                if (i + 1 < minusAddrOffsets32.size()) {
                    fprintf(fp3, ", ");
                }
            }
            fprintf(fp3, "} /*addr32FixupArray*/,\n");

            fprintf(fp3, "std::array<FastInterpSymbolFixupRecord, %d>{", static_cast<int>(allList32.size()));
            for (size_t i = 0; i < allList32.size(); i++)
            {
                fprintf(fp3, "FastInterpSymbolFixupRecord{%d, %d}", static_cast<int>(allList32[i].first), static_cast<int>(allList32[i].second));
                if (i + 1 < allList32.size()) {
                    fprintf(fp3, ", ");
                }
                fprintf(fp3, "\n");
            }
            fprintf(fp3, "} /*symbol32FixupArray*/,\n");

            fprintf(fp3, "std::array<FastInterpSymbolFixupRecord, %d>{", static_cast<int>(allList64.size()));
            for (size_t i = 0; i < allList64.size(); i++)
            {
                fprintf(fp3, "FastInterpSymbolFixupRecord{%d, %d}", static_cast<int>(allList64[i].first), static_cast<int>(allList64[i].second));
                if (i + 1 < allList64.size()) {
                    fprintf(fp3, ", ");
                }
                fprintf(fp3, "\n");
            }
            fprintf(fp3, "} /*symbol64FixupArray*/,\n");

            fprintf(fp3, "%d /*highestBoilerplateFnptrPlaceholderOrdinal*/,\n", static_cast<int>(highestBPFPOrdinal));
            fprintf(fp3, "%d /*highestUInt64PlaceholderOrdinal*/,\n", static_cast<int>(highestU64Ordinal));
            fprintf(fp3, "%d /*numCppFnPtrPlaceholders*/,\n", static_cast<int>(usedCfpOrdinalCnt));

            fprintf(fp3, "std::array<uint16_t, %d>{", static_cast<int>(cfpOrdinalMap.size()));
            for (size_t i = 0; i < cfpOrdinalMap.size(); i++)
            {
                fprintf(fp3, "static_cast<uint16_t>(%d)", static_cast<int>(cfpOrdinalMap[i]));
                if (i + 1 < cfpOrdinalMap.size()) {
                    fprintf(fp3, ", ");
                }
            }
            fprintf(fp3, "} /*cppFnPtrPlaceholderOrdinalToId*/\n");

            fprintf(fp3, "#ifdef TESTBUILD\n");
            fprintf(fp3, ", %lluULL /*usedBoilerplateFnPtrPlaceholderMask*/\n", static_cast<unsigned long long>(usedBpfpMask));
            fprintf(fp3, ", %lluULL /*usedCppFnptrPlaceholderMask*/\n", static_cast<unsigned long long>(usedCfpMask));
            fprintf(fp3, ", %lluULL /*usedUInt64PlaceholderMask*/\n", static_cast<unsigned long long>(usedU64Mask));
            fprintf(fp3, ", \"%s\" /*symbolName*/\n", inst.m_symbolName.c_str());
            fprintf(fp3, "#endif\n");
            fprintf(fp3, ");\n\n");

            blueprint_varname_suffix++;
        }
        ReleaseAssert(begin_suffix + static_cast<int>(bp.m_instances.size()) == blueprint_varname_suffix);

        size_t n = bp.m_params.size();
        if (n == 0)
        {
            // TODO: handle
            ReleaseAssert(false);
        }
        else
        {
            CuckooHashTable ht = GetCuckooHashTable(n, bp.m_instances);
            fprintf(fp1, "extern const FastInterpBoilerplateSelectionHashTableHelperWrapper<%d, %d> %s%d;\n",
                    static_cast<int>(n), static_cast<int>(ht.ht.size()), lib_varname_prefix.c_str(), lib_varname_suffix);

            fprintf(fp3, "constexpr FastInterpBoilerplateSelectionHashTableHelperWrapper<%d, %d> %s%d(\n",
                    static_cast<int>(n), static_cast<int>(ht.ht.size()), lib_varname_prefix.c_str(), lib_varname_suffix);
            fprintf(fp3, "std::array<uint32_t, %d> {", static_cast<int>(n * 3));
            ReleaseAssert(ht.h1.size() == n && ht.h2.size() == n && ht.h3.size() == n);
            for (size_t i = 0; i < ht.h1.size(); i++)
            {
                fprintf(fp3, "%lluU", static_cast<unsigned long long>(ht.h1[i]));
                fprintf(fp3, ", ");
            }
            for (size_t i = 0; i < ht.h2.size(); i++)
            {
                fprintf(fp3, "%lluU", static_cast<unsigned long long>(ht.h2[i]));
                fprintf(fp3, ", ");
            }
            for (size_t i = 0; i < ht.h3.size(); i++)
            {
                fprintf(fp3, "%lluU", static_cast<unsigned long long>(ht.h3[i]));
                if (i + 1 < ht.h3.size()) {
                    fprintf(fp3, ", ");
                }
            }
            fprintf(fp3, "} /*hashfns*/,\n");
            fprintf(fp3, "std::array<FastInterpBoilerplateSelectionHashTableEntry, %d> {\n", static_cast<int>(ht.ht.size()));
            for (size_t i = 0; i < ht.ht.size(); i++)
            {
                fprintf(fp3, "FastInterpBoilerplateSelectionHashTableEntry {");
                if (ht.ht[i].first == static_cast<uint32_t>(-1))
                {
                    fprintf(fp3, "nullptr");
                }
                else
                {
                    fprintf(fp3, "&%s%d", blueprint_varname_prefix.c_str(), begin_suffix + static_cast<int>(ht.ht[i].first));
                }
                fprintf(fp3, ", %lluULL }", static_cast<unsigned long long>(ht.ht[i].second));
                if (i + 1 < ht.ht.size()) {
                    fprintf(fp3, ", ");
                }
                fprintf(fp3, "\n");
            }
            fprintf(fp3, "} /*hashtable*/\n");
            fprintf(fp3, "#ifdef TESTBUILD\n");
            fprintf(fp3, ", std::array<uint64_t, %d> {", static_cast<int>(n * ht.ht.size()));
            ReleaseAssert(ht.ht.size() == ht.trueEntries.size());
            {
                int cnt = 0;
                for (size_t i = 0; i < ht.trueEntries.size(); i++)
                {
                    ReleaseAssert(ht.trueEntries[i].size() == n);
                    for (size_t j = 0; j < n; j++)
                    {
                        fprintf(fp3, "%lluULL", static_cast<unsigned long long>(ht.trueEntries[i][j]));
                        if (i + 1 < ht.trueEntries.size() || j + 1 < n)
                        {
                            fprintf(fp3, ", ");
                        }
                        cnt++;
                        if (cnt % 16 == 15)
                        {
                            fprintf(fp3, "\n");
                        }
                    }
                }
            }
            fprintf(fp3, "\n} /*trueEntries*/\n");
            fprintf(fp3, "#endif\n");
            fprintf(fp3, ");\n\n");

            fprintf(fp2, "template<>\nstruct FastInterpBoilerplateLibrary<%s>\n{\n",
                    it->first.c_str());
            fprintf(fp2, "    static const FastInterpBoilerplateBluePrint* SelectBoilerplateBluePrint(\n");
            for (size_t i = 0; i < bp.m_params.size(); i++)
            {
                BoilerplateParam param = bp.m_params[i];
                fprintf(fp2, "        %s %s", param.m_typename.c_str(), param.m_name.c_str());
                if (i + 1 < bp.m_params.size())
                {
                    fprintf(fp2, ",");
                }
                else
                {
                    fprintf(fp2, ")");
                }
                fprintf(fp2, "\n");
            }
            fprintf(fp2, "    {\n");
            fprintf(fp2, "        return %s%d.SelectBoilerplateBluePrint(std::array<uint64_t, %d> {\n",
                    lib_varname_prefix.c_str(), lib_varname_suffix, static_cast<int>(n));
            for (size_t i = 0; i < bp.m_params.size(); i++)
            {
                BoilerplateParam param = bp.m_params[i];
                if (param.m_type == PochiVM::MetaVarType::PRIMITIVE_TYPE)
                {
                    fprintf(fp2, "            %s.GetTypeId().value", param.m_name.c_str());
                }
                else
                {
                    fprintf(fp2, "            static_cast<uint64_t>(%s)", param.m_name.c_str());
                }
                if (i + 1 < bp.m_params.size())
                {
                    fprintf(fp2, ",");
                }
                fprintf(fp2, "\n");
            }
            fprintf(fp2, "        });\n");
            fprintf(fp2, "    }\n");
            fprintf(fp2, "};\n\n");

            lib_varname_suffix++;
        }
    }

    fprintf(fp1, "} // namespace PochiVM \n\n");
    fprintf(fp2, "} // namespace PochiVM \n\n");
    fprintf(fp3, "} // namespace PochiVM \n\n");

    fclose(fp1);
    fclose(fp2);
    fclose(fp3);

    return 0;
}
