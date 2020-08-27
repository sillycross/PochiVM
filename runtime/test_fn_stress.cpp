#include "test_fn_stress.h"

TestLargeClass TestLargeClass::F4(TestSmallClass a)
{
    m_val1 += a.m_val2;
    TestLargeClass r = *this;
    r.m_val1 += a.m_val3;
    return r;
}

TestSmallClass TestLargeClass::F5(TestSmallClass a)
{
    m_val1 += a.m_val2;
    a.m_val3 += m_val3;
    return a;
}

TestSmallClass FreeFnTestSmallClass2(TestSmallClass a, TestSmallClass b, TestSmallClass c)
{
    a.m_val3 += b.m_val3;
    a.m_val2 += c.m_val2;
    return a;
}

TestSmallClass& FreeFnTestSmallClass4(TestSmallClass a, TestSmallClass& b, TestSmallClass c)
{
    b.m_val2 -= a.m_val2;
    b.m_val3 -= c.m_val3;
    return b;
}

int FreeFnTestSmallClass5(TestSmallClass a, int c)
{
    if (a.m_val1 == nullptr)
    {
        return c + 1;
    }
    else
    {
        return c * 2;
    }
}

TestLargeClass FreeFnTestLargeClass2(TestLargeClass a, TestLargeClass b, TestLargeClass c)
{
    a.m_val3 += b.m_val3;
    a.m_val2 += c.m_val2;
    return a;
}

TestLargeClass& FreeFnTestLargeClass4(TestLargeClass a, TestLargeClass& b, TestLargeClass c)
{
    b.m_val2 -= a.m_val2;
    b.m_val3 -= c.m_val3;
    return b;
}

int TestNonTrivialCopyConstructor::counter = 0;

double TestMismatchedLLVMTypeName(std::pair<double, float>* v)
{
    return v->first + static_cast<double>(v->second);
}

uint64_t TestMismatchedLLVMTypeName3(std::pair<uint32_t, uint16_t>* v)
{
    if (v->first == 0 || v->second == 0)
    {
        return static_cast<uint64_t>(v->first) + v->second;
    }
    std::pair<uint16_t, uint32_t> x = std::make_pair(v->second - 1, v->first - 1);
    return TestMismatchedLLVMTypeName4(&x) + 2;
}

int g_testGlobalVariable1 = 12345;
