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
void CheckTpchQueryCorrectness(const std::string& expectedResult, bool checkDebugInterp = true)
{
    buildQueryFn();

    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    using FnPrototype = void(*)(SqlResultPrinter*);
    if (checkDebugInterp)
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

#if 0
    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string &dump = rso.str();

    AssertIsExpectedOutput(dump);
#endif
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

    auto aggregation_row_reader = new AggregationRowGenerator();
    aggregation_row_reader->m_src = aggregator_container;
    aggregation_row_reader->m_dst = aggregation_row;

    auto aggregation_field_0_result = aggregation_row->GetSqlProjectionField(0);

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(aggregation_field_0_result);

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = aggregation_row_reader;
    stage2->m_outputter = outputter;

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

namespace
{

void BuildTpchQuery1()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto tableRow = x_tpchtable_lineitem.GetTableRow();
    auto where_clause_lhs = tableRow->GetSqlField(&TpchLineItemTableRow::l_shipdate);
    auto where_clause_rhs = new SqlLiteral(static_cast<uint32_t>(904719600));
    auto where_cond = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL, where_clause_lhs, where_clause_rhs);

    auto table_src = new SqlTableContainer("lineitem");

    auto generator = new SqlTableRowGenerator();
    generator->m_src = table_src;
    generator->m_dst = tableRow;

    auto row_filter_processor = new SqlFilterProcessor(where_cond);

    auto groupby_field1 = tableRow->GetSqlField(&TpchLineItemTableRow::l_returnflag);
    auto groupby_field2 = tableRow->GetSqlField(&TpchLineItemTableRow::l_linestatus);

    auto groupby_container = new HashTableContainer();
    groupby_container->m_row = tableRow;
    groupby_container->m_groupByFields.push_back(groupby_field1);
    groupby_container->m_groupByFields.push_back(groupby_field2);

    auto aggregation_row_init_field0 = tableRow->GetSqlField(&TpchLineItemTableRow::l_returnflag);
    auto aggregation_row_init_field1 = tableRow->GetSqlField(&TpchLineItemTableRow::l_linestatus);
    auto aggregation_row_init_field2 = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_init_field3 = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_init_field4 = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_init_field5 = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_init_field6 = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_init_field7 = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_init_field8 = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_init_field9 = new SqlLiteral(static_cast<int64_t>(0));

    auto aggregation_row = new SqlAggregationRow({
        aggregation_row_init_field0,
        aggregation_row_init_field1,
        aggregation_row_init_field2,
        aggregation_row_init_field3,
        aggregation_row_init_field4,
        aggregation_row_init_field5,
        aggregation_row_init_field6,
        aggregation_row_init_field7,
        aggregation_row_init_field8,
        aggregation_row_init_field9
    });

    auto aggregation_row_update_field2 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(2),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_quantity));
    aggregation_row->SetUpdateExpr(2, aggregation_row_update_field2);

    auto aggregation_row_update_field3 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(3),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice));
    aggregation_row->SetUpdateExpr(3, aggregation_row_update_field3);

    auto update_field4_expr1 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::SUB,
                new SqlLiteral(static_cast<double>(1)),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_discount));
    auto update_field4_expr2 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                tableRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                update_field4_expr1);
    auto aggregation_row_update_field4 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(4),
                update_field4_expr2);
    aggregation_row->SetUpdateExpr(4, aggregation_row_update_field4);

    auto update_field5_expr1 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::SUB,
                new SqlLiteral(static_cast<double>(1)),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_discount));
    auto update_field5_expr2 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                tableRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                update_field5_expr1);
    auto update_field5_expr3 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                new SqlLiteral(static_cast<double>(1)),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_tax));
    auto update_field5_expr4 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                update_field5_expr2,
                update_field5_expr3);
    auto aggregation_row_update_field5 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(5),
                update_field5_expr4);
    aggregation_row->SetUpdateExpr(5, aggregation_row_update_field5);

    auto aggregation_row_update_field6 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(6),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_quantity));
    aggregation_row->SetUpdateExpr(6, aggregation_row_update_field6);

    auto aggregation_row_update_field7 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(7),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice));
    aggregation_row->SetUpdateExpr(7, aggregation_row_update_field7);

    auto aggregation_row_update_field8 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(8),
                tableRow->GetSqlField(&TpchLineItemTableRow::l_discount));
    aggregation_row->SetUpdateExpr(8, aggregation_row_update_field8);

    auto aggregation_row_update_field9 = new SqlArithmeticOperator(
                TypeId::Get<int64_t>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(9),
                new SqlLiteral(static_cast<int64_t>(1)));
    aggregation_row->SetUpdateExpr(9, aggregation_row_update_field9);

    auto groupby_outputter = new GroupByHashTableOutputter();
    groupby_outputter->m_container = groupby_container;
    groupby_outputter->m_inputRow = tableRow;
    groupby_outputter->m_aggregationRow = aggregation_row;

    auto stage1 = new QueryPlanPipelineStage();
    stage1->m_generator = generator;
    stage1->m_processor.push_back(row_filter_processor);
    stage1->m_outputter = groupby_outputter;

    auto groupby_result_reader = new GroupByHashTableGenerator();
    groupby_result_reader->m_container = groupby_container;
    groupby_result_reader->m_dst = aggregation_row;

    auto projection_row_field0 = aggregation_row->GetSqlProjectionField(0);
    auto projection_row_field1 = aggregation_row->GetSqlProjectionField(1);
    auto projection_row_field2 = aggregation_row->GetSqlProjectionField(2);
    auto projection_row_field3 = aggregation_row->GetSqlProjectionField(3);
    auto projection_row_field4 = aggregation_row->GetSqlProjectionField(4);
    auto projection_row_field5 = aggregation_row->GetSqlProjectionField(5);
    auto projection_row_field6 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::DIV,
                aggregation_row->GetSqlProjectionField(6),
                new SqlCastOperator(TypeId::Get<double>(), aggregation_row->GetSqlProjectionField(9)));
    auto projection_row_field7 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::DIV,
                aggregation_row->GetSqlProjectionField(7),
                new SqlCastOperator(TypeId::Get<double>(), aggregation_row->GetSqlProjectionField(9)));
    auto projection_row_field8 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::DIV,
                aggregation_row->GetSqlProjectionField(8),
                new SqlCastOperator(TypeId::Get<double>(), aggregation_row->GetSqlProjectionField(9)));
    auto projection_row_field9 = aggregation_row->GetSqlProjectionField(9);

    auto projection_row = new SqlProjectionRow({
        projection_row_field0,
        projection_row_field1,
        projection_row_field2,
        projection_row_field3,
        projection_row_field4,
        projection_row_field5,
        projection_row_field6,
        projection_row_field7,
        projection_row_field8,
        projection_row_field9
    });

    auto projection_row_processor = new SqlProjectionProcessor();
    projection_row_processor->m_output = projection_row;

    auto temptable_container = new TempTableContainer();

    auto temptable_outputter = new TempTableRowOutputter();
    temptable_outputter->m_src = projection_row;
    temptable_outputter->m_dst = temptable_container;

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = groupby_result_reader;
    stage2->m_processor.push_back(projection_row_processor);
    stage2->m_outputter = temptable_outputter;

    auto stage3 = new QueryPlanOrderByStage();
    stage3->m_row = projection_row;
    stage3->m_container = temptable_container;
    stage3->m_orderByFields.push_back(projection_row->GetSqlProjectionField(0));
    stage3->m_orderByFields.push_back(projection_row->GetSqlProjectionField(1));

    auto temptable_reader = new TempTableRowGenerator();
    temptable_reader->m_dst = projection_row;
    temptable_reader->m_container = temptable_container;

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(0));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(1));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(2));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(3));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(4));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(5));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(6));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(7));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(8));
    outputter->m_projections.push_back(projection_row->GetSqlProjectionField(9));

    auto stage4 = new QueryPlanPipelineStage();
    stage4->m_generator = temptable_reader;
    stage4->m_outputter = outputter;

    auto plan = new QueryPlan();
    plan->m_neededContainers.push_back(table_src);
    plan->m_neededContainers.push_back(groupby_container);
    plan->m_neededContainers.push_back(temptable_container);
    plan->m_stages.push_back(stage1);
    plan->m_stages.push_back(stage2);
    plan->m_stages.push_back(stage3);
    plan->m_stages.push_back(stage4);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery1_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult =
        "| A | F | 11799894.000000 | 17694594440.489922 | 16809747534.070972 | 17481727589.532120 | 25.510857 | 38254.943185 | 0.050024 | 462544 |\n"
        "| N | F | 307146.000000 | 461415894.149998 | 438370222.578399 | 456021204.722030 | 25.557164 | 38393.733912 | 0.050182 | 12018 |\n"
        "| N | O | 23232475.000000 | 34817796765.830833 | 33077127289.602264 | 34400024394.849609 | 25.529154 | 38259.759511 | 0.049991 | 910037 |\n"
        "| R | F | 11788252.000000 | 17672163035.450062 | 16787827349.368053 | 17459618619.351471 | 25.495168 | 38220.660065 | 0.050049 | 462372 |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery1>(expectedResult, false /*checkDebugInterp*/);
}

