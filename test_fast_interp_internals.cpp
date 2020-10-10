#include "gtest/gtest.h"

#include "fastinterp/fastinterp.h"

using namespace PochiVM;

#if 0
TEST(TestFastInterpInternal, SanitySymbolNames)
{
    // Sanity check 'SelectBoilerplateBluePrint' indeed selected the boilerplate we expected by checking its symbol name
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIArithmeticExprImpl>;
    const FastInterpBoilerplateBluePrint* blueprint;
    blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               FIOperandShapeCategory::LITERAL_NONZERO,
                                                               FIOperandShapeCategory::LITERAL_NONZERO,
                                                               AstArithmeticExprType::ADD);
    TestAssert(blueprint->TestOnly_GetSymbolName() == std::string("_ZN7PochiVM20FIArithmeticExprImpl1fIiiiLNS_22FIOperandShapeCategoryE4ELS2_4ELNS_21AstArithmeticExprTypeE0EEET_v"));
    blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               FIOperandShapeCategory::COMPLEX,
                                                               FIOperandShapeCategory::ZERO,
                                                               AstArithmeticExprType::MUL);
    TestAssert(blueprint->TestOnly_GetSymbolName() == std::string("_ZN7PochiVM20FIArithmeticExprImpl1fIdiiLNS_22FIOperandShapeCategoryE6ELS2_5ELNS_21AstArithmeticExprTypeE2EEET_v"));
    std::ignore = blueprint;
}

