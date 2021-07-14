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
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();                  \
    auto interpFn = thread_pochiVMContext->m_curModule->                         \
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn"); \
    ReleaseAssert(interpFn);                                                     \
    auto fastinterpFn = thread_pochiVMContext->m_curModule->                     \
                           GetFastInterpGeneratedFunction<FnPrototype>("MyFn");  \
    ReleaseAssert(fastinterpFn);                                                 \
                                                                                 \
    thread_pochiVMContext->m_curModule->EmitIR();                                \
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);              \
                                                                                 \
    SimpleJIT* jit = new SimpleJIT();                                            \
    jit->SetModule(thread_pochiVMContext->m_curModule);                          \
                                                                                 \
    FnPrototype jitFn = jit->GetFunction<FnPrototype>("MyFn");                   \
    auto gold = [](T v1, T v2) -> retType {                                      \
        return v1 opName v2;                                                     \
    };                                                                           \
    std::function<void(T,T)> compare = [gold, interpFn, jitFn, fastinterpFn]     \
                                       (T v1, T v2) {                            \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), interpFn(v1,v2));       \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), jitFn(v1,v2));          \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), fastinterpFn(v1,v2));   \
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

// Test pochivm addition of two numbers with potentially different types.
//
template<typename T, typename U>
void AdditionWithDifferentTypesHelper(T lhs, U rhs)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;
    NewModule("test");
    using FnPrototype = typename AstTypeHelper::ArithReturnType<T, U>::type (*)(T, U);
    auto [fn, a, b] = NewFunction<FnPrototype>("MyFn");
    fn.SetBody(
        Return(a + b)
    );
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    auto interpFn = thread_pochiVMContext->m_curModule->
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");
    ReleaseAssert(interpFn);
    auto fastinterpFn = thread_pochiVMContext->m_curModule->
                           GetFastInterpGeneratedFunction<FnPrototype>("MyFn");
    ReleaseAssert(fastinterpFn);                                                 
    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);
    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);
    FnPrototype jitFn = jit.GetFunction<FnPrototype>("MyFn");
    std::vector<typename AstTypeHelper::ArithReturnType<T, U>::type> answers = {jitFn(lhs, rhs), interpFn(lhs, rhs), fastinterpFn(lhs, rhs)};

    // Ignore warnings about implicit float/double conversions/promotions
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
    #pragma clang diagnostic ignored "-Wdouble-promotion"

    typename AstTypeHelper::ArithReturnType<T, U>::type expected = lhs + rhs;

    #pragma clang diagnostic pop

    ReleaseAssert(typeid(answers[0]) == typeid(expected));
    for(auto answer : answers)
        CompareResults(static_cast<typename AstTypeHelper::ArithReturnType<T, U>::type>(lhs),
                    static_cast<typename AstTypeHelper::ArithReturnType<T, U>::type>(rhs), answer, expected);
}

template <typename T, typename U>
void TestAdditionWithDifferentTypesSingleCase(T v1, U v2) {
    AdditionWithDifferentTypesHelper(v1, v2);
    AdditionWithDifferentTypesHelper(v2, v1);
}

#define ENUMERATE_ALL_PARAMS(a, b, c, d) \
F(a, a) \
F(a, b) \
F(a, c) \
F(a, d) \
F(b, b) \
F(b, c) \
F(b, d) \
F(c, c) \
F(c, d) \
F(d, d)

#define ENUMERATE_ALL_PARAMS_WITH_FIRST(a, b, c, d, e) \
F(a, b) \
F(a, c) \
F(a, d) \
F(a, e)

