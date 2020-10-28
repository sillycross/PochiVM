#include "gtest/gtest.h"

#include "fastinterp/fastinterp.hpp"

using namespace PochiVM;

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
                FastInterpBoilerplateLibrary<FIPartialInlineLhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst2->PopulateConstantPlaceholder<uint64_t>(0, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>().GetDefaultFastInterpTypeId(), true, 233, inst);
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
                FastInterpBoilerplateLibrary<FIPartialInlineLhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst->PopulateConstantPlaceholder<int>(1, 123);
    inst->PopulateConstantPlaceholder<int>(2, 45678);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst2->PopulateConstantPlaceholder<uint64_t>(0, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>().GetDefaultFastInterpTypeId(), true, 233, inst);
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
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::MUL,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineLhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst->PopulateConstantPlaceholder<float>(1, static_cast<float>(123.456));
    inst->PopulateConstantPlaceholder<float>(2, static_cast<float>(789.012));
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst2->PopulateConstantPlaceholder<uint64_t>(0, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>().GetDefaultFastInterpTypeId(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    void* fnPtrVoid = gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233));
    ReleaseAssert(fnPtrVoid != nullptr);
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(fnPtrVoid);

    float result = 233.4f;
    fnPtr(reinterpret_cast<uintptr_t>(&result));
    ReleaseAssert(fabs(static_cast<double>(result) - 123.456 * 789.012) < 1e-3);
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
                    true /*isLhsQAP*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineLhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
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
    inst4->PopulateConstantPlaceholder<uint64_t>(0, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>().GetDefaultFastInterpTypeId(), true, 233, inst);
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
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::ADD,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'c-d'
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(1)));
    // 'div'
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::DIV,
                    true /*isLhsQAP*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineLhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst4);
    inst4->PopulateBoilerplateFnPtrPlaceholder(0, inst5);

    inst->PopulateConstantPlaceholder<float>(1, 321.09f);
    inst->PopulateConstantPlaceholder<float>(2, 567.23f);
    inst2->PopulateConstantPlaceholder<float>(1, -123.12f);
    inst2->PopulateConstantPlaceholder<float>(2, -89.8f);
    inst4->PopulateConstantPlaceholder<uint64_t>(0, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>().GetDefaultFastInterpTypeId(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233)));

    float result = 233.4f;
    fnPtr(reinterpret_cast<uintptr_t>(&result));
    ReleaseAssert(fabs(static_cast<double>(result) - (321.09 + 567.23) / (-123.12 - (-89.8))) < 1e-3);
}

TEST(TestFastInterpInternal, Sanity_6)
{
    // Test spill to memory
    //
    FastInterpCodegenEngine engine;
    // 'a+b'
    FastInterpBoilerplateInstance* inst = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::ADD,
                    true /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'c-d'
    FastInterpBoilerplateInstance* inst2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    FISimpleOperandShapeCategory::LITERAL_NONZERO,
                    AstArithmeticExprType::SUB,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    // 'div'
    FastInterpBoilerplateInstance* inst3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::DIV,
                    false /*isLhsQAP*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst4 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineLhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<float>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    FastInterpBoilerplateInstance* inst5 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    inst->PopulateBoilerplateFnPtrPlaceholder(0, inst2);
    inst2->PopulateBoilerplateFnPtrPlaceholder(0, inst3);
    inst3->PopulateBoilerplateFnPtrPlaceholder(0, inst4);
    inst4->PopulateBoilerplateFnPtrPlaceholder(0, inst5);

    inst->PopulateConstantPlaceholder<uint64_t>(0, 4);
    inst->PopulateConstantPlaceholder<float>(1, 321.09f);
    inst->PopulateConstantPlaceholder<float>(2, 567.23f);
    inst2->PopulateConstantPlaceholder<uint64_t>(0, 8);
    inst2->PopulateConstantPlaceholder<float>(1, -123.12f);
    inst2->PopulateConstantPlaceholder<float>(2, -89.8f);
    inst3->PopulateConstantPlaceholder<uint64_t>(1, 4);
    inst4->PopulateConstantPlaceholder<uint64_t>(0, 0, true);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>().GetDefaultFastInterpTypeId(), true, 233, inst);
    std::unique_ptr<FastInterpGeneratedProgram> gp = engine.Materialize();
    using FnType = void(*)(uintptr_t);
    FnType fnPtr = reinterpret_cast<FnType>(gp->GetGeneratedFunctionAddress(reinterpret_cast<AstFunction*>(233)));

    float result[4];
    result[0] = result[1] = result[2] = result[3] = 233.4f;
    fnPtr(reinterpret_cast<uintptr_t>(result));
    ReleaseAssert(fabs(static_cast<double>(result[0]) - (321.09 + 567.23) / (-123.12 - (-89.8))) < 1e-3);
}

TEST(TestFastInterpInternal, SanityHandwrittenEulerSieve)
{
    // This test handrolls Euler's Sieve using FastInterp, just as a more complex sanity test
    //
    FastInterpCodegenEngine engine;

    FastInterpBoilerplateInstance* terminal = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
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
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonFavourTrueBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL), 4 /*log2FunctionAlignment*/);
    outer_for_loop->PopulateConstantPlaceholder<uint64_t>(0, 36);
    outer_for_loop->PopulateConstantPlaceholder<uint64_t>(2, 8);
    main_stmt2->PopulateBoilerplateFnPtrPlaceholder(0, outer_for_loop);

    // if (lp[i] == 0) ...
    //
    FastInterpBoilerplateInstance* if_cond = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonFavourTrueBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOperandShapeCategory::ZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::EQUAL));
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
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonFavourTrueBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::VARIABLE,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_THAN), 4 /*log2FunctionAlignment*/);
    inner_loop_cond_1->PopulateConstantPlaceholder<uint64_t>(0, 40);
    inner_loop_cond_1->PopulateConstantPlaceholder<uint64_t>(2, 32);
    outer_loop_stmt_2->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_cond_1);

    // pr[j] <= lp[i]
    //
    FastInterpBoilerplateInstance* inner_loop_cond_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonFavourTrueBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOperandShapeCategory::VARPTR_VAR,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL));
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
                    FIOpaqueParamsHelper::GetMaxOFP()));
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
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstArithmeticExprType::MUL));
    ixprj->PopulateConstantPlaceholder<uint64_t>(1, 24);
    ixprj->PopulateConstantPlaceholder<uint64_t>(2, 40);
    value_of_i->PopulateBoilerplateFnPtrPlaceholder(0, ixprj);

    // i * pr[j] <= n
    //
    FastInterpBoilerplateInstance* inner_loop_cond_3 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlinedComparisonFavourTrueBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    false /*isInlinedSideLhs*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL));
    inner_loop_cond_3->PopulateConstantPlaceholder<uint64_t>(0, 8);
    ixprj->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_cond_3);

    // 'i'
    //
    FastInterpBoilerplateInstance* value_of_i_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIDerefVariableImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP()));
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
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstArithmeticExprType::MUL));
    ixprj_2->PopulateConstantPlaceholder<uint64_t>(1, 24);
    ixprj_2->PopulateConstantPlaceholder<uint64_t>(2, 40);
    value_of_i_2->PopulateBoilerplateFnPtrPlaceholder(0, ixprj_2);

    // lp + i * pr[j]
    //
    FastInterpBoilerplateInstance* inner_loop_stmt_1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineLhsPointerArithmeticImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::ADD,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    static_cast<FIPowerOfTwoObjectSize>(2) /*int is 2^2 bytes*/));
    inner_loop_stmt_1->PopulateConstantPlaceholder<uint64_t>(1, 16);
    ixprj_2->PopulateBoilerplateFnPtrPlaceholder(0, inner_loop_stmt_1);

    // *... = pr[j]
    //
    FastInterpBoilerplateInstance* inner_loop_stmt_2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIPartialInlineRhsAssignImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARPTR_VAR,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    inner_loop_stmt_2->PopulateConstantPlaceholder<uint64_t>(0, 24);
    inner_loop_stmt_2->PopulateConstantPlaceholder<uint64_t>(1, 40);
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

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<void>().GetDefaultFastInterpTypeId(), true /*isNoExcept*/, 233, main_stmt1);
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
                FastInterpBoilerplateLibrary<FIFullyInlinedComparisonFavourTrueBranchImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<int>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    TypeId::Get<int32_t>().GetDefaultFastInterpTypeId(),
                    FIOperandShapeCategory::VARIABLE,
                    FIOperandShapeCategory::LITERAL_NONZERO,
                    FIOpaqueParamsHelper::GetMaxOIP(),
                    FIOpaqueParamsHelper::GetMaxOFP(),
                    AstComparisonExprType::LESS_EQUAL), 4);
    fib_fn->PopulateConstantPlaceholder<uint64_t>(0, 8);
    fib_fn->PopulateConstantPlaceholder<int>(2, 2);

    FastInterpBoilerplateInstance* lit_1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FILiteralMcMediumImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetOneLevelPtrFastInterpTypeId(),
                    false /*isAllBitsZero*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    FIOpaqueParamsHelper::GetMaxOFP()));
    lit_1->PopulateConstantPlaceholder<uint64_t>(1, 1);
    fib_fn->PopulateBoilerplateFnPtrPlaceholder(0, lit_1);

    FastInterpBoilerplateInstance* ret_node1 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetOneLevelPtrFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    lit_1->PopulateBoilerplateFnPtrPlaceholder(0, ret_node1);

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
                    static_cast<FICallExprParamOrd>(0),
                    false /*hasMore*/));
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
                    static_cast<FICallExprParamOrd>(0),
                    false /*hasMore*/));
    populate_rhs_param->PopulateConstantPlaceholder<uint64_t>(0, 8);
    populate_rhs_param->PopulateBoilerplateFnPtrPlaceholder(0, fib_fn);
    compute_rhs_param->PopulateBoilerplateFnPtrPlaceholder(0, populate_rhs_param);

    FastInterpBoilerplateInstance* compute_sum = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedArithmeticExprImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    AstArithmeticExprType::ADD,
                    false /*isLhsQAP*/,
                    false /*spillOutput*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    compute_sum->PopulateConstantPlaceholder<uint64_t>(1, 16);
    compute_rhs->PopulateBoilerplateFnPtrPlaceholder(0, compute_sum);

    FastInterpBoilerplateInstance* ret_node2 = engine.InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                    TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(),
                    true /*isNoExcept*/,
                    false /*exceptionThrown*/,
                    static_cast<FINumOpaqueIntegralParams>(0),
                    static_cast<FINumOpaqueFloatingParams>(0)));
    compute_sum->PopulateBoilerplateFnPtrPlaceholder(0, ret_node2);

    engine.TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId::Get<uint64_t>().GetDefaultFastInterpTypeId(), true /*isNoExcept*/, 233, fib_fn);
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
