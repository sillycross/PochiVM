#pragma once

#include "fastinterp_helper.h"
#include "x86_64_get_fs_base_helper.h"
#include "fastinterp_context.h"

namespace PochiVM
{

namespace internal
{

template<typename T, size_t N>
constexpr void constexpr_copy_helper(T* dst, const std::array<T, N>& src)
{
    for (size_t i = 0; i < N; i++)
    {
        dst[i] = src[i];
    }
}

}   // namespace internal

template<uint32_t addr32FixupArrayLength,
         uint32_t symbol32FixupArrayLength,
         uint32_t symbol64FixupArrayLength,
         uint32_t numJmp32,
         uint32_t numJcc32>
class FastInterpBoilerplateBluePrintWrapper : public FastInterpBoilerplateBluePrint
{
public:
    constexpr FastInterpBoilerplateBluePrintWrapper(
            const uint8_t* content
          , uint32_t contentLength
          , const std::array<uint32_t, addr32FixupArrayLength>& addr32FixupArray
          , const std::array<FastInterpSymbolFixupRecord, symbol32FixupArrayLength>& symbol32FixupArray
          , const std::array<FastInterpSymbolFixupRecord, symbol64FixupArrayLength>& symbol64FixupArray
          , uint16_t highestBoilerplateFnptrPlaceholderOrdinal
          , uint16_t highestCppFnptrPlaceholderOrdinal
          , uint16_t highestUInt64PlaceholderOrdinal
          , int lastInstructionTailCallOrd
          , const std::array<uint32_t, numJmp32>& jmp32Offsets
          , const std::array<uint32_t, numJcc32>& jcc32Offsets
#ifdef TESTBUILD
          , uint64_t usedBoilerplateFnPtrPlaceholderMask
          , uint64_t usedCppFnptrPlaceholderMask
          , uint64_t usedUInt64PlaceholderMask
          , const char* symbolName
#endif
    )
        : FastInterpBoilerplateBluePrint(
                content
              , contentLength
              , m_addr32FixupArrayData
              , addr32FixupArrayLength
              , m_symbol32FixupArrayData
              , symbol32FixupArrayLength
              , m_symbol64FixupArrayData
              , symbol64FixupArrayLength
              , highestBoilerplateFnptrPlaceholderOrdinal
              , highestCppFnptrPlaceholderOrdinal
              , highestUInt64PlaceholderOrdinal
              , lastInstructionTailCallOrd
              , numJmp32
              , m_jmp32OffsetData
              , numJcc32
              , m_jcc32OffsetData
#ifdef TESTBUILD
              , usedBoilerplateFnPtrPlaceholderMask
              , usedCppFnptrPlaceholderMask
              , usedUInt64PlaceholderMask
              , symbolName
#endif
        )
    {
        ReleaseAssert(contentLength % x_fastinterp_function_alignment == 0);
        for (uint32_t i = 0; i < addr32FixupArrayLength; i++)
        {
            ReleaseAssert(addr32FixupArray[i] + 4 <= contentLength);
        }
        for (uint32_t i = 0; i < symbol32FixupArrayLength; i++)
        {
            ReleaseAssert(symbol32FixupArray[i].m_offset + 4 <= contentLength);
            ReleaseAssert(symbol32FixupArray[i].m_ordinalIntoPlaceholderArray < highestBoilerplateFnptrPlaceholderOrdinal +
                          highestCppFnptrPlaceholderOrdinal + highestUInt64PlaceholderOrdinal);
        }
        for (uint32_t i = 0; i < symbol64FixupArrayLength; i++)
        {
            ReleaseAssert(symbol64FixupArray[i].m_offset + 8 <= contentLength);
            ReleaseAssert(symbol64FixupArray[i].m_ordinalIntoPlaceholderArray < highestBoilerplateFnptrPlaceholderOrdinal +
                          highestCppFnptrPlaceholderOrdinal + highestUInt64PlaceholderOrdinal);
        }
        internal::constexpr_copy_helper(m_addr32FixupArrayData, addr32FixupArray);
        internal::constexpr_copy_helper(m_symbol32FixupArrayData, symbol32FixupArray);
        internal::constexpr_copy_helper(m_symbol64FixupArrayData, symbol64FixupArray);
        internal::constexpr_copy_helper(m_jmp32OffsetData, jmp32Offsets);
        internal::constexpr_copy_helper(m_jcc32OffsetData, jcc32Offsets);
    }

private:
    // We must use C array, not std::array, since base class is initialized before std::array
    // and then calling .data() of an unconstructed std::array is undefined behavior
    //
    uint32_t m_addr32FixupArrayData[addr32FixupArrayLength];
    FastInterpSymbolFixupRecord m_symbol32FixupArrayData[symbol32FixupArrayLength];
    FastInterpSymbolFixupRecord m_symbol64FixupArrayData[symbol64FixupArrayLength];
    uint32_t m_jmp32OffsetData[numJmp32];
    uint32_t m_jcc32OffsetData[numJcc32];
};

template<uint32_t N, uint32_t M, uint32_t... HashFns>
class FastInterpBoilerplateSelectionHashTable
{
public:
    template<typename T>
    friend struct FastInterpBoilerplateLibrary;