TEST(TestFastInterpInternal, SanityThreadLocal_1)
{
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FIVariableImpl>;
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                BoilerplateLibrary::SelectBoilerplateBluePrint(
                    TypeId::Get<int*>().GetDefaultFastInterpTypeId()));
    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<int*>(), 233, inst1);
    inst1->PopulateConstantPlaceholder<uint32_t>(0, 24);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = int*(*)();
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    auto check = [fnPtr](uint8_t* sf, bool setStackFrame)
    {
        if (setStackFrame)
        {
            __pochivm_thread_fastinterp_context.m_stackFrame = reinterpret_cast<uintptr_t>(sf);
        }
        ReleaseAssert(reinterpret_cast<uintptr_t>(fnPtr()) == reinterpret_cast<uintptr_t>(sf) + 24);
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
std::function<void(void*)> GetGetReusltFnInternal(void* ptr)
{
    return [ptr](void* out)
    {
        using FnProto = T(*)();
        FnProto fnPtr = reinterpret_cast<FnProto>(ptr);
        T result = fnPtr();
        *reinterpret_cast<T*>(out) = result;
    };
}

GEN_FUNCTION_SELECTOR(GetGetReusltFnSelector, GetGetReusltFnInternal, AstTypeHelper::is_primitive_type);

using GetReusltFnGetter = std::function<void(void*)>(*)(void*);
GetReusltFnGetter GetGetReusltFn(TypeId typeId)
{
    return reinterpret_cast<GetReusltFnGetter>(GetGetReusltFnSelector(typeId));
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
        bool isLhs, TypeId dataType, TypeId indexType, FIOperandShapeCategory osc,
        FastInterpCodegenEngine* engine, FastInterpBoilerplateInstance* inst, std::function<void(void*)> dataFn)
{
    uint32_t startOrd = (isLhs ? 0 : 2);
    uint32_t varOffset = (isLhs ? 8 : 24);
    if (isLhs && rand() % 2 == 0) { varOffset -= 8; }
    if (osc == FIOperandShapeCategory::ZERO)
    {
        return;
    }
    else if (osc == FIOperandShapeCategory::LITERAL_NONZERO)
    {
        PopulateConstantPlaceholderFn pcpFn = GetPopulateConstantPlaceholderFn(dataType);
        pcpFn(inst, startOrd, dataFn);
    }
    else if (osc == FIOperandShapeCategory::COMPLEX)
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
    else if (osc == FIOperandShapeCategory::VARIABLE)
    {
        inst->PopulateConstantPlaceholder<uint32_t>(startOrd, varOffset);
        dataFn(reinterpret_cast<void*>(__pochivm_thread_fastinterp_context.m_stackFrame + varOffset));
    }
    else if (osc == FIOperandShapeCategory::VARPTR_VAR)
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
    else if (osc == FIOperandShapeCategory::VARPTR_LIT_NONZERO)
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
    else if (osc == FIOperandShapeCategory::VARPTR_DEREF)
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
        for (int lhsOscInt = 0; lhsOscInt < static_cast<int>(FIOperandShapeCategory::X_END_OF_ENUM); lhsOscInt++)
        {
            for (TypeId lhsIndexType : indexTypes)
            {
                FIOperandShapeCategory lhsOsc = static_cast<FIOperandShapeCategory>(lhsOscInt);
                if (lhsOsc != FIOperandShapeCategory::VARPTR_VAR && lhsOsc != FIOperandShapeCategory::VARPTR_LIT_NONZERO)
                {
                    if (lhsIndexType != TypeId::Get<int32_t>())
                    {
                        continue;
                    }
                }
                for (int rhsOscInt = 0; rhsOscInt < static_cast<int>(FIOperandShapeCategory::X_END_OF_ENUM); rhsOscInt++)
                {
                    for (TypeId rhsIndexType : indexTypes)
                    {
                        for (AstArithmeticExprType arithType : { AstArithmeticExprType::SUB, AstArithmeticExprType::MUL })
                        {
                            FIOperandShapeCategory rhsOsc = static_cast<FIOperandShapeCategory>(rhsOscInt);
                            if (rhsOsc != FIOperandShapeCategory::VARPTR_VAR && rhsOsc != FIOperandShapeCategory::VARPTR_LIT_NONZERO)
                            {
                                if (rhsIndexType != TypeId::Get<int32_t>())
                                {
                                    continue;
                                }
                            }
                            std::function<void(void*)> lhsFn;
                            if (lhsOsc == FIOperandShapeCategory::ZERO)
                            {
                                lhsFn = GetZeroValueGenerator(dataType)();
                            }
                            else
                            {
                                lhsFn = GetRandValueGenerator(dataType)(arithType == AstArithmeticExprType::MUL /*forMult*/);
                            }
                            std::function<void(void*)> rhsFn;
                            if (rhsOsc == FIOperandShapeCategory::ZERO)
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
                            engine.TestOnly_RegisterUnitTestFunctionEntryPoint(dataType, 233, inst);
                            std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
                            void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
                            ReleaseAssert(fnPtrVoid != nullptr);
                            std::function<void(void*)> resultFn = GetGetReusltFn(dataType)(fnPtrVoid);
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
            engine.TestOnly_RegisterUnitTestFunctionEntryPoint(dataType, 233, inst);
            std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
            void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
            ReleaseAssert(fnPtrVoid != nullptr);
            std::function<void(void*)> resultFn = GetGetReusltFn(dataType)(fnPtrVoid);
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
        engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void*>(), 233, inst);
        std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
        void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
        ReleaseAssert(fnPtrVoid != nullptr);
        using FnType = void*(*)();
        FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
        ReleaseAssert(fnPtr() == reinterpret_cast<void*>(2333));
    }

    {
        FastInterpCodegenEngine engine;
        using BoilerplateLibrary = FastInterpBoilerplateLibrary<FILiteralImpl>;
        FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                    BoilerplateLibrary::SelectBoilerplateBluePrint(
                        TypeId::Get<void*>().GetDefaultFastInterpTypeId(), true /*isZero*/));
        engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void*>(), 233, inst);
        std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
        void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
        ReleaseAssert(fnPtrVoid != nullptr);
        using FnType = void*(*)();
        FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
        ReleaseAssert(fnPtr() == nullptr);
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
            for (int lhsOscInt = 0; lhsOscInt < static_cast<int>(FIOperandShapeCategory::X_END_OF_ENUM); lhsOscInt++)
            {
                for (TypeId lhsIndexType : indexTypes)
                {
                    FIOperandShapeCategory lhsOsc = static_cast<FIOperandShapeCategory>(lhsOscInt);
                    if (lhsOsc != FIOperandShapeCategory::VARPTR_VAR && lhsOsc != FIOperandShapeCategory::VARPTR_LIT_NONZERO)
                    {
                        if (lhsIndexType != TypeId::Get<int32_t>())
                        {
                            continue;
                        }
                    }
                    for (int rhsOscInt = 0; rhsOscInt < static_cast<int>(FIOperandShapeCategory::X_END_OF_ENUM); rhsOscInt++)
                    {
                        for (TypeId rhsIndexType : indexTypes)
                        {
                            FIOperandShapeCategory rhsOsc = static_cast<FIOperandShapeCategory>(rhsOscInt);
                            if (rhsOsc != FIOperandShapeCategory::VARPTR_VAR && rhsOsc != FIOperandShapeCategory::VARPTR_LIT_NONZERO)
                            {
                                if (rhsIndexType != TypeId::Get<int32_t>())
                                {
                                    continue;
                                }
                            }
                            if (lhsOsc == FIOperandShapeCategory::LITERAL_NONZERO && rhsOsc == FIOperandShapeCategory::LITERAL_NONZERO)
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
                                if (lhsOsc == FIOperandShapeCategory::ZERO)
                                {
                                    lhsFn = GetZeroValueGenerator(dataType)();
                                }
                                else
                                {
                                    lhsFn = GetRandValueGenerator(dataType)(false /*forMult*/);
                                }
                                std::function<void(void*)> rhsFn;
                                if (rhsOsc == FIOperandShapeCategory::ZERO)
                                {
                                    rhsFn = GetZeroValueGenerator(dataType)();
                                }
                                else
                                {
                                    rhsFn = GetRandValueGenerator(dataType)(false /*forMult*/);
                                }
                                bool expectSame = false;
                                if (compareOrd == 3 && (lhsOsc == FIOperandShapeCategory::ZERO) == (rhsOsc == FIOperandShapeCategory::ZERO))
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
                                engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<bool>(), 233, inst);
                                std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
                                void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
                                ReleaseAssert(fnPtrVoid != nullptr);
                                using FnType = bool(*)();
                                FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
                                std::function<void(void*)> resultFn = [fnPtr](void* out)
                                {
                                    *reinterpret_cast<bool*>(out) = fnPtr();
                                };
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
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
                    AstArithmeticExprType::SUB));
    inst1->PopulateConstantPlaceholder<uint32_t>(0, 8);
    inst1->PopulateConstantPlaceholder<uint32_t>(2, 16);

    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FISimpleReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::COMPLEX));
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
                    false /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_32,
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(2),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_32) *
                                                         static_cast<int>(FIABIDistinctType::X_END_OF_ENUM) +
                                                         static_cast<int>(FIABIDistinctType::INT_32))));
    inst6->PopulateBoilerplateFnPtrPlaceholder(0, inst5);
    inst6->PopulateConstantPlaceholder<uint32_t>(0, 24 /*stackFrameSize*/);
    inst6->PopulateBoilerplateFnPtrPlaceholder(1, inst3);
    inst6->PopulateBoilerplateFnPtrPlaceholder(2, inst4);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<int>(), 233, inst6);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = int(*)();
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    ReleaseAssert(fnPtr() == 123 - 456);
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
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
                    AstArithmeticExprType::SUB));
    inst1->PopulateConstantPlaceholder<uint32_t>(0, 8);
    inst1->PopulateConstantPlaceholder<uint32_t>(2, 16);

    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FISimpleReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::COMPLEX));
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst1);

    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
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
                    false /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_32,
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(2),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_32) *
                                                         static_cast<int>(FIABIDistinctType::X_END_OF_ENUM) +
                                                         static_cast<int>(FIABIDistinctType::INT_32))));
    inst6->PopulateBoilerplateFnPtrPlaceholder(0, inst5);
    inst6->PopulateConstantPlaceholder<uint32_t>(0, 24 /*stackFrameSize*/);
    inst6->PopulateBoilerplateFnPtrPlaceholder(1, inst3);
    inst6->PopulateBoilerplateFnPtrPlaceholder(2, inst4);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<int>(), 233, inst6);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = int(*)();
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);
    ReleaseAssert(fnPtr() == 1234 - 5678 - 789);
    // Assert that after the called function returns, stack frame is unchanged
    //
    ReleaseAssert(__pochivm_thread_fastinterp_context.m_stackFrame == reinterpret_cast<uintptr_t>(sf));
}

TEST(TestFastInterpInternal, SanityCallExpr_3)
{
    // This test tests calling CPP function noexcept case
    //
    auto cppFnLambda = [](void** params) noexcept -> int
    {
        return *reinterpret_cast<int*>(params[0]) - *reinterpret_cast<int*>(params[1]);
    };
    using CppFnProto = int(*)(void**) noexcept;
    CppFnProto cppFn = cppFnLambda;
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    false /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_32,
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(2),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_32) *
                                                         static_cast<int>(FIABIDistinctType::X_END_OF_ENUM) +
                                                         static_cast<int>(FIABIDistinctType::INT_32))));
    inst->PopulateConstantPlaceholder<uint32_t>(0, 2);

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

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<int>(), 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = int(*)();
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    ReleaseAssert(fnPtr() == 12345 - 67890);
}

