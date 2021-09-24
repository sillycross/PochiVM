#include "gtest/gtest.h"
#include <limits>
#include <type_traits>

#include "pochivm.h"
#include "pochivm/ast_type_helper.h"
#include "pochivm/for_each_primitive_type.h"
#include "test_util_helper.h"

using namespace PochiVM;

namespace {

template<typename T, typename U, typename retType>
void CompareResults(T v1, U v2, retType r1, retType r2)
{
    if constexpr (std::is_floating_point<T>::value || std::is_floating_point<U>::value)
    {
        double diff = fabs(static_cast<double>(r1) - static_cast<double>(r2));
        double tol = 1e-6;
        if (diff < tol) { return; }
        double relDiff = diff / std::max(fabs(static_cast<double>(v1)), fabs(static_cast<double>(v2)));
        ReleaseAssert(relDiff < tol);
    } else {
        ReleaseAssert(r1 == r2);
    }
}

// TODO: currently 'new SimpleJIT()' leaks
//
#define GenNoLiteralArithFnTester(fnName, opName, retType)              \
template<typename T, typename U>                                        \
std::function<void(T, U)> fnName()                                      \
{                                                                       \
    thread_pochiVMContext->m_curModule = new AstModule("test");\
    using CommonType = typename AstTypeHelper::ArithReturnType<T, U>::type; \
    using FnPrototype = retType(*)(T, U);                               \
    auto [fn, val1, val2] = NewFunction<FnPrototype>("MyFn");           \
    fn.SetBody(Return(val1 opName val2));                               \
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());      \
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();        \
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();         \
    auto interpFn = thread_pochiVMContext->m_curModule->                \
        GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");           \
    ReleaseAssert(interpFn);                                            \
    auto fastinterpFn = thread_pochiVMContext->m_curModule->            \
        GetFastInterpGeneratedFunction<FnPrototype>("MyFn");            \
    ReleaseAssert(fastinterpFn);                                        \
                                                                        \
    thread_pochiVMContext->m_curModule->EmitIR();                       \
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/); \
                                                                        \
    SimpleJIT* jit = new SimpleJIT();                                   \
    jit->SetModule(thread_pochiVMContext->m_curModule);                 \
                                                                        \
    FnPrototype jitFn = jit->GetFunction<FnPrototype>("MyFn");          \
    auto gold = [](T v1, U v2) -> retType {                             \
        return static_cast<CommonType>(v1 opName v2);                   \
    };                                                                  \
    std::function<void(T,U)> compare = [gold, interpFn, jitFn, fastinterpFn]     \
                                       (T v1, U v2) {                            \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), interpFn(v1,v2));       \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), jitFn(v1,v2));          \
        CompareResults<T, retType>(v1, v2, gold(v1, v2), fastinterpFn(v1,v2));   \
    };                                                                           \
    return compare;                                                     \
}                                                                       \
template<typename T, typename U>                                        \
void fnName(T lhs, U rhs)                                               \
{                                                                       \
    AutoThreadPochiVMContext apv;                                       \
    AutoThreadErrorContext arc;                                         \
    AutoThreadLLVMCodegenContext alc;                                   \
    thread_pochiVMContext->m_curModule = new AstModule("test");         \
    static std::function<void(T, U)> test_fn = fnName<T, U>();          \
    test_fn(lhs, rhs);                                                  \
}

#define GenLeftLiteralArithFnTester(fnName, opName, retType)            \
template<typename T, typename U>                                        \
void fnName(T lhs, U rhs)                                               \
{                                                                       \
    AutoThreadPochiVMContext apv;                                       \
    AutoThreadErrorContext arc;                                         \
    AutoThreadLLVMCodegenContext alc;                                   \
    thread_pochiVMContext->m_curModule = new AstModule("test");         \
    using CommonType = typename AstTypeHelper::ArithReturnType<T, U>::type; \
    using FnPrototype = retType(*)(U);                                  \
    auto [fn, val1] = NewFunction<FnPrototype>("MyFn");                 \
    fn.SetBody(Return(lhs opName val1));                                \
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());      \
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();        \
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();         \
    auto interpFn = thread_pochiVMContext->m_curModule->                \
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn"); \
    ReleaseAssert(interpFn);                                                     \
    auto fastinterpFn = thread_pochiVMContext->m_curModule->                     \
                           GetFastInterpGeneratedFunction<FnPrototype>("MyFn");  \
    ReleaseAssert(fastinterpFn);                                                 \
                                                                                 \
    thread_pochiVMContext->m_curModule->EmitIR();                                \
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);              \
                                                                                 \
    auto gold = [](T v1, U v2) -> retType {                                      \
        return static_cast<CommonType>(v1 opName v2);                            \
    };                                                                           \
    CompareResults<T, U, retType>(lhs, rhs, gold(lhs, rhs), interpFn(rhs));      \
    CompareResults<T, U, retType>(lhs, rhs, gold(lhs, rhs), fastinterpFn(rhs));  \
}

#define GenRightLiteralArithFnTester(fnName, opName, retType)           \
template<typename T, typename U>                                        \
void fnName(T lhs, U rhs)                                               \
{                                                                       \
    AutoThreadPochiVMContext apv;                                       \
    AutoThreadErrorContext arc;                                         \
    AutoThreadLLVMCodegenContext alc;                                   \
    thread_pochiVMContext->m_curModule = new AstModule("test");         \
    using CommonType = typename AstTypeHelper::ArithReturnType<T, U>::type; \
    using FnPrototype = retType(*)(T);                                  \
    auto [fn, val1] = NewFunction<FnPrototype>("MyFn");                 \
    fn.SetBody(Return(val1 opName rhs));                                \
    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());      \
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();        \
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();         \
    auto interpFn = thread_pochiVMContext->m_curModule->                \
        GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");           \
    ReleaseAssert(interpFn);                                            \
    auto fastinterpFn = thread_pochiVMContext->m_curModule->            \
        GetFastInterpGeneratedFunction<FnPrototype>("MyFn");            \
    ReleaseAssert(fastinterpFn);                                        \
                                                                        \
    thread_pochiVMContext->m_curModule->EmitIR();                       \
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/); \
                                                                        \
    auto gold = [](T v1, U v2) -> retType {                             \
        return static_cast<CommonType>(v1 opName v2);                   \
    };                                                                  \
    CompareResults<T, U, retType>(lhs, rhs, gold(lhs, rhs), interpFn(lhs)); \
    CompareResults<T, U, retType>(lhs, rhs, gold(lhs, rhs), fastinterpFn(lhs)); \
}
#define COMMA ,
#define ArithRet typename AstTypeHelper::ArithReturnType<T COMMA U>::type
#pragma clang diagnostic push
// CompareResults already accounts for any lost precision between conversions so
// ignore warnings about them
//
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"

GenLeftLiteralArithFnTester(TestAddLeftLiteral, +, ArithRet);
GenRightLiteralArithFnTester(TestAddRightLiteral, +, ArithRet);
GenNoLiteralArithFnTester(TestAddNoLiteral, +, ArithRet);

GenLeftLiteralArithFnTester(TestSubLeftLiteral, +, ArithRet);
GenRightLiteralArithFnTester(TestSubRightLiteral, +, ArithRet);
GenNoLiteralArithFnTester(TestSubNoLiteral, +, ArithRet);

GenLeftLiteralArithFnTester(TestMulLeftLiteral, *, ArithRet);
GenRightLiteralArithFnTester(TestMulRightLiteral, *, ArithRet);
GenNoLiteralArithFnTester(TestMulNoLiteral, *, ArithRet);

GenLeftLiteralArithFnTester(TestDivLeftLiteral, /, ArithRet);
GenRightLiteralArithFnTester(TestDivRightLiteral, /, ArithRet);
GenNoLiteralArithFnTester(TestDivNoLiteral, /, ArithRet);

GenLeftLiteralArithFnTester(TestModLeftLiteral, %, ArithRet);
GenRightLiteralArithFnTester(TestModRightLiteral, %, ArithRet);
GenNoLiteralArithFnTester(TestModNoLiteral, %, ArithRet);

GenLeftLiteralArithFnTester(TestLTLeftLiteral, <, bool);
GenRightLiteralArithFnTester(TestLTRightLiteral, <, bool);
GenNoLiteralArithFnTester(TestLTNoLiteral, <, bool);

GenLeftLiteralArithFnTester(TestLEQLeftLiteral, <=, bool);
GenRightLiteralArithFnTester(TestLEQRightLiteral, <=, bool);
GenNoLiteralArithFnTester(TestLEQNoLiteral, <=, bool);

GenLeftLiteralArithFnTester(TestEQLeftLiteral, ==, bool);
GenRightLiteralArithFnTester(TestEQRightLiteral, ==, bool);
GenNoLiteralArithFnTester(TestEQNoLiteral, ==, bool);

GenLeftLiteralArithFnTester(TestNEQLeftLiteral, !=, bool);
GenRightLiteralArithFnTester(TestNEQRightLiteral, !=, bool);
GenNoLiteralArithFnTester(TestNEQNoLiteral, !=, bool);

GenLeftLiteralArithFnTester(TestGEQLeftLiteral, >=, bool);
GenRightLiteralArithFnTester(TestGEQRightLiteral, >=, bool);
GenNoLiteralArithFnTester(TestGEQNoLiteral, >=, bool);

GenLeftLiteralArithFnTester(TestGTLeftLiteral, >, bool);
GenRightLiteralArithFnTester(TestGTRightLiteral, >, bool);
GenNoLiteralArithFnTester(TestGTNoLiteral, >, bool);

#pragma clang diagnostic pop
}   // anonymous namespace

