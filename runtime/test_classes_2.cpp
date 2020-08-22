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

uint64_t TestMismatchedLLVMTypeName3(std::pair<uint32_t, uint16_t>* v);;

uint64_t TestMismatchedLLVMTypeName4(std::pair<uint16_t, uint32_t>* v)
{
    if (v->first == 0 || v->second == 0)
    {
        return static_cast<uint64_t>(v->first) + v->second;
    }
    std::pair<uint32_t, uint16_t> x = std::make_pair(v->second - 1, v->first - 1);
    return TestMismatchedLLVMTypeName3(&x) + 2;
}
