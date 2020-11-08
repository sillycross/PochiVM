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
    stage3->m_orderByFields.push_back(std::make_pair(projection_row->GetSqlProjectionField(0), true /*isAscend*/));
    stage3->m_orderByFields.push_back(std::make_pair(projection_row->GetSqlProjectionField(1), true /*isAscent*/));

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
                new SqlCastOperator(TypeId::Get<int64_t>(), field1_cmp_result),
                aggregation_row->GetSqlProjectionField(1));
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
                new SqlCastOperator(TypeId::Get<int64_t>(), field2_cmp_result),
                aggregation_row->GetSqlProjectionField(2));
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
    stage4->m_orderByFields.push_back(std::make_pair(aggregation_row->GetSqlProjectionField(0), true /*isAscend*/));

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

namespace
{

void BuildTpchQuery19()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto partRow = x_tpchtable_part.GetTableRow();
    auto joinkey1 = partRow->GetSqlField(&TpchPartTableRow::p_partkey);

    auto hashtable = new HashTableContainer();
    hashtable->m_row = partRow;
    hashtable->m_groupByFields.push_back(joinkey1);

    auto part_table_container = new SqlTableContainer("part");

    auto part_table_reader = new SqlTableRowGenerator();
    part_table_reader->m_src = part_table_container;
    part_table_reader->m_dst = partRow;

    auto hash_join_outputter = new HashJoinHashTableOutputter();
    hash_join_outputter->m_inputRow = partRow;
    hash_join_outputter->m_container = hashtable;

    auto stage1 = new QueryPlanPipelineStage();
    stage1->m_generator = part_table_reader;
    stage1->m_outputter = hash_join_outputter;

    auto lineitemRow = x_tpchtable_lineitem.GetTableRow();
    auto lineitem_table_container = new SqlTableContainer("lineitem");

    auto lineitem_table_reader = new SqlTableRowGenerator();
    lineitem_table_reader->m_src = lineitem_table_container;
    lineitem_table_reader->m_dst = lineitemRow;

    auto join_processor = new TableHashJoinProcessor();
    join_processor->m_container = hashtable;
    join_processor->m_inputRowJoinFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_partkey));

    auto where_0_0_1 = new SqlCompareWithStringLiteralOperator(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipmode),
                                                               "AIR",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_0_0_2 = new SqlCompareWithStringLiteralOperator(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipmode),
                                                               "AIR REG",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_0_1 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_0_0_1, where_0_0_2);
    auto where_0_2 = new SqlCompareWithStringLiteralOperator(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipinstruct),
                                                            "DELIVER IN PERSON",
                                                            SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);

    auto where_1_1 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_brand),
                                                             "Brand#12",
                                                             SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_1_2_1 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "SM CASE",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_1_2_2 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "SM BOX",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_1_2_3 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "SM PACK",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_1_2_4 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "SM PKG",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_1_2_p2 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_1_2_1, where_1_2_2);
    auto where_1_2_p3 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_1_2_p2, where_1_2_3);
    auto where_1_2 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_1_2_p3, where_1_2_4);
    auto where_1_3 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                               lineitemRow->GetSqlField(&TpchLineItemTableRow::l_quantity),
                                               new SqlLiteral(static_cast<double>(1)));
    auto where_1_4 = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL,
                                               lineitemRow->GetSqlField(&TpchLineItemTableRow::l_quantity),
                                               new SqlLiteral(static_cast<double>(11)));
    auto where_1_5 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                               partRow->GetSqlField(&TpchPartTableRow::p_size),
                                               new SqlLiteral(static_cast<int>(1)));
    auto where_1_6 = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL,
                                               partRow->GetSqlField(&TpchPartTableRow::p_size),
                                               new SqlLiteral(static_cast<int>(5)));

    auto where_1_p2 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_1_1, where_1_2);
    auto where_1_p3 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_1_p2, where_1_3);
    auto where_1_p4 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_1_p3, where_1_4);
    auto where_1_p5 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_1_p4, where_1_5);
    auto where_1 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_1_p5, where_1_6);

    auto where_2_1 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_brand),
                                                             "Brand#23",
                                                             SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_2_2_1 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "MED BAG",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_2_2_2 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "MED BOX",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_2_2_3 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "MED PKG",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_2_2_4 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "MED PACK",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_2_2_p2 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_2_2_1, where_2_2_2);
    auto where_2_2_p3 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_2_2_p2, where_2_2_3);
    auto where_2_2 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_2_2_p3, where_2_2_4);
    auto where_2_3 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                               lineitemRow->GetSqlField(&TpchLineItemTableRow::l_quantity),
                                               new SqlLiteral(static_cast<double>(10)));
    auto where_2_4 = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL,
                                               lineitemRow->GetSqlField(&TpchLineItemTableRow::l_quantity),
                                               new SqlLiteral(static_cast<double>(20)));
    auto where_2_5 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                               partRow->GetSqlField(&TpchPartTableRow::p_size),
                                               new SqlLiteral(static_cast<int>(1)));
    auto where_2_6 = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL,
                                               partRow->GetSqlField(&TpchPartTableRow::p_size),
                                               new SqlLiteral(static_cast<int>(10)));

    auto where_2_p2 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_2_1, where_2_2);
    auto where_2_p3 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_2_p2, where_2_3);
    auto where_2_p4 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_2_p3, where_2_4);
    auto where_2_p5 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_2_p4, where_2_5);
    auto where_2 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_2_p5, where_2_6);

    auto where_3_1 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_brand),
                                                             "Brand#34",
                                                             SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_3_2_1 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "LG CASE",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_3_2_2 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "LG BOX",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_3_2_3 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "LG PACK",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_3_2_4 = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_container),
                                                               "LG PKG",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto where_3_2_p2 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_3_2_1, where_3_2_2);
    auto where_3_2_p3 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_3_2_p2, where_3_2_3);
    auto where_3_2 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_3_2_p3, where_3_2_4);
    auto where_3_3 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                               lineitemRow->GetSqlField(&TpchLineItemTableRow::l_quantity),
                                               new SqlLiteral(static_cast<double>(20)));
    auto where_3_4 = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL,
                                               lineitemRow->GetSqlField(&TpchLineItemTableRow::l_quantity),
                                               new SqlLiteral(static_cast<double>(30)));
    auto where_3_5 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                               partRow->GetSqlField(&TpchPartTableRow::p_size),
                                               new SqlLiteral(static_cast<int>(1)));
    auto where_3_6 = new SqlComparisonOperator(AstComparisonExprType::LESS_EQUAL,
                                               partRow->GetSqlField(&TpchPartTableRow::p_size),
                                               new SqlLiteral(static_cast<int>(15)));

    auto where_3_p2 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_3_1, where_3_2);
    auto where_3_p3 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_3_p2, where_3_3);
    auto where_3_p4 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_3_p3, where_3_4);
    auto where_3_p5 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_3_p4, where_3_5);
    auto where_3 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_3_p5, where_3_6);

    auto where_0_p2 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_1, where_2);
    auto where_0_p3 = new SqlBinaryLogicalOperator(false /*isAnd*/, where_0_p2, where_3);

    auto where_p2 = new SqlBinaryLogicalOperator(true /*isAnd*/, where_0_1, where_0_2);
    auto where_clause = new SqlBinaryLogicalOperator(true /*isAnd*/, where_p2, where_0_p3);

    auto filter_processor = new SqlFilterProcessor(where_clause);

    auto aggregation_row_init_field0 = new SqlLiteral(static_cast<double>(0));

    auto aggregation_row = new SqlAggregationRow({ aggregation_row_init_field0 });

    auto sum_expr_1 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::SUB,
                new SqlLiteral(static_cast<double>(1)),
                lineitemRow->GetSqlField(&TpchLineItemTableRow::l_discount));
    auto sum_expr = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                lineitemRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                sum_expr_1);
    auto update_field0_expr = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                sum_expr,
                aggregation_row->GetSqlProjectionField(0));
    aggregation_row->SetUpdateExpr(0, update_field0_expr);

    auto aggregator_container = new AggregatedRowContainer(aggregation_row);
    auto aggregation_outputter = new AggregationOutputter();
    aggregation_outputter->m_row = aggregation_row;
    aggregation_outputter->m_container = aggregator_container;

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = lineitem_table_reader;
    stage2->m_processor.push_back(join_processor);
    stage2->m_processor.push_back(filter_processor);
    stage2->m_outputter = aggregation_outputter;

    auto aggregation_row_reader = new AggregationRowGenerator();
    aggregation_row_reader->m_src = aggregator_container;
    aggregation_row_reader->m_dst = aggregation_row;

    auto aggregation_field_0_result = aggregation_row->GetSqlProjectionField(0);

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(aggregation_field_0_result);

    auto stage3 = new QueryPlanPipelineStage();
    stage3->m_generator = aggregation_row_reader;
    stage3->m_outputter = outputter;

    auto plan = new QueryPlan();
    plan->m_neededContainers.push_back(part_table_container);
    plan->m_neededContainers.push_back(hashtable);
    plan->m_neededContainers.push_back(lineitem_table_container);
    plan->m_neededContainers.push_back(aggregator_container);
    plan->m_stages.push_back(stage1);
    plan->m_stages.push_back(stage2);
    plan->m_stages.push_back(stage3);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery19_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult = "| 18654.358500 |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery19>(expectedResult, false /*checkDebugInterp*/);
}

