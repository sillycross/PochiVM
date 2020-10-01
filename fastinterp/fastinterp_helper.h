#pragma once

// Internal utility classes. One should always include "fastinterp.h" instead, not this file.
//

#include "pochivm/common.h"
#include "pochivm/ast_type_helper.h"
#include "fastinterp_function_alignment.h"

namespace PochiVM
{

// Utility classes for fast interp.
// Basic workflow:
//
//    const FastInterpBoilerplateBluePrint* boilerplate;
//    boilerplate = FastInterpBoilerplateLibaray<AstNodeType>::SelectBoilerplateBluePrint(...);
//        Selects a boilerplate for 'AstNodeType', using a list of parameters as specified in 'fastinterp_tpl.cpp'
//
//    FastInterpCodegenEngine engine;
//        The manager class to build a FastInterp program
//
//    FastInterpBoilerplateInstance* instance;
//    instance = engine.InstantiateBoilerplate(const FastInterpBoilerplateBluePrint* boilerplate);
//        Put a copy of 'boilerplate' into the generated program, returns reference to the instantiation of the boilerplate.
//
//    engine.RegisterGeneratedFunctionEntryPoint(AstFunction* fn, FastInterpBoilerplateInstance* instance);
//        Register 'instance' as the entry point for AstFunction 'fn'.
//
//    instance->PopulateBoilerplateFnPtrPlaceholder(...)
//    instance->PopulateCppFnPtrPlaceholder(...)
//    instance->PopulateConstantPlaceholder(...)
//        Populate the placeholders in the boilerplate instantiation with concrete values.
//
//    std::unique_ptr<FastInterpGeneratedProgram> generatedProgram;
//    generatedProgram = engine.Materialize();
//        After all boilerplates are instantiated and all placeholders of all instantiations are populated,
//        use the above code to materialize the generated program.
//
//    void* entryPoint = generatedProgram->GetGeneratedFunctionAddress(AstFunction* fn);
//        Returns the function pointer for boilerplate instance registered in 'RegisterGeneratedFunctionEntryPoint'.
//

struct FastInterpSymbolFixupRecord
{
    constexpr FastInterpSymbolFixupRecord()
        : m_offset(0), m_ordinalIntoPlaceholderArray(static_cast<uint32_t>(-1))
    { }

    constexpr FastInterpSymbolFixupRecord(uint32_t offset, uint32_t ord)
        : m_offset(offset), m_ordinalIntoPlaceholderArray(ord)
    { }

    uint32_t m_offset;
    uint32_t m_ordinalIntoPlaceholderArray;
};

// Safe unaligned memory read/write.
//
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

class FastInterpBoilerplateBluePrint : NonCopyable, NonMovable
{
public:
    friend class FastInterpBoilerplateInstance;

    size_t GetCodeSectionLength() const
    {
        return m_contentLength;
    }