    constexpr FastInterpBoilerplateSelectionHashTable(
            const std::array<FastInterpBoilerplateSelectionHashTableEntry, M>& hashtable
#ifdef TESTBUILD
          , const std::array<uint64_t, M * N>& trueEntries
#endif
    )
    {
        for (uint32_t i = 0; i < M; i++)
        {
            ReleaseAssert((hashtable[i].m_value == nullptr) == (hashtable[i].m_fingerprint == static_cast<uint32_t>(-1)));
        }
        internal::constexpr_copy_helper(m_hashtableData, hashtable);
#ifdef TESTBUILD
        internal::constexpr_copy_helper(m_trueEntriesData, trueEntries);
#endif
    }

    // It is a programming error to select a non-existent boilerplate!
    //
    const FastInterpBoilerplateBluePrint* ALWAYS_INLINE_IN_NONDEBUG SelectBoilerplateBluePrint(const std::array<uint64_t, N>& key) const
    {
        static_assert(N > 0, "Bad N");
        uint32_t h1 = ComputeHash<0>(key);
        uint32_t h2 = ComputeHash<1>(key);
        uint32_t p1 = MapToHashTableSlot(h1);
        uint32_t p2 = MapToHashTableSlot(h2);
        uint32_t result;
        if (m_hashtableData[p1].m_fingerprint == h2)
        {
            // h1 and h2 are always selected so that pre-computed hash table has no collisions
            // Also all non-existent entries have fingerprint -1
            //
            TestAssert(p1 == p2 || m_hashtableData[p2].m_fingerprint != h1);
            result = p1;
        }
        else
        {
            // It is a programming error to select non-existent boilerplate
            //
            TestAssert(m_hashtableData[p2].m_fingerprint == h1);
            result = p2;
        }
        // It is a programming error to select non-existent boilerplate
        //
        TestAssert(memcmp(m_trueEntriesData + result * N, key.data(), N * sizeof(uint64_t)) == 0);
        TestAssert(m_hashtableData[result].m_value != nullptr);
        return m_hashtableData[result].m_value;
    }

private:
    template<size_t ord>
    uint32_t ALWAYS_INLINE_IN_NONDEBUG ComputeHash(const std::array<uint64_t, N>& key) const
    {
        static_assert(ord < 2, "bad ordinal");
        static_assert(sizeof...(HashFns) == N * 2);
        constexpr std::array<uint32_t, N * 2> hashFnsData { HashFns... };
        uint32_t ret = 0;
        for (uint32_t i = 0; i < N; i++)
        {
            ret += static_cast<uint32_t>(key[i]) * hashFnsData[ord * N + i];
        }
        return ret;
    }

    uint32_t ALWAYS_INLINE_IN_NONDEBUG MapToHashTableSlot(uint32_t hashValue) const
    {
        return static_cast<uint32_t>((static_cast<uint64_t>(hashValue) * M) >> 32);
    }

    FastInterpBoilerplateSelectionHashTableEntry m_hashtableData[M];
#ifdef TESTBUILD
    uint64_t m_trueEntriesData[M * N];
#endif
};

class FastInterpInitFixupThreadLocalHelper
{
public:
    template<size_t N>
    FastInterpInitFixupThreadLocalHelper(uint8_t* content,
                                         uint64_t contentLength,
                                         const std::array<size_t, N>& fixupSites)
    {
        // This function currently only handles R_X86_64_TPOFF32
        //
        uint32_t u32Pv = static_cast<uint32_t>(GetPatchValue());
        for (size_t i = 0; i < N; i++)
        {
            TestAssert(fixupSites[i] + sizeof(uint32_t) <= contentLength);
            UnalignedAddAndWriteback<uint32_t>(content + fixupSites[i], u32Pv);
        }
        std::ignore = contentLength;
    }

private:
    static uint64_t GetPatchValue()
    {
        if (!m_patchValueInitialized)
        {
            m_patchValueInitialized = true;
            uint64_t fs_base = x86_64_get_fs_segmentation_register_base();
            uint64_t addr = reinterpret_cast<uint64_t>(&__pochivm_thread_fastinterp_context);
            m_patchValue = addr - fs_base;
        }
        return m_patchValue;
    }

    static inline uint64_t m_patchValue;
    static inline bool m_patchValueInitialized = false;
};

}   // namespace PochiVM