TEST(PaperBenchmark, TpchQuery19)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 19 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery19>();
}

namespace {

void BuildTpchQuery14()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto partRow = x_tpchtable_part.GetTableRow();
    auto joinkey1 = partRow->GetSqlField(&TpchPartTableRow::p_partkey);

    auto hashtable = new HashTableContainer();
    hashtable->m_row = partRow;
    hashtable->m_groupByFields.push_back(joinkey1);

    auto part_table_container = new SqlTableContainer("part");

    auto part_table_reader = new SqlTableRowGenerator();
    part_table_reader->m_src = part_table_container;
    part_table_reader->m_dst = partRow;

    auto hash_join_outputter = new HashJoinHashTableOutputter();
    hash_join_outputter->m_inputRow = partRow;
    hash_join_outputter->m_container = hashtable;

    auto stage1 = new QueryPlanPipelineStage();
    stage1->m_generator = part_table_reader;
    stage1->m_outputter = hash_join_outputter;

    auto lineitemRow = x_tpchtable_lineitem.GetTableRow();
    auto lineitem_table_container = new SqlTableContainer("lineitem");

    auto lineitem_table_reader = new SqlTableRowGenerator();
    lineitem_table_reader->m_src = lineitem_table_container;
    lineitem_table_reader->m_dst = lineitemRow;

    auto where_1 = new SqlComparisonOperator(AstComparisonExprType::GREATER_EQUAL,
                                             lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipdate),
                                             new SqlLiteral(static_cast<uint32_t>(809938800)));
    auto where_2 = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN,
                                             lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipdate),
                                             new SqlLiteral(static_cast<uint32_t>(812530800)));
    auto where_clause = new SqlBinaryLogicalOperator(true /*isAnd*/, where_1, where_2);

    auto filter_processor = new SqlFilterProcessor(where_clause);

    auto join_processor = new TableHashJoinProcessor();
    join_processor->m_container = hashtable;
    join_processor->m_inputRowJoinFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_partkey));

    auto aggregation_field0_init = new SqlLiteral(static_cast<double>(0));
    auto aggregation_field1_init = new SqlLiteral(static_cast<double>(0));

    auto aggregation_row = new SqlAggregationRow({ aggregation_field0_init,
                                                   aggregation_field1_init });

    auto sum_clause_cond = new SqlCompareWithStringLiteralOperator(partRow->GetSqlField(&TpchPartTableRow::p_type),
                                                                   "PROMO",
                                                                   SqlCompareWithStringLiteralOperator::CompareMode::START_WITH);
    auto sum_1_expr = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                lineitemRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                new SqlArithmeticOperator(
                    TypeId::Get<double>(),
                    AstArithmeticExprType::SUB,
                    new SqlLiteral(static_cast<double>(1)),
                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_discount)));

    auto sum_1_addend = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                new SqlCastOperator(TypeId::Get<double>(), sum_clause_cond),
                sum_1_expr);
    auto sum_1 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(0),
                sum_1_addend);
    aggregation_row->SetUpdateExpr(0, sum_1);

    auto sum_2_expr = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                lineitemRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                new SqlArithmeticOperator(
                    TypeId::Get<double>(),
                    AstArithmeticExprType::SUB,
                    new SqlLiteral(static_cast<double>(1)),
                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_discount)));
    auto sum_2 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                aggregation_row->GetSqlProjectionField(1),
                sum_2_expr);
    aggregation_row->SetUpdateExpr(1, sum_2);

    auto aggregator_container = new AggregatedRowContainer(aggregation_row);
    auto aggregation_outputter = new AggregationOutputter();
    aggregation_outputter->m_row = aggregation_row;
    aggregation_outputter->m_container = aggregator_container;

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = lineitem_table_reader;
    stage2->m_processor.push_back(filter_processor);
    stage2->m_processor.push_back(join_processor);
    stage2->m_outputter = aggregation_outputter;

    auto aggregation_row_reader = new AggregationRowGenerator();
    aggregation_row_reader->m_src = aggregator_container;
    aggregation_row_reader->m_dst = aggregation_row;

    auto result_0 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                new SqlLiteral(static_cast<double>(100)),
                aggregation_row->GetSqlProjectionField(0));
    auto result_1 = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::DIV,
                result_0,
                aggregation_row->GetSqlProjectionField(1));

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(result_1);

    auto stage3 = new QueryPlanPipelineStage();
    stage3->m_generator = aggregation_row_reader;
    stage3->m_outputter = outputter;

    auto plan = new QueryPlan();
    plan->m_neededContainers.push_back(part_table_container);
    plan->m_neededContainers.push_back(hashtable);
    plan->m_neededContainers.push_back(lineitem_table_container);
    plan->m_neededContainers.push_back(aggregator_container);
    plan->m_stages.push_back(stage1);
    plan->m_stages.push_back(stage2);
    plan->m_stages.push_back(stage3);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery14_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult = "| 17.932138 |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery14>(expectedResult, false /*checkDebugInterp*/);
}

