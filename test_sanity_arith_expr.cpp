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

template<typename T, typename U>
void AdditionWithDifferentTypesHelper(T lhs, U rhs)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;
    NewModule("test");
    using FnPrototype = decltype(lhs + rhs) (*)(T, U);
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
    std::vector<decltype(jitFn(lhs, rhs))> answers = {jitFn(lhs, rhs), interpFn(lhs, rhs), fastinterpFn(lhs, rhs)};

    // Ignore warnings about implicit float/double conversions/promotions
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
    #pragma clang diagnostic ignored "-Wdouble-promotion"

    auto expected = lhs + rhs;

    #pragma clang diagnostic pop

    ReleaseAssert(typeid(answers[0]) == typeid(expected));
    for(auto answer : answers)
        CompareResults(static_cast<decltype(lhs + rhs)>(lhs),
                    static_cast<decltype(lhs + rhs)>(rhs), answer, expected);
}

template <typename T, typename U>
void TestAdditionWithDifferentTypes(T v1, U v2) {
    AdditionWithDifferentTypesHelper(v1, v2);
    AdditionWithDifferentTypesHelper(v2, v1);
}

#define FOR_EACH_PARAMS(a, b, c, d) \
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

void TestAdditionSignedWithPromotion() {
    int32_t pos_32 = 15;
    int32_t neg_32 = -15;
    int64_t pos_64 = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
    int64_t neg_64 = static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1;
    #define F(v1, v2) TestAdditionWithDifferentTypes(v1, v2);
    FOR_EACH_PARAMS(pos_32, neg_32, pos_64, neg_64)
    #undef F
    int32_t max_32 = std::numeric_limits<int32_t>::max();
    int32_t min_32 = std::numeric_limits<int32_t>::min();

    TestAdditionWithDifferentTypes(max_32, pos_64);
    TestAdditionWithDifferentTypes(max_32, neg_64);
    TestAdditionWithDifferentTypes(min_32, pos_64);
    TestAdditionWithDifferentTypes(min_32, neg_64);

    float pos_float = static_cast<float>(1.234567);
    float neg_float = static_cast<float>(-1.234567);
    double pos_double = static_cast<double>(std::numeric_limits<float>::max()) + 3.03;
    double neg_double = static_cast<double>(std::numeric_limits<float>::min()) - 3.03;
    int64_t small_pos_64 = 15;
    int64_t small_neg_64 = -15;

    TestAdditionWithDifferentTypes(pos_float, pos_32);
    TestAdditionWithDifferentTypes(pos_float, neg_32);
    TestAdditionWithDifferentTypes(pos_float, small_pos_64);
    TestAdditionWithDifferentTypes(pos_float, small_neg_64);
    TestAdditionWithDifferentTypes(pos_double, pos_32);
    TestAdditionWithDifferentTypes(pos_double, neg_32);
    TestAdditionWithDifferentTypes(pos_double, max_32);
    TestAdditionWithDifferentTypes(pos_double, min_32);
    TestAdditionWithDifferentTypes(pos_double, small_pos_64);
    TestAdditionWithDifferentTypes(pos_double, small_neg_64);
    TestAdditionWithDifferentTypes(pos_double, pos_64);
    TestAdditionWithDifferentTypes(pos_double, neg_64);

    TestAdditionWithDifferentTypes(pos_double, pos_float);
    TestAdditionWithDifferentTypes(pos_double, neg_float);
    TestAdditionWithDifferentTypes(neg_double, pos_float);
    TestAdditionWithDifferentTypes(neg_double, neg_float);

    TestAdditionWithDifferentTypes(neg_float, pos_32);
    TestAdditionWithDifferentTypes(neg_float, neg_32);
    TestAdditionWithDifferentTypes(neg_float, small_pos_64);
    TestAdditionWithDifferentTypes(neg_float, small_neg_64);
    TestAdditionWithDifferentTypes(neg_double, pos_32);
    TestAdditionWithDifferentTypes(neg_double, neg_32);
    TestAdditionWithDifferentTypes(neg_double, max_32);
    TestAdditionWithDifferentTypes(neg_double, min_32);
    TestAdditionWithDifferentTypes(neg_double, small_pos_64);
    TestAdditionWithDifferentTypes(neg_double, small_neg_64);
    TestAdditionWithDifferentTypes(neg_double, pos_64);
    TestAdditionWithDifferentTypes(neg_double, neg_64);
}

void TestAdditionUnsignedWithPromotion() {
    uint32_t pos_32 = 15;
    uint32_t max_32 = std::numeric_limits<uint32_t>::max();
    uint64_t pos_64 = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;
    uint64_t max_64 = std::numeric_limits<uint64_t>::max();
    #define F(v1, v2) TestAdditionWithDifferentTypes(v1, v2);
    FOR_EACH_PARAMS(pos_32, max_32, pos_64, max_64)
    #undef F
    uint64_t small_pos_64 = 15;
    float pos_float = static_cast<float>(1.234567);
    double pos_double = static_cast<double>(std::numeric_limits<float>::max()) + static_cast<double>(3.03);

    TestAdditionWithDifferentTypes(pos_float, pos_32);
    TestAdditionWithDifferentTypes(pos_float, small_pos_64);
    TestAdditionWithDifferentTypes(pos_float, pos_double);
    TestAdditionWithDifferentTypes(pos_double, pos_32);
    TestAdditionWithDifferentTypes(pos_double, max_32);
    TestAdditionWithDifferentTypes(pos_double, small_pos_64);
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
