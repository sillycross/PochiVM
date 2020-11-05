#pragma once

#include "pochivm/common.h"
#include "pochivm/global_codegen_memory_pool.h"

namespace MiniDbBackend
{

std::vector<uintptr_t>* GetTpchTableHelper(const char* name);
inline std::vector<uintptr_t>* GetCustomerTable() { return GetTpchTableHelper("customer"); }
inline std::vector<uintptr_t>* GetLineitemTable() { return GetTpchTableHelper("lineitem"); }
inline std::vector<uintptr_t>* GetNationTable() { return GetTpchTableHelper("nation"); }
inline std::vector<uintptr_t>* GetOrdersTable() { return GetTpchTableHelper("orders"); }
inline std::vector<uintptr_t>* GetPartTable() { return GetTpchTableHelper("part"); }
inline std::vector<uintptr_t>* GetPartSuppTable() { return GetTpchTableHelper("partsupp"); }
inline std::vector<uintptr_t>* GetRegionTable() { return GetTpchTableHelper("region"); }
inline std::vector<uintptr_t>* GetSupplierTable() { return GetTpchTableHelper("supplier"); }
inline std::vector<uintptr_t>* GetTestTable1() { return GetTpchTableHelper("testtable1"); }

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

    void PrintInt32(int32_t value)
    {
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), "| %d ", value);
        TestAssert(m_current < m_end);
    }

    void PrintUInt32(uint32_t value)
    {
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), "| %u ", value);
        TestAssert(m_current < m_end);
    }

    void PrintInt64(int64_t value)
    {
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), "| %ld ", value);
        TestAssert(m_current < m_end);
    }

    void PrintUInt64(uint64_t value)
    {
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), "| %lu ", value);
        TestAssert(m_current < m_end);
    }

    void PrintDouble(double value)
    {
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), "| %lf ", value);
        TestAssert(m_current < m_end);
    }

    void PrintString(char* value)
    {
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), "| %s ", value);
        TestAssert(m_current < m_end);
    }

    void PrintNewLine()
    {
        m_current += snprintf(m_current, static_cast<size_t>(m_end - m_current), "|\n");
        TestAssert(m_current < m_end);
    }

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