TEST(PaperBenchmark, TpchQuery14)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 14 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery14>();
}

namespace {

void BuildTpchQuery3()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto custRow = x_tpchtable_customer.GetTableRow();
    auto cust_joinkey1 = custRow->GetSqlField(&TpchCustomerTableRow::c_custkey);

    auto hashtable = new HashTableContainer();
    hashtable->m_row = custRow;
    hashtable->m_groupByFields.push_back(cust_joinkey1);

    auto cust_table_container = new SqlTableContainer("customer");

    auto cust_table_reader = new SqlTableRowGenerator();
    cust_table_reader->m_src = cust_table_container;
    cust_table_reader->m_dst = custRow;

    auto cust_filter = new SqlCompareWithStringLiteralOperator(custRow->GetSqlField(&TpchCustomerTableRow::c_mktsegment),
                                                               "BUILDING",
                                                               SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto cust_filter_processor = new SqlFilterProcessor(cust_filter);

    auto hash_join_outputter = new HashJoinHashTableOutputter();
    hash_join_outputter->m_inputRow = custRow;
    hash_join_outputter->m_container = hashtable;

    auto stage1 = new QueryPlanPipelineStage();
    stage1->m_generator = cust_table_reader;
    stage1->m_processor.push_back(cust_filter_processor);
    stage1->m_outputter = hash_join_outputter;

    auto ordersRow = x_tpchtable_orders.GetTableRow();
    auto orders_joinkey1 = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderkey);

    auto hashtable2 = new HashTableContainer();
    hashtable2->m_row = ordersRow;
    hashtable2->m_groupByFields.push_back(orders_joinkey1);

    auto orders_table_container = new SqlTableContainer("orders");

    auto orders_table_reader = new SqlTableRowGenerator();
    orders_table_reader->m_src = orders_table_container;
    orders_table_reader->m_dst = ordersRow;

    auto orders_filter = new SqlComparisonOperator(AstComparisonExprType::LESS_THAN,
                                                   ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderdate),
                                                   new SqlLiteral(static_cast<uint32_t>(795254400)));
    auto orders_filter_processor = new SqlFilterProcessor(orders_filter);

    auto orders_join_processor = new TableHashJoinProcessor();
    orders_join_processor->m_container = hashtable;
    orders_join_processor->m_inputRowJoinFields.push_back(ordersRow->GetSqlField(&TpchOrdersTableRow::o_custkey));

    auto hash_join_outputter2 = new HashJoinHashTableOutputter();
    hash_join_outputter2->m_inputRow = ordersRow;
    hash_join_outputter2->m_container = hashtable2;

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = orders_table_reader;
    stage2->m_processor.push_back(orders_filter_processor);
    stage2->m_processor.push_back(orders_join_processor);
    stage2->m_outputter = hash_join_outputter2;

    auto lineitemRow = x_tpchtable_lineitem.GetTableRow();

    auto lineitem_table_container = new SqlTableContainer("lineitem");

    auto lineitem_table_reader = new SqlTableRowGenerator();
    lineitem_table_reader->m_src = lineitem_table_container;
    lineitem_table_reader->m_dst = lineitemRow;

    auto lineitem_filter = new SqlComparisonOperator(AstComparisonExprType::GREATER_THAN,
                                                     lineitemRow->GetSqlField(&TpchLineItemTableRow::l_shipdate),
                                                     new SqlLiteral(static_cast<uint32_t>(795254400)));
    auto lineitem_filter_processor = new SqlFilterProcessor(lineitem_filter);

    auto lineitem_join_processor = new TableHashJoinProcessor();
    lineitem_join_processor->m_container = hashtable2;
    lineitem_join_processor->m_inputRowJoinFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_orderkey));

    auto aggregation_row_field0_init = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderkey);
    auto aggregation_row_field1_init = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_field2_init = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderdate);
    auto aggregation_row_field3_init = ordersRow->GetSqlField(&TpchOrdersTableRow::o_shippriority);

    auto aggregation_row = new SqlAggregationRow({
                                                     aggregation_row_field0_init,
                                                     aggregation_row_field1_init,
                                                     aggregation_row_field2_init,
                                                     aggregation_row_field3_init
                                                 });

    auto sum_expr = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                lineitemRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                new SqlArithmeticOperator(
                    TypeId::Get<double>(),
                    AstArithmeticExprType::SUB,
                    new SqlLiteral(static_cast<double>(1)),
                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_discount)));

    auto aggregation_row_field1_update = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                sum_expr,
                aggregation_row->GetSqlProjectionField(1));
    aggregation_row->SetUpdateExpr(1, aggregation_row_field1_update);

    auto groupby_field1 = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderkey);
    auto groupby_field2 = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderdate);
    auto groupby_field3 = ordersRow->GetSqlField(&TpchOrdersTableRow::o_shippriority);

    auto groupby_container = new HashTableContainer();
    groupby_container->m_row = ordersRow;
    groupby_container->m_groupByFields.push_back(groupby_field1);
    groupby_container->m_groupByFields.push_back(groupby_field2);
    groupby_container->m_groupByFields.push_back(groupby_field3);

    auto groupby_outputter = new GroupByHashTableOutputter();
    groupby_outputter->m_container = groupby_container;
    groupby_outputter->m_inputRow = ordersRow;
    groupby_outputter->m_aggregationRow = aggregation_row;

    auto stage3 = new QueryPlanPipelineStage();
    stage3->m_generator = lineitem_table_reader;
    stage3->m_processor.push_back(lineitem_filter_processor);
    stage3->m_processor.push_back(lineitem_join_processor);
    stage3->m_outputter = groupby_outputter;

    auto groupby_result_reader = new GroupByHashTableGenerator();
    groupby_result_reader->m_container = groupby_container;
    groupby_result_reader->m_dst = aggregation_row;

    auto temptable_container = new TempTableContainer();

    auto temptable_outputter = new TempTableRowOutputter();
    temptable_outputter->m_src = aggregation_row;
    temptable_outputter->m_dst = temptable_container;

    auto stage4 = new QueryPlanPipelineStage();
    stage4->m_generator = groupby_result_reader;
    stage4->m_outputter = temptable_outputter;

    auto stage5 = new QueryPlanOrderByStage();
    stage5->m_row = aggregation_row;
    stage5->m_container = temptable_container;
    stage5->m_orderByFields.push_back(std::make_pair(aggregation_row->GetSqlProjectionField(1), false /*isAscend*/));
    stage5->m_orderByFields.push_back(std::make_pair(aggregation_row->GetSqlProjectionField(2), true /*isAscend*/));

    auto temptable_reader = new TempTableRowGenerator();
    temptable_reader->m_dst = aggregation_row;
    temptable_reader->m_container = temptable_container;

    auto limit_processor = new LimitClauseProcessor();
    limit_processor->m_limit = 20;

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(0));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(1));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(2));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(3));

    auto stage6 = new QueryPlanPipelineStage();
    stage6->m_generator = temptable_reader;
    stage6->m_processor.push_back(limit_processor);
    stage6->m_outputter = outputter;

    auto plan = new QueryPlan();
    plan->m_neededContainers.push_back(cust_table_container);
    plan->m_neededContainers.push_back(orders_table_container);
    plan->m_neededContainers.push_back(lineitem_table_container);
    plan->m_neededContainers.push_back(hashtable);
    plan->m_neededContainers.push_back(hashtable2);
    plan->m_neededContainers.push_back(groupby_container);
    plan->m_neededContainers.push_back(temptable_container);
    plan->m_stages.push_back(stage1);
    plan->m_stages.push_back(stage2);
    plan->m_stages.push_back(stage3);
    plan->m_stages.push_back(stage4);
    plan->m_stages.push_back(stage5);
    plan->m_stages.push_back(stage6);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery3_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult =
        "| 56515460 | 386191.289900 | 792057600 | 0 |\n"
        "| 38559075 | 340093.917200 | 791366400 | 0 |\n"
        "| 28666663 | 301296.454700 | 792576000 | 0 |\n"
        "| 53469703 | 281069.901600 | 794217600 | 0 |\n"
        "| 45613187 | 266925.396100 | 792748800 | 0 |\n"
        "| 52263431 | 254077.847600 | 794476800 | 0 |\n"
        "| 26894336 | 246784.136700 | 793699200 | 0 |\n"
        "| 50551300 | 224597.129300 | 794044800 | 0 |\n"
        "| 43752419 | 220553.883700 | 789465600 | 0 |\n"
        "| 19974213 | 185434.449100 | 793180800 | 0 |\n"
        "| 22083299 | 183986.848000 | 792748800 | 0 |\n"
        "| 37017408 | 183120.638600 | 792489600 | 0 |\n"
        "| 26112001 | 180333.315600 | 790416000 | 0 |\n"
        "| 4143395 | 178770.101200 | 791712000 | 0 |\n"
        "| 31777029 | 177249.845200 | 790848000 | 0 |\n"
        "| 37973732 | 174125.616900 | 791971200 | 0 |\n"
        "| 33717218 | 171613.598900 | 791971200 | 0 |\n"
        "| 31945287 | 167527.535000 | 793180800 | 0 |\n"
        "| 14591234 | 162619.165200 | 794908800 | 0 |\n"
        "| 10786852 | 161931.240100 | 792662400 | 0 |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery3>(expectedResult, false /*checkDebugInterp*/);
}