    size_t GetDataSectionLength() const
    {
        return m_numCppFnPtrPlaceholders * sizeof(uint64_t);
    }

#ifdef TESTBUILD
    const char* TestOnly_GetSymbolName() const
    {
        return m_symbolName;
    }
#endif

protected:
    constexpr FastInterpBoilerplateBluePrint(
            const uint8_t* content
          , uint32_t contentLength
          , const uint32_t* addr32FixupArray
          , uint32_t addr32FixupArrayLength
          , const FastInterpSymbolFixupRecord* symbol32FixupArray
          , uint32_t symbol32FixupArrayLength
          , const FastInterpSymbolFixupRecord* symbol64FixupArray
          , uint32_t symbol64FixupArrayLength
          , const uint16_t* cppFnPtrPlaceholderOrdinalToId
          , uint16_t highestBoilerplateFnptrPlaceholderOrdinal
          , uint16_t highestCppFnptrPlaceholderOrdinal
          , uint16_t highestUInt64PlaceholderOrdinal
          , uint16_t numCppFnPtrPlaceholders
#ifdef TESTBUILD
          , uint64_t usedBoilerplateFnPtrPlaceholderMask
          , uint64_t usedCppFnptrPlaceholderMask
          , uint64_t usedUInt64PlaceholderMask
          , const char* symbolName
#endif
    )
        : m_content(content)
        , m_addr32FixupArray(addr32FixupArray)
        , m_symbol32FixupArray(symbol32FixupArray)
        , m_symbol64FixupArray(symbol64FixupArray)
        , m_cppFnPtrPlaceholderOrdinalToId(cppFnPtrPlaceholderOrdinalToId)
        , m_contentLength(contentLength)
        , m_addr32FixupArrayLength(addr32FixupArrayLength)
        , m_symbol32FixupArrayLength(symbol32FixupArrayLength)
        , m_symbol64FixupArrayLength(symbol64FixupArrayLength)
        , m_highestBoilerplateFnptrPlaceholderOrdinal(highestBoilerplateFnptrPlaceholderOrdinal)
        , m_highestCppFnptrPlaceholderOrdinal(highestCppFnptrPlaceholderOrdinal)
        , m_highestUInt64PlaceholderOrdinal(highestUInt64PlaceholderOrdinal)
        , m_numCppFnPtrPlaceholders(numCppFnPtrPlaceholders)
#ifdef TESTBUILD
        , m_usedBoilerplateFnPtrPlaceholderMask(usedBoilerplateFnPtrPlaceholderMask)
        , m_usedCppFnptrPlaceholderMask(usedCppFnptrPlaceholderMask)
        , m_usedUInt64PlaceholderMask(usedUInt64PlaceholderMask)
        , m_symbolName(symbolName)
#endif
    { }

private:
    void MaterializeCodeSection(uint8_t* destAddr, uint64_t* fixupValues) const
    {
        TestAssert(reinterpret_cast<uint64_t>(destAddr) % x_fastinterp_function_alignment == 0);
        memcpy(destAddr, m_content, m_contentLength);

        {
            uint32_t addend = static_cast<uint32_t>(-static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uint64_t>(destAddr))));
            for (uint32_t i = 0; i < m_addr32FixupArrayLength; i++)
            {
                UnalignedAddAndWriteback<uint32_t>(destAddr + m_addr32FixupArray[i], addend);
            }
        }

        for (uint32_t i = 0; i < m_symbol32FixupArrayLength; i++)
        {
            uint32_t addend = static_cast<uint32_t>(fixupValues[m_symbol32FixupArray[i].m_ordinalIntoPlaceholderArray]);
            UnalignedAddAndWriteback<uint32_t>(destAddr + m_symbol32FixupArray[i].m_offset, addend);
        }

        for (uint32_t i = 0; i < m_symbol64FixupArrayLength; i++)
        {
            uint64_t addend = static_cast<uint64_t>(fixupValues[m_symbol64FixupArray[i].m_ordinalIntoPlaceholderArray]);
            UnalignedAddAndWriteback<uint64_t>(destAddr + m_symbol64FixupArray[i].m_offset, addend);
        }
    }

    // The pre-fixup binary content
    //
    const uint8_t* m_content;

    // The list of positions that needs to fix up by "-fn_addr", 32bit
    //
    const uint32_t* m_addr32FixupArray;

    // The list of positions that needs to fix up by "+value", 32bit
    //
    const FastInterpSymbolFixupRecord* m_symbol32FixupArray;

    // The list of positions that needs to fix up by "+value", 64bit
    //
    const FastInterpSymbolFixupRecord* m_symbol64FixupArray;

    const uint16_t* m_cppFnPtrPlaceholderOrdinalToId;

    uint32_t m_contentLength;
    uint32_t m_addr32FixupArrayLength;
    uint32_t m_symbol32FixupArrayLength;
    uint32_t m_symbol64FixupArrayLength;

    uint16_t m_highestBoilerplateFnptrPlaceholderOrdinal;
    uint16_t m_highestCppFnptrPlaceholderOrdinal;
    uint16_t m_highestUInt64PlaceholderOrdinal;
    uint16_t m_numCppFnPtrPlaceholders;

#ifdef TESTBUILD
    // bitmask denoting which placeholder ordinals are used, for test assertion purpose
    //
    uint64_t m_usedBoilerplateFnPtrPlaceholderMask;
    uint64_t m_usedCppFnptrPlaceholderMask;
    uint64_t m_usedUInt64PlaceholderMask;
    // The symbol name of the original function that generated this boilerplate
    //
    const char* m_symbolName;
