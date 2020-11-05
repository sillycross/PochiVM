#pragma once

#include "pochivm/common.h"
#include "pochivm/global_codegen_memory_pool.h"

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
        // TODO: support large size allocation
        //
        size_t alignment = 8;
        TestAssert(size <= g_queryExecutionMemoryPool.x_memoryChunkSize - 4096);
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
};

}   // namespace MiniDbBackend