TEST(PaperBenchmark, TpchQuery3)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 3 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery3>();
}

namespace {

void BuildTpchQuery10()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto nationRow = x_tpchtable_nation.GetTableRow();
    auto nation_joinkey1 = nationRow->GetSqlField(&TpchNationTableRow::n_nationkey);

    auto nation_hashtable = new HashTableContainer();
    nation_hashtable->m_row = nationRow;
    nation_hashtable->m_groupByFields.push_back(nation_joinkey1);

    auto nation_table_container = new SqlTableContainer("nation");

    auto nation_table_reader = new SqlTableRowGenerator();
    nation_table_reader->m_src = nation_table_container;
    nation_table_reader->m_dst = nationRow;

    auto nation_hash_table_outputter = new HashJoinHashTableOutputter();
    nation_hash_table_outputter->m_inputRow = nationRow;
    nation_hash_table_outputter->m_container = nation_hashtable;

    auto stage1 = new QueryPlanPipelineStage();
    stage1->m_generator = nation_table_reader;
    stage1->m_outputter = nation_hash_table_outputter;

    auto custRow = x_tpchtable_customer.GetTableRow();

    auto cust_nation_join_processor = new TableHashJoinProcessor();
    cust_nation_join_processor->m_container = nation_hashtable;
    cust_nation_join_processor->m_inputRowJoinFields.push_back(custRow->GetSqlField(&TpchCustomerTableRow::c_nationkey));

    auto projection_field0 = nationRow->GetSqlField(&TpchNationTableRow::n_name);
    auto projection_field1 = custRow->GetSqlField(&TpchCustomerTableRow::c_custkey);
    auto projection_field2 = custRow->GetSqlField(&TpchCustomerTableRow::c_name);
    auto projection_field3 = custRow->GetSqlField(&TpchCustomerTableRow::c_acctbal);
    auto projection_field4 = custRow->GetSqlField(&TpchCustomerTableRow::c_phone);
    auto projection_field5 = custRow->GetSqlField(&TpchCustomerTableRow::c_address);
    auto projection_field6 = custRow->GetSqlField(&TpchCustomerTableRow::c_comment);

    auto projectionRow = new SqlProjectionRow({
                                                  projection_field0,
                                                  projection_field1,
                                                  projection_field2,
                                                  projection_field3,
                                                  projection_field4,
                                                  projection_field5,
                                                  projection_field6
                                              });

    auto projection_processor = new SqlProjectionProcessor();
    projection_processor->m_output = projectionRow;

    auto cust_nation_joinkey = projectionRow->GetSqlProjectionField(1);
    auto cust_nation_hashtable = new HashTableContainer();
    cust_nation_hashtable->m_row = projectionRow;
    cust_nation_hashtable->m_groupByFields.push_back(cust_nation_joinkey);

    auto cust_table_container = new SqlTableContainer("customer");

    auto cust_table_reader = new SqlTableRowGenerator();
    cust_table_reader->m_src = cust_table_container;
    cust_table_reader->m_dst = custRow;

    auto cust_nation_hash_join_outputter = new HashJoinHashTableOutputter();
    cust_nation_hash_join_outputter->m_inputRow = projectionRow;
    cust_nation_hash_join_outputter->m_container = cust_nation_hashtable;

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = cust_table_reader;
    stage2->m_processor.push_back(cust_nation_join_processor);
    stage2->m_processor.push_back(projection_processor);
    stage2->m_outputter = cust_nation_hash_join_outputter;

    auto ordersRow = x_tpchtable_orders.GetTableRow();
    auto orders_table_container = new SqlTableContainer("orders");

    auto orders_table_reader = new SqlTableRowGenerator();
    orders_table_reader->m_src = orders_table_container;
    orders_table_reader->m_dst = ordersRow;