TEST(TestFastInterpInternal, SanityCallExpr_4)
{
    // This test tests calling CPP function may throw case (but does not throw)
    //
    auto cppFnLambda = [](void** params) noexcept -> bool
    {
        **reinterpret_cast<int**>(params[0]) = 123;
        return false;
    };
    auto exnHandlerLambda = [](void* /*exnContext*/, uintptr_t /*sfBase*/) noexcept -> void
    {
        ReleaseAssert(false);
    };
    using CppFnProto = bool(*)(void**) noexcept;
    CppFnProto cppFn = cppFnLambda;
    using ExnFnProto = void(*)(void*, uintptr_t) noexcept;
    ExnFnProto exnHandler = exnHandlerLambda;

    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    true /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_8,
                    false /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_64))));
    inst->PopulateConstantPlaceholder<uint32_t>(0, 1);

    int value = 0;
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void*>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst2->PopulateConstantPlaceholder<void*>(0, &value);

    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst->PopulateCppFnPtrPlaceholder(0, cppFn);
    inst->PopulateConstantPlaceholder<void*>(1, reinterpret_cast<void*>(12345));
    inst->PopulateCppFnPtrPlaceholder(1, exnHandler);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)();
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    fnPtr();
    ReleaseAssert(value == 123);
}

TEST(TestFastInterpInternal, SanityCallExpr_5)
{
    // This test tests calling CPP function may throw case (which actually throws)
    //
    __pochivm_thread_fastinterp_context.m_stackFrame = 233333;
    __pochivm_thread_fastinterp_context.m_ehTarget = nullptr;
    static bool shallThrow = false;
    auto cppFnLambda = [](void** params) noexcept -> bool
    {
        **reinterpret_cast<int**>(params[0]) = 123;
        return shallThrow;
    };
    auto cppFnLambda2 = [](void** params) noexcept -> void
    {
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
    using CppFnProtoMayThrow = bool(*)(void**) noexcept;
    CppFnProtoMayThrow cppFn = cppFnLambda;
    using ExnFnProto = void(*)(void*, uintptr_t) noexcept;
    ExnFnProto exnHandler = exnHandlerLambda;
    using CppFnProtoNoThrow = void(*)(void**) noexcept;
    CppFnProtoNoThrow cppFn2 = cppFnLambda2;

    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    true /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_8,
                    false /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_64))));
    inst1->PopulateConstantPlaceholder<uint32_t>(0, 1);

    int value = 0;
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void*>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    inst2->PopulateConstantPlaceholder<void*>(0, &value);

    inst1->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst1->PopulateConstantPlaceholder<void*>(1, reinterpret_cast<void*>(12345));
    inst1->PopulateCppFnPtrPlaceholder(0, cppFn);
    inst1->PopulateCppFnPtrPlaceholder(1, exnHandler);

    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    true /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_8,
                    true /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_64))));
    inst3->PopulateConstantPlaceholder<uint32_t>(0, 1);
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

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<bool>(), 233, inst5);
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

TEST(TestFastInterpInternal, SanityHandwrittenFibonacci_2)
{
    // Same logic as the previous test, but now the function is not noexcept (but does not actually throw either)
    // Just as another sanity test, and to understand the cost of setjmp()
    // fib(40) noexcept is 1.05s, fib(40) with setjmp but no longjmp happening is 1.73s
    //
    auto exnHandlerLambda = [](void* /*exnContext*/, uintptr_t /*sfBase*/) noexcept -> void
    {
        ReleaseAssert(false);
    };
    using ExnFnProto = void(*)(void*, uintptr_t) noexcept;
    ExnFnProto exnHandler = exnHandlerLambda;

    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* fib_fn = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFunctionImpl>::SelectBoilerplateBluePrint(
                    false /*isNoExcept*/,
                    static_cast<FIFunctionNumStatements>(1) /*numStmts*/,
                    static_cast<FIFunctionStmtsMayReturnMask>(1) /*mayReturnMask*/));

    FastInterpBoilerplateInstance* if_stmt = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIIfStatementImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    static_cast<FIIfStmtNumStatements>(1) /*trueBranchNumStmts*/,
                    static_cast<FIIfStmtMayCFRMask>(1) /*trueBranchMayCFRMask*/,
                    static_cast<FIIfStmtNumStatements>(1) /*falseBranchNumStmts*/,
                    static_cast<FIIfStmtMayCFRMask>(1) /*falseBranchMayCFRMask*/,
                    FIConditionShapeCategory::SIMPLE_COMPARISON,
                    AstComparisonExprType::LESS_EQUAL,
                    FIConditionOperandShapeCategory::VARIABLE,
                    FIConditionOperandShapeCategory::LITERAL_NONZERO_OR_32BIT));

    if_stmt->PopulateConstantPlaceholder<uint32_t>(0, 8);
    if_stmt->PopulateConstantPlaceholder<int>(1, 2);

    FastInterpBoilerplateInstance* true_br = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FISimpleReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::LITERAL_NONZERO));
    true_br->PopulateConstantPlaceholder<uint64_t>(0, 1);
    if_stmt->PopulateBoilerplateFnPtrPlaceholder(1, true_br);

    FastInterpBoilerplateInstance* false_br = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIReturnArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::COMPLEX,
                    FIOperandShapeCategory::COMPLEX,
                    AstArithmeticExprType::ADD));

    FastInterpBoilerplateInstance* call1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallGeneratedFnImpl>::SelectBoilerplateBluePrint(
                    false /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_64,
                    false /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_32))));
    call1->PopulateConstantPlaceholder(0, 16 /*stackFrameSize*/);
    call1->PopulateBoilerplateFnPtrPlaceholder(0, fib_fn);
    call1->PopulateCppFnPtrPlaceholder(0, exnHandler);
    call1->PopulateConstantPlaceholder<uint64_t>(1, 1);

    FastInterpBoilerplateInstance* call1_param = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB));
    call1_param->PopulateConstantPlaceholder<uint32_t>(0, 8 /*varOffset*/);
    call1_param->PopulateConstantPlaceholder<int>(2, 1);
    call1->PopulateBoilerplateFnPtrPlaceholder(1, call1_param);

    false_br->PopulateBoilerplateFnPtrPlaceholder(0, call1);

    FastInterpBoilerplateInstance* call2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallGeneratedFnImpl>::SelectBoilerplateBluePrint(
                    false /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_64,
                    false /*isNoExcept*/,
                    static_cast<FICallExprNumParameters>(1),
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(FIABIDistinctType::INT_32))));
    call2->PopulateConstantPlaceholder(0, 16 /*stackFrameSize*/);
    call2->PopulateBoilerplateFnPtrPlaceholder(0, fib_fn);
    call2->PopulateCppFnPtrPlaceholder(0, exnHandler);
    call2->PopulateConstantPlaceholder<uint64_t>(1, 1);

    FastInterpBoilerplateInstance* call2_param = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB));
    call2_param->PopulateConstantPlaceholder<uint32_t>(0, 8 /*varOffset*/);
    call2_param->PopulateConstantPlaceholder<int>(2, 2);
    call2->PopulateBoilerplateFnPtrPlaceholder(1, call2_param);

    false_br->PopulateBoilerplateFnPtrPlaceholder(2, call2);

    if_stmt->PopulateBoilerplateFnPtrPlaceholder(6, false_br);

    fib_fn->PopulateBoilerplateFnPtrPlaceholder(0, if_stmt);

    engine.RegisterGeneratedFunctionEntryPoint(reinterpret_cast<AstFunction*>(233), fib_fn, false /*isNoExcept*/);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    using FnProto = bool(*)();
    FnProto fib = reinterpret_cast<FnProto>(fnPtrVoid);

    {
        uint8_t* stackFrame = reinterpret_cast<uint8_t*>(alloca(16));
        for (size_t i = 0; i < 16; i++) { stackFrame[i] = static_cast<uint8_t>(rand() % 256); }
        *reinterpret_cast<int*>(stackFrame + 8) = 25;
        __pochivm_thread_fastinterp_context.m_stackFrame = reinterpret_cast<uintptr_t>(stackFrame);
        ReleaseAssert(fib() == false);
        uint64_t result = *reinterpret_cast<uint64_t*>(stackFrame);
        ReleaseAssert(result == 75025);
    }
}