#endif
};

class FastInterpBoilerplateInstance : NonCopyable, NonMovable
{
public:
    friend class FastInterpCodegenEngine;

    void PopulateBoilerplateFnPtrPlaceholder(uint32_t ordinal, FastInterpBoilerplateInstance* value)
    {
        TestAssert(ordinal < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal && value != nullptr);
        TestAssert(!(m_populatedBoilerplateFnPtrPlaceholderMask & (1ULL << ordinal)));
        TestAssert(m_owner->m_usedBoilerplateFnPtrPlaceholderMask & (1ULL << ordinal));
#ifdef TESTBUILD
        m_populatedBoilerplateFnPtrPlaceholderMask |= (1ULL << ordinal);
#endif
        m_fixupValues[ordinal] = reinterpret_cast<uint64_t>(value->m_relativeCodeAddr);
    }

    template<typename R, typename... Args>
    void PopulateCppFnPtrPlaceholder(uint32_t ordinal, R(*value)(Args...) noexcept)
    {
        // Not allowed to hold null pointer
        //
        TestAssert(value != nullptr);
        TestAssert(ordinal < m_owner->m_highestCppFnptrPlaceholderOrdinal);
        TestAssert(!(m_populatedCppFnptrPlaceholderMask & (1ULL << ordinal)));
        TestAssert(m_owner->m_usedCppFnptrPlaceholderMask & (1ULL << ordinal));
#ifdef TESTBUILD
        m_populatedCppFnptrPlaceholderMask |= (1ULL << ordinal);
#endif
        m_dataValues[m_owner->m_cppFnPtrPlaceholderOrdinalToId[ordinal]] = reinterpret_cast<uint64_t>(value);
        m_fixupValues[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal + ordinal] = reinterpret_cast<uint64_t>(
                    m_relativeDataAddr + m_owner->m_cppFnPtrPlaceholderOrdinalToId[ordinal] * sizeof(uint64_t));
    }

    // The constant type must match what is defined in the boilerplate
    //
    template<typename T>
    void PopulateConstantPlaceholder(uint32_t ordinal, T value)
    {
        static_assert(sizeof(T) <= 8 && (std::is_fundamental<T>::value || std::is_pointer<T>::value) &&
                (!std::is_function<typename std::remove_pointer<T>::type>::value), "must be a primitive type");
        // If T is 8-byte long, it is not allowed to pass in 0
        //
        TestAssertImp(sizeof(T) == 8, !is_all_underlying_bits_zero(value));
        // It is possible that we populate a constant placeholder that does not exist in boilerplate:
        // the compiler might have optimized it out. For example, comparison of literal >= 0 of unsigned type,
        // which is trivially true so the compiler may have optimized out the constant 'literal'.
        // This should only happen to constant: functions may have side effects so compiler will never optimize it out,
        // and currently we never do function calls inside a condition control flow that may be optimized out.
        //
        TestAssert(ordinal < 64);
        TestAssert(!(m_populatedUInt64PlaceholderMask & (1ULL << ordinal)));
#ifdef TESTBUILD
        m_populatedUInt64PlaceholderMask |= (1ULL << ordinal);
#endif
        // We just allocated enough memory for that many ordinals.
        // If it is a ordinal that has been optimized out by compiler, don't write.
        //
        if (ordinal < m_owner->m_highestUInt64PlaceholderOrdinal)
        {
            union U {
                uint64_t u64v;
                T tv;
            };
            U u;
            u.u64v = 0;
            u.tv = value;
            m_fixupValues[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
                    m_owner->m_highestCppFnptrPlaceholderOrdinal + ordinal] = u.u64v;
        }
    }

private:
    FastInterpBoilerplateInstance(const FastInterpBoilerplateBluePrint* owner,
                                  uintptr_t relativeCodeAddr,
                                  uintptr_t relativeDataAddr)
        : m_relativeCodeAddr(relativeCodeAddr)
        , m_relativeDataAddr(relativeDataAddr)
        , m_owner(owner)
#ifdef TESTBUILD
        , m_populatedBoilerplateFnPtrPlaceholderMask(0)
        , m_populatedUInt64PlaceholderMask(0)
        , m_populatedCppFnptrPlaceholderMask(0)
        , m_isMaterialized(false)
#endif
    {
        m_fixupValues = new uint64_t[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
                m_owner->m_highestUInt64PlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal];
        m_dataValues = new uint64_t[m_owner->GetDataSectionLength() / sizeof(uint64_t)];
    }

