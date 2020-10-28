#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"

using namespace PochiVM;

namespace {

void TestEachInterestingIntParam(const std::function<void(uint64_t)>& testFn)
{
    const static uint64_t choice[4] = {
        static_cast<uint64_t>(123),
        static_cast<uint64_t>(-123),
        static_cast<uint64_t>(0),
        static_cast<uint64_t>(233)
    };

    for (int bitmask = 0; bitmask < (4 << 8); bitmask++)
    {
        uint64_t value = 0;
        int x = bitmask;
        for (int i = 0; i < 8; i++)
        {
            value += (choice[x % 4]) * (static_cast<uint64_t>(1) << (i * 8));
            x /= 4;
        }
        testFn(value);
    }
}

template<typename T, typename U>
std::function<U(T)> GetStaticCastFn()
{
    using FnPrototype = U(*)(T);
    auto [fn, val] = NewFunction<FnPrototype>("MyFn");

    fn.SetBody(Return(StaticCast<U>(val)));

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);

    // TODO: this leaks. Fix later
    //
    SimpleJIT* jit = new SimpleJIT();

    jit->SetModule(thread_pochiVMContext->m_curModule);

    FnPrototype jitFn = jit->GetFunction<FnPrototype>("MyFn");

    auto interpFn = thread_pochiVMContext->m_curModule->
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");
    ReleaseAssert(interpFn);

    FastInterpFunction<FnPrototype> fastInterpFn = thread_pochiVMContext->m_curModule->
                           GetFastInterpGeneratedFunction<FnPrototype>("MyFn");

    return [jitFn, interpFn, fastInterpFn](T value) -> U {
        U out1 = interpFn(value);
        U out2 = jitFn(value);
        U out3 = fastInterpFn(value);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
        // float equal is safe here: static cast is deterministic
        //
        ReleaseAssert(out1 == out2);
        ReleaseAssert(out1 == out3);
#pragma clang diagnostic pop
        return out1;
    };
}

template<typename T, typename U>
void TestStaticCastBetweenIntTypes()
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    std::function<U(T)> fn = GetStaticCastFn<T, U>();

    auto testFn = [&](uint64_t _v)
    {
        T value = static_cast<T>(_v);
        ReleaseAssert(fn(value) == static_cast<U>(value));
    };

    TestEachInterestingIntParam(testFn);
}

template<typename T, typename U>
void TestStaticCastBetweenFloatTypes()
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    std::function<U(T)> fn = GetStaticCastFn<T, U>();

    // float equal is safe here: static cast is deterministic
    //
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    auto testFn = [&](T value)
    {
        ReleaseAssert(fn(value) == static_cast<U>(value));
    };
#pragma clang diagnostic pop

    testFn(static_cast<T>(0));

    testFn(static_cast<T>(123.45));
    testFn(static_cast<T>(-123.45));

    testFn(static_cast<T>(987.65));
    testFn(static_cast<T>(-987.65));
}

template<typename T, typename U>
void TestStaticCastIntToFloat()
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    std::function<U(T)> fn = GetStaticCastFn<T, U>();

    auto testFn = [&](uint64_t _v)
    {
        T value = static_cast<T>(_v);
        U result = fn(value);
        double diff = fabs(static_cast<double>(value) - static_cast<double>(result));
        double tol = (std::is_same<U, double>::value) ? 1e-14 : 1e-6;
        // If absolute diff < tol, just return. 'value' may be 0...
        if (diff < tol) { return; }

        double relDiff = diff / fabs(static_cast<double>(value));
        ReleaseAssert(relDiff < tol);
    };

    TestEachInterestingIntParam(testFn);
}

void TestStaticCastPointerTypes()
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    // currently the only possible one is casting to void*
    // TODO: add more when we support CPP classes
    //
    std::function<void*(int*)> fn = GetStaticCastFn<int*, void*>();
    int x = 233;
    int* px = &x;
    ReleaseAssert(static_cast<void*>(px) == fn(px));
}

}   // anonymous namespace

