#pragma once

#include "pochivm/common.h"

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

}   // namespace MiniDbBackend
