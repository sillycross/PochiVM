#include "gtest/gtest.h"

#include "fastinterp/fastinterp.h"

using namespace PochiVM;

TEST(TestFastInterpInternal, SanitySymbolNames)
{
    // Sanity check 'SelectBoilerplateBluePrint' indeed selected the boilerplate we expected by checking its symbol name
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
    const FastInterpBoilerplateBluePrint* blueprint;
    blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               OperandShapeCategory::LITERAL_NONZERO,
                                                               OperandShapeCategory::LITERAL_NONZERO,
                                                               AstArithmeticExprType::ADD);
    TestAssert(blueprint->TestOnly_GetSymbolName() == std::string("_ZN7PochiVM20FIArithmeticExprImpl1fIiiiLNS_20OperandShapeCategoryE4ELS2_4ELNS_21AstArithmeticExprTypeE0EEEvPT_"));
    blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               OperandShapeCategory::COMPLEX,
                                                               OperandShapeCategory::ZERO,
                                                               AstArithmeticExprType::MUL);
    TestAssert(blueprint->TestOnly_GetSymbolName() == std::string("_ZN7PochiVM20FIArithmeticExprImpl1fIdiiLNS_20OperandShapeCategoryE6ELS2_5ELNS_21AstArithmeticExprTypeE2EEEvPT_"));
    std::ignore = blueprint;
}

TEST(TestFastInterpInternal, Sanity_1)
{
    // Test the simplest case: add two zeros.. no placeholders shall be needed
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
    const FastInterpBoilerplateBluePrint* blueprint;
    blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               OperandShapeCategory::ZERO,
                                                               OperandShapeCategory::ZERO,
                                                               AstArithmeticExprType::ADD);
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(blueprint);
    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(int*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    int out = 12345;
    fnPtr(&out);
    ReleaseAssert(out == 0);
}

TEST(TestFastInterpInternal, Sanity_2)
{
    // Test arith operation on two literal values
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
    const FastInterpBoilerplateBluePrint* blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(
                TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                OperandShapeCategory::LITERAL_NONZERO,
                OperandShapeCategory::LITERAL_NONZERO,
                AstArithmeticExprType::MUL);
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(blueprint);
    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
    inst->PopulateConstantPlaceholder<int>(0, 123);
    inst->PopulateConstantPlaceholder<int>(2, 45678);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(int*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    int out = 12345;
    fnPtr(&out);
    ReleaseAssert(out == 123 * 45678);
}

TEST(TestFastInterpInternal, Sanity_3)
{
    // Test arith operation on two literal values, type is double to test that bitcasting works as expected.
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
    const FastInterpBoilerplateBluePrint* blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(
                TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                OperandShapeCategory::LITERAL_NONZERO,
                OperandShapeCategory::LITERAL_NONZERO,
                AstArithmeticExprType::MUL);
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(blueprint);
    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
    inst->PopulateConstantPlaceholder<double>(0, 123.456);
    inst->PopulateConstantPlaceholder<double>(2, 789.012);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(double*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    double out = 12345;
    fnPtr(&out);
    ReleaseAssert(fabs(out - 123.456 * 789.012) < 1e-11);
}

TEST(TestFastInterpInternal, Sanity_4)
{
    // Test a expression tree '(a + b) * (c - d)'
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::LITERAL_NONZERO,
                    OperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::ADD));
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::LITERAL_NONZERO,
                    OperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::COMPLEX,
                    OperandShapeCategory::COMPLEX,
                    AstArithmeticExprType::MUL));
    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst3);
    inst1->PopulateConstantPlaceholder<int>(0, 321);
    inst1->PopulateConstantPlaceholder<int>(2, 567);
    inst2->PopulateConstantPlaceholder<int>(0, -123);
    inst2->PopulateConstantPlaceholder<int>(2, -89);
    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst1);
    inst3->PopulateBoilerplateFnPtrPlaceholder(2, inst2);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    using FnType = void(*)(int*);
    FnType fnPtr = reinterpret_cast<FnType>(gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233)));
    ReleaseAssert(fnPtr != nullptr);
    int out = 12345;
    fnPtr(&out);
    ReleaseAssert(out == (321 + 567) * (-123 - (-89)));
}

TEST(TestFastInterpInternal, Sanity_5)
{
    // Test a expression tree '(a + b) / (c - d)', double type
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::LITERAL_NONZERO,
                    OperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::ADD));
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::LITERAL_NONZERO,
                    OperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::COMPLEX,
                    OperandShapeCategory::COMPLEX,
                    AstArithmeticExprType::DIV));
    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst3);
    inst1->PopulateConstantPlaceholder<double>(0, 321);
    inst1->PopulateConstantPlaceholder<double>(2, 567);
    inst2->PopulateConstantPlaceholder<double>(0, -123);
    inst2->PopulateConstantPlaceholder<double>(2, -89);
    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst1);
    inst3->PopulateBoilerplateFnPtrPlaceholder(2, inst2);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(double*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    double out = 12345;
    fnPtr(&out);
    ReleaseAssert(fabs(out - (double(321) + double(567)) / (double(-123) - double(-89))) < 1e-11);
}

