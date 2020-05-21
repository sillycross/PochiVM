#include "test_classes.h"

void TestClassA::SetY(int y)
{
    m_y = y;
}

void TestClassA::SortVector()
{
    std::sort(m_vec.begin(), m_vec.end());
}

void FreeFunctionStoreValue(int* dst, int src)
{
    *dst = src;
}