TEST(TestFastInterpInternal, SanityCallExprParams)
{
    static bool cppFnExecuted;
    auto cppFnLambda = [](void** params) noexcept -> void
    {
        cppFnExecuted = true;
        ReleaseAssert(*reinterpret_cast<int8_t*>(params[0]) == static_cast<int8_t>(-123));
        ReleaseAssert(*reinterpret_cast<uint8_t*>(params[1]) == static_cast<uint8_t>(210));
        ReleaseAssert(*reinterpret_cast<int16_t*>(params[2]) == static_cast<int16_t>(-1234));
        ReleaseAssert(*reinterpret_cast<uint16_t*>(params[3]) == static_cast<uint16_t>(12345));
        ReleaseAssert(*reinterpret_cast<int32_t*>(params[4]) == static_cast<int32_t>(-1234567));
        ReleaseAssert(*reinterpret_cast<uint32_t*>(params[5]) == static_cast<uint32_t>(3214567890U));
        ReleaseAssert(*reinterpret_cast<int64_t*>(params[6]) == static_cast<int64_t>(-12345678987654321LL));
        ReleaseAssert(*reinterpret_cast<uint64_t*>(params[7]) == static_cast<uint64_t>(12345678987654321ULL));
        ReleaseAssert(*reinterpret_cast<bool*>(params[8]) == true);
        ReleaseAssert(*reinterpret_cast<bool*>(params[9]) == false);
        ReleaseAssert(fabs(static_cast<double>(*reinterpret_cast<float*>(params[10])) - 1.2345) < 1e-7);
        ReleaseAssert(fabs(*reinterpret_cast<double*>(params[11]) - 2.3456) < 1e-13);
    };
    using CppFnProto = void(*)(void**) noexcept;
    CppFnProto cppFn = cppFnLambda;

    constexpr int FIABIBase = static_cast<int>(FIABIDistinctType::X_END_OF_ENUM);
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallCppFnImpl>::SelectBoilerplateBluePrint(
                    true /*isReturnTypeVoid*/,
                    FIABIDistinctType::INT_8,
                    true /*isNoExcept*/,
                    FICallExprNumParameters::MORE_THAN_THREE,
                    static_cast<FICallExprParamTypeMask>(static_cast<int>(GetFIABIDistinctType<int8_t>())
                           + math::power(FIABIBase, 1) * static_cast<int>(GetFIABIDistinctType<uint8_t>())
                           + math::power(FIABIBase, 2) * static_cast<int>(GetFIABIDistinctType<int16_t>()))));
    inst->PopulateConstantPlaceholder<uint32_t>(0, 12);
    inst->PopulateCppFnPtrPlaceholder(0, cppFn);

    FastInterpBoilerplateInstance* param0 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int8_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param0->PopulateConstantPlaceholder<int8_t>(0, static_cast<int8_t>(-123));

    FastInterpBoilerplateInstance* param1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint8_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param1->PopulateConstantPlaceholder<uint8_t>(0, static_cast<uint8_t>(210));

    FastInterpBoilerplateInstance* param2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int16_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param2->PopulateConstantPlaceholder<int16_t>(0, static_cast<int16_t>(-1234));

    FastInterpBoilerplateInstance* extraParamList1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprExtraParamsImpl>::SelectBoilerplateBluePrint(
                    FICallExprNumExtraParameters::MORE_THAN_FOUR,
                    static_cast<FICallExprExtraParamTypeMask>(static_cast<int>(GetFIABIDistinctType<uint16_t>())
                           + math::power(FIABIBase, 1) * static_cast<int>(GetFIABIDistinctType<int32_t>())
                           + math::power(FIABIBase, 2) * static_cast<int>(GetFIABIDistinctType<uint32_t>())
                           + math::power(FIABIBase, 3) * static_cast<int>(GetFIABIDistinctType<int64_t>()))));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, param0);
    inst->PopulateBoilerplateFnPtrPlaceholder(1, param1);
    inst->PopulateBoilerplateFnPtrPlaceholder(2, param2);
    inst->PopulateBoilerplateFnPtrPlaceholder(3, extraParamList1);

    FastInterpBoilerplateInstance* param3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint16_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param3->PopulateConstantPlaceholder<uint16_t>(0, static_cast<uint16_t>(12345));

    FastInterpBoilerplateInstance* param4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param4->PopulateConstantPlaceholder<int32_t>(0, static_cast<int32_t>(-1234567));

    FastInterpBoilerplateInstance* param5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint32_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param5->PopulateConstantPlaceholder<uint32_t>(0, static_cast<uint32_t>(3214567890U));

    FastInterpBoilerplateInstance* param6 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int64_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param6->PopulateConstantPlaceholder<int64_t>(0, static_cast<int64_t>(-12345678987654321LL));

    FastInterpBoilerplateInstance* extraParamList2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprExtraParamsImpl>::SelectBoilerplateBluePrint(
                    FICallExprNumExtraParameters::MORE_THAN_FOUR,
                    static_cast<FICallExprExtraParamTypeMask>(static_cast<int>(GetFIABIDistinctType<uint64_t>())
                           + math::power(FIABIBase, 1) * static_cast<int>(GetFIABIDistinctType<bool>())
                           + math::power(FIABIBase, 2) * static_cast<int>(GetFIABIDistinctType<bool>())
                           + math::power(FIABIBase, 3) * static_cast<int>(GetFIABIDistinctType<float>()))));

    extraParamList1->PopulateBoilerplateFnPtrPlaceholder(0, param3);
    extraParamList1->PopulateBoilerplateFnPtrPlaceholder(1, param4);
    extraParamList1->PopulateBoilerplateFnPtrPlaceholder(2, param5);
    extraParamList1->PopulateBoilerplateFnPtrPlaceholder(3, param6);
    extraParamList1->PopulateBoilerplateFnPtrPlaceholder(4, extraParamList2);

    FastInterpBoilerplateInstance* param7 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param7->PopulateConstantPlaceholder<uint64_t>(0, static_cast<uint64_t>(12345678987654321ULL));

    FastInterpBoilerplateInstance* param8 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<bool>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param8->PopulateConstantPlaceholder<bool>(0, true);

    FastInterpBoilerplateInstance* param9 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<bool>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param9->PopulateConstantPlaceholder<bool>(0, false);

    FastInterpBoilerplateInstance* param10 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param10->PopulateConstantPlaceholder<float>(0, static_cast<float>(1.2345));

    FastInterpBoilerplateInstance* extraParamList3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprExtraParamsImpl>::SelectBoilerplateBluePrint(
                    static_cast<FICallExprNumExtraParameters>(1),
                    static_cast<FICallExprExtraParamTypeMask>(static_cast<int>(GetFIABIDistinctType<double>()))));

    extraParamList2->PopulateBoilerplateFnPtrPlaceholder(0, param7);
    extraParamList2->PopulateBoilerplateFnPtrPlaceholder(1, param8);
    extraParamList2->PopulateBoilerplateFnPtrPlaceholder(2, param9);
    extraParamList2->PopulateBoilerplateFnPtrPlaceholder(3, param10);
    extraParamList2->PopulateBoilerplateFnPtrPlaceholder(4, extraParamList3);

    FastInterpBoilerplateInstance* param11 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    false /*isAllUnderlyingBitsZero*/));
    param11->PopulateConstantPlaceholder<double>(0, static_cast<double>(2.3456));

    extraParamList3->PopulateBoilerplateFnPtrPlaceholder(0, param11);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)();
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    cppFnExecuted = false;
    fnPtr();
    ReleaseAssert(cppFnExecuted);
}

