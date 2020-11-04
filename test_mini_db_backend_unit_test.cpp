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
    stage->m_name = "select1";
    stage->m_neededContainers.push_back(table_src);
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