// Tests addition of signed integers and floats/doubles where they may not have the same type so
// integer->bigger integer promotion/integer->float promotion is required. The difference between
// this and the unsigned integer test is that overflow in signed integer addition is UB so this test
// can only enumerate the options that don't have overflow
//
void TestAdditionSignedWithPromotion() {
    int8_t pos_8 = 15;
    int8_t neg_8 = -15;
    int16_t pos_16 = static_cast<int16_t>(std::numeric_limits<int8_t>::max()) + 1;
    int16_t neg_16 = static_cast<int16_t>(std::numeric_limits<int8_t>::min()) - 1;
    int32_t pos_32 = static_cast<int32_t>(std::numeric_limits<int16_t>::max()) + 1;
    int32_t neg_32 = static_cast<int32_t>(std::numeric_limits<int16_t>::min()) - 1;
    int64_t pos_64 = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
    int64_t neg_64 = static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1;
    float pos_float = static_cast<float>(1.234567);
    float neg_float = static_cast<float>(-1.234567);
    double pos_double = static_cast<double>(std::numeric_limits<float>::max()) + 3.03;
    double neg_double = static_cast<double>(std::numeric_limits<float>::min()) - 3.03;

    #define F(v1, v2) TestAdditionWithDifferentTypesSingleCase(v1, v2);
    ENUMERATE_ALL_PARAMS_WITH_FIRST(pos_8, pos_16, pos_32, pos_double, pos_float)
    ENUMERATE_ALL_PARAMS_WITH_FIRST(neg_8, neg_16, neg_32, neg_double, neg_float)
    ENUMERATE_ALL_PARAMS_WITH_FIRST(pos_16, pos_32, pos_64, pos_float, pos_double)
    ENUMERATE_ALL_PARAMS_WITH_FIRST(neg_16, neg_32, neg_64, neg_float, neg_double)
    ENUMERATE_ALL_PARAMS_WITH_FIRST(pos_32, pos_32, pos_64, pos_float, pos_double)
    ENUMERATE_ALL_PARAMS_WITH_FIRST(neg_32, neg_32, neg_64, neg_float, neg_double)
    #undef F
}

// Tests addition of unsigned integers and floats/doubles where they may not have the same type so
// integer->bigger integer promotion/integer->float promotion is required.
//
void TestAdditionUnsignedWithPromotion() {
    uint8_t pos_8 = 15;
    uint8_t max_8 = std::numeric_limits<uint8_t>::max();
    uint16_t pos_16 = std::numeric_limits<uint8_t>::max() + 1;
    uint16_t max_16 = std::numeric_limits<uint16_t>::max();
    uint32_t pos_32 = std::numeric_limits<uint16_t>::max() + 1;
    uint32_t max_32 = std::numeric_limits<uint32_t>::max();
    uint64_t pos_64 = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;
    uint64_t max_64 = std::numeric_limits<uint64_t>::max();
    #define F(v1, v2) TestAdditionWithDifferentTypesSingleCase(v1, v2);
    ENUMERATE_ALL_PARAMS(pos_8, max_8, pos_16, max_16)
    ENUMERATE_ALL_PARAMS(pos_8, max_8, pos_32, max_32)
    ENUMERATE_ALL_PARAMS(pos_8, max_8, pos_64, max_64)
    ENUMERATE_ALL_PARAMS(pos_16, max_16, pos_32, max_32)
    ENUMERATE_ALL_PARAMS(pos_16, max_16, pos_64, max_64)
    ENUMERATE_ALL_PARAMS(pos_64, max_64, pos_32, max_32)
    #undef F
    uint64_t small_pos_64 = 15;
    float pos_float = static_cast<float>(1.234567);
    double pos_double = static_cast<double>(std::numeric_limits<float>::max()) + static_cast<double>(3.03);

    // We must be careful to not cause overflow when adding with floats/doubles since that is UB
    #define F(v1, v2) TestAdditionWithDifferentTypesSingleCase(v1, v2);
    ENUMERATE_ALL_PARAMS_WITH_FIRST(pos_float, max_8, max_16, pos_32, small_pos_64)
    ENUMERATE_ALL_PARAMS_WITH_FIRST(pos_double, pos_float, max_16, max_32, pos_64)
    #undef F
}

void TestAdditionWithPromotion() {
    TestAdditionSignedWithPromotion();
    TestAdditionUnsignedWithPromotion();
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
#define F(type) TestInterestingIntegerParams<type>(GetDivFn<type>, false /*isMulOp*/, true /*isDivOp*/);
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

    TestAdditionWithPromotion();
}