#endif

TEST(TestFastInterpInternal, Sanity_1)
{
    // Test the simplest case: add two zeros.. no placeholders shall be needed
    //
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::ZERO,
                    FISimpleOperandShapeCategory::ZERO,
                    AstArithmeticExprType::ADD,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    true /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst2->PopulateConstantPlaceholder<uint64_t>(1, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    int result = 233;
    fnPtr(reinterpret_cast<uintptr_t>(&result));
    ReleaseAssert(result == 0);
}

TEST(TestFastInterpInternal, Sanity_2)
{
    // Test arith operation on two literal values
    //
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::MUL,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    true /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst->PopulateConstantPlaceholder<int>(1, 123);
    inst->PopulateConstantPlaceholder<int>(2, 45678);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst2->PopulateConstantPlaceholder<uint64_t>(1, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    int result = 233;
    fnPtr(reinterpret_cast<uintptr_t>(&result));
    ReleaseAssert(result == 123 * 45678);
}

TEST(TestFastInterpInternal, Sanity_3)
{
    // Test arith operation on two literal values, type is double to test that bitcasting works as expected.
    //
    FastInterpCodegenEngine engine;
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::MUL,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    true /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst->PopulateConstantPlaceholder<double>(1, 123.456);
    inst->PopulateConstantPlaceholder<double>(2, 789.012);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst2->PopulateConstantPlaceholder<uint64_t>(1, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    double result = 233.4;
    fnPtr(reinterpret_cast<uintptr_t>(&result));
    ReleaseAssert(fabs(result - 123.456 * 789.012) < 1e-11);
}

TEST(TestFastInterpInternal, Sanity_4)
{
    // Test a expression tree '(a + b) * (c - d)'
    //
    FastInterpCodegenEngine engine;
    // 'a+b'
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::ADD,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'c-d'
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(1),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'mul'
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::MUL,
                    static_cast<FIBinaryOpNumQuickAccessParams>(2),
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    true /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst4);
    inst4->PopulateBoilerplateFnPtrPlaceholder(0, inst5);

    inst->PopulateConstantPlaceholder<int>(1, 321);
    inst->PopulateConstantPlaceholder<int>(2, 567);
    inst2->PopulateConstantPlaceholder<int>(1, -123);
    inst2->PopulateConstantPlaceholder<int>(2, -89);
    inst4->PopulateConstantPlaceholder<uint64_t>(1, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233)));

    int result = 233;
    fnPtr(reinterpret_cast<uintptr_t>(&result));
    ReleaseAssert(result == (321 + 567) * (-123 - (-89)));
}

TEST(TestFastInterpInternal, Sanity_5)
{
    // Test a expression tree '(a + b) / (c - d)'
    //
    FastInterpCodegenEngine engine;
    // 'a+b'
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::ADD,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'c-d'
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(1)));
    // 'div'
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::DIV,
                    static_cast<FIBinaryOpNumQuickAccessParams>(2),
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    true /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst4);
    inst4->PopulateBoilerplateFnPtrPlaceholder(0, inst5);

    inst->PopulateConstantPlaceholder<double>(1, 321.09);
    inst->PopulateConstantPlaceholder<double>(2, 567.23);
    inst2->PopulateConstantPlaceholder<double>(1, -123.12);
    inst2->PopulateConstantPlaceholder<double>(2, -89.8);
    inst4->PopulateConstantPlaceholder<uint64_t>(1, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233)));

    double result = 233.4;
    fnPtr(reinterpret_cast<uintptr_t>(&result));
    ReleaseAssert(fabs(result - (321.09 + 567.23) / (-123.12 - (-89.8))) < 1e-11);
}