    void Materialize(uintptr_t baseAddress)
    {
        TestAssert(!m_isMaterialized);
        TestAssert(baseAddress % x_fastinterp_function_alignment == 0);
        TestAssert(m_populatedBoilerplateFnPtrPlaceholderMask == m_owner->m_usedBoilerplateFnPtrPlaceholderMask);
        TestAssert((m_populatedUInt64PlaceholderMask & m_owner->m_usedUInt64PlaceholderMask) == m_owner->m_usedUInt64PlaceholderMask);
        TestAssert(m_populatedCppFnptrPlaceholderMask == m_owner->m_usedCppFnptrPlaceholderMask);
#ifdef TESTBUILD
        m_isMaterialized = true;
#endif
        // The 'boilerplate' and 'cpp_fn' fixups are all relative pointers to the final baseAddress
        // Now the base address is known, add it so we get the true pointer
        //
        for (uint32_t i = 0; i < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal; i++)
        {
            m_fixupValues[i] += baseAddress;
        }
        uint8_t* trueCodeBaseAddress = reinterpret_cast<uint8_t*>(baseAddress + m_relativeCodeAddr);
        uint8_t* trueDataBaseAddress = reinterpret_cast<uint8_t*>(baseAddress + m_relativeDataAddr);
        memcpy(trueDataBaseAddress, m_dataValues, m_owner->GetDataSectionLength());
        m_owner->MaterializeCodeSection(trueCodeBaseAddress, m_fixupValues);
    }

    uintptr_t m_relativeCodeAddr;
    uintptr_t m_relativeDataAddr;

    const FastInterpBoilerplateBluePrint* m_owner;

    // An array of length m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
    // m_owner->m_highestUInt64PlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal
    //
    uint64_t* m_fixupValues;

    // An array of length m_owner->GetDataSectionLength / sizeof(uint64_t)
    //
    uint64_t* m_dataValues;

#ifdef TESTBUILD
    uint64_t m_populatedBoilerplateFnPtrPlaceholderMask;
    uint64_t m_populatedUInt64PlaceholderMask;
    uint64_t m_populatedCppFnptrPlaceholderMask;
    bool m_isMaterialized;
#endif
};

class AstFunction;

// A owning generated program.
// When this class is destructed, the program is gone.
// So it is undefined behavior if this class is destructed while the program is still running.
//
class FastInterpGeneratedProgram : NonCopyable, NonMovable
{
public:
    friend class FastInterpCodegenEngine;

    ~FastInterpGeneratedProgram()
    {
        munmap(m_addr, m_length);
    }

    void* GetGeneratedFunctionAddress(AstFunction* fn)
    {
        auto it = m_fnEntryPoint.find(fn);
        if (it == m_fnEntryPoint.end()) {
            return nullptr;
        } else {
            TestAssert(it->second != nullptr);
            return it->second;
        }
    }

private:
    FastInterpGeneratedProgram(void* addr, size_t length, std::unordered_map<AstFunction*, void*>&& fnEntryPoint)
        : m_addr(addr), m_length(length), m_fnEntryPoint(std::move(fnEntryPoint))
    {
        TestAssert(m_addr != nullptr && reinterpret_cast<uintptr_t>(m_addr) % 4096 == 0 && m_length % 4096 == 0);
    }

    void* m_addr;
    size_t m_length;
    std::unordered_map<AstFunction*, void*> m_fnEntryPoint;
};

class FastInterpCodegenEngine : NonCopyable, NonMovable
{
public:
    FastInterpCodegenEngine()
        : m_codeSectionLength(0)
        , m_dataSectionLength(0)
#ifdef TESTBUILD
        , m_materialized(false)
#endif
    { }