TEST(Sanity, StaticCast)
{
    // Static cast between all primitive integer types
    //
#define F(type) TestStaticCastBetweenIntTypes<bool, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<int8_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<uint8_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<int16_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<uint16_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<int32_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<uint32_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<int64_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastBetweenIntTypes<uint64_t, type>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F

    // Static cast between floating point types
    //
    TestStaticCastBetweenFloatTypes<float, float>();
    TestStaticCastBetweenFloatTypes<float, double>();
    TestStaticCastBetweenFloatTypes<double, float>();
    TestStaticCastBetweenFloatTypes<double, double>();

    // Static cast from int to floating point types
    //
#define F(type) TestStaticCastIntToFloat<type, float>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
#define F(type) TestStaticCastIntToFloat<type, double>();
    FOR_EACH_PRIMITIVE_INT_TYPE
#undef F

    // Static cast on pointer types
    //
    TestStaticCastPointerTypes();
}

TEST(SanityError, StaticCastNullptrDisallowed)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    std::function<void*(int*)> fn = GetStaticCastFn<int*, void*>();

#ifdef TESTBUILD
    // static_cast on NULL (0) fires a TestAssert
    // Google test somehow triggers a bunch of warnings...
    //
    printf("Expecting a TestAssert being fired...\n");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wcovered-switch-default"

    // TODO: This is not good enough, since we want to make sure both interpFn and generatedFn fires assert,
    // and the death is caused by the assert we expected, not some random other failures.
    // And furthermore, currently generatedFn does NOT fire the assert. Fix later.
    //
    ASSERT_DEATH(fn(reinterpret_cast<int*>(0)), "");

#pragma clang diagnostic pop
#endif
    std::ignore = fn;
}

TEST(Sanity, ReinterpretCast_1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    double x = 123.4;
    using FnPrototype = uint64_t*(*)(double*);
    auto [fn, val] = NewFunction<FnPrototype>("MyFn");

    fn.SetBody(Return(ReinterpretCast<uint64_t*>(val)));

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto interpFn = thread_pochiVMContext->m_curModule->
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");

    FastInterpFunction<FnPrototype> fastinterpFn = thread_pochiVMContext->m_curModule->
                           GetFastInterpGeneratedFunction<FnPrototype>("MyFn");

    union {
        double vd;
        uint64_t vi;
    } u;
    u.vd = x;
    ReleaseAssert(*interpFn(&x) == u.vi);
    ReleaseAssert(*fastinterpFn(&x) == u.vi);
}

TEST(Sanity, ReinterpretCast_2)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    double x = 123.4;
    using FnPrototype = uint64_t(*)(double*);
    auto [fn, val] = NewFunction<FnPrototype>("MyFn");

    fn.SetBody(Return(ReinterpretCast<uint64_t>(val)));

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto interpFn = thread_pochiVMContext->m_curModule->
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");

    FastInterpFunction<FnPrototype> fastinterpFn = thread_pochiVMContext->m_curModule->
                           GetFastInterpGeneratedFunction<FnPrototype>("MyFn");

    ReleaseAssert(interpFn(&x) == reinterpret_cast<uintptr_t>(&x));
    ReleaseAssert(fastinterpFn(&x) == reinterpret_cast<uintptr_t>(&x));
}

TEST(Sanity, ReinterpretCast_3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    double x = 123.4;
    using FnPrototype = double*(*)(uint64_t);
    auto [fn, val] = NewFunction<FnPrototype>("MyFn");

    fn.SetBody(Return(ReinterpretCast<double*>(val)));

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    auto interpFn = thread_pochiVMContext->m_curModule->
                           GetDebugInterpGeneratedFunction<FnPrototype>("MyFn");

    FastInterpFunction<FnPrototype> fastinterpFn = thread_pochiVMContext->m_curModule->
                           GetFastInterpGeneratedFunction<FnPrototype>("MyFn");

    ReleaseAssert(interpFn(reinterpret_cast<uintptr_t>(&x)) == &x);
    ReleaseAssert(fastinterpFn(reinterpret_cast<uintptr_t>(&x)) == &x);
}
