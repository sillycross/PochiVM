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