TEST(TestFastInterpInternal, SanityThreadLocal_1)
{
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIVariableImpl>;
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<int*>().GetDefaultFastInterpTypeId()));
    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst1);
    inst1->PopulateConstantPlaceholder<uint32_t>(0, 24);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(int**);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    auto check = [fnPtr](uint8_t* sf, bool setStackFrame)
    {
        if (setStackFrame)
        {
            __pochivm_thread_fastinterp_context.m_stackFrame = reinterpret_cast<uintptr_t>(sf);
        }
        int* out = nullptr;
        fnPtr(&out);
        ReleaseAssert(reinterpret_cast<uintptr_t>(out) == reinterpret_cast<uintptr_t>(sf) + 24);
    };
    uint8_t* sf1 = new uint8_t[1024];
    uint8_t* sf2 = new uint8_t[1024];
    check(sf1, true);
    // Create a new thread, make sure it is actually thread local
    //
    std::thread thr(check, sf2, true);
    thr.join();
    // Check again in main thread, make sure it is actually thread local
    //
    check(sf1, false);
}

namespace
{

uint16_t GetRand16()
{
    return rand() & ((1<<16)-1);
}

uint32_t GetRand32()
{
    return (static_cast<uint32_t>(GetRand16()) << 16) + GetRand16();
}

uint64_t GetRand64()
{
    return (static_cast<uint64_t>(GetRand32()) << 32) + GetRand32();
}

template<typename T>
std::function<void(void*)> GetZeroValueHelperInternal()
{
    return [](void* out)
    {
        *reinterpret_cast<T*>(out) = 0;
    };
}

GEN_FUNCTION_SELECTOR(GetZeroValueHelperSelector, GetZeroValueHelperInternal, AstTypeHelper::is_primitive_type);

template<typename T>
std::function<void(void*)> GetRandValueHelperInternal(bool forMult)
{
    T result;
    while (true)
    {
        if constexpr(std::is_same<T, bool>::value)
        {
            result = static_cast<bool>(rand() % 2);
        }
        else if constexpr(std::is_integral<T>::value)
        {
            // signed overflow is undefined. Do not do it.
            //
            if (std::is_signed<T>::value)
            {
                if (!forMult)
                {
                    result = static_cast<T>(GetRand64()) / 2;
                }
                else
                {
                    uint64_t ub = static_cast<uint64_t>(sqrt(static_cast<double>((1ULL << (sizeof(T) * 8 - 1)) - 1)));
                    result = static_cast<T>(static_cast<int64_t>(GetRand64() % (ub * 2 + 1) - ub));
                }
            }
            else
            {
                result = static_cast<T>(GetRand64());
            }
        }
        else
        {
            static_assert(std::is_floating_point<T>::value, "unexpected T");
            result = static_cast<T>(rand()) / static_cast<T>(100000);
        }
        if (!is_all_underlying_bits_zero<T>(result))
        {
            break;
        }
    }
    return [result](void* out)
    {
        *reinterpret_cast<T*>(out) = result;
    };
}

GEN_FUNCTION_SELECTOR(GetRandValueHelperSelector, GetRandValueHelperInternal, AstTypeHelper::is_primitive_type);

using RandValueGeneratorFn = std::function<void(void*)>(*)(bool);
RandValueGeneratorFn GetRandValueGenerator(TypeId typeId)
{
    return reinterpret_cast<RandValueGeneratorFn>(GetRandValueHelperSelector(typeId));
}

using ZeroValueGeneratorFn = std::function<void(void*)>(*)();
ZeroValueGeneratorFn GetZeroValueGenerator(TypeId typeId)
{
    return reinterpret_cast<ZeroValueGeneratorFn>(GetZeroValueHelperSelector(typeId));
}

template<typename T>
void PopulateConstantPlaceholderInternal(FastInterpBoilerplateInstance* inst, uint32_t ord, std::function<void(void*)> dataFn)
{
    T out;
    dataFn(&out);
    inst->PopulateConstantPlaceholder<T>(ord, out);
}

GEN_FUNCTION_SELECTOR(PopulateConstantPlaceholderSelector, PopulateConstantPlaceholderInternal, AstTypeHelper::is_primitive_type);

using PopulateConstantPlaceholderFn = void(*)(FastInterpBoilerplateInstance* inst, uint32_t ord, std::function<void(void*)> dataFn);
PopulateConstantPlaceholderFn GetPopulateConstantPlaceholderFn(TypeId typeId)
{
    return reinterpret_cast<PopulateConstantPlaceholderFn>(PopulateConstantPlaceholderSelector(typeId));
}

template<typename T>
void CheckMinusResultInternal(std::function<void(void*)> lhsFn, std::function<void(void*)> rhsFn, std::function<void(void*)> resultFn)
{
    T lhs;
    lhsFn(&lhs);
    T rhs;
    rhsFn(&rhs);
    T result;
    resultFn(&result);
    if constexpr(std::is_floating_point<T>::value)
    {
        double diff = fabs(static_cast<double>(lhs - rhs - result));
        ReleaseAssert(diff < 1e-8);
    }
    else
    {
        static_assert(std::is_integral<T>::value, "unexpected T");
        // PochiVM has no integer promotion behavior.
        //
        ReleaseAssert(static_cast<T>(lhs - rhs) == result);
    }
}

GEN_FUNCTION_SELECTOR(CheckMinusResultSelector, CheckMinusResultInternal, AstTypeHelper::is_primitive_type);

using CheckMinusResultFn = void(*)(std::function<void(void*)>, std::function<void(void*)>, std::function<void(void*)>);
CheckMinusResultFn GetCheckMinusResultFn(TypeId typeId)
{
    return reinterpret_cast<CheckMinusResultFn>(CheckMinusResultSelector(typeId));
}

template<typename T>
void CheckMulResultInternal(std::function<void(void*)> lhsFn, std::function<void(void*)> rhsFn, std::function<void(void*)> resultFn)
{
    T lhs;
    lhsFn(&lhs);
    T rhs;
    rhsFn(&rhs);
    T result;
    resultFn(&result);
    if constexpr(std::is_floating_point<T>::value)
    {
        double diff = fabs(static_cast<double>(lhs * rhs - result));
        ReleaseAssert(diff < 1e-8 || diff / fabs(static_cast<double>(result)) < 1e-8);
    }
    else
    {
        static_assert(std::is_integral<T>::value, "unexpected T");
        // PochiVM has no integer promotion behavior.
        //
        ReleaseAssert(static_cast<T>(lhs * rhs) == result);
    }
}

GEN_FUNCTION_SELECTOR(CheckMulResultSelector, CheckMulResultInternal, AstTypeHelper::is_primitive_type);

using CheckMulResultFn = void(*)(std::function<void(void*)>, std::function<void(void*)>, std::function<void(void*)>);
CheckMulResultFn GetCheckMulResultFn(TypeId typeId)
{
    return reinterpret_cast<CheckMulResultFn>(CheckMulResultSelector(typeId));
}

template<typename T>
bool IsAllUnderlyingBitsZeroInternal(std::function<void(void*)> dataFn)
{
    T pad;
    dataFn(&pad);
    return PochiVM::is_all_underlying_bits_zero<T>(pad);
}

GEN_FUNCTION_SELECTOR(CheckIsAllUnderlyingBitsZeroSelector, IsAllUnderlyingBitsZeroInternal, AstTypeHelper::primitive_or_pointer_type);

using CheckAllUnderlyingBitsZeroFn = bool(*)(std::function<void(void*)> dataFn);
CheckAllUnderlyingBitsZeroFn GetAllUnderlyingBitsZeroChecker(TypeId typeId)
{
    return reinterpret_cast<CheckAllUnderlyingBitsZeroFn>(CheckIsAllUnderlyingBitsZeroSelector(typeId));
}

void FillPlaceholderForArithOrCompareExpr(
        bool isLhs, TypeId dataType, TypeId indexType, OperandShapeCategory osc,
        FastInterpCodegenEngine* engine, FastInterpBoilerplateInstance* inst, std::function<void(void*)> dataFn)
{
    uint32_t startOrd = (isLhs ? 0 : 2);
    uint32_t varOffset = (isLhs ? 8 : 24);
    if (isLhs && rand() % 2 == 0) { varOffset -= 8; }
    if (osc == OperandShapeCategory::ZERO)
    {
        return;
    }
    else if (osc == OperandShapeCategory::LITERAL_NONZERO)
    {
        PopulateConstantPlaceholderFn pcpFn = GetPopulateConstantPlaceholderFn(dataType);
        pcpFn(inst, startOrd, dataFn);
    }
    else if (osc == OperandShapeCategory::COMPLEX)
    {
        CheckAllUnderlyingBitsZeroFn checker = GetAllUnderlyingBitsZeroChecker(dataType);
        bool isZero = checker(dataFn);
        FastInterpBoilerplateInstance* lit = engine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                        dataType.GetDefaultFastInterpTypeId(),
                        isZero));
        if (!isZero)
        {
            PopulateConstantPlaceholderFn pcpFn = GetPopulateConstantPlaceholderFn(dataType);
            pcpFn(lit, 0, dataFn);
        }
        inst->PopulateBoilerplateFnPtrPlaceholder(startOrd, lit);
    }
    else if (osc == OperandShapeCategory::VARIABLE)
    {
        inst->PopulateConstantPlaceholder<uint32_t>(startOrd, varOffset);
        dataFn(reinterpret_cast<void*>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset));
    }
    else if (osc == OperandShapeCategory::VARPTR_VAR)
    {
        inst->PopulateConstantPlaceholder<uint32_t>(startOrd, varOffset);
        inst->PopulateConstantPlaceholder<uint32_t>(startOrd + 1, varOffset + 8);
        uint8_t* v = new uint8_t[24];
        for (size_t i = 0; i < 24; i++) v[i] = static_cast<uint8_t>(rand() % 256);
        int offset;
        if (indexType.IsSigned())
        {
            offset = rand() % 3 - 1;
        }
        else
        {
            offset = rand() % 2;
        }
        uint8_t* b = v + 8;
        dataFn(b + static_cast<ssize_t>(dataType.Size()) * offset);
        *reinterpret_cast<uint8_t**>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset) = b;
        if (indexType == TypeId::Get<int32_t>())
        {
            *reinterpret_cast<int32_t*>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset + 8) = offset;
        }
        else if (indexType == TypeId::Get<uint32_t>())
        {
            *reinterpret_cast<uint32_t*>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset + 8) = static_cast<uint32_t>(offset);
        }
        else if (indexType == TypeId::Get<int64_t>())
        {
            *reinterpret_cast<int64_t*>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset + 8) = offset;
        }
        else if (indexType == TypeId::Get<uint64_t>())
        {
            *reinterpret_cast<uint64_t*>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset + 8) = static_cast<uint64_t>(offset);
        }
        else
        {
            ReleaseAssert(false);
        }
    }
    else if (osc == OperandShapeCategory::VARPTR_LIT_NONZERO)
    {
        inst->PopulateConstantPlaceholder<uint32_t>(startOrd, varOffset);
        uint8_t* v = new uint8_t[24];
        for (size_t i = 0; i < 24; i++) v[i] = static_cast<uint8_t>(rand() % 256);
        int offset;
        if (indexType.IsSigned())
        {
            offset = ((rand() % 2 == 0) ? 1 : - 1);
        }
        else
        {
            offset = 1;
        }
        uint8_t* b = v + 8;
        dataFn(b + static_cast<ssize_t>(dataType.Size()) * offset);
        *reinterpret_cast<uint8_t**>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset) = b;
        PopulateConstantPlaceholderFn pcpFn = GetPopulateConstantPlaceholderFn(indexType);
        pcpFn(inst, startOrd + 1, [indexType, offset](void* out)
        {
            if (indexType == TypeId::Get<int32_t>())
            {
                *reinterpret_cast<int32_t*>(out) = offset;
            }
            else if (indexType == TypeId::Get<uint32_t>())
            {
                *reinterpret_cast<uint32_t*>(out) = static_cast<uint32_t>(offset);
            }
            else if (indexType == TypeId::Get<int64_t>())
            {
                *reinterpret_cast<int64_t*>(out) = offset;
            }
            else if (indexType == TypeId::Get<uint64_t>())
            {
                *reinterpret_cast<uint64_t*>(out) = static_cast<uint64_t>(offset);
            }
            else
            {
                ReleaseAssert(false);
            }
        });
    }
    else if (osc == OperandShapeCategory::VARPTR_DEREF)
    {
        inst->PopulateConstantPlaceholder<uint32_t>(startOrd, varOffset);
        uint8_t* v = new uint8_t[8];
        for (size_t i = 0; i < 8; i++) v[i] = static_cast<uint8_t>(rand() % 256);
        dataFn(v);
        *reinterpret_cast<uint8_t**>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset) = v;
    }
    else
    {
        ReleaseAssert(false);
    }
};

}