    auto orders_filter_1 = new SqlComparisonOperator(
                AstComparisonExprType::GREATER_EQUAL,
                ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderdate),
                new SqlLiteral(static_cast<uint32_t>(749458800)));
    auto orders_filter_2 = new SqlComparisonOperator(
                AstComparisonExprType::LESS_THAN,
                ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderdate),
                new SqlLiteral(static_cast<uint32_t>(757411200)));
    auto orders_filter = new SqlBinaryLogicalOperator(true /*isAnd*/, orders_filter_1, orders_filter_2);
    auto orders_filter_processor = new SqlFilterProcessor(orders_filter);

    auto orders_join_processor = new TableHashJoinProcessor();
    orders_join_processor->m_container = cust_nation_hashtable;
    orders_join_processor->m_inputRowJoinFields.push_back(ordersRow->GetSqlField(&TpchOrdersTableRow::o_custkey));

    auto projection2_field0 = projectionRow->GetSqlProjectionField(0);
    auto projection2_field1 = projectionRow->GetSqlProjectionField(1);
    auto projection2_field2 = projectionRow->GetSqlProjectionField(2);
    auto projection2_field3 = projectionRow->GetSqlProjectionField(3);
    auto projection2_field4 = projectionRow->GetSqlProjectionField(4);
    auto projection2_field5 = projectionRow->GetSqlProjectionField(5);
    auto projection2_field6 = projectionRow->GetSqlProjectionField(6);
    auto projection2_field7 = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderkey);

    auto projectionRow2 = new SqlProjectionRow({
                                                   projection2_field0,
                                                   projection2_field1,
                                                   projection2_field2,
                                                   projection2_field3,
                                                   projection2_field4,
                                                   projection2_field5,
                                                   projection2_field6,
                                                   projection2_field7
                                               });

    auto projection_processor2 = new SqlProjectionProcessor();
    projection_processor2->m_output = projectionRow2;

    auto ht_joinkey = projectionRow2->GetSqlProjectionField(7);
    auto hashtable = new HashTableContainer();
    hashtable->m_row = projectionRow2;
    hashtable->m_groupByFields.push_back(ht_joinkey);

    auto hashtable_outputter = new HashJoinHashTableOutputter();
    hashtable_outputter->m_inputRow = projectionRow2;
    hashtable_outputter->m_container = hashtable;

    auto stage3 = new QueryPlanPipelineStage();
    stage3->m_generator = orders_table_reader;
    stage3->m_processor.push_back(orders_filter_processor);
    stage3->m_processor.push_back(orders_join_processor);
    stage3->m_processor.push_back(projection_processor2);
    stage3->m_outputter = hashtable_outputter;

    auto lineitemRow = x_tpchtable_lineitem.GetTableRow();
    auto lineitem_table_container = new SqlTableContainer("lineitem");

    auto lineitem_table_reader = new SqlTableRowGenerator();
    lineitem_table_reader->m_src = lineitem_table_container;
    lineitem_table_reader->m_dst = lineitemRow;

    auto lineitem_filter = new SqlCompareWithStringLiteralOperator(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_returnflag),
                                                                   "R",
                                                                   SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto lineitem_filter_processor = new SqlFilterProcessor(lineitem_filter);

    auto lineitem_join_processor = new TableHashJoinProcessor();
    lineitem_join_processor->m_container = hashtable;
    lineitem_join_processor->m_inputRowJoinFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_orderkey));

    auto aggregation_row_field0_init = projectionRow2->GetSqlProjectionField(1);
    auto aggregation_row_field1_init = projectionRow2->GetSqlProjectionField(2);
    auto aggregation_row_field2_init = new SqlLiteral(static_cast<double>(0));
    auto aggregation_row_field3_init = projectionRow2->GetSqlProjectionField(3);
    auto aggregation_row_field4_init = projectionRow2->GetSqlProjectionField(0);
    auto aggregation_row_field5_init = projectionRow2->GetSqlProjectionField(5);
    auto aggregation_row_field6_init = projectionRow2->GetSqlProjectionField(4);
    auto aggregation_row_field7_init = projectionRow2->GetSqlProjectionField(6);

    auto aggregation_row = new SqlAggregationRow({
                                                     aggregation_row_field0_init,
                                                     aggregation_row_field1_init,
                                                     aggregation_row_field2_init,
                                                     aggregation_row_field3_init,
                                                     aggregation_row_field4_init,
                                                     aggregation_row_field5_init,
                                                     aggregation_row_field6_init,
                                                     aggregation_row_field7_init
                                                 });

    auto sum_expr = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                lineitemRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                new SqlArithmeticOperator(
                    TypeId::Get<double>(),
                    AstArithmeticExprType::SUB,
                    new SqlLiteral(static_cast<double>(1)),
                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_discount)));

    auto aggregation_row_field2_update = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                sum_expr,
                aggregation_row->GetSqlProjectionField(2));
    aggregation_row->SetUpdateExpr(2, aggregation_row_field2_update);

    auto groupby_container = new HashTableContainer();
    groupby_container->m_row = projectionRow2;
    groupby_container->m_groupByFields.push_back(projectionRow2->GetSqlProjectionField(0));
    groupby_container->m_groupByFields.push_back(projectionRow2->GetSqlProjectionField(1));
    groupby_container->m_groupByFields.push_back(projectionRow2->GetSqlProjectionField(2));
    groupby_container->m_groupByFields.push_back(projectionRow2->GetSqlProjectionField(4));
    groupby_container->m_groupByFields.push_back(projectionRow2->GetSqlProjectionField(5));
    groupby_container->m_groupByFields.push_back(projectionRow2->GetSqlProjectionField(6));

    auto groupby_outputter = new GroupByHashTableOutputter();
    groupby_outputter->m_container = groupby_container;
    groupby_outputter->m_inputRow = projectionRow2;
    groupby_outputter->m_aggregationRow = aggregation_row;

    auto stage4 = new QueryPlanPipelineStage();
    stage4->m_generator = lineitem_table_reader;
    stage4->m_processor.push_back(lineitem_filter_processor);
    stage4->m_processor.push_back(lineitem_join_processor);
    stage4->m_outputter = groupby_outputter;

    auto groupby_result_reader = new GroupByHashTableGenerator();
    groupby_result_reader->m_container = groupby_container;
    groupby_result_reader->m_dst = aggregation_row;

    auto temptable_container = new TempTableContainer();

    auto temptable_outputter = new TempTableRowOutputter();
    temptable_outputter->m_src = aggregation_row;
    temptable_outputter->m_dst = temptable_container;

    auto stage5 = new QueryPlanPipelineStage();
    stage5->m_generator = groupby_result_reader;
    stage5->m_outputter = temptable_outputter;

    auto stage6 = new QueryPlanOrderByStage();
    stage6->m_row = aggregation_row;
    stage6->m_container = temptable_container;
    stage6->m_orderByFields.push_back(std::make_pair(aggregation_row->GetSqlProjectionField(2), false /*isAscend*/));

    auto temptable_reader = new TempTableRowGenerator();
    temptable_reader->m_dst = aggregation_row;
    temptable_reader->m_container = temptable_container;

    auto limit_processor = new LimitClauseProcessor();
    limit_processor->m_limit = 20;

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(0));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(1));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(2));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(3));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(4));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(5));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(6));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(7));

    auto stagenana = new QueryPlanPipelineStage();
    stagenana->m_generator = temptable_reader;
    stagenana->m_processor.push_back(limit_processor);
    stagenana->m_outputter = outputter;

    auto plan = new QueryPlan();
    plan->m_neededContainers.push_back(nation_table_container);
    plan->m_neededContainers.push_back(cust_table_container);
    plan->m_neededContainers.push_back(orders_table_container);
    plan->m_neededContainers.push_back(lineitem_table_container);
    plan->m_neededContainers.push_back(nation_hashtable);
    plan->m_neededContainers.push_back(cust_nation_hashtable);
    plan->m_neededContainers.push_back(hashtable);
    plan->m_neededContainers.push_back(groupby_container);
    plan->m_neededContainers.push_back(temptable_container);
    plan->m_stages.push_back(stage1);
    plan->m_stages.push_back(stage2);
    plan->m_stages.push_back(stage3);
    plan->m_stages.push_back(stage4);
    plan->m_stages.push_back(stage5);
    plan->m_stages.push_back(stage6);
    plan->m_stages.push_back(stagenana);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery10_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult =
        "| 463510 | Customer#000463510 | 209303.065500 | 5515.660000 | ALGERIA | WvyLxW0HOhUzyGsZ1X0J | 10-886-559-3779 | ily. furiously final dolphins sleep final deposits. quickly pending foxes sleep fl |\n"
        "| 521620 | Customer#000521620 | 157110.972600 | 2127.640000 | ALGERIA | xr9n 7WlIV7tjop2a0tY0 | 10-827-271-6272 | boost about the unusual deposits. blithely even excuses are. blithely even foxes against the pe |\n"
        "| 639649 | Customer#000639649 | 145055.757900 | 8816.620000 | ALGERIA | 9MOLLfjIFxC6jumAmGFJHCOu9ZzU5 | 10-774-432-1353 | es. slyly regular ideas sleep quickly after th |\n"
        "| 638350 | Customer#000638350 | 140226.826400 | 3518.340000 | ALGERIA | dw8S06MXpeCQX 0Fiw Cbu0sz7wN,fNGcC | 10-668-433-1463 | g above the slyly bold instructions. pending pinto beans eat carefully across the final packages. e |\n"
        "| 701903 | Customer#000701903 | 134877.240100 | 5017.740000 | ALGERIA | WmdH8vJXChLKhXhM7hoHkEbToDKaV9q2vraXbK | 10-832-227-5806 | usly final accounts use furiously about the theodolites. requests wake blithely regular re |\n"
        "| 579899 | Customer#000579899 | 130741.053200 | 872.130000 | ALGERIA | oLE5L,duosliECu119bYx CHTQ3 b | 10-776-291-7870 | al requests are. quickly ironic instructions sl |\n"
        "| 1075513 | Customer#001075513 | 126779.880000 | 8309.000000 | ALGERIA | GjkVQxHUFAOkmlA5 | 10-968-838-3385 | unts. slyly pending accounts detect furiously about the quickly ironic packages. carefully even deposits al |\n"
        "| 1294861 | Customer#001294861 | 109390.992800 | 2498.430000 | ALGERIA | 4stLzw O2bSyPPdANHiF1BlLkUSLRxUr  | 10-496-804-3150 | ronic ideas. final packages use fluffily pending, unusual accounts. |\n"
        "| 319955 | Customer#000319955 | 104879.291400 | 7165.890000 | ALGERIA | 2oNsjQp3MHz2MP5GVfVzATSElGAEiCxczCi1 | 10-566-462-2241 | s. furiously ironic asymptotes breach silent, even deposits. slyly even packages n |\n"
        "| 553939 | Customer#000553939 | 91792.428000 | 7532.680000 | ALGERIA | BOTXcsnOE9h7cL0C,5YJwP0MIOmoey | 10-139-836-6635 | uickly ironic deposits alongside of the evenly ironic packages believe arou |\n"
        "| 1145489 | Customer#001145489 | 61757.382000 | 7135.320000 | ALGERIA | LekyLfXFJ3c8HelAKoB4owNGXQS | 10-348-967-5828 | fluffily blithely regular depths. blithely pending depths are carefully-- decoy |\n"
        "| 226939 | Customer#000226939 | 54124.188300 | 1561.110000 | ALGERIA | lohl5sV97l5 | 10-266-523-7291 | totes. carefully express requests sleep across the i |\n"
        "| 1366306 | Customer#001366306 | 52389.234400 | 5532.930000 | ALGERIA | NzrMTEghXXJTDoS4z4p | 10-689-829-9568 |  alongside of the carefully bold accounts haggle bold accounts. slyly special deposits acco |\n"
        "| 501572 | Customer#000501572 | 50902.127100 | 2408.030000 | ALGERIA | ppPglgpPKO | 10-830-582-6827 | carefully bold packages nag carefull |\n"
        "| 1114199 | Customer#001114199 | 34950.300000 | -935.430000 | ALGERIA | azCSIBJZND dYRzVGM4QDNZjU | 10-503-648-2561 | lithely furiously pending frays. ironic asymptotes  |\n"
        "| 1099228 | Customer#001099228 | 28067.544000 | 5859.980000 | ALGERIA | STyiULA18QY9wpe79ibbVwk903p9Ff | 10-919-516-9178 | the slyly ironic waters sleep at the somas. regular, ironic hockey players sleep. instructions sleep. re |\n"
        "| 334444 | Customer#000334444 | 11806.294500 | 9919.840000 | ALGERIA | 4P3phxzpTbc4XZ7pTIfx9JNpBZ8Nm,ugh9 | 10-327-401-9058 | use. furiously bold platelets cajole. quickly express requests instead of the quickly close deposits wake fluffily  |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery10>(expectedResult, false /*checkDebugInterp*/);
}

