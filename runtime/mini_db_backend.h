#pragma once

#include "pochivm/common.h"
#include "pochivm/global_codegen_memory_pool.h"
#include "pochivm/pochivm_function_pointer.h"

namespace MiniDbBackend
{

enum TpchTableName
{
    TPCH_CUSTOMER,
    TPCH_LINEITEM,
    TPCH_NATION,
    TPCH_ORDERS,
    TPCH_PART,
    TPCH_PARTSUPP,
    TPCH_REGION,
    TPCH_SUPPLIER,
    UNITTEST_TABLE1
};

std::vector<uintptr_t>* GetTpchTableHelper(int);
inline std::vector<uintptr_t>* GetCustomerTable() { return GetTpchTableHelper(TPCH_CUSTOMER); }
inline std::vector<uintptr_t>* GetLineitemTable() { return GetTpchTableHelper(TPCH_LINEITEM); }
inline std::vector<uintptr_t>* GetNationTable() { return GetTpchTableHelper(TPCH_NATION); }
inline std::vector<uintptr_t>* GetOrdersTable() { return GetTpchTableHelper(TPCH_ORDERS); }
inline std::vector<uintptr_t>* GetPartTable() { return GetTpchTableHelper(TPCH_PART); }
inline std::vector<uintptr_t>* GetPartSuppTable() { return GetTpchTableHelper(TPCH_PARTSUPP); }
inline std::vector<uintptr_t>* GetRegionTable() { return GetTpchTableHelper(TPCH_REGION); }
inline std::vector<uintptr_t>* GetSupplierTable() { return GetTpchTableHelper(TPCH_SUPPLIER); }
inline std::vector<uintptr_t>* GetTestTable1() { return GetTpchTableHelper(UNITTEST_TABLE1); }

struct SqlResultPrinter
{
    SqlResultPrinter()
        : m_start(nullptr)
    {
        size_t len = 1000000;
        m_start = new char[len];
        *m_start = '\0';
        m_current = m_start;
        m_end = m_start + len;
    }

    ~SqlResultPrinter()
    {
        if (m_start != nullptr)
        {
            delete [] m_start;
        }
    }

    // It seems like LLJIT has some bug handling global constant strings when CodeGenOpt::Less or higher is given.
    // Weird.. workaround it for now.
    //
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wformat-security"
    void PrintInt32(int32_t value)
    {
        int8_t fmt[] = { '|', ' ', '%', 'd', ' ', '\0' };
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), reinterpret_cast<char*>(fmt), value);
        TestAssert(m_current < m_end);
    }

    void PrintUInt32(uint32_t value)
    {
        int8_t fmt[] = { '|', ' ', '%', 'u', ' ', '\0' };
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), reinterpret_cast<char*>(fmt), value);
        TestAssert(m_current < m_end);
    }

    void PrintInt64(int64_t value)
    {
        int8_t fmt[] = { '|', ' ', '%', 'l', 'd', ' ', '\0' };
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), reinterpret_cast<char*>(fmt),  value);
        TestAssert(m_current < m_end);
    }

    void PrintUInt64(uint64_t value)
    {
        int8_t fmt[] = { '|', ' ', '%', 'l', 'u', ' ', '\0' };
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), reinterpret_cast<char*>(fmt),  value);
        TestAssert(m_current < m_end);
    }

    void PrintDouble(double value)
    {
        int8_t fmt[] = { '|', ' ', '%', 'l', 'f', ' ', '\0' };
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), reinterpret_cast<char*>(fmt), value);
        TestAssert(m_current < m_end);
    }

    void PrintString(char* value)
    {
        int8_t fmt[] = { '|', ' ', '%', 's', ' ', '\0' };
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), reinterpret_cast<char*>(fmt), value);
        TestAssert(m_current < m_end);
    }

    void PrintNewLine()
    {
        int8_t fmt[] = { '|', '\n', '\0' };
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), reinterpret_cast<char*>(fmt));
        TestAssert(m_current < m_end);
    }
#pragma clang diagnostic pop

    char* m_start;
    char* m_current;
    char* m_end;
};

// TODO: inline global variable crashes dump_symbol.cpp at exit,
// probably some weird issue with global destructors.
// I think this is just the runConstructor() bug that has been fixed in LLVM 11,
// but figure out what's going on later.
//
extern PochiVM::GlobalCodegenMemoryPool g_queryExecutionMemoryPool;

class QueryExecutionTempAllocator
{
public:
    QueryExecutionTempAllocator()
        : m_listHead(0)
        , m_currentAddress(8)
        , m_currentAddressEnd(0)
        , m_largeAllocationHead(0)
    { }

    ~QueryExecutionTempAllocator()
    {
        FreeAllMemoryChunks();
    }

    void Reset()
    {
        FreeAllMemoryChunks();
    }

    uintptr_t Allocate(size_t size)
    {
        size_t alignment = 8;
        if (size > g_queryExecutionMemoryPool.x_memoryChunkSize - 4096)
        {
            char* buf = new char[size + 8];
            *reinterpret_cast<uintptr_t*>(buf) = m_largeAllocationHead;
            m_largeAllocationHead = reinterpret_cast<uintptr_t>(buf);
            return reinterpret_cast<uintptr_t>(buf) + 8;
        }
        AlignCurrentAddress(alignment);
        if (m_currentAddress + size > m_currentAddressEnd)
        {
            GetNewMemoryChunk();
            AlignCurrentAddress(alignment);
            TestAssert(m_currentAddress + size <= m_currentAddressEnd);
        }
        TestAssert(m_currentAddress % alignment == 0);
        uintptr_t result = m_currentAddress;
        m_currentAddress += size;
        TestAssert(m_currentAddress <= m_currentAddressEnd);
        return result;
    }

private:
    void GetNewMemoryChunk()
    {
        uintptr_t address = g_queryExecutionMemoryPool.GetMemoryChunk();
        AppendToList(address);
        // the first 8 bytes of the region is used as linked list
        //
        m_currentAddress = address + 8;
        m_currentAddressEnd = address + g_queryExecutionMemoryPool.x_memoryChunkSize;
    }