TEST(TestFastInterpInternal, SanityArithmeticExpr)
{
    std::vector<TypeId> types {
#define F(t) TypeId::Get<t>(),
    FOR_EACH_PRIMITIVE_TYPE
    TypeId::Get<void>()
#undef F
    };
    types.pop_back();

    std::vector<TypeId> indexTypes {
        TypeId::Get<int32_t>(),
        TypeId::Get<uint32_t>(),
        TypeId::Get<int64_t>(),
        TypeId::Get<uint64_t>()
    };

    int numChecked = 0;
    for (TypeId dataType : types)
    {
        if (dataType.IsBool())
        {
            continue;
        }
        for (int lhsOscInt = 0; lhsOscInt < static_cast<int>(OperandShapeCategory::X_END_OF_ENUM); lhsOscInt++)
        {
            for (TypeId lhsIndexType : indexTypes)
            {
                OperandShapeCategory lhsOsc = static_cast<OperandShapeCategory>(lhsOscInt);
                if (lhsOsc != OperandShapeCategory::VARPTR_VAR && lhsOsc != OperandShapeCategory::VARPTR_LIT_NONZERO)
                {
                    if (lhsIndexType != TypeId::Get<int32_t>())
                    {
                        continue;
                    }
                }
                for (int rhsOscInt = 0; rhsOscInt < static_cast<int>(OperandShapeCategory::X_END_OF_ENUM); rhsOscInt++)
                {
                    for (TypeId rhsIndexType : indexTypes)
                    {
                        for (AstArithmeticExprType arithType : { AstArithmeticExprType::SUB, AstArithmeticExprType::MUL })
                        {
                            OperandShapeCategory rhsOsc = static_cast<OperandShapeCategory>(rhsOscInt);
                            if (rhsOsc != OperandShapeCategory::VARPTR_VAR && rhsOsc != OperandShapeCategory::VARPTR_LIT_NONZERO)
                            {
                                if (rhsIndexType != TypeId::Get<int32_t>())
                                {
                                    continue;
                                }
                            }
                            std::function<void(void*)> lhsFn;
                            if (lhsOsc == OperandShapeCategory::ZERO)
                            {
                                lhsFn = GetZeroValueGenerator(dataType)();
                            }
                            else
                            {
                                lhsFn = GetRandValueGenerator(dataType)(arithType == AstArithmeticExprType::MUL /*forMult*/);
                            }
                            std::function<void(void*)> rhsFn;
                            if (rhsOsc == OperandShapeCategory::ZERO)
                            {
                                rhsFn = GetZeroValueGenerator(dataType)();
                            }
                            else
                            {
                                rhsFn = GetRandValueGenerator(dataType)(arithType == AstArithmeticExprType::MUL /*forMult*/);
                            }
                            uint8_t* pad = new uint8_t[40];
                            for (size_t i = 0; i < 40; i++) { pad[i] = static_cast<uint8_t>(rand() % 256); }
                            __pochivm_thread_fastinterp_context.m_stackFrame = reinterpret_cast<uintptr_t>(pad);
                            FastInterpCodegenEngine engine;
                            using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
                            FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                                        BoilerplateLibrary::SelectBoilerplateBluePrint(
                                            dataType.GetDefaultFastInterpTypeId(),
                                            lhsIndexType.GetDefaultFastInterpTypeId(),
                                            rhsIndexType.GetDefaultFastInterpTypeId(),
                                            lhsOsc,
                                            rhsOsc,
                                            arithType));
                            FillPlaceholderForArithOrCompareExpr(true /*isLhs*/, dataType, lhsIndexType, lhsOsc, &engine, inst, lhsFn);
                            FillPlaceholderForArithOrCompareExpr(false /*isLhs*/, dataType, rhsIndexType, rhsOsc, &engine, inst, rhsFn);
                            engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
                            std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
                            void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
                            ReleaseAssert(fnPtrVoid != nullptr);
                            using FnType = void(*)(void*);
                            FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
                            std::function<void(void*)> resultFn = fnPtr;
                            if (arithType == AstArithmeticExprType::SUB)
                            {
                                CheckMinusResultFn checkFn = GetCheckMinusResultFn(dataType);
                                checkFn(lhsFn, rhsFn, resultFn);
                            }
                            else
                            {
                                CheckMulResultFn checkFn = GetCheckMulResultFn(dataType);
                                checkFn(lhsFn, rhsFn, resultFn);
                            }
                            numChecked++;
                        }
                    }
                }
            }
        }
    }
    ReleaseAssert(numChecked == 10 * 169 * 2);
}

