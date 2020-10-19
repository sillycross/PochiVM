#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"

using namespace PochiVM;

namespace {

template<typename T, typename retType>
void CompareResults(T /*v1*/, T /*v2*/, retType r1, retType r2)
{
    ReleaseAssert(r1 == r2);
}

template<>
void CompareResults(float v1, float v2, float r1, float r2)
{
    double diff = fabs(static_cast<double>(r1) - static_cast<double>(r2));
    double tol = 1e-6;
    if (diff < tol) { return; }
    double relDiff = diff / std::max(fabs(static_cast<double>(v1)), fabs(static_cast<double>(v2)));
    ReleaseAssert(relDiff < tol);
}

template<>
void CompareResults(double v1, double v2, double r1, double r2)
{
    double diff = fabs(r1 - r2);
    double tol = 1e-12;
    if (diff < tol) { return; }
    double relDiff = diff / std::max(fabs(v1), fabs(v2));
    ReleaseAssert(relDiff < tol);
}

// TODO: currently 'new SimpleJIT()' leaks
//
#define GetArithFn(fnName, opName, retType)                                      \
template<typename T>                                                             \
std::function<void(T, T)> fnName()                                               \
{                                                                                \
    using FnPrototype = retType(*)(T, T);                                        \
    auto [fn, val1, val2] = NewFunction<FnPrototype>("MyFn");                    \
    fn.SetBody(Return(val1 opName val2));                                        \
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());               \
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();                 \
    auto interpFn = thread_pochiVMContext->m_curModule->                         \
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn"); \
    ReleaseAssert(interpFn);                                                     \
                                                                                 \
    thread_pochiVMContext->m_curModule->EmitIR();                                \
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();              \
                                                                                 \
    SimpleJIT* jit = new SimpleJIT();                                            \
    jit->SetModule(thread_pochiVMContext->m_curModule);                          \
                                                                                 \
    FnPrototype jitFn = jit->GetFunction<FnPrototype>("MyFn");                   \
    auto gold = [](T v1, T v2) -> retType {                                      \
        return v1 opName v2;                                                     \
    };                                                                           \
    std::function<void(T,T)> compare = [gold, interpFn, jitFn](T v1, T v2) {     \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), interpFn(v1,v2));       \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), jitFn(v1,v2));          \
    };                                                                           \
    return compare;                                                              \
}

GetArithFn(GetAddFn, +, T)
GetArithFn(GetSubFn, -, T)
GetArithFn(GetMulFn, *, T)
GetArithFn(GetDivFn, /, T)
GetArithFn(GetModFn, %, T)
GetArithFn(GetLtFn, <, bool)
GetArithFn(GetLEqFn, <=, bool)
GetArithFn(GetGtFn, >, bool)
GetArithFn(GetGEqFn, >=, bool)
// float point direct comparison is safe in this specific case (we only provide constants)
//
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
GetArithFn(GetEqFn, ==, bool)
GetArithFn(GetNEqFn, !=, bool)
#pragma clang diagnostic pop

template<typename T>
void TestInterestingIntegerParams(std::function<std::function<void(T, T)>()> fnGen,
                                  bool isMulOp, bool isDivOp)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    std::function<void(T,T)> fn = fnGen();
    T start = 0;
    if (std::is_signed<T>::value)
    {
        start = static_cast<T>(-50);
        if (std::is_same<T, int8_t>::value)
        {
            start = static_cast<T>(-10);     // don't overflow
        }
    }
    T end = 50;
    if (sizeof(T) == 1)
    {
        end = static_cast<T>(10);            // don't overflow
    }
    T sf;
    if (!isMulOp)
    {
        sf = static_cast<T>(1) << (sizeof(T) * 8 - 8);
    }
    else
    {
        if (sizeof(T) == 1)
        {
            sf = 1;
        }
        else
        {
            sf = static_cast<T>(1) << ((sizeof(T) * 8 - 16) / 2);
        }
    }
    for (int mx = 0; mx < 4; mx++)
    {
        for (T v1 = start; v1 <= end; v1++)
        {
            for (T v2 = start; v2 <= end; v2++)
            {
                if (isDivOp && v2 == 0) continue;
                T x1 = v1;
                T x2 = v2;
                if (mx % 2 == 0) x1 *= sf;
                if (mx / 2 == 0) x2 *= sf;
                fn(x1, x2);
            }
        }
    }
}

template<typename T>
void TestInterestingFloatParams(std::function<std::function<void(T, T)>()> fnGen, bool isDivOp)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    std::function<void(T,T)> fn = fnGen();
    const static T values[21] = {
        static_cast<T>(0),
        static_cast<T>(-5),
        static_cast<T>(-4.5),
        static_cast<T>(-4),
        static_cast<T>(-3.5),
        static_cast<T>(-3),
        static_cast<T>(-2.5),
        static_cast<T>(-2),
        static_cast<T>(-1.5),
        static_cast<T>(-1),
        static_cast<T>(-0.5),
        static_cast<T>(0.5),
        static_cast<T>(1),
        static_cast<T>(1.5),
        static_cast<T>(2),
        static_cast<T>(2.5),
        static_cast<T>(3),
        static_cast<T>(3.5),
        static_cast<T>(4),
        static_cast<T>(4.5),
        static_cast<T>(5)
    };

    for (int i = 0; i < 21; i++)
    {
        for (int j = 0; j < 21; j++)
        {
            if (isDivOp && j == 0) continue;
            fn(values[i], values[j]);
        }
    }
}

void TestBoolParams(std::function<std::function<void(bool, bool)>()> fnGen)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    std::function<void(bool, bool)> fn = fnGen();

    for (bool v1 : {false, true})
    {
        for (bool v2 : {false, true})
        {
            fn(v1, v2);
        }
    }
}

}   // anonymous namespace


TEST(Sanity, ArithAndCompareExpr)
{
    // Test int types
    //
#define F(type) TestInterestingIntegerParams<type>(GetAddFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetSubFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetMulFn<type>, true /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetModFn<type>, false /*isMulOp*/, true /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetEqFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetNEqFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetLtFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetLEqFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetGtFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F
#define F(type) TestInterestingIntegerParams<type>(GetGEqFn<type>, false /*isMulOp*/, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
#undef F

    // Test float types
    //
#define F(type) TestInterestingFloatParams<type>(GetAddFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetSubFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetMulFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetDivFn<type>, true /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetEqFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetNEqFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetLtFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetLEqFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetGtFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F
#define F(type) TestInterestingFloatParams<type>(GetGEqFn<type>, false /*isDivOp*/);
    FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F

    // Test bool types, only == and != are supported
    //
    TestBoolParams(GetEqFn<bool>);
    TestBoolParams(GetNEqFn<bool>);
}