TEST(PaperBenchmark, TpchQuery10)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 10 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery10>();
}

namespace {

void BuildTpchQuery5()
{
    thread_pochiVMContext->m_curModule = new AstModule("test");

    auto nationRow = x_tpchtable_nation.GetTableRow();
    auto nation_joinkey1 = nationRow->GetSqlField(&TpchNationTableRow::n_regionkey);

    auto nation_hashtable = new HashTableContainer();
    nation_hashtable->m_row = nationRow;
    nation_hashtable->m_groupByFields.push_back(nation_joinkey1);

    auto nation_table_container = new SqlTableContainer("nation");

    auto nation_table_reader = new SqlTableRowGenerator();
    nation_table_reader->m_src = nation_table_container;
    nation_table_reader->m_dst = nationRow;

    auto nation_hash_table_outputter = new HashJoinHashTableOutputter();
    nation_hash_table_outputter->m_inputRow = nationRow;
    nation_hash_table_outputter->m_container = nation_hashtable;

    auto stage1 = new QueryPlanPipelineStage();
    stage1->m_generator = nation_table_reader;
    stage1->m_outputter = nation_hash_table_outputter;

    auto regionRow = x_tpchtable_region.GetTableRow();
    auto region_table_container = new SqlTableContainer("region");

    auto region_table_reader = new SqlTableRowGenerator();
    region_table_reader->m_src = region_table_container;
    region_table_reader->m_dst = regionRow;

    auto region_filter = new SqlCompareWithStringLiteralOperator(regionRow->GetSqlField(&TpchRegionTableRow::r_name),
                                                                 "AFRICA",
                                                                 SqlCompareWithStringLiteralOperator::CompareMode::EQUAL);
    auto region_filter_processor = new SqlFilterProcessor(region_filter);
    auto region_join_processor = new TableHashJoinProcessor();
    region_join_processor->m_container = nation_hashtable;
    region_join_processor->m_inputRowJoinFields.push_back(regionRow->GetSqlField(&TpchRegionTableRow::r_regionkey));

    auto region_nation_joinkey = nationRow->GetSqlField(&TpchNationTableRow::n_nationkey);
    auto region_nation_hashtable = new HashTableContainer();
    region_nation_hashtable->m_row = nationRow;
    region_nation_hashtable->m_groupByFields.push_back(region_nation_joinkey);

    auto region_nation_hashtable_outputter = new HashJoinHashTableOutputter();
    region_nation_hashtable_outputter->m_inputRow = nationRow;
    region_nation_hashtable_outputter->m_container = region_nation_hashtable;

    auto stage2 = new QueryPlanPipelineStage();
    stage2->m_generator = region_table_reader;
    stage2->m_processor.push_back(region_filter_processor);
    stage2->m_processor.push_back(region_join_processor);
    stage2->m_outputter = region_nation_hashtable_outputter;

    auto custRow = x_tpchtable_customer.GetTableRow();
    auto cust_table_container = new SqlTableContainer("customer");

    auto cust_table_reader = new SqlTableRowGenerator();
    cust_table_reader->m_src = cust_table_container;
    cust_table_reader->m_dst = custRow;

    auto cust_join_processor = new TableHashJoinProcessor();
    cust_join_processor->m_container = region_nation_hashtable;
    cust_join_processor->m_inputRowJoinFields.push_back(custRow->GetSqlField(&TpchCustomerTableRow::c_nationkey));

    auto cust_projection_row_field0 = custRow->GetSqlField(&TpchCustomerTableRow::c_custkey);
    auto cust_projection_row_field1 = nationRow->GetSqlField(&TpchNationTableRow::n_nationkey);

    auto cust_projection_row = new SqlProjectionRow({
                                                        cust_projection_row_field0,
                                                        cust_projection_row_field1
                                                    });
    auto cust_projection_processor = new SqlProjectionProcessor();
    cust_projection_processor->m_output = cust_projection_row;

    auto cust_joinkey = cust_projection_row->GetSqlProjectionField(0);
    auto cust_hashtable = new HashTableContainer();
    cust_hashtable->m_row = cust_projection_row;
    cust_hashtable->m_groupByFields.push_back(cust_joinkey);

    auto cust_hashtable_outputter = new HashJoinHashTableOutputter();
    cust_hashtable_outputter->m_inputRow = cust_projection_row;
    cust_hashtable_outputter->m_container = cust_hashtable;

    auto stage3 = new QueryPlanPipelineStage();
    stage3->m_generator = cust_table_reader;
    stage3->m_processor.push_back(cust_join_processor);
    stage3->m_processor.push_back(cust_projection_processor);
    stage3->m_outputter = cust_hashtable_outputter;

    auto ordersRow = x_tpchtable_orders.GetTableRow();
    auto orders_table_container = new SqlTableContainer("orders");

    auto orders_table_reader = new SqlTableRowGenerator();
    orders_table_reader->m_src = orders_table_container;
    orders_table_reader->m_dst = ordersRow;

    auto orders_filter_1 = new SqlComparisonOperator(
                AstComparisonExprType::GREATER_EQUAL,
                ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderdate),
                new SqlLiteral(static_cast<uint32_t>(757411200)));
    auto orders_filter_2 = new SqlComparisonOperator(
                AstComparisonExprType::LESS_THAN,
                ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderdate),
                new SqlLiteral(static_cast<uint32_t>(1072944000)));
    auto orders_filter = new SqlBinaryLogicalOperator(true /*isAnd*/, orders_filter_1, orders_filter_2);
    auto orders_filter_processor = new SqlFilterProcessor(orders_filter);

    auto orders_join_processor = new TableHashJoinProcessor();
    orders_join_processor->m_container = cust_hashtable;
    orders_join_processor->m_inputRowJoinFields.push_back(ordersRow->GetSqlField(&TpchOrdersTableRow::o_custkey));

    auto orders_projection_field0 = cust_projection_row->GetSqlProjectionField(1);
    auto orders_projection_field1 = ordersRow->GetSqlField(&TpchOrdersTableRow::o_orderkey);

    auto orders_projection_row = new SqlProjectionRow({
                                                      orders_projection_field0,
                                                      orders_projection_field1
                                                  });
    auto orders_projection_processor = new SqlProjectionProcessor();
    orders_projection_processor->m_output = orders_projection_row;

    auto orders_joinkey = orders_projection_row->GetSqlProjectionField(1);
    auto orders_hashtable = new HashTableContainer();
    orders_hashtable->m_row = orders_projection_row;
    orders_hashtable->m_groupByFields.push_back(orders_joinkey);

    auto orders_hashtable_outputter = new HashJoinHashTableOutputter();
    orders_hashtable_outputter->m_inputRow = orders_projection_row;
    orders_hashtable_outputter->m_container = orders_hashtable;

    auto stage4 = new QueryPlanPipelineStage();
    stage4->m_generator = orders_table_reader;
    stage4->m_processor.push_back(orders_filter_processor);
    stage4->m_processor.push_back(orders_join_processor);
    stage4->m_processor.push_back(orders_projection_processor);
    stage4->m_outputter = orders_hashtable_outputter;

    auto suppRow = x_tpchtable_supplier.GetTableRow();
    auto supp_table_container = new SqlTableContainer("supplier");

    auto supp_table_reader = new SqlTableRowGenerator();
    supp_table_reader->m_src = supp_table_container;
    supp_table_reader->m_dst = suppRow;

    auto supp_join_processor = new TableHashJoinProcessor();
    supp_join_processor->m_container = region_nation_hashtable;
    supp_join_processor->m_inputRowJoinFields.push_back(suppRow->GetSqlField(&TpchSupplierTableRow::s_nationkey));

    auto supp_projection_field0 = nationRow->GetSqlField(&TpchNationTableRow::n_name);
    auto supp_projection_field1 = nationRow->GetSqlField(&TpchNationTableRow::n_nationkey);
    auto supp_projection_field2 = suppRow->GetSqlField(&TpchSupplierTableRow::s_suppkey);

    auto supp_projection_row = new SqlProjectionRow({
                                                        supp_projection_field0,
                                                        supp_projection_field1,
                                                        supp_projection_field2
                                                    });
    auto supp_projection_processor = new SqlProjectionProcessor();
    supp_projection_processor->m_output = supp_projection_row;

    auto supp_joinkey = supp_projection_row->GetSqlProjectionField(2);
    auto supp_hashtable = new HashTableContainer();
    supp_hashtable->m_row = supp_projection_row;
    supp_hashtable->m_groupByFields.push_back(supp_joinkey);

    auto supp_hashtable_outputter = new HashJoinHashTableOutputter();
    supp_hashtable_outputter->m_inputRow = supp_projection_row;
    supp_hashtable_outputter->m_container = supp_hashtable;

    auto stage5 = new QueryPlanPipelineStage();
    stage5->m_generator = supp_table_reader;
    stage5->m_processor.push_back(supp_join_processor);
    stage5->m_processor.push_back(supp_projection_processor);
    stage5->m_outputter = supp_hashtable_outputter;

    auto lineitemRow = x_tpchtable_lineitem.GetTableRow();
    auto lineitem_table_container = new SqlTableContainer("lineitem");

    auto lineitem_table_reader = new SqlTableRowGenerator();
    lineitem_table_reader->m_src = lineitem_table_container;
    lineitem_table_reader->m_dst = lineitemRow;

    auto lineitem_orders_join_processor = new TableHashJoinProcessor();
    lineitem_orders_join_processor->m_container = orders_hashtable;
    lineitem_orders_join_processor->m_inputRowJoinFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_orderkey));

    auto lineitem_supp_join_processor = new TableHashJoinProcessor();
    lineitem_supp_join_processor->m_container = supp_hashtable;
    lineitem_supp_join_processor->m_inputRowJoinFields.push_back(lineitemRow->GetSqlField(&TpchLineItemTableRow::l_suppkey));

    auto lineitem_filter = new SqlComparisonOperator(
                AstComparisonExprType::EQUAL,
                orders_projection_row->GetSqlProjectionField(0),
                supp_projection_row->GetSqlProjectionField(1));
    auto lineitem_filter_processor = new SqlFilterProcessor(lineitem_filter);

    auto aggregation_row_field0_init = supp_projection_row->GetSqlProjectionField(0);
    auto aggregation_row_field1_init = new SqlLiteral(static_cast<double>(0));

    auto aggregation_row = new SqlAggregationRow({
                                                     aggregation_row_field0_init,
                                                     aggregation_row_field1_init
                                                 });

    auto sum_expr = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::MUL,
                lineitemRow->GetSqlField(&TpchLineItemTableRow::l_extendedprice),
                new SqlArithmeticOperator(
                    TypeId::Get<double>(),
                    AstArithmeticExprType::SUB,
                    new SqlLiteral(static_cast<double>(1)),
                    lineitemRow->GetSqlField(&TpchLineItemTableRow::l_discount)));

    auto aggregation_row_field1_update = new SqlArithmeticOperator(
                TypeId::Get<double>(),
                AstArithmeticExprType::ADD,
                sum_expr,
                aggregation_row->GetSqlProjectionField(1));
    aggregation_row->SetUpdateExpr(1, aggregation_row_field1_update);

    auto groupby_container = new HashTableContainer();
    groupby_container->m_row = supp_projection_row;
    groupby_container->m_groupByFields.push_back(supp_projection_row->GetSqlProjectionField(0));

    auto groupby_outputter = new GroupByHashTableOutputter();
    groupby_outputter->m_container = groupby_container;
    groupby_outputter->m_inputRow = supp_projection_row;
    groupby_outputter->m_aggregationRow = aggregation_row;

    auto stage6 = new QueryPlanPipelineStage();
    stage6->m_generator = lineitem_table_reader;
    stage6->m_processor.push_back(lineitem_orders_join_processor);
    stage6->m_processor.push_back(lineitem_supp_join_processor);
    stage6->m_processor.push_back(lineitem_filter_processor);
    stage6->m_outputter = groupby_outputter;

    auto groupby_result_reader = new GroupByHashTableGenerator();
    groupby_result_reader->m_container = groupby_container;
    groupby_result_reader->m_dst = aggregation_row;

    auto temptable_container = new TempTableContainer();

    auto temptable_outputter = new TempTableRowOutputter();
    temptable_outputter->m_src = aggregation_row;
    temptable_outputter->m_dst = temptable_container;

    auto stage7 = new QueryPlanPipelineStage();
    stage7->m_generator = groupby_result_reader;
    stage7->m_outputter = temptable_outputter;

    auto stage8 = new QueryPlanOrderByStage();
    stage8->m_row = aggregation_row;
    stage8->m_container = temptable_container;
    stage8->m_orderByFields.push_back(std::make_pair(aggregation_row->GetSqlProjectionField(1), false /*isAscend*/));

    auto temptable_reader = new TempTableRowGenerator();
    temptable_reader->m_dst = aggregation_row;
    temptable_reader->m_container = temptable_container;

    auto limit_processor = new LimitClauseProcessor();
    limit_processor->m_limit = 20;

    auto outputter = new SqlResultOutputter();
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(0));
    outputter->m_projections.push_back(aggregation_row->GetSqlProjectionField(1));

    auto stage9 = new QueryPlanPipelineStage();
    stage9->m_generator = temptable_reader;
    stage9->m_processor.push_back(limit_processor);
    stage9->m_outputter = outputter;

    auto plan = new QueryPlan();
    plan->m_neededContainers.push_back(nation_table_container);
    plan->m_neededContainers.push_back(region_table_container);
    plan->m_neededContainers.push_back(cust_table_container);
    plan->m_neededContainers.push_back(orders_table_container);
    plan->m_neededContainers.push_back(supp_table_container);
    plan->m_neededContainers.push_back(lineitem_table_container);
    plan->m_neededContainers.push_back(nation_hashtable);
    plan->m_neededContainers.push_back(region_nation_hashtable);
    plan->m_neededContainers.push_back(cust_hashtable);
    plan->m_neededContainers.push_back(orders_hashtable);
    plan->m_neededContainers.push_back(supp_hashtable);
    plan->m_neededContainers.push_back(groupby_container);
    plan->m_neededContainers.push_back(temptable_container);
    plan->m_stages.push_back(stage1);
    plan->m_stages.push_back(stage2);
    plan->m_stages.push_back(stage3);
    plan->m_stages.push_back(stage4);
    plan->m_stages.push_back(stage5);
    plan->m_stages.push_back(stage6);
    plan->m_stages.push_back(stage7);
    plan->m_stages.push_back(stage8);
    plan->m_stages.push_back(stage9);

    plan->CodeGen();

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, TpchQuery5_Correctness)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    std::string expectedResult = "| ALGERIA | 52516.800000 |\n";

    CheckTpchQueryCorrectness<BuildTpchQuery5>(expectedResult, false /*checkDebugInterp*/);
}

TEST(PaperBenchmark, TpchQuery5)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    TpchLoadDatabase();

    printf("******* TPCH Query 5 *******\n");
    BenchmarkTpchQuery<BuildTpchQuery5>();
}