namespace {

template<typename T>
void CheckLiteralResultInternal(std::function<void(void*)> dataFn, std::function<void(void*)> resultFn)
{
    T data;
    dataFn(&data);
    T result;
    resultFn(&result);
    if constexpr(std::is_floating_point<T>::value)
    {
        double diff = fabs(static_cast<double>(data - result));
        ReleaseAssert(diff < 1e-8);
    }
    else
    {
        static_assert(std::is_integral<T>::value || std::is_pointer<T>::value, "unexpected T");
        ReleaseAssert(data == result);
    }
}

GEN_FUNCTION_SELECTOR(CheckLiteralResultSelector, CheckLiteralResultInternal, AstTypeHelper::primitive_or_pointer_type);

using CheckLiteralResultFn = void(*)(std::function<void(void*)>, std::function<void(void*)>);
CheckLiteralResultFn GetCheckLiteralResultFn(TypeId typeId)
{
    return reinterpret_cast<CheckLiteralResultFn>(CheckLiteralResultSelector(typeId));
}

}

TEST(TestFastInterpInternal, SanityLiteralExpr)
{
    std::vector<TypeId> types {
#define F(t) TypeId::Get<t>(),
    FOR_EACH_PRIMITIVE_TYPE
    TypeId::Get<void>()
#undef F
    };
    types.pop_back();

    for (TypeId dataType : types)
    {
        for (bool isZero : { false, true })
        {
            std::function<void(void*)> dataFn;
            if (isZero)
            {
                dataFn = GetZeroValueGenerator(dataType)();
            }
            else
            {
                dataFn = GetRandValueGenerator(dataType)(false /*forMult*/);
            }
            FastInterpCodegenEngine engine;
            using BoilerplateLibrary = FastInterpBoilerplateLibrary<FILiteralImpl>;
            FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                        BoilerplateLibrary::SelectBoilerplateBluePrint(
                            dataType.GetDefaultFastInterpTypeId(), isZero));
            if (!isZero)
            {
                PopulateConstantPlaceholderFn pcpFn = GetPopulateConstantPlaceholderFn(dataType);
                pcpFn(inst, 0, dataFn);
            }
            engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
            std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
            void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
            ReleaseAssert(fnPtrVoid != nullptr);
            using FnType = void(*)(void*);
            FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
            std::function<void(void*)> resultFn = fnPtr;
            CheckLiteralResultFn checkFn = GetCheckLiteralResultFn(dataType);
            checkFn(dataFn, resultFn);
        }
    }

    {
        FastInterpCodegenEngine engine;
        using BoilerplateLibrary = FastInterpBoilerplateLibrary<FILiteralImpl>;
        FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                    BoilerplateLibrary::SelectBoilerplateBluePrint(
                        TypeId::Get<void*>().GetDefaultFastInterpTypeId(), false /*isZero*/));
        inst->PopulateConstantPlaceholder<void*>(0, reinterpret_cast<void*>(2333));
        engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
        std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
        void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
        ReleaseAssert(fnPtrVoid != nullptr);
        using FnType = void(*)(void**);
        FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
        void* result = reinterpret_cast<void*>(23456);
        fnPtr(&result);
        ReleaseAssert(result == reinterpret_cast<void*>(2333));
    }

    {
        FastInterpCodegenEngine engine;
        using BoilerplateLibrary = FastInterpBoilerplateLibrary<FILiteralImpl>;
        FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                    BoilerplateLibrary::SelectBoilerplateBluePrint(
                        TypeId::Get<void*>().GetDefaultFastInterpTypeId(), true /*isZero*/));
        engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
        std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
        void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
        ReleaseAssert(fnPtrVoid != nullptr);
        using FnType = void(*)(void**);
        FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
        void* result = reinterpret_cast<void*>(23456);
        fnPtr(&result);
        ReleaseAssert(result == nullptr);
    }
}

