#include "test_classes.h"

TestClassA* TestClassB::GetAp()
{
    return m_ap;
}

void TestClassB::SetA(TestClassA* r)
{
    m_a = *r;
}
