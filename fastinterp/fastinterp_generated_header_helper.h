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
         uint16_t highestCppFnptrPlaceholderOrdinal>
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
          , uint16_t highestUInt64PlaceholderOrdinal
          , uint16_t numCppFnPtrPlaceholders
          , const std::array<uint16_t, highestCppFnptrPlaceholderOrdinal>& cppFnPtrPlaceholderOrdinalToId
          , int lastInstructionTailCallOrd
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
              , m_cppFnPtrPlaceholderOrdinalToIdData
              , highestBoilerplateFnptrPlaceholderOrdinal
              , highestCppFnptrPlaceholderOrdinal
              , highestUInt64PlaceholderOrdinal
              , numCppFnPtrPlaceholders
              , lastInstructionTailCallOrd
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
        {
            uint16_t cur = 0;
            for (uint16_t i = 0; i < highestCppFnptrPlaceholderOrdinal; i++)
            {
                ReleaseAssert(cppFnPtrPlaceholderOrdinalToId[i] == static_cast<uint16_t>(-1) ||
                              cppFnPtrPlaceholderOrdinalToId[i] == cur);
                if (cppFnPtrPlaceholderOrdinalToId[i] == cur)
                {
                    cur++;
                }
            }
            ReleaseAssert(cur == numCppFnPtrPlaceholders);
        }
        internal::constexpr_copy_helper(m_addr32FixupArrayData, addr32FixupArray);
        internal::constexpr_copy_helper(m_symbol32FixupArrayData, symbol32FixupArray);
        internal::constexpr_copy_helper(m_symbol64FixupArrayData, symbol64FixupArray);
        internal::constexpr_copy_helper(m_cppFnPtrPlaceholderOrdinalToIdData, cppFnPtrPlaceholderOrdinalToId);
    }

private:
    // We must use C array, not std::array, since base class is initialized before std::array
    // and then calling .data() of an unconstructed std::array is undefined behavior
    //
    uint32_t m_addr32FixupArrayData[addr32FixupArrayLength];
    FastInterpSymbolFixupRecord m_symbol32FixupArrayData[symbol32FixupArrayLength];
    FastInterpSymbolFixupRecord m_symbol64FixupArrayData[symbol64FixupArrayLength];
    uint16_t m_cppFnPtrPlaceholderOrdinalToIdData[highestCppFnptrPlaceholderOrdinal];
};

template<uint32_t N, uint32_t M>
class FastInterpBoilerplateSelectionHashTableHelperWrapper: public FastInterpBoilerplateSelectionHashTableHelper
{
public:
    constexpr FastInterpBoilerplateSelectionHashTableHelperWrapper(
            const std::array<uint32_t, N * 3>& hashFns
          , const std::array<FastInterpBoilerplateSelectionHashTableEntry, M>& hashtable
#ifdef TESTBUILD
          , const std::array<uint64_t, M * N>& trueEntries
#endif
    )
        : FastInterpBoilerplateSelectionHashTableHelper(
              N, M, m_hashFnsData, m_hashtableData
#ifdef TESTBUILD
            , m_trueEntriesData
#endif
        )
    {
        for (uint32_t i = 0; i < M; i++)
        {
            ReleaseAssert((hashtable[i].m_value == nullptr) == (hashtable[i].m_fingerprint == static_cast<uint64_t>(-1)));
        }
        internal::constexpr_copy_helper(m_hashFnsData, hashFns);
        internal::constexpr_copy_helper(m_hashtableData, hashtable);
#ifdef TESTBUILD
        internal::constexpr_copy_helper(m_trueEntriesData, trueEntries);
#endif
    }

private:
    // We must use C array, same as FastInterpBoilerplateWrapper
    //
    uint32_t m_hashFnsData[N * 3];
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
