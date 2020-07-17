#pragma once

#include "pochivm/common.h"

class TestSmallClass;

// This class should be passed in by pointer as parameter, or by SRET as return value in C++ ABI
//
class TestLargeClass
{
public:
    TestLargeClass() {}
    TestLargeClass(uint64_t val1, uint64_t val2, uint16_t val3, uint32_t val4, TestSmallClass* val5)
        : m_val1(val1), m_val2(val2), m_val3(val3), m_val4(val4), m_val5(val5)
    { }

    TestLargeClass F1(uint64_t a)
    {
        m_val1 += a;
        return *this;
    }

    TestLargeClass& F2(uint64_t a)
    {
        m_val1 -= a;
        return *this;
    }

    TestLargeClass& F3(TestSmallClass* a)
    {
        m_val5 = a;
        return *this;
    }

    TestLargeClass F4(TestSmallClass a);
    TestSmallClass F5(TestSmallClass a);

    uint64_t m_val1;
    uint64_t m_val2;
    uint16_t m_val3;
    uint32_t m_val4;
    TestSmallClass* m_val5;
};
static_assert(sizeof(TestLargeClass) > 16, "should be > 16 bytes for testing purpose");

// A small class (<=16 bytes). The C++ ABI would pass this class in registers.
//
class TestSmallClass
{
public:
    TestSmallClass() {}
    TestSmallClass(TestLargeClass* val1, uint8_t val2, uint16_t val3)
        : m_val1(val1), m_val2(val2), m_val3(val3)
    { }

    TestLargeClass F1(TestSmallClass a)
    {
        m_val2 += a.m_val2;
        return TestLargeClass(
            m_val1->m_val1 + a.m_val1->m_val1 + m_val2 + a.m_val2,
            m_val1->m_val2 + a.m_val1->m_val2 + m_val3 + a.m_val3,
            m_val1->m_val3 + a.m_val1->m_val3 + m_val3 + a.m_val3,
            m_val1->m_val4 + a.m_val1->m_val4 + m_val2 + a.m_val2,
            m_val1->m_val5
        );
    }

    TestLargeClass F2(const TestSmallClass a)
    {
        m_val2 += a.m_val2;
        return TestLargeClass(
                m_val1->m_val1 - a.m_val1->m_val1 + m_val2 + a.m_val2,
                m_val1->m_val2 - a.m_val1->m_val2 + m_val3 + a.m_val3,
                m_val1->m_val3 - a.m_val1->m_val3 + m_val3 + a.m_val3,
                m_val1->m_val4 - a.m_val1->m_val4 + m_val2 + a.m_val2,
                a.m_val1->m_val5
        );
    }

    TestLargeClass F3(TestLargeClass a)
    {
        m_val3 += a.m_val3;
        a.m_val1 += m_val2;
        a.m_val2 += m_val3;
        return a;
    }

    TestLargeClass& F4(TestLargeClass& a)
    {
        m_val3 += a.m_val3;
        a.m_val1 += m_val3;
        a.m_val2 += m_val2;
        return a;
    }

    const TestLargeClass F5(TestLargeClass* a)
    {
        m_val3 += a->m_val3;
        a->m_val1 += m_val2 + m_val3;
        TestLargeClass b = *a;
        b.m_val2 += m_val3;
        return b;
    }

    const TestLargeClass& F6(const TestLargeClass& a) const
    {
        m_val1->m_val1 += a.m_val1;
        return a;
    }

    const TestLargeClass* F7(const TestLargeClass* a) const
    {
        m_val1->m_val2 += a->m_val2;
        return a;
    }

    TestSmallClass G1(TestLargeClass a)
    {
        m_val3 += a.m_val3;
        TestSmallClass r = *this;
        r.m_val3 += a.m_val3;
        return r;
    }

    TestSmallClass G2(const TestLargeClass a)
    {
        m_val3 -= a.m_val3;
        TestSmallClass r = *this;
        r.m_val3 -= a.m_val3;
        return r;
    }

    TestSmallClass G3(TestSmallClass a)
    {
        m_val2 += a.m_val2;
        a.m_val2 += m_val2;
        return a;
    }

    TestSmallClass& G4(TestSmallClass& a)
    {
        m_val2 += a.m_val2 + 1;
        a.m_val2 += m_val2 + 1;
        return a;
    }

    const TestSmallClass G5(TestSmallClass* a)
    {
        m_val2 += a->m_val2 + 2;
        a->m_val2 += m_val2 + 2;
        return *a;
    }

    const TestSmallClass& G6(const TestSmallClass& a)
    {
        m_val3 += a.m_val3;
        return a;
    }

