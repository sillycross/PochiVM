#pragma once

#include "query_plan_ast.h"

namespace MiniDbBackend
{

using namespace PochiVM;

inline Variable<uintptr_t>& SqlRow::GetAddress()
{
    auto it = thread_queryCodegenContext.m_rowMap.find(this);
    TestAssert(it != thread_queryCodegenContext.m_rowMap.end());
    return *(it->second);
}

inline ValueVT WARN_UNUSED SqlField::Codegen()
{
    Variable<uintptr_t>& var = m_owner->GetAddress();
    if (m_owner->GetRowType() == SqlRowType::TABLE_ROW && GetType().IsPointerType())
    {
        TestAssert(GetType() == TypeId::Get<char*>());
        return ReinterpretCast<char*>(var + Literal<size_t>(m_offset));
    }
    else
    {
        return *ReinterpretCast(GetType().AddPointer(), var + Literal<size_t>(m_offset));
    }
}

inline ValueVT WARN_UNUSED SqlLiteral::Codegen()
{
    return Literal(GetType(), m_value);
}

inline ValueVT WARN_UNUSED SqlArithmeticOperator::Codegen()
{
    ValueVT lhs = m_lhs->Codegen();
    ValueVT rhs = m_rhs->Codegen();
    return CreateArithmeticExpr(lhs, rhs, m_arithType);
}

inline ValueVT WARN_UNUSED SqlComparisonOperator::Codegen()
{
    ValueVT lhs = m_lhs->Codegen();
    ValueVT rhs = m_rhs->Codegen();
    return CreateComparisonExpr(lhs, rhs, m_comparisonType);
}

inline ValueVT WARN_UNUSED SqlBinaryLogicalOperator::Codegen()
{
    Value<bool> lhs = m_lhs->Codegen();
    Value<bool> rhs = m_rhs->Codegen();
    if (m_isAnd)
    {
        return lhs && rhs;
    }
    else
    {
        return lhs || rhs;
    }
}

inline ValueVT WARN_UNUSED SqlCastOperator::Codegen()
{
    return StaticCast(GetType(), m_operand->Codegen());
}

inline ValueVT WARN_UNUSED SqlCountAggregator::Codegen()
{
    ValueVT oldValue = m_oldAggregatedValue->Codegen();
    uint64_t v = 1;
    return CreateArithmeticExpr(oldValue, Literal(GetType(), &v), AstArithmeticExprType::ADD);
}

inline Block WARN_UNUSED SqlTableContainer::EmitDeclaration()
{
    using T = std::vector<uintptr_t>*;
    m_var = new Variable<T>(thread_queryCodegenContext.m_curFunction->NewVariable<T>());
    if (m_tableName == "customer") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetCustomerTable()));
    }
    else if (m_tableName == "lineitem") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetLineitemTable()));
    }
    else if (m_tableName == "nation") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetNationTable()));
    }
    else if (m_tableName == "orders") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetOrdersTable()));
    }
    else if (m_tableName == "part") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetPartTable()));
    }
    else if (m_tableName == "partsupp") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetPartSuppTable()));
    }
    else if (m_tableName == "region") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetRegionTable()));
    }
    else if (m_tableName == "supplier") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetSupplierTable()));
    }
    else if (m_tableName == "testtable1") {
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::GetTestTable1()));
    }
    else
    {
        ReleaseAssert(false);
    }
}

inline VariableVT WARN_UNUSED SqlTableContainer::GetVariable()
{
    TestAssert(m_var != nullptr);
    return VariableVT(m_var->__pochivm_var_ptr);
}

inline Block WARN_UNUSED TempTableContainer::EmitDeclaration()
{
    using T = std::vector<uintptr_t>;
    m_var = new Variable<T>(thread_queryCodegenContext.m_curFunction->NewVariable<T>());
    return Block(Declare(*m_var));
}

inline VariableVT WARN_UNUSED TempTableContainer::GetVariable()
{
    TestAssert(m_var != nullptr);
    return VariableVT(m_var->__pochivm_var_ptr);
}