TEST(PaperBenchmark, TpchQuery1)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 1 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery1>();
}

namespace
{

void BuildTpchQuery12()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto ordersRow = x_tpchtable_orders.GetTableRow();
    auto joinkey1 = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderkey);

    auto hashtable = new HashTableContainer();
    hashtable->m_row = ordersRow;
    hashtable->m_groupByFields.push_back(joinkey1);

    auto orders_table_container = new SqlTableContainer("orders");

    auto orders_table_reader = new SqlTableRowGenerator();
    orders_table_reader->m_src = orders_table_container;
    orders_table_reader->m_dst = ordersRow;

    auto hash_join_outputter = new HashJoinHashTableOutputter();
    hash_join_outputter->m_inputRow = ordersRow;
    hash_join_outputter->m_container = hashtable;

    auto stage1 = new QueryPlanPipelineStage();
    stage1->m_generator = orders_table_reader;
    stage1->m_outputter = hash_join_outputter;

    auto lineitemRow = x_tpchtable_lineitem.GetTableRow();
    auto where_clause_1_1 = new SqlCompareWithStringLiteralOperator(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipmode),
                                                                    "MAIL",
                                                                    SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_clause_1_2 = new SqlCompareWithStringLiteralOperator(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipmode),
                                                                    "SHIP",
                                                                    SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_clause_1 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_clause_1_1, where_clause_1_2);
    auto where_clause_2 = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN,
                                                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_commitdate),
                                                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_receiptdate));
    auto where_clause_3 = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN,
                                                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipdate),
                                                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_commitdate));
    auto where_clause_4 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_receiptdate),
                                                    new SqlLiteral(static_cast<uint32_t>(757411200)));
    auto where_clause_5 = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN,
                                                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_receiptdate),
                                                    new SqlLiteral(static_cast<uint32_t>(788947200)));

    auto where_part_2 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_clause_1, where_clause_2);
    auto where_part_3 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_part_2, where_clause_3);
    auto where_part_4 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_part_3, where_clause_4);
    auto where_part_5 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_part_4, where_clause_5);

    auto filter_processor = new SqlFilterProcessor(where_part_5);

    auto lineitem_table_container = new SqlTableContainer("lineitem");

    auto lineitem_table_reader = new SqlTableRowGenerator();
    lineitem_table_reader->m_src = lineitem_table_container;
    lineitem_table_reader->m_dst = lineitemRow;

    auto join_processor = new TableHashJoinProcessor();
    join_processor->m_container = hashtable;
    join_processor->m_inputRowJoinFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_orderkey));

    auto aggregation_row_init_field0 = lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipmode);
    auto aggregation_row_init_field1 = new SqlLiteral(static_cast<int64_t>(0));
    auto aggregation_row_init_field2 = new SqlLiteral(static_cast<int64_t>(0));

    auto aggregation_row = new SqlAggregationRow({ aggregation_row_init_field0,
                                                   aggregation_row_init_field1,
                                                   aggregation_row_init_field2
                                                 });

    auto field1_cmp_1 = new SqlCompareWithStringLiteralOperator(ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderpriority),
                                                                "1-URGENT",
                                                                SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto field1_cmp_2 = new SqlCompareWithStringLiteralOperator(ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderpriority),
                                                                "2-HIGH",
                                                                SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto field1_cmp_result = new SqlBinaryLogicalOperator(false /*isAnd*/, field1_cmp_1, field1_cmp_2);
    auto aggregation_row_update_field1 = new SqlArithmeticOperator(
                TypeId::Get<int64_t>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(1),
                new SqlCastOperator(TypeId::Get<int64_t>(), field1_cmp_result));
    aggregation_row->SetUpdateExpr(1, aggregation_row_update_field1);

    auto field2_cmp_1 = new SqlCompareWithStringLiteralOperator(ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderpriority),
                                                                "1-URGENT",
                                                                SqlCompareWithStringLiteralOperator::CompareMode::NOT_EQUAL);
    auto field2_cmp_2 = new SqlCompareWithStringLiteralOperator(ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderpriority),
                                                                "2-HIGH",
                                                                SqlCompareWithStringLiteralOperator::CompareMode::NOT_EQUAL);
    auto field2_cmp_result = new SqlBinaryLogicalOperator(true /*isAnd*/, field2_cmp_1, field2_cmp_2);
    auto aggregation_row_update_field2 = new SqlArithmeticOperator(
                TypeId::Get<int64_t>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(2),
                new SqlCastOperator(TypeId::Get<int64_t>(), field2_cmp_result));
    aggregation_row->SetUpdateExpr(2, aggregation_row_update_field2);

    auto groupby_container = new HashTableContainer();
    groupby_container->m_row = lineitemRow;
    groupby_container->m_groupByFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipmode));

    auto groupby_outputter = new GroupByHashTableOutputter();
    groupby_outputter->m_container = groupby_container;
    groupby_outputter->m_inputRow = lineitemRow;
    groupby_outputter->m_aggregationRow = aggregation_row;

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = lineitem_table_reader;
    stage2->m_processor.push_back(filter_processor);
    stage2->m_processor.push_back(join_processor);
    stage2->m_outputter = groupby_outputter;

    auto groupby_result_reader = new GroupByHashTableGenerator();
    groupby_result_reader->m_container = groupby_container;
    groupby_result_reader->m_dst = aggregation_row;

    auto temptable_container = new TempTableContainer();

    auto temptable_outputter = new TempTableRowOutputter();
    temptable_outputter->m_src = aggregation_row;
    temptable_outputter->m_dst = temptable_container;

    auto stage3 = new QueryPlanPipelineStage();
    stage3->m_generator = groupby_result_reader;
    stage3->m_outputter = temptable_outputter;

    auto stage4 = new QueryPlanOrderByStage();
    stage4->m_row = aggregation_row;
    stage4->m_container = temptable_container;
    stage4->m_orderByFields.push_back(aggregation_row->GetSqlProjectionField(0));

    auto temptable_reader = new TempTableRowGenerator();
    temptable_reader->m_dst = aggregation_row;
    temptable_reader->m_container = temptable_container;

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(0));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(1));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(2));

    auto stage5 = new QueryPlanPipelineStage();
    stage5->m_generator = temptable_reader;
    stage5->m_outputter = outputter;

    auto plan = new QueryPlan();
    plan->m_neededContainers.push_back(orders_table_container);
    plan->m_neededContainers.push_back(lineitem_table_container);
    plan->m_neededContainers.push_back(hashtable);
    plan->m_neededContainers.push_back(groupby_container);
    plan->m_neededContainers.push_back(temptable_container);
    plan->m_stages.push_back(stage1);
    plan->m_stages.push_back(stage2);
    plan->m_stages.push_back(stage3);
    plan->m_stages.push_back(stage4);
    plan->m_stages.push_back(stage5);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery12_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult =
        "| MAIL | 1917 | 2942 |\n"
        "| SHIP | 1948 | 2890 |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery12>(expectedResult, false /*checkDebugInterp*/);
}

TEST(PaperBenchmark, TpchQuery12)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 12 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery12>();
}