    const TestSmallClass* G7(const TestSmallClass* a) const
    {
        m_val1->m_val1 += a->m_val3;
        return a;
    }

    static TestSmallClass S1(TestSmallClass a)
    {
        a.m_val2 *= 3;
        return a;
    }

    static TestLargeClass S2(TestLargeClass a)
    {
        a.m_val1 *= 5;
        return a;
    }

    void Overloaded(int a)
    {
        m_val2 += static_cast<uint8_t>(a);
    }

    void Overloaded(double a)
    {
        m_val3 += static_cast<uint16_t>(trunc(a));
    }

    void Overloaded(TestSmallClass a) const
    {
        a.m_val2 += m_val2;
    }

    TestLargeClass* m_val1;
    uint8_t m_val2;
    uint16_t m_val3;
};
static_assert(sizeof(TestSmallClass) <= 16, "should be <= 16 bytes for testing purpose");

inline TestSmallClass FreeFnTestSmallClass1(TestSmallClass a, TestSmallClass b)
{
    a.m_val3 += b.m_val3;
    return a;
}

TestSmallClass FreeFnTestSmallClass2(TestSmallClass a, TestSmallClass b, TestSmallClass c);

inline TestSmallClass& FreeFnTestSmallClass3(TestSmallClass& a, TestSmallClass b)
{
    a.m_val3 -= b.m_val3;
    return a;
}

TestSmallClass& FreeFnTestSmallClass4(TestSmallClass a, TestSmallClass& b, TestSmallClass c);

int FreeFnTestSmallClass5(TestSmallClass a, int c);

inline TestLargeClass FreeFnTestLargeClass1(TestLargeClass a, TestLargeClass b)
{
    a.m_val3 += b.m_val3;
    return a;
}

TestLargeClass FreeFnTestLargeClass2(TestLargeClass a, TestLargeClass b, TestLargeClass c);

inline TestLargeClass& FreeFnTestLargeClass3(TestLargeClass& a, TestLargeClass b)
{
    a.m_val3 -= b.m_val3;
    return a;
}

TestLargeClass& FreeFnTestLargeClass4(TestLargeClass a, TestLargeClass& b, TestLargeClass c);

inline int FreeFnRecursive(int x)
{
    if (x <= 1)
    {
        return 1;
    }
    else
    {
        return FreeFnRecursive(x - 1) + FreeFnRecursive(x - 2);
    }
}

inline int FreeFnRecursive2(std::string xstr)
{
    int x = std::stoi(xstr);
    if (x <= 1)
    {
        return 1;
    }
    else
    {
        return FreeFnRecursive2(std::to_string(x - 1)) + FreeFnRecursive2(std::to_string(x - 2));
    }
}

inline std::string FreeFnRecursive3(std::string xstr)
{
    static const int len_prefix = 100;
    std::string prefix = std::string(len_prefix, '_');
    xstr = xstr.substr(len_prefix);
    int x = std::stoi(xstr);
    if (x <= 1)
    {
        return prefix + "1";
    }
    else
    {
        std::string r1 = FreeFnRecursive3(prefix + std::to_string(x - 1));
        std::string r2 = FreeFnRecursive3(prefix + std::to_string(x - 2));
        int i1 = std::stoi(r1.substr(len_prefix));
        int i2 = std::stoi(r2.substr(len_prefix));
        return prefix + std::to_string(i1 + i2);
    }
}

class TestNonTrivialConstructor
{
public:
    TestNonTrivialConstructor(int v)
        : m_value(v)
    { }

    static TestNonTrivialConstructor Create(int v)
    {
        return TestNonTrivialConstructor(v);
    }

    int GetValue() { return m_value; }

    int m_value;
};

struct TestNonTrivialCopyConstructor
{
    static int counter;

    TestNonTrivialCopyConstructor(int x)
        : value(x)
    { }

    TestNonTrivialCopyConstructor(const TestNonTrivialCopyConstructor& other)
    {
        counter++;
        value = other.value;
    }

    TestNonTrivialCopyConstructor& operator=(const TestNonTrivialCopyConstructor& other)
    {
        counter++;
        value = other.value;
        return (*this);
    }

    static int Fn(TestNonTrivialCopyConstructor h)
    {
        return h.value;
    }

    int value;
};

inline void CopyVectorInt(std::vector<int>* src, std::vector<int>* dst)
{
    *src = *dst;
}

class TestConstructor1
{
public:
    TestConstructor1() { value = 233; }
    int GetValue() { return value; }

    int value;
};

class TestConstructor2
{
public:
    TestConstructor2(int x) { value = x; }
    int GetValue() { return value; }

    int value;
};