TEST(TestFastInterpInternal, Sanity_6)
{
    // Test spill to memory
    //
    FastInterpCodegenEngine engine;
    // 'a+b'
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::ADD,
                    true /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'c-d'
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB,
                    true /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'div'
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::DIV,
                    static_cast<FIBinaryOpNumQuickAccessParams>(0),
                    true /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    false /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst4);
    inst4->PopulateBoilerplateFnPtrPlaceholder(0, inst5);

    inst->PopulateConstantPlaceholder<uint64_t>(0, 8);
    inst->PopulateConstantPlaceholder<double>(1, 321.09);
    inst->PopulateConstantPlaceholder<double>(2, 567.23);
    inst2->PopulateConstantPlaceholder<uint64_t>(0, 16);
    inst2->PopulateConstantPlaceholder<double>(1, -123.12);
    inst2->PopulateConstantPlaceholder<double>(2, -89.8);
    inst3->PopulateConstantPlaceholder<uint64_t>(1, 8);
    inst3->PopulateConstantPlaceholder<uint64_t>(2, 16);
    inst3->PopulateConstantPlaceholder<uint64_t>(0, 24);
    inst4->PopulateConstantPlaceholder<uint64_t>(0, 24);
    inst4->PopulateConstantPlaceholder<uint64_t>(1, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233)));

    double result[4];
    result[0] = result[1] = result[2] = result[3] = 233.4;
    fnPtr(reinterpret_cast<uintptr_t>(result));
    ReleaseAssert(fabs(result[0] - (321.09 + 567.23) / (-123.12 - (-89.8))) < 1e-11);
}