    void AlignCurrentAddress(size_t alignment)
    {
        size_t mask = alignment - 1;
        m_currentAddress += mask;
        m_currentAddress &= ~mask;
    }

    void AppendToList(uintptr_t address)
    {
        *reinterpret_cast<uintptr_t*>(address) = m_listHead;
        m_listHead = address;
    }

    void FreeAllMemoryChunks()
    {
        while (m_largeAllocationHead != 0)
        {
            uintptr_t next = *reinterpret_cast<uintptr_t*>(m_largeAllocationHead);
            char* c = reinterpret_cast<char*>(m_largeAllocationHead);
            delete [] c;
            m_largeAllocationHead = next;
        }
        while (m_listHead != 0)
        {
            uintptr_t next = *reinterpret_cast<uintptr_t*>(m_listHead);
            g_queryExecutionMemoryPool.FreeMemoryChunk(m_listHead);
            m_listHead = next;
        }
        m_currentAddress = 8;
        m_currentAddressEnd = 0;
    }

    uintptr_t m_listHead;
    uintptr_t m_currentAddress;
    uintptr_t m_currentAddressEnd;
    uintptr_t m_largeAllocationHead;
};


inline size_t HashString(char* input)
{
    size_t result = 0;
    while (*input != '\0')
    {
        result = result * 10007 + static_cast<size_t>(*input);
        input++;
    }
    return result;
}

inline bool CompareStringEqual(char* input1, char* input2)
{
    return strcmp(input1, input2) == 0;
}

inline int CompareString(char* input1, char* input2)
{
    return strcmp(input1, input2);
}

}   // namespace MiniDbbackend

// TODO: the generated header file still has some name resolution issue, workaround for now
//
struct GeneratedKeyCmpFnOperator
{
    using KeyType = uintptr_t;
    bool operator()(const KeyType& lhs, const KeyType& rhs) const
    {
        using FnPrototype = bool(*)(KeyType, KeyType) noexcept;
        return PochiVM::GeneratedFunctionPointer<FnPrototype>(m_cmpFn)(lhs, rhs);
        //return MiniDbBackend::CompareStringEqual(reinterpret_cast<char*>(lhs) + 56, reinterpret_cast<char*>(rhs) + 56)
        //        && MiniDbBackend::CompareStringEqual(reinterpret_cast<char*>(lhs) + 58, reinterpret_cast<char*>(rhs) + 58);
    }
    uintptr_t m_cmpFn;
};

struct GeneratedKeyHashFnOperator
{
    using KeyType = uintptr_t;
    size_t operator()(const KeyType& k) const
    {
        using FnPrototype = size_t(*)(KeyType) noexcept;
        return PochiVM::GeneratedFunctionPointer<FnPrototype>(m_hashFn)(k);
        // return MiniDbBackend::HashString(reinterpret_cast<char*>(k) + 56) * 13331ULL
        //        + MiniDbBackend::HashString(reinterpret_cast<char*>(k) + 58);
    }
    uintptr_t m_hashFn;
};

struct GeneratedKeyCmpFnOperator2
{
    using KeyType = uintptr_t;
    bool operator()(const KeyType& lhs, const KeyType& rhs) const
    {
        using FnPrototype = bool(*)(KeyType, KeyType) noexcept;
        return PochiVM::GeneratedFunctionPointer<FnPrototype>(m_cmpFn)(lhs, rhs);
        // int x = MiniDbBackend::CompareString(reinterpret_cast<char**>(lhs)[0], reinterpret_cast<char**>(rhs)[0]);
        // if (x != 0) { return x < 0; }
        // x = MiniDbBackend::CompareString(reinterpret_cast<char**>(lhs)[1], reinterpret_cast<char**>(rhs)[1]);
        // if (x != 0) { return x < 0; }
        // return false;
    }
    uintptr_t m_cmpFn;
};

namespace MiniDbBackend
{

using QEHashTable = std::unordered_map<uintptr_t, uintptr_t, GeneratedKeyHashFnOperator, GeneratedKeyCmpFnOperator>;

inline QEHashTable CreateQEHashTable(uintptr_t hashFn, uintptr_t equalFn)
{
    return QEHashTable(32 /*initBucketCount*/,
                       GeneratedKeyHashFnOperator { hashFn },
                       GeneratedKeyCmpFnOperator { equalFn });
}

inline void SortRows(uintptr_t* rowsStart, size_t numRows, uintptr_t cmpFn)
{
    std::sort(rowsStart, rowsStart + numRows, GeneratedKeyCmpFnOperator2 { cmpFn });
}

inline void DumpHashTable(QEHashTable& ht, std::vector<uintptr_t>& output)
{
    for (auto it = ht.begin(); it != ht.end(); it++)
    {
        output.push_back(it->second);
    }
}

}   // namespace MiniDbBackend