    FastInterpBoilerplateInstance* WARN_UNUSED InstantiateBoilerplate(const FastInterpBoilerplateBluePrint* boilerplate)
    {
        // Code section grows up from base address
        //
        TestAssert(m_codeSectionLength % x_fastinterp_function_alignment == 0);
        uintptr_t relativeCodeSectionBase = m_codeSectionLength;
        m_codeSectionLength += boilerplate->GetCodeSectionLength();
        // Data section grows down from base address,
        // its data section range is [-(oldDSLength + thisDsLength), -oldDsLength)
        //
        m_dataSectionLength += boilerplate->GetDataSectionLength();
        uintptr_t relativeDataSectionBase = static_cast<uintptr_t>(-static_cast<ssize_t>(m_dataSectionLength));
        FastInterpBoilerplateInstance* inst = new FastInterpBoilerplateInstance(boilerplate, relativeCodeSectionBase, relativeDataSectionBase);
        m_allBoilerplateInstances.push_back(inst);
        return inst;
    }

    void RegisterGeneratedFunctionEntryPoint(AstFunction* fn, FastInterpBoilerplateInstance* inst)
    {
        TestAssert(!m_functionEntryPoint.count(fn));
        m_functionEntryPoint[fn] = inst;
    }

    FastInterpBoilerplateInstance* WARN_UNUSED GetGeneratedFunctionEntryPoint(AstFunction* fn)
    {
        auto it = m_functionEntryPoint.find(fn);
        if (it == m_functionEntryPoint.end()) {
            return nullptr;
        } else {
            return it->second;
        }
    }

    // Materialize the generated program. All placeholders must have been populated.
    // If anything failed, return nullptr.
    // After calling this function, this class no longer needs to exist.
    //
    std::unique_ptr<FastInterpGeneratedProgram> WARN_UNUSED Materialize()
    {
        TestAssert(!m_materialized);
#ifdef TESTBUILD
        m_materialized = true;
#endif
        size_t dataSectionSize = (m_dataSectionLength + 4095) / 4096 * 4096;
        size_t codeSectionSize = (m_codeSectionLength + 4095) / 4096 * 4096;
        size_t mmapLength = dataSectionSize + codeSectionSize;
        void* mmapResult = mmap(nullptr, mmapLength, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        if (mmapResult == MAP_FAILED) {
            return std::unique_ptr<FastInterpGeneratedProgram>(nullptr);
        }

        bool success = false;
        Auto(
            if (!success) {
                munmap(mmapResult, mmapLength);
            }
        );

        uintptr_t baseAddress = reinterpret_cast<uintptr_t>(mmapResult) + dataSectionSize;
        for (FastInterpBoilerplateInstance* instance : m_allBoilerplateInstances)
        {
            instance->Materialize(baseAddress);
        }

        std::unordered_map<AstFunction*, void*> entryPointMap;
        for (auto it = m_functionEntryPoint.begin(); it != m_functionEntryPoint.end(); it++)
        {
            TestAssert(!entryPointMap.count(it->first));
            entryPointMap[it->first] = reinterpret_cast<void*>(baseAddress + it->second->m_relativeCodeAddr);
        }

        {
            int r = mprotect(mmapResult, dataSectionSize, PROT_READ);
            if (r != 0) {
                return std::unique_ptr<FastInterpGeneratedProgram>(nullptr);
            }
        }

        {
            int r = mprotect(reinterpret_cast<void*>(baseAddress), codeSectionSize, PROT_READ | PROT_EXEC);
            if (r != 0) {
                return std::unique_ptr<FastInterpGeneratedProgram>(nullptr);
            }
        }

        InvalidateInstructionCache(reinterpret_cast<void*>(baseAddress), codeSectionSize);

        std::unique_ptr<FastInterpGeneratedProgram> ret(new FastInterpGeneratedProgram(mmapResult, mmapLength, std::move(entryPointMap)));
        success = true;
        return ret;
    }

private:
    void InvalidateInstructionCache(const void* addr, size_t len);

    std::unordered_map<AstFunction*, FastInterpBoilerplateInstance*> m_functionEntryPoint;
    std::vector<FastInterpBoilerplateInstance*> m_allBoilerplateInstances;
    size_t m_codeSectionLength;
    size_t m_dataSectionLength;
#ifdef TESTBUILD
    bool m_materialized;
#endif
};

struct FastInterpBoilerplateSelectionHashTableEntry
{
    constexpr FastInterpBoilerplateSelectionHashTableEntry()
        : m_value(nullptr), m_fingerprint(static_cast<uint64_t>(-1))
    { }