namespace  {

template<typename T>
void CheckLessThanResultInternal(std::function<void(void*)> lhsFn, std::function<void(void*)> rhsFn, std::function<void(void*)> resultFn)
{
    T lhs;
    lhsFn(&lhs);
    T rhs;
    rhsFn(&rhs);
    bool result;
    resultFn(&result);
    ReleaseAssert(result == (lhs < rhs));
}

GEN_FUNCTION_SELECTOR(CheckLessThanResultSelector, CheckLessThanResultInternal, AstTypeHelper::is_primitive_type);

using CheckLessThanResultFn = void(*)(std::function<void(void*)>, std::function<void(void*)>, std::function<void(void*)>);
CheckLessThanResultFn GetCheckLessThanResultFn(TypeId typeId)
{
    return reinterpret_cast<CheckLessThanResultFn>(CheckLessThanResultSelector(typeId));
}

template<typename T>
void CheckLessEqualResultInternal(std::function<void(void*)> lhsFn, std::function<void(void*)> rhsFn, std::function<void(void*)> resultFn)
{
    T lhs;
    lhsFn(&lhs);
    T rhs;
    rhsFn(&rhs);
    bool result;
    resultFn(&result);
    ReleaseAssert(result == (lhs <= rhs));
}

GEN_FUNCTION_SELECTOR(CheckLessEqualResultSelector, CheckLessEqualResultInternal, AstTypeHelper::is_primitive_type);

using CheckLessEqualResultFn = void(*)(std::function<void(void*)>, std::function<void(void*)>, std::function<void(void*)>);
CheckLessEqualResultFn GetCheckLessEqualResultFn(TypeId typeId)
{
    return reinterpret_cast<CheckLessEqualResultFn>(CheckLessEqualResultSelector(typeId));
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
template<typename T>
void CheckEqualResultInternal(std::function<void(void*)> lhsFn, std::function<void(void*)> rhsFn, std::function<void(void*)> resultFn, bool expectSame)
{
    T lhs;
    lhsFn(&lhs);
    T rhs;
    rhsFn(&rhs);
    bool result;
    resultFn(&result);
    ReleaseAssert(result == (lhs == rhs));
    if (expectSame) { ReleaseAssert(result); }
}
#pragma clang diagnostic pop

GEN_FUNCTION_SELECTOR(CheckEqualResultSelector, CheckEqualResultInternal, AstTypeHelper::is_primitive_type);

using CheckEqualResultFn = void(*)(std::function<void(void*)>, std::function<void(void*)>, std::function<void(void*)>, bool);
CheckEqualResultFn GetCheckEqualResultFn(TypeId typeId)
{
    return reinterpret_cast<CheckEqualResultFn>(CheckEqualResultSelector(typeId));
}

}

TEST(TestFastInterpInternal, SanityComparisonExpr)
{
    std::vector<TypeId> types {
#define F(t) TypeId::Get<t>(),
    FOR_EACH_PRIMITIVE_TYPE
    TypeId::Get<void>()
#undef F
    };
    types.pop_back();

    std::vector<TypeId> indexTypes {
        TypeId::Get<int32_t>(),
        TypeId::Get<uint32_t>(),
        TypeId::Get<int64_t>(),
        TypeId::Get<uint64_t>()
    };

    for (int numIter = 0; numIter < 5; numIter++)
    {
        int numChecked = 0;
        for (TypeId dataType : types)
        {
            for (int lhsOscInt = 0; lhsOscInt < static_cast<int>(OperandShapeCategory::X_END_OF_ENUM); lhsOscInt++)
            {
                for (TypeId lhsIndexType : indexTypes)
                {
                    OperandShapeCategory lhsOsc = static_cast<OperandShapeCategory>(lhsOscInt);
                    if (lhsOsc != OperandShapeCategory::VARPTR_VAR && lhsOsc != OperandShapeCategory::VARPTR_LIT_NONZERO)
                    {
                        if (lhsIndexType != TypeId::Get<int32_t>())
                        {
                            continue;
                        }
                    }
                    for (int rhsOscInt = 0; rhsOscInt < static_cast<int>(OperandShapeCategory::X_END_OF_ENUM); rhsOscInt++)
                    {
                        for (TypeId rhsIndexType : indexTypes)
                        {
                            OperandShapeCategory rhsOsc = static_cast<OperandShapeCategory>(rhsOscInt);
                            if (rhsOsc != OperandShapeCategory::VARPTR_VAR && rhsOsc != OperandShapeCategory::VARPTR_LIT_NONZERO)
                            {
                                if (rhsIndexType != TypeId::Get<int32_t>())
                                {
                                    continue;
                                }
                            }
                            if (lhsOsc == OperandShapeCategory::LITERAL_NONZERO && rhsOsc == OperandShapeCategory::LITERAL_NONZERO)
                            {
                                continue;
                            }
                            std::vector<AstComparisonExprType> allCompareType {
                                AstComparisonExprType::LESS_THAN,
                                AstComparisonExprType::LESS_EQUAL,
                                AstComparisonExprType::EQUAL,
                                AstComparisonExprType::EQUAL
                            };
                            for (size_t compareOrd = 0; compareOrd < allCompareType.size(); compareOrd++)
                            {
                                AstComparisonExprType compareType = allCompareType[compareOrd];
                                std::function<void(void*)> lhsFn;
                                if (lhsOsc == OperandShapeCategory::ZERO)
                                {
                                    lhsFn = GetZeroValueGenerator(dataType)();
                                }
                                else
                                {
                                    lhsFn = GetRandValueGenerator(dataType)(false /*forMult*/);
                                }
                                std::function<void(void*)> rhsFn;
                                if (rhsOsc == OperandShapeCategory::ZERO)
                                {
                                    rhsFn = GetZeroValueGenerator(dataType)();
                                }
                                else
                                {
                                    rhsFn = GetRandValueGenerator(dataType)(false /*forMult*/);
                                }
                                bool expectSame = false;
                                if (compareOrd == 3 && (lhsOsc == OperandShapeCategory::ZERO) == (rhsOsc == OperandShapeCategory::ZERO))
                                {
                                    rhsFn = lhsFn;
                                    expectSame = true;
                                }
                                uint8_t* pad = new uint8_t[40];
                                for (size_t i = 0; i < 40; i++) { pad[i] = static_cast<uint8_t>(rand() % 256); }
                                __pochivm_thread_fastinterp_context.m_stackFrame = reinterpret_cast<uintptr_t>(pad);
                                FastInterpCodegenEngine engine;
                                using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIComparisonExprImpl>;
                                FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                                            BoilerplateLibrary::SelectBoilerplateBluePrint(
                                                dataType.GetDefaultFastInterpTypeId(),
                                                lhsIndexType.GetDefaultFastInterpTypeId(),
                                                rhsIndexType.GetDefaultFastInterpTypeId(),
                                                lhsOsc,
                                                rhsOsc,
                                                compareType));
                                FillPlaceholderForArithOrCompareExpr(true /*isLhs*/, dataType, lhsIndexType, lhsOsc, &engine, inst, lhsFn);
                                FillPlaceholderForArithOrCompareExpr(false /*isLhs*/, dataType, rhsIndexType, rhsOsc, &engine, inst, rhsFn);
                                engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
                                std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
                                void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
                                ReleaseAssert(fnPtrVoid != nullptr);
                                using FnType = void(*)(void*);
                                FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
                                std::function<void(void*)> resultFn = fnPtr;
                                if (compareType == AstComparisonExprType::LESS_THAN)
                                {
                                    CheckLessThanResultFn checkFn = GetCheckLessThanResultFn(dataType);
                                    checkFn(lhsFn, rhsFn, resultFn);
                                }
                                else if (compareType == AstComparisonExprType::LESS_EQUAL)
                                {
                                    CheckLessEqualResultFn checkFn = GetCheckLessEqualResultFn(dataType);
                                    checkFn(lhsFn, rhsFn, resultFn);
                                }
                                else if (compareType == AstComparisonExprType::EQUAL)
                                {
                                    CheckEqualResultFn checkFn = GetCheckEqualResultFn(dataType);
                                    checkFn(lhsFn, rhsFn, resultFn, expectSame);
                                }
                                else
                                {
                                    ReleaseAssert(false);
                                }
                                numChecked++;
                            }
                        }
                    }
                }
            }
        }
        ReleaseAssert(numChecked == 11 * 168 * 4);
    }
}