// execute each fn in `fns` with lhs and rhs
template<typename T, typename U>
void apply_fns(std::vector<std::function<void(T, U)>> fns, T lhs, U rhs)
{
    for(std::function<void(T, U)>fn : fns)
    {
        fn(lhs, rhs);
    }
}
template <typename T, typename U>
void TestSignedAdditionAndSubtractionAndComparison()
{
    T minT = std::numeric_limits<T>::min() + 1;
    T maxT = std::numeric_limits<T>::max() - 1;
    U minU = std::numeric_limits<U>::min() + 1;
    U maxU = std::numeric_limits<U>::max() - 1;
    std::vector<std::function<void(T, U)>> testers;
    testers.push_back([](T lhs, U rhs){TestAddNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestAddLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestAddRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGTNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGTLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGTRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestNEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestNEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestNEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLTNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLTLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLTRightLiteral(lhs, rhs);});
    // Test signed promotions. All other cases have been covered in the unsigned tests
    // so don't iterate over possible values to save time
    apply_fns<T, U>(testers, maxT / 2, maxU / 2);
    apply_fns<T, U>(testers, minT / 2, minU / 2);
}

template <typename T, typename U>
void TestSignedMultiplicationModAndDivision()
{
    T minT = std::numeric_limits<T>::min() + 1;
    T maxT = std::numeric_limits<T>::max() - 1;
    U minU = std::numeric_limits<U>::min() + 1;
    U maxU = std::numeric_limits<U>::max() - 1;
    std::vector<std::function<void(T, U)>> testers;
    testers.push_back([](T lhs, U rhs){TestMulNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestMulLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestMulRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestModNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestModLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestModRightLiteral(lhs, rhs);});
    // Test signed promotions. All other cases have been covered in the unsigned tests
    // so don't iterate over possible values to save time
    apply_fns<T, U>(testers, maxT / 2, static_cast<U>(2));
    apply_fns<T, U>(testers, minT / 2, static_cast<U>(2));
    apply_fns<T, U>(testers, static_cast<T>(2), maxU / 2);
    apply_fns<T, U>(testers, static_cast<T>(2), minU / 2);
}

