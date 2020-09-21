#include "gtest/gtest.h"

#include "fastinterp/fastinterp.h"

using namespace PochiVM;

TEST(TestFastInterpInternal, SanitySymbolNames)
{
    // Sanity check 'SelectBoilerplateBluePrint' indeed selected the boilerplate we expected by checking its symbol name
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FastInterpArithmeticExprImpl>;
    const FastInterpBoilerplateBluePrint* blueprint;
    blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               OperandShapeCategory::LITERAL_NONZERO,
                                                               OperandShapeCategory::LITERAL_NONZERO,
                                                               AstArithmeticExprType::ADD);
    TestAssert(blueprint->TestOnly_GetSymbolName() == std::string("_ZN7PochiVM28FastInterpArithmeticExprImpl1fIiiiLNS_20OperandShapeCategoryE4ELS2_4ELNS_21AstArithmeticExprTypeE0EEEvPT_"));
    blueprint = BoilerplateLibrary::SelectBoilerplateBluePrint(TypeId::Get<double>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                                                               OperandShapeCategory::COMPLEX,
                                                               OperandShapeCategory::ZERO,
                                                               AstArithmeticExprType::MUL);
    TestAssert(blueprint->TestOnly_GetSymbolName() == std::string("_ZN7PochiVM28FastInterpArithmeticExprImpl1fIdiiLNS_20OperandShapeCategoryE6ELS2_5ELNS_21AstArithmeticExprTypeE2EEEvPT_"));
    std::ignore = blueprint;
}

TEST(TestFastInterpInternal, Sanity_1)
{
    // Test the simplest case: add two zeros.. no placeholders shall be needed
    //
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FastInterpArithmeticExprImpl>;
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
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FastInterpArithmeticExprImpl>;
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
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FastInterpArithmeticExprImpl>;
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
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FastInterpArithmeticExprImpl>;
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
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FastInterpArithmeticExprImpl>;
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
    using BoilerplateLibrary = FastInterpBoilerplateLibrary<FastInterpVariableImpl>;
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