TEST(TestFastInterpInternal, SanityCallExpr_1)
{
    __pochivm_thread_fastinterp_context.m_stackFrame = 2333;
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::VARIABLE,
                    OperandShapeCategory::VARIABLE,
                    AstArithmeticExprType::SUB));
    inst1->PopulateConstantPlaceholder<uint32_t>(0, 8);
    inst1->PopulateConstantPlaceholder<uint32_t>(2, 16);

    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FISimpleReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::COMPLEX));
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst1);

    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst3->PopulateConstantPlaceholder<int>(0, 123);

    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst4->PopulateConstantPlaceholder<int>(0, 456);

    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFunctionImpl>::SelectBoilerplateBluePrint(
                    true /*isNoExcept*/,
                    static_cast<PochiVM::FIFunctionNumStatements>(1) /*numStmts*/,
                    static_cast<PochiVM::FIFunctionStmtsMayReturnMask>(1) /*mayReturnMask*/));
    inst5->PopulateBoilerplateFnPtrPlaceholder(0, inst2);

    FastInterpBoilerplateInstance* inst6 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallGeneratedFnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(2) /*numParams*/));
    inst6->PopulateBoilerplateFnPtrPlaceholder(0, inst5);
    inst6->PopulateConstantPlaceholder<uint32_t>(0, 24 /*stackFrameSize*/);
    inst6->PopulateBoilerplateFnPtrPlaceholder(1, inst3);
    inst6->PopulateBoilerplateFnPtrPlaceholder(2, inst4);

    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst6);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(int*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    int output = 0;
    fnPtr(&output);
    ReleaseAssert(output == 123 - 456);
    // Assert that after the called function returns, stack frame is unchanged
    //
    ReleaseAssert(__pochivm_thread_fastinterp_context.m_stackFrame == 2333);
}