template <typename T, typename U>
void TestUnsignedAdditionAndSubtractionAndComparison()
{
    T maxT = std::numeric_limits<T>::max();
    U maxU = std::numeric_limits<U>::max();
    std::vector<std::function<void(T, U)>> testers;
    testers.push_back([](T lhs, U rhs){TestAddNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestAddLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestAddRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGTNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGTLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGTRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestGEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestNEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestNEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestNEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLEQNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLEQLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLEQRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLTNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLTLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestLTRightLiteral(lhs, rhs);});
    unsigned int steps = 8;
    for(T t = 0; t < maxT - maxT / steps; t += maxT / steps)
    {
        for(U u = 0; u < maxU - maxU / steps; u += maxU / steps)
        {
            apply_fns<T, U>(testers, t, u);
        }
    }
}

template <typename T, typename U>
void TestUnsignedMultiplicationModAndDivision()
{
    T maxT = std::numeric_limits<T>::max();
    U maxU = std::numeric_limits<U>::max();
    std::vector<std::function<void(T, U)>> testers;
    testers.push_back([](T lhs, U rhs){TestMulNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestMulLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestMulRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestModNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestModLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestModRightLiteral(lhs, rhs);});
    unsigned int steps = 8;
    for(T t = 1; t <= steps + 1/* ensures overflow */; ++t)
    {
        U u = maxU / steps;
        apply_fns<T, U>(testers, t, u);
    }
    for(U u = 1; u <= steps + 1; ++u)
    {
        T t = maxT / steps;
        apply_fns<T, U>(testers, t, u);
    }
}

