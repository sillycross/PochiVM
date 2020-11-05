#include "gtest/gtest.h"

#include "mini_db_backend/tpch_ddl.h"
#include "mini_db_backend/tpch_db_loader.h"
#include "mini_db_backend/query_plan_ast.hpp"

#include "test_util_helper.h"

using namespace MiniDbBackend;
using namespace PochiVM;

TEST(MiniDbBackendUnitTest, LoadDatabase)
{
    TpchLoadDatabase();
    ReleaseAssert(x_tpchtable_customer.m_data.size() == 46611);
    ReleaseAssert(x_tpchtable_lineitem.m_data.size() == 1873428);
    ReleaseAssert(x_tpchtable_nation.m_data.size() == 1);
    ReleaseAssert(x_tpchtable_orders.m_data.size() == 468418);
    ReleaseAssert(x_tpchtable_part.m_data.size() == 61884);
    ReleaseAssert(x_tpchtable_partsupp.m_data.size() == 247536);
    ReleaseAssert(x_tpchtable_region.m_data.size() == 1);
    ReleaseAssert(x_tpchtable_supplier.m_data.size() == 3170);
}

TEST(MiniDbBackendUnitTest, SimpleSelect)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    TpchLoadUnitTestDatabase();

    auto tableRow = x_unittesttable_table1.GetTableRow();
    auto sf1 = tableRow->GetSqlField(&TestTable1Row::a);
    auto sf2 = tableRow->GetSqlField(&TestTable1Row::b);
    auto sum = new SqlArithmeticOperator(TypeId::Get<int>(), AstArithmeticExprType::ADD, sf1, sf2);
    auto cmp_rhs = new SqlLiteral(static_cast<int>(8));
    auto cmp_result = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN, sum, cmp_rhs);

    auto table_src = new SqlTableContainer("testtable1");

    auto generator = new SqlTableRowGenerator();
    generator->m_src = table_src;
    generator->m_dst = tableRow;

    auto row_processor = new SqlFilterProcessor(cmp_result);
    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(tableRow->GetSqlField(&TestTable1Row::a));
    outputter->m_projections.push_back(tableRow->GetSqlField(&TestTable1Row::b));
    outputter->m_projections.push_back(tableRow->GetSqlField(&TestTable1Row::a));

    auto stage = new QueryPlanPipelineStage();
    stage->m_generator = generator;
    stage->m_processor.push_back(row_processor);
    stage->m_outputter = outputter;
    stage->m_neededRows.push_back(tableRow);

    auto plan = new QueryPlan();
    plan->m_stages.push_back(stage);
    plan->m_neededContainers.push_back(table_src);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    std::string expectedResult =
        "| 1 | 2 | 1 |\n"
        "| 3 | 4 | 3 |\n"
        "| 4 | 3 | 4 |\n"
        "| 2 | 1 | 2 |\n";

    using FnPrototype = void(*)(SqlResultPrinter*);
    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("execute_query");
        SqlResultPrinter result;
        interpFn(&result);
        ReleaseAssert(expectedResult == result.m_start);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("execute_query");
        SqlResultPrinter result;
        interpFn(&result);
        ReleaseAssert(expectedResult == result.m_start);
    }

    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);

    SimpleJIT jit;
    jit.SetAllowResolveSymbolInHostProcess(true);
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("execute_query");
        SqlResultPrinter result;
        jitFn(&result);
        ReleaseAssert(expectedResult == result.m_start);
    }
}