    constexpr FastInterpBoilerplateSelectionHashTableEntry(const FastInterpBoilerplateBluePrint* value, uint64_t fingerprint)
        : m_value(value), m_fingerprint(fingerprint)
    { }

    const FastInterpBoilerplateBluePrint* m_value;
    uint64_t m_fingerprint;
};

class FastInterpBoilerplateSelectionHashTableHelper : NonCopyable, NonMovable
{
public:
    template<typename T>
    friend struct FastInterpBoilerplateLibrary;

protected:
    constexpr FastInterpBoilerplateSelectionHashTableHelper(
            uint32_t numElements, uint32_t hashtableSize, uint32_t* hashFns
          , FastInterpBoilerplateSelectionHashTableEntry* hashtable
#ifdef TESTBUILD
          , uint64_t* trueEntries
#endif
    )
        : m_numElements(numElements)
        , m_hashtableSize(hashtableSize)
        , m_hashFns(hashFns)
        , m_hashtable(hashtable)
#ifdef TESTBUILD
        , m_trueEntries(trueEntries)
#endif
    { }

private:
    // It is a programming error to select a non-existent boilerplate!
    //
    template<size_t N>
    const FastInterpBoilerplateBluePrint* SelectBoilerplateBluePrint(const std::array<uint64_t, N>& key) const
    {
        static_assert(N > 0, "Bad N");
        TestAssert(N == m_numElements);
        FastInterpBoilerplateSelectionHashTableEntry* h1 = m_hashtable + ComputeHash<0>(key) % m_hashtableSize;
        FastInterpBoilerplateSelectionHashTableEntry* h2 = m_hashtable + ComputeHash<1>(key) % m_hashtableSize;
        uint64_t h3 = ComputeHash<2>(key);
        // h3 is always selected so that it's not -1 for all keys in hash table
        //
        TestAssert(h3 != static_cast<uint64_t>(-1));
        FastInterpBoilerplateSelectionHashTableEntry* result = nullptr;
        if (h1->m_fingerprint == h3)
        {
            // h3 is always selected so that pre-computed hash table has no collisions
            // Also all non-existent entries have fingerprint -1
            //
            TestAssert(h1 == h2 || h2->m_fingerprint != h3);
            result = h1;
        }
        else
        {
            // It is a programming error to select non-existent boilerplate
            //
            TestAssert(h2->m_fingerprint == h3);
            result = h2;
        }
        // It is a programming error to select non-existent boilerplate
        //
        TestAssert(memcmp(m_trueEntries + static_cast<size_t>(result - m_hashtable) * N, key.data(), N * sizeof(uint64_t)) == 0);
        TestAssert(result->m_value != nullptr);
        return result->m_value;
    }

    template<size_t ord, size_t N>
    uint64_t ComputeHash(const std::array<uint64_t, N>& key) const
    {
        static_assert(ord < 3, "bad ordinal");
        uint64_t ret = 0;
        for (uint32_t i = 0; i < N; i++)
        {
            ret += key[i] * m_hashFns[ord * N + i];
        }
        return ret;
    }

    uint32_t m_numElements;
    uint32_t m_hashtableSize;
    uint32_t* m_hashFns;
    FastInterpBoilerplateSelectionHashTableEntry* m_hashtable;
#ifdef TESTBUILD
    uint64_t* m_trueEntries;
#endif
};

// const FastInterpBoilerplateBluePrint* FastInterpBoilerplateLibrary<T>::SelectBoilerplateBluePrint(...)
//    select a boilerplate for AstNodeType based on compile-time specialization parameters
//
// This class is defined in generated header
//

}   // namespace PochiVM