// Ensures that everything is promoted to float when necessary
//
template <typename T, typename U>
void TestFloatingPromotions()
{
    std::vector<std::function<void(T, U)>> testers;
    testers.push_back([](T lhs, U rhs){TestAddNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestAddLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestAddRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestSubRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestMulNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestMulLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestMulRightLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivNoLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivLeftLiteral(lhs, rhs);});
    testers.push_back([](T lhs, U rhs){TestDivRightLiteral(lhs, rhs);});
    T t = static_cast<T>(100) / static_cast<T>(11);
    U u = static_cast<U>(100) / static_cast<U>(11);
    apply_fns<T, U>(testers, t, u);
}

void TestBoolParams()
{
    std::vector<std::function<void(bool, bool)>> testers;
    testers.push_back([](bool lhs, bool rhs){TestEQNoLiteral(lhs, rhs);});
    testers.push_back([](bool lhs, bool rhs){TestEQLeftLiteral(lhs, rhs);});
    testers.push_back([](bool lhs, bool rhs){TestEQRightLiteral(lhs, rhs);});
    testers.push_back([](bool lhs, bool rhs){TestNEQNoLiteral(lhs, rhs);});
    testers.push_back([](bool lhs, bool rhs){TestNEQLeftLiteral(lhs, rhs);});
    testers.push_back([](bool lhs, bool rhs){TestNEQRightLiteral(lhs, rhs);});
    apply_fns<bool, bool>(testers, true, true);
    apply_fns<bool, bool>(testers, true, false);
    apply_fns<bool, bool>(testers, false, true);
    apply_fns<bool, bool>(testers, false, false);
}

TEST(Sanity, ArithAndCompareExpr)
{
    ENUMERATE_ALL_TYPES(TestSignedAdditionAndSubtractionAndComparison, int8_t, int16_t, int32_t, int64_t)
    ENUMERATE_ALL_TYPES(TestUnsignedAdditionAndSubtractionAndComparison, uint8_t, uint16_t, uint32_t, uint64_t)
    ENUMERATE_ALL_TYPES(TestSignedMultiplicationModAndDivision, int8_t, int16_t, int32_t, int64_t)
    ENUMERATE_ALL_TYPES(TestUnsignedMultiplicationModAndDivision, uint8_t, uint16_t, uint32_t, uint64_t)
    #define F(type) TestFloatingPromotions<type, float>(); TestFloatingPromotions<type, double>(); \
                    TestFloatingPromotions<float, type>(); TestFloatingPromotions<double, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE_EXCEPT_BOOL
    #undef F
    TestBoolParams();
}