template<auto buildQueryFn>
void BenchmarkTpchQuery()
{
    const int numRuns = 10;
    AstModule* modules[numRuns * 4];
    for (int i = 0; i < numRuns * 4; i++)
    {
        buildQueryFn();
        modules[i] = thread_pochiVMContext->m_curModule;
    }

    double fastInterpCodegenTime = 1e100;
    for (int i = 0; i < numRuns * 2; i++)
    {
        thread_pochiVMContext->m_curModule = modules[i];
        double ts;
        {
            AutoTimer t(&ts);
            thread_pochiVMContext->m_curModule->PrepareForFastInterp();
        }
        fastInterpCodegenTime = std::min(fastInterpCodegenTime, ts);
        if (i == 0)
        {
            // By default when the next module is generated, we delete the previous module
            // Null it out to prevent it from being deleted.
            //
            thread_pochiVMContext->m_fastInterpGeneratedProgram = nullptr;
        }
    }

    using FnPrototype = void(*)(SqlResultPrinter*);

    double llvmCodegenTime[4] = { 1e100, 1e100, 1e100, 1e100 };
    for (int optLevel = 0; optLevel <= 3; optLevel++)
    {
        for (int i = 0; i < numRuns; i++)
        {
            thread_pochiVMContext->m_curModule = modules[optLevel * numRuns + i];
            TestJitHelper* jit;
            double ts;
            {
                AutoTimer t(&ts);
                jit = new TestJitHelper();
                thread_pochiVMContext->m_curModule->EmitIR();
                jit->Init(optLevel);
                FnPrototype jitFn = jit->GetFunction<FnPrototype>("execute_query");
                std::ignore = jitFn;
            }
            llvmCodegenTime[optLevel] = std::min(llvmCodegenTime[optLevel], ts);
        }
    }

    double fastInterpPerformance = 1e100;
    thread_pochiVMContext->m_curModule = modules[0];
    for (int i = 0; i < numRuns; i++)
    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                GetFastInterpGeneratedFunction<FnPrototype>("execute_query");
        double ts;
        SqlResultPrinter printer;
        {
            AutoTimer t(&ts);
            interpFn(&printer);
        }
        fastInterpPerformance = std::min(fastInterpPerformance, ts);
    }

    double llvmPerformance[4] = { 1e100, 1e100, 1e100, 1e100 };
    for (int optLevel = 0; optLevel <= 3; optLevel++)
    {
        buildQueryFn();

        TestJitHelper jit;
        thread_pochiVMContext->m_curModule->EmitIR();
        jit.Init(optLevel);
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("execute_query");
        std::ignore = jitFn;

        for (int i = 0; i < numRuns; i++)
        {
            double ts;
            SqlResultPrinter printer;
            {
                AutoTimer t(&ts);
                jitFn(&printer);
            }
            llvmPerformance[optLevel] = std::min(llvmPerformance[optLevel], ts);
        }
    }

    printf("==============================\n");
    printf("   Codegen Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpCodegenTime);
    printf("LLVM -O0:   %.7lf\n", llvmCodegenTime[0]);
    printf("LLVM -O1:   %.7lf\n", llvmCodegenTime[1]);
    printf("LLVM -O2:   %.7lf\n", llvmCodegenTime[2]);
    printf("LLVM -O3:   %.7lf\n", llvmCodegenTime[3]);
    printf("==============================\n\n");

    printf("==============================\n");
    printf("  Execution Time Comparison\n");
    printf("------------------------------\n");
    printf("FastInterp: %.7lf\n", fastInterpPerformance);
    printf("LLVM -O0:   %.7lf\n", llvmPerformance[0]);
    printf("LLVM -O1:   %.7lf\n", llvmPerformance[1]);
    printf("LLVM -O2:   %.7lf\n", llvmPerformance[2]);
    printf("LLVM -O3:   %.7lf\n", llvmPerformance[3]);
    printf("==============================\n");
}

template<auto buildQueryFn>
void CheckTpchQueryCorrectness(const std::string& expectedResult)
{
    buildQueryFn();

    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    using FnPrototype = void(*)(SqlResultPrinter*);
    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("execute_query");
        SqlResultPrinter result;
        interpFn(&result);
        ReleaseAssert(expectedResult == result.m_start);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("execute_query");
        SqlResultPrinter result;
        interpFn(&result);
        ReleaseAssert(expectedResult == result.m_start);
    }

    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode(2 /*optLevel*/);

    SimpleJIT jit;
    jit.SetAllowResolveSymbolInHostProcess(true);
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("execute_query");
        SqlResultPrinter result;
        jitFn(&result);
        ReleaseAssert(expectedResult == result.m_start);
    }
}