TEST(TestFastInterpInternal, SanityCallExpr_2)
{
    // This test additionally tests that parameters are correctly evaluated in old stack frame
    //
    uint32_t* sf = new uint32_t[10];
    sf[7] = 1234;
    sf[8] = 5678;
    __pochivm_thread_fastinterp_context.m_stackFrame = reinterpret_cast<uintptr_t>(sf);
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::VARIABLE,
                    OperandShapeCategory::VARIABLE,
                    AstArithmeticExprType::SUB));
    inst1->PopulateConstantPlaceholder<uint32_t>(0, 8);
    inst1->PopulateConstantPlaceholder<uint32_t>(2, 16);

    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FISimpleReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::COMPLEX));
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst1);

    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    OperandShapeCategory::VARIABLE,
                    OperandShapeCategory::VARIABLE,
                    AstArithmeticExprType::SUB));
    inst3->PopulateConstantPlaceholder<uint32_t>(0, 7 * 4);
    inst3->PopulateConstantPlaceholder<uint32_t>(2, 8 * 4);

    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst4->PopulateConstantPlaceholder<int>(0, 789);

    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFunctionImpl>::SelectBoilerplateBluePrint(
                    true /*isNoExcept*/,
                    static_cast<PochiVM::FIFunctionNumStatements>(1) /*numStmts*/,
                    static_cast<PochiVM::FIFunctionStmtsMayReturnMask>(1) /*mayReturnMask*/));
    inst5->PopulateBoilerplateFnPtrPlaceholder(0, inst2);

    FastInterpBoilerplateInstance* inst6 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallGeneratedFnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(2) /*numParams*/));
    inst6->PopulateBoilerplateFnPtrPlaceholder(0, inst5);
    inst6->PopulateConstantPlaceholder<uint32_t>(0, 24 /*stackFrameSize*/);
    inst6->PopulateBoilerplateFnPtrPlaceholder(1, inst3);
    inst6->PopulateBoilerplateFnPtrPlaceholder(2, inst4);

    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst6);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(int*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    int output = 0;
    fnPtr(&output);
    ReleaseAssert(output == 1234 - 5678 - 789);
    // Assert that after the called function returns, stack frame is unchanged
    //
    ReleaseAssert(__pochivm_thread_fastinterp_context.m_stackFrame == reinterpret_cast<uintptr_t>(sf));
}

