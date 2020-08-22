#include "test_classes.h"

TestClassA* TestClassB::GetAp()
{
    return m_ap;
}

void TestClassB::SetA(TestClassA* r)
{
    m_a = *r;
}

double TestMismatchedLLVMTypeName2(std::pair<double, uint64_t>* v)
{
    return v->first + static_cast<double>(v->second);
}