TEST(TestFastInterpInternal, SanityHandwrittenEulerSieve)
{
    // This test handrolls Euler's Sieve using FastInterp, just as a more complex sanity test
    //
    FastInterpCodegenEngine engine;

    FastInterpBoilerplateInstance* terminal = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));

    // Stack frame: n @ 8, lp @ 16, pr @ 24, cnt @ 32, i @ 36, j @ 40, k @ 44
    // int cnt = 0;
    //
    FastInterpBoilerplateInstance* main_stmt1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::ZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    main_stmt1->PopulateConstantPlaceholder<uint64_t>(0, 32);

    // int i = 2;
    //
    FastInterpBoilerplateInstance* main_stmt2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    main_stmt2->PopulateConstantPlaceholder<uint64_t>(0, 36);
    main_stmt2->PopulateConstantPlaceholder<int>(2, 2);
    main_stmt1->PopulateBoilerplateFnPtrPlaceholder(0, main_stmt2);

    // for (; i <=n; ...) ...
    //
    FastInterpBoilerplateInstance* outer_for_loop = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL,
                    false /*putFalseBranchAtEnd*/), 4 /*log2FunctionAlignment*/);
    outer_for_loop->PopulateConstantPlaceholder<uint64_t>(0, 36);
    outer_for_loop->PopulateConstantPlaceholder<uint64_t>(2, 8);
    main_stmt2->PopulateBoilerplateFnPtrPlaceholder(0, outer_for_loop);

    // if (lp[i] == 0) ...
    //
    FastInterpBoilerplateInstance* if_cond = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOperandShapeCategory::ZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::EQUAL,
                    false /*putFalseBranchAtEnd*/));
    if_cond->PopulateConstantPlaceholder<uint64_t>(0, 16);
    if_cond->PopulateConstantPlaceholder<uint64_t>(1, 36);
    outer_for_loop->PopulateBoilerplateFnPtrPlaceholder(0, if_cond);

    // lp[i] = i
    //
    FastInterpBoilerplateInstance* true_br_stmt_1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    true_br_stmt_1->PopulateConstantPlaceholder<uint64_t>(0, 16);
    true_br_stmt_1->PopulateConstantPlaceholder<uint64_t>(1, 36);
    true_br_stmt_1->PopulateConstantPlaceholder<uint64_t>(2, 36);
    if_cond->PopulateBoilerplateFnPtrPlaceholder(0, true_br_stmt_1);

    // pr[cnt] = i
    //
    FastInterpBoilerplateInstance* true_br_stmt_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    true_br_stmt_2->PopulateConstantPlaceholder<uint64_t>(0, 24);
    true_br_stmt_2->PopulateConstantPlaceholder<uint64_t>(1, 32);
    true_br_stmt_2->PopulateConstantPlaceholder<uint64_t>(2, 36);
    true_br_stmt_1->PopulateBoilerplateFnPtrPlaceholder(0, true_br_stmt_2);

    // cnt++
    //
    FastInterpBoilerplateInstance* true_br_stmt_3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignArithExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstArithmeticExprType::ADD));
    true_br_stmt_3->PopulateConstantPlaceholder<uint64_t>(0, 32);
    true_br_stmt_3->PopulateConstantPlaceholder<uint64_t>(1, 32);
    true_br_stmt_3->PopulateConstantPlaceholder<int>(3, 1);
    true_br_stmt_2->PopulateBoilerplateFnPtrPlaceholder(0, true_br_stmt_3);

    // int j = 0
    //
    FastInterpBoilerplateInstance* outer_loop_stmt_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::ZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    outer_loop_stmt_2->PopulateConstantPlaceholder<uint64_t>(0, 40);
    true_br_stmt_3->PopulateBoilerplateFnPtrPlaceholder(0, outer_loop_stmt_2);
    if_cond->PopulateBoilerplateFnPtrPlaceholder(1, outer_loop_stmt_2);

    // j < cnt
    //
    FastInterpBoilerplateInstance* inner_loop_cond_1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_THAN,
                    false /*putFalseBranchAtEnd*/), 4 /*log2FunctionAlignment*/);
    inner_loop_cond_1->PopulateConstantPlaceholder<uint64_t>(0, 40);
    inner_loop_cond_1->PopulateConstantPlaceholder<uint64_t>(2, 32);
    outer_loop_stmt_2->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_cond_1);

    // pr[j] <= lp[i]
    //
    FastInterpBoilerplateInstance* inner_loop_cond_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL,
                    false /*putFalseBranchAtEnd*/));
    inner_loop_cond_2->PopulateConstantPlaceholder<uint64_t>(0, 24);
    inner_loop_cond_2->PopulateConstantPlaceholder<uint64_t>(1, 40);
    inner_loop_cond_2->PopulateConstantPlaceholder<uint64_t>(2, 16);
    inner_loop_cond_2->PopulateConstantPlaceholder<uint64_t>(3, 36);
    inner_loop_cond_1->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_cond_2);

    // 'i'
    //
    FastInterpBoilerplateInstance* value_of_i = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIDerefVariableImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    value_of_i->PopulateConstantPlaceholder<uint64_t>(1, 36);
    inner_loop_cond_2->PopulateBoilerplateFnPtrPlaceholder(0, value_of_i);

    // i * pr[j]
    //
    FastInterpBoilerplateInstance* ixprj = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    false /*isInlinedSideLhs*/,
                    true /*isQuickAccessOperand*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstArithmeticExprType::MUL));
    ixprj->PopulateConstantPlaceholder<uint64_t>(2, 24);
    ixprj->PopulateConstantPlaceholder<uint64_t>(3, 40);
    value_of_i->PopulateBoilerplateFnPtrPlaceholder(0, ixprj);

    // i * pr[j] <= n
    //
    FastInterpBoilerplateInstance* inner_loop_cond_3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    false /*isInlinedSideLhs*/,
                    true /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL,
                    false /*putFalseBranchAtEnd*/));
    inner_loop_cond_3->PopulateConstantPlaceholder<uint64_t>(1, 8);
    ixprj->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_cond_3);

    // 'i'
    //
    FastInterpBoilerplateInstance* value_of_i_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIDerefVariableImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    value_of_i_2->PopulateConstantPlaceholder<uint64_t>(1, 36);
    inner_loop_cond_3->PopulateBoilerplateFnPtrPlaceholder(0, value_of_i_2);

    // i * pr[j]
    //
    FastInterpBoilerplateInstance* ixprj_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    false /*isInlinedSideLhs*/,
                    true /*isQuickAccessOperand*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstArithmeticExprType::MUL));
    ixprj_2->PopulateConstantPlaceholder<uint64_t>(2, 24);
    ixprj_2->PopulateConstantPlaceholder<uint64_t>(3, 40);
    value_of_i_2->PopulateBoilerplateFnPtrPlaceholder(0, ixprj_2);

    // lp + i * pr[j]
    //
    FastInterpBoilerplateInstance* inner_loop_stmt_1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineLhsPointerArithmeticImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::ADD,
                    true /*isQAP*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    static_cast<FIPowerOfTwoObjectSize>(2) /*int is 2^2 bytes*/));
    inner_loop_stmt_1->PopulateConstantPlaceholder<uint64_t>(2, 16);
    ixprj_2->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_stmt_1);

    // *... = pr[j]
    //
    FastInterpBoilerplateInstance* inner_loop_stmt_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineRhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    true /*isQAP*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    inner_loop_stmt_2->PopulateConstantPlaceholder<uint64_t>(1, 24);
    inner_loop_stmt_2->PopulateConstantPlaceholder<uint64_t>(2, 40);
    inner_loop_stmt_1->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_stmt_2);

    // j++
    //
    FastInterpBoilerplateInstance* inner_loop_step_stmt = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignArithExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstArithmeticExprType::ADD));
    inner_loop_step_stmt->PopulateConstantPlaceholder<uint64_t>(0, 40);
    inner_loop_step_stmt->PopulateConstantPlaceholder<uint64_t>(1, 40);
    inner_loop_step_stmt->PopulateConstantPlaceholder<int>(3, 1);
    inner_loop_stmt_2->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_step_stmt);
    inner_loop_step_stmt->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_cond_1);

    // i++
    //
    FastInterpBoilerplateInstance* outer_loop_step_stmt = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignArithExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstArithmeticExprType::ADD));
    outer_loop_step_stmt->PopulateConstantPlaceholder<uint64_t>(0, 36);
    outer_loop_step_stmt->PopulateConstantPlaceholder<uint64_t>(1, 36);
    outer_loop_step_stmt->PopulateConstantPlaceholder<int>(3, 1);
    inner_loop_cond_1->PopulateBoilerplateFnPtrPlaceholder(1, outer_loop_step_stmt);
    inner_loop_cond_2->PopulateBoilerplateFnPtrPlaceholder(1, outer_loop_step_stmt);
    inner_loop_cond_3->PopulateBoilerplateFnPtrPlaceholder(1, outer_loop_step_stmt);
    outer_loop_step_stmt->PopulateBoilerplateFnPtrPlaceholder(0, outer_for_loop);

    // return cnt
    //
    FastInterpBoilerplateInstance* return_stmt = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlineAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    return_stmt->PopulateConstantPlaceholder<uint64_t>(0, 0, true);
    return_stmt->PopulateConstantPlaceholder<uint64_t>(2, 32);
    outer_for_loop->PopulateBoilerplateFnPtrPlaceholder(1, return_stmt);
    return_stmt->PopulateBoilerplateFnPtrPlaceholder(0, terminal);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>(), true /*isNoExcept*/, 233, main_stmt1);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    using FnProto = void(*)(uintptr_t);
    FnProto fnPtr = reinterpret_cast<FnProto>(fnPtrVoid);

    int n = 1000000;
    int* lp = new int[static_cast<size_t>(n + 10)];
    memset(lp, 0, sizeof(int) * static_cast<size_t>(n + 10));
    int* pr = new int[static_cast<size_t>(n + 10)];
    memset(pr, 0, sizeof(int) * static_cast<size_t>(n + 10));

    {
        uint8_t* stackFrame = reinterpret_cast<uint8_t*>(alloca(48));
        for (size_t i = 0; i < 48; i++) { stackFrame[i] = static_cast<uint8_t>(rand() % 256); }
        *reinterpret_cast<int*>(stackFrame + 8) = n;
        *reinterpret_cast<int**>(stackFrame + 16) = lp;
        *reinterpret_cast<int**>(stackFrame + 24) = pr;
        __pochivm_thread_fastinterp_context.m_stackFrame = reinterpret_cast<uintptr_t>(stackFrame);
        fnPtr(reinterpret_cast<uintptr_t>(stackFrame));
        int result = *reinterpret_cast<int*>(stackFrame);
        ReleaseAssert(result == 78498);
    }
}