inline Scope WARN_UNUSED QueryPlanPipelineStage::Codegen()
{
    TestAssert(thread_queryCodegenContext.m_rowMap.empty());
    for (SqlRow* row : m_neededRows)
    {
        auto var = new Variable<uintptr_t>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>());
        thread_queryCodegenContext.m_rowMap[row] = var;
    }

    Scope nextStage;
    Scope result = nextStage;
    nextStage = m_generator->Codegen(nextStage);
    for (QueryPlanRowProcessor* rowProcessor : m_processor)
    {
        nextStage = rowProcessor->Codegen(nextStage);
    }

    m_outputter->Codegen(nextStage);

    thread_queryCodegenContext.m_rowMap.clear();
    return result;
}

inline Scope WARN_UNUSED SqlTableRowGenerator::Codegen(Scope insertPoint)
{
    Scope ret;
    Variable<std::vector<uintptr_t>*> v(thread_queryCodegenContext.GetContainer(m_src));
    Variable<uintptr_t>& row = thread_queryCodegenContext.GetRow(m_dst);
    auto length = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
    auto i = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
    auto data = thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t*>();
    insertPoint.Append(Block(
        Declare(data, v->data()),
        Declare(length, v->size()),
        For(Declare(i, Literal<size_t>(0)), i < length, Increment(i)).Do(
            Declare(row, data[i]),
            ret
        )
    ));
    return ret;
}

inline void TempTableRowOutputter::Codegen(Scope insertPoint)
{
    Variable<std::vector<uintptr_t>> v(thread_queryCodegenContext.GetContainer(m_dst));
    Variable<uintptr_t>& row = thread_queryCodegenContext.GetRow(m_src);
    insertPoint.Append(v.push_back(row));
}

inline void SqlResultOutputter::Codegen(Scope insertPoint)
{
    Variable<SqlResultPrinter*>& p = *thread_queryCodegenContext.m_sqlResultPrinter;
    for (SqlValueBase* value : m_projections)
    {
        if (value->GetType() == TypeId::Get<int32_t>())
        {
            insertPoint.Append(p->PrintInt32(Value<int32_t>(value->Codegen())));
        }
        else if (value->GetType() == TypeId::Get<uint32_t>())
        {
            insertPoint.Append(p->PrintUInt32(Value<uint32_t>(value->Codegen())));
        }
        else if (value->GetType() == TypeId::Get<int64_t>())
        {
            insertPoint.Append(p->PrintInt64(Value<int64_t>(value->Codegen())));
        }
        else if (value->GetType() == TypeId::Get<uint64_t>())
        {
            insertPoint.Append(p->PrintUInt64(Value<uint64_t>(value->Codegen())));
        }
        else if (value->GetType() == TypeId::Get<double>())
        {
            insertPoint.Append(p->PrintDouble(Value<double>(value->Codegen())));
        }
        else if (value->GetType() == TypeId::Get<char*>())
        {
            insertPoint.Append(p->PrintString(Value<char*>(value->Codegen())));
        }
        else
        {
            TestAssert(false);
        }
    }
    insertPoint.Append(p->PrintNewLine());
}

inline Scope WARN_UNUSED SqlFilterProcessor::Codegen(Scope insertPoint)
{
    Scope ret;
    insertPoint.Append(If(Value<bool>(m_condition->Codegen())).Then(ret));
    return ret;
}

inline void QueryPlan::CodeGen()
{
    TestAssert(thread_queryCodegenContext.m_containerMap.empty());
    TestAssert(thread_queryCodegenContext.m_sqlResultPrinter == nullptr);
    TestAssert(thread_queryCodegenContext.m_rowMap.empty());
    TestAssert(thread_queryCodegenContext.m_curFunction == nullptr);
    auto [fn, printer] = NewFunction<void(*)(SqlResultPrinter*)>("execute_query");
    thread_queryCodegenContext.m_curFunction = &fn;
    thread_queryCodegenContext.m_sqlResultPrinter = &printer;
    Scope body;
    fn.SetBody(body);
    for (SqlRowContainer* container : m_neededContainers)
    {
        body.Append(container->EmitDeclaration());
        VariableVT* vvt = new VariableVT(container->GetVariable());
        thread_queryCodegenContext.m_containerMap[container] = vvt;
    }
    for (QueryPlanPipelineStage* stage : m_stages)
    {
        body.Append(stage->Codegen());
    }
    thread_queryCodegenContext.m_sqlResultPrinter = nullptr;
    thread_queryCodegenContext.m_containerMap.clear();
    TestAssert(thread_queryCodegenContext.m_rowMap.empty());
    thread_queryCodegenContext.m_curFunction = nullptr;
}

}   // namespace MiniDbBackend
