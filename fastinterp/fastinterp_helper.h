#pragma once

// Internal utility classes. One should always include "fastinterp.h" instead, not this file.
//

#include "pochivm/common.h"
#include "pochivm/cxx2a_bit_cast_helper.h"
#include "fastinterp_function_alignment.h"
#include "x86_64_asm_helper.h"

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
          , int lastInstructionTailCallOrd
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
        , m_lastInstructionTailCallOrd(lastInstructionTailCallOrd)
#ifdef TESTBUILD
        , m_usedBoilerplateFnPtrPlaceholderMask(usedBoilerplateFnPtrPlaceholderMask)
        , m_usedCppFnptrPlaceholderMask(usedCppFnptrPlaceholderMask)
        , m_usedUInt64PlaceholderMask(usedUInt64PlaceholderMask)
        , m_symbolName(symbolName)
#endif
    { }

private:
    void MaterializeCodeSection(uint8_t* destAddr, uint64_t* fixupValues, bool shouldStripLITC) const
    {
        TestAssert(reinterpret_cast<uint64_t>(destAddr) % x_fastinterp_function_alignment == 0);
        uint32_t trueContentLength = m_contentLength;
        if (shouldStripLITC)
        {
            TestAssert(trueContentLength >= x86_64_rip_relative_jmp_instruction_len &&
                       m_content[trueContentLength - x86_64_rip_relative_jmp_instruction_len] == x86_64_jmp_instruction_opcode);
            trueContentLength -= x86_64_rip_relative_jmp_instruction_len;
        }
        memcpy(destAddr, m_content, trueContentLength);

        {
            uint32_t limit = m_addr32FixupArrayLength;
            if (shouldStripLITC)
            {
                TestAssert(limit > 0 && m_addr32FixupArray[limit - 1] == trueContentLength + x86_64_jmp_opcode_num_bytes);
                limit--;
            }
            uint32_t addend = static_cast<uint32_t>(-static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uint64_t>(destAddr))));
            for (uint32_t i = 0; i < limit; i++)
            {
                TestAssert(m_addr32FixupArray[i] + sizeof(uint32_t) <= trueContentLength);
                UnalignedAddAndWriteback<uint32_t>(destAddr + m_addr32FixupArray[i], addend);
            }
        }

        {
            uint32_t limit = m_symbol32FixupArrayLength;
            if (shouldStripLITC)
            {
                TestAssert(limit > 0 && m_symbol32FixupArray[limit - 1].m_offset == trueContentLength + x86_64_jmp_opcode_num_bytes);
                limit--;
            }
            for (uint32_t i = 0; i < limit; i++)
            {
                TestAssert(m_symbol32FixupArray[i].m_offset + sizeof(uint32_t) <= trueContentLength);
                uint32_t addend = static_cast<uint32_t>(fixupValues[m_symbol32FixupArray[i].m_ordinalIntoPlaceholderArray]);
                UnalignedAddAndWriteback<uint32_t>(destAddr + m_symbol32FixupArray[i].m_offset, addend);
            }
        }

        for (uint32_t i = 0; i < m_symbol64FixupArrayLength; i++)
        {
            TestAssert(m_symbol64FixupArray[i].m_offset + sizeof(uint64_t) <= trueContentLength);
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
    int m_lastInstructionTailCallOrd;

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
