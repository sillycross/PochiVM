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

TEST(MiniDbBackend, TpchQuery6)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    TpchLoadDatabase();

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
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();

    std::string expectedResult =
        "| 38212505.914700 |\n";

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