TEST(TestFastInterpInternal, SanityHandwrittenFibonacci)
{
    // This test handrolls fibonacci sequence using FastInterp, just as a more complex sanity test
    //
    FastInterpCodegenEngine engine;

    // stack frame: n @ 8, tmp @ 16
    //
    FastInterpBoilerplateInstance* fib_fn = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL,
                    true /*putFalseBranchAtEnd*/), 4);
    fib_fn->PopulateConstantPlaceholder<uint64_t>(0, 8);
    fib_fn->PopulateConstantPlaceholder<int>(2, 2);

    FastInterpBoilerplateInstance* ret_node1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    true /*isLiteral*/,
                    false /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    ret_node1->PopulateConstantPlaceholder<uint64_t>(0, 1);
    fib_fn->PopulateBoilerplateFnPtrPlaceholder(0, ret_node1);

    FastInterpBoilerplateInstance* compute_lhs = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    true /*spillReturnValue*/,
                    true /*isCalleeNoExcept*/,
                    FIStackframeSizeCategoryHelper::SelectCategory(24)));
    compute_lhs->PopulateConstantPlaceholder<uint64_t>(0, 16);
    fib_fn->PopulateBoilerplateFnPtrPlaceholder(1, compute_lhs);

    FastInterpBoilerplateInstance* compute_lhs_param = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::VARIABLE,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(1),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    compute_lhs_param->PopulateConstantPlaceholder<uint64_t>(1, 8);
    compute_lhs_param->PopulateConstantPlaceholder<int>(2, 1);
    compute_lhs->PopulateBoilerplateFnPtrPlaceholder(1, compute_lhs_param);

    FastInterpBoilerplateInstance* populate_lhs_param = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprStoreParamImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    true /*isQuickAccess*/,
                    false /*hasMore*/));
    populate_lhs_param->PopulateConstantPlaceholder<uint64_t>(0, 8);
    populate_lhs_param->PopulateBoilerplateFnPtrPlaceholder(0, fib_fn);
    compute_lhs_param->PopulateBoilerplateFnPtrPlaceholder(0, populate_lhs_param);

    FastInterpBoilerplateInstance* compute_rhs = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    false /*spillReturnValue*/,
                    true /*isCalleeNoExcept*/,
                    FIStackframeSizeCategoryHelper::SelectCategory(24)));
    compute_lhs->PopulateBoilerplateFnPtrPlaceholder(0, compute_rhs);

    FastInterpBoilerplateInstance* compute_rhs_param = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::VARIABLE,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(1),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    compute_rhs_param->PopulateConstantPlaceholder<uint64_t>(1, 8);
    compute_rhs_param->PopulateConstantPlaceholder<int>(2, 2);
    compute_rhs->PopulateBoilerplateFnPtrPlaceholder(1, compute_rhs_param);

    FastInterpBoilerplateInstance* populate_rhs_param = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprStoreParamImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    true /*isQuickAccess*/,
                    false /*hasMore*/));
    populate_rhs_param->PopulateConstantPlaceholder<uint64_t>(0, 8);
    populate_rhs_param->PopulateBoilerplateFnPtrPlaceholder(0, fib_fn);
    compute_rhs_param->PopulateBoilerplateFnPtrPlaceholder(0, populate_rhs_param);

    FastInterpBoilerplateInstance* compute_sum = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::ADD,
                    static_cast<FIBinaryOpNumQuickAccessParams>(1) /*numQAP*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    compute_sum->PopulateConstantPlaceholder<uint64_t>(1, 16);
    compute_rhs->PopulateBoilerplateFnPtrPlaceholder(0, compute_sum);

    FastInterpBoilerplateInstance* ret_node2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FITerminatorOperatorImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    false /*isLiteral*/,
                    true /*isQuickAccess*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    compute_sum->PopulateBoilerplateFnPtrPlaceholder(0, ret_node2);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<uint64_t>(), true /*isNoExcept*/, 233, fib_fn);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    using FnProto = uint64_t(*)(uintptr_t);
    FnProto fib = reinterpret_cast<FnProto>(fnPtrVoid);

    {
        uint8_t* stackFrame = reinterpret_cast<uint8_t*>(alloca(24));
        for (size_t i = 0; i < 24; i++) { stackFrame[i] = static_cast<uint8_t>(rand() % 256); }
        *reinterpret_cast<int*>(stackFrame + 8) = 25;
        uint64_t result = fib(reinterpret_cast<uintptr_t>(stackFrame));
        ReleaseAssert(result == 75025);
    }
}

// Test StackFrameManager
//
TEST(TestFastInterpInternal, SanityStackFrameManager_1)
{
    FIStackFrameManager sfm;
    ReleaseAssert(sfm.PushLocalVar(TypeId::Get<int>()) == 8);
    ReleaseAssert(sfm.PushLocalVar(TypeId::Get<uint64_t>()) == 16);

    sfm.PushTemp(TypeId::Get<int>());
    sfm.PushTemp(TypeId::Get<int>());
    sfm.PushTemp(TypeId::Get<int>());
    sfm.PushTemp(TypeId::Get<float>());
    sfm.PushTemp(TypeId::Get<double>());
    sfm.PushTemp(TypeId::Get<double>());
    sfm.PushTemp(TypeId::Get<double>());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).IsNoSpill());
    sfm.PushTemp(TypeId::Get<double>());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).IsNoSpill());
    sfm.PushTemp(TypeId::Get<int>());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).IsNoSpill());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).IsNoSpill());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).IsNoSpill());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<float>()).GetSpillLocation() == 24);
    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).IsNoSpill());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).IsNoSpill());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).GetSpillLocation() == 32);

    sfm.PopLocalVar(TypeId::Get<uint64_t>());
    sfm.PopLocalVar(TypeId::Get<int>());

    ReleaseAssert(sfm.GetFinalStackFrameSize() == 40);
}

TEST(TestFastInterpInternal, SanityStackFrameManager_2)
{
    FIStackFrameManager sfm;
    ReleaseAssert(sfm.PushLocalVar(TypeId::Get<int>()) == 8);
    ReleaseAssert(sfm.PushLocalVar(TypeId::Get<uint64_t>()) == 16);

    sfm.PushTemp(TypeId::Get<int>());
    sfm.PushTemp(TypeId::Get<int>());
    sfm.PushTemp(TypeId::Get<int>());
    sfm.PushTemp(TypeId::Get<float>());
    sfm.PushTemp(TypeId::Get<double>());
    sfm.PushTemp(TypeId::Get<double>());
    sfm.PushTemp(TypeId::Get<double>());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).IsNoSpill());
    sfm.PushTemp(TypeId::Get<double>());
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).IsNoSpill());
    sfm.PushTemp(TypeId::Get<int>());

    sfm.ForceSpillAll();

    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).GetSpillLocation() == 56);
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).GetSpillLocation() == 72);
    ReleaseAssert(sfm.PopTemp(TypeId::Get<double>()).GetSpillLocation() == 64);
    ReleaseAssert(sfm.PopTemp(TypeId::Get<float>()).GetSpillLocation() == 24);
    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).GetSpillLocation() == 48);
    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).GetSpillLocation() == 40);
    ReleaseAssert(sfm.PopTemp(TypeId::Get<int>()).GetSpillLocation() == 32);

    sfm.PopLocalVar(TypeId::Get<uint64_t>());
    sfm.PopLocalVar(TypeId::Get<int>());

    ReleaseAssert(sfm.GetFinalStackFrameSize() == 80);
}