namespace
{

void BuildTpchQuery6()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto tableRow = x_tpchtable_lineitem.GetTableRow();
    auto where_clause1_lhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_shipdate);
    auto where_clause1_rhs = new SqlLiteral(static_cast<uint32_t>(757411200));
    auto where_clause1_result = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL, where_clause1_lhs, where_clause1_rhs);

    auto where_clause2_lhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_shipdate);
    auto where_clause2_rhs = new SqlLiteral(static_cast<uint32_t>(788947200));
    auto where_clause2_result = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN, where_clause2_lhs, where_clause2_rhs);

    auto where_clause3_lhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_discount);
    auto where_clause3_rhs = new SqlLiteral(static_cast<double>(0.05));
    auto where_clause3_result = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL, where_clause3_lhs, where_clause3_rhs);

    auto where_clause4_lhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_discount);
    auto where_clause4_rhs = new SqlLiteral(static_cast<double>(0.07));
    auto where_clause4_result = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL, where_clause4_lhs, where_clause4_rhs);

    auto where_clause5_lhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_quantity);
    auto where_clause5_rhs = new SqlLiteral(static_cast<double>(24));
    auto where_clause5_result = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN, where_clause5_lhs, where_clause5_rhs);

    auto where_and_1 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_clause1_result, where_clause2_result);
    auto where_and_2 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_and_1, where_clause3_result);
    auto where_and_3 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_and_2, where_clause4_result);
    auto where_result = new SqlBinaryLogicalOperator(true /*isAnd*/, where_and_3, where_clause5_result);

    auto table_src = new SqlTableContainer("lineitem");

    auto aggregation_field_0_init = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row = new SqlAggregationRow({ aggregation_field_0_init });
    auto aggregator_container = new AggregatedRowContainer(aggregation_row);

    auto projection_mul_lhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice);
    auto projection_mul_rhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_discount);
    auto projection_sum_term = new SqlArithmeticOperator(TypeId::Get<double>(), AstArithmeticExprType::MUL, projection_mul_lhs, projection_mul_rhs);

    auto aggregator_update_field_0 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(0),
                projection_sum_term);
    aggregation_row->SetUpdateExpr(0, aggregator_update_field_0);

    auto generator = new SqlTableRowGenerator();
    generator->m_src = table_src;
    generator->m_dst = tableRow;

    auto row_filter_processor = new SqlFilterProcessor(where_result);

    auto aggregation_outputter = new AggregationOutputter();
    aggregation_outputter->m_row = aggregation_row;
    aggregation_outputter->m_container = aggregator_container;

    auto stage = new QueryPlanPipelineStage();
    stage->m_generator = generator;
    stage->m_processor.push_back(row_filter_processor);
    stage->m_outputter = aggregation_outputter;
    stage->m_neededRows.push_back(tableRow);
    stage->m_neededRows.push_back(aggregation_row);

    auto aggregation_row_reader = new AggregationRowGenerator();
    aggregation_row_reader->m_src = aggregator_container;
    aggregation_row_reader->m_dst = aggregation_row;

    auto aggregation_field_0_result = aggregation_row->GetSqlProjectionField(0);

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(aggregation_field_0_result);

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = aggregation_row_reader;
    stage2->m_outputter = outputter;
    stage2->m_neededRows.push_back(aggregation_row);

    auto plan = new QueryPlan();
    plan->m_stages.push_back(stage);
    plan->m_stages.push_back(stage2);
    plan->m_neededContainers.push_back(table_src);
    plan->m_neededContainers.push_back(aggregator_container);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
};

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery6_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult =
        "| 38212505.914700 |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery6>(expectedResult);
}

TEST(PaperBenchmark, TpchQuery6)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 6 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery6>();
}