TEST(TestFastInterpInternal, SanityCallExpr_3)
{
    // This test tests calling CPP function noexcept case
    //
    auto cppFnLambda = [](void* ret, void** params) noexcept -> void
    {
        *reinterpret_cast<int*>(ret) = *reinterpret_cast<int*>(params[0]) - *reinterpret_cast<int*>(params[1]);
    };
    using CppFnProto = void(*)(void*, void**) noexcept;
    CppFnProto cppFn = cppFnLambda;
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    false /*isRetValVoid*/,
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(2) /*numParams*/));

    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst2->PopulateConstantPlaceholder<int>(0, 12345);

    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst3->PopulateConstantPlaceholder<int>(0, 67890);

    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst->PopulateBoilerplateFnPtrPlaceholder(1, inst3);
    inst->PopulateCppFnPtrPlaceholder(0, cppFn);

    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(int*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    int output = 0;
    fnPtr(&output);
    ReleaseAssert(output == 12345 - 67890);
}

TEST(TestFastInterpInternal, SanityCallExpr_4)
{
    // This test tests calling CPP function may throw case (but does not throw)
    //
    auto cppFnLambda = [](void* ret, void** params) noexcept -> bool
    {
        ReleaseAssert(ret == nullptr);
        **reinterpret_cast<int**>(params[0]) = 123;
        return false;
    };
    auto exnHandlerLambda = [](void* /*exnContext*/, uintptr_t /*sfBase*/) noexcept -> void
    {
        ReleaseAssert(false);
    };
    using CppFnProto = bool(*)(void*, void**) noexcept;
    CppFnProto cppFn = cppFnLambda;
    using ExnFnProto = void(*)(void*, uintptr_t) noexcept;
    ExnFnProto exnHandler = exnHandlerLambda;

    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    true /*isRetValVoid*/,
                    false /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1) /*numParams*/));

    int value = 0;
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void*>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst2->PopulateConstantPlaceholder<void*>(0, &value);

    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst->PopulateCppFnPtrPlaceholder(0, cppFn);
    inst->PopulateConstantPlaceholder<void*>(0, reinterpret_cast<void*>(12345));
    inst->PopulateCppFnPtrPlaceholder(1, exnHandler);

    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(void*);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    fnPtr(nullptr);
    ReleaseAssert(value == 123);
}

TEST(TestFastInterpInternal, SanityCallExpr_5)
{
    // This test tests calling CPP function may throw case (which actually throws)
    //
    __pochivm_thread_fastinterp_context.m_stackFrame = 233333;
    __pochivm_thread_fastinterp_context.m_ehTarget = nullptr;
    static bool shallThrow = false;
    auto cppFnLambda = [](void* ret, void** params) noexcept -> bool
    {
        ReleaseAssert(ret == nullptr);
        **reinterpret_cast<int**>(params[0]) = 123;
        return shallThrow;
    };
    auto cppFnLambda2 = [](void* ret, void** params) noexcept -> void
    {
        ReleaseAssert(ret == nullptr);
        ReleaseAssert(!shallThrow);
        **reinterpret_cast<int**>(params[0]) = 456;
    };
    auto exnHandlerLambda = [](void* exnContext, uintptr_t sfBase) noexcept -> void
    {
        if (!shallThrow)
        {
            ReleaseAssert(false);
        }
        else
        {
            ReleaseAssert(exnContext == reinterpret_cast<void*>(12345));
            ReleaseAssert(sfBase == 233333);
            __builtin_longjmp(__pochivm_thread_fastinterp_context.m_ehTarget, 1);
        }
    };
    using CppFnProtoMayThrow = bool(*)(void*, void**) noexcept;
    CppFnProtoMayThrow cppFn = cppFnLambda;
    using ExnFnProto = void(*)(void*, uintptr_t) noexcept;
    ExnFnProto exnHandler = exnHandlerLambda;
    using CppFnProtoNoThrow = void(*)(void*, void**) noexcept;
    CppFnProtoNoThrow cppFn2 = cppFnLambda2;

    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    true /*isRetValVoid*/,
                    false /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1) /*numParams*/));

    int value = 0;
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void*>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst2->PopulateConstantPlaceholder<void*>(0, &value);

    inst1->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst1->PopulateConstantPlaceholder<void*>(0, reinterpret_cast<void*>(12345));
    inst1->PopulateCppFnPtrPlaceholder(0, cppFn);
    inst1->PopulateCppFnPtrPlaceholder(1, exnHandler);

    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    true /*isRetValVoid*/,
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1) /*numParams*/));
    inst3->PopulateCppFnPtrPlaceholder(0, cppFn2);

    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void*>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst4->PopulateConstantPlaceholder<void*>(0, &value);

    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst4);

    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFunctionImpl>::SelectBoilerplateBluePrint(
                    false /*isNoExcept*/,
                    static_cast<PochiVM::FIFunctionNumStatements>(2) /*numStmts*/,
                    static_cast<PochiVM::FIFunctionStmtsMayReturnMask>(0) /*mayReturnMask*/));
    inst5->PopulateBoilerplateFnPtrPlaceholder(0, inst1);
    inst5->PopulateBoilerplateFnPtrPlaceholder(1, inst3);

    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), inst5);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = bool(*)();
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    {
        shallThrow = false;
        value = 0;
        ReleaseAssert(fnPtr() == false);
        ReleaseAssert(value == 456);
    }

    {
        shallThrow = true;
        value = 0;
        ReleaseAssert(fnPtr() == true);
        ReleaseAssert(value == 123);
    }

    {
        shallThrow = false;
        value = 0;
        ReleaseAssert(fnPtr() == false);
        ReleaseAssert(value == 456);
    }
}
