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

inline ReferenceVT WARN_UNUSED SqlField::CodegenForWrite()
{
    TestAssertImp(m_owner->GetRowType() == SqlRowType::TABLE_ROW, !GetType().IsPointerType());
    Variable<uintptr_t>& var = m_owner->GetAddress();
    return *ReinterpretCast(GetType().AddPointer(), var + Literal<size_t>(m_offset));
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

inline Block WARN_UNUSED SqlAggregationRow::EmitInit()
{
    Block result;
    size_t len = m_fields.size();
    TestAssert(len == m_initialValues.size());
    for (size_t i = 0; i < len; i++)
    {
        result.Append(Assign(m_fields[i]->CodegenForWrite(), m_initialValues[i]->Codegen()));
    }
    return result;
}

inline Block WARN_UNUSED SqlAggregationRow::EmitUpdate()
{
    Block result;
    size_t len = m_fields.size();
    TestAssert(len == m_updateExprs.size());
    for (size_t i = 0; i < len; i++)
    {
        if (m_updateExprs[i] != nullptr)
        {
            result.Append(Assign(m_fields[i]->CodegenForWrite(), m_updateExprs[i]->Codegen()));
        }
    }
    return result;
}

inline Block WARN_UNUSED SqlProjectionRow::Codegen()
{
    Block result;
    size_t len = m_fields.size();
    TestAssert(len == m_values.size());
    for (size_t i = 0; i < len; i++)
    {
        result.Append(Assign(m_fields[i]->CodegenForWrite(), m_values[i]->Codegen()));
    }
    return result;
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

inline Block WARN_UNUSED TempTableContainer::EmitDeclaration()
{
    using T = std::vector<uintptr_t>;
    m_var = new Variable<T>(thread_queryCodegenContext.m_curFunction->NewVariable<T>());
    return Block(Declare(*m_var));
}

inline Block WARN_UNUSED AggregatedRowContainer::EmitDeclaration()
{
    auto alloc = thread_queryCodegenContext.m_curFunction->NewVariable<QueryExecutionTempAllocator>();
    m_var = new Variable<uintptr_t>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>());
    TestAssert(!thread_queryCodegenContext.m_rowMap.count(m_row));
    thread_queryCodegenContext.m_rowMap[m_row] = m_var;

    Block result = Block(
        Declare(alloc),
        Declare(*m_var, alloc.Allocate(Literal<size_t>(m_row->GetRowSize()))),
        m_row->EmitInit()
    );

    thread_queryCodegenContext.m_rowMap.erase(thread_queryCodegenContext.m_rowMap.find(m_row));
    return result;
}

inline Block WARN_UNUSED GroupByHashTableContainer::EmitDeclaration()
{
    m_alloc = new Variable<QueryExecutionTempAllocator>(thread_queryCodegenContext.m_curFunction->NewVariable<QueryExecutionTempAllocator>());

    std::string hashFnName = thread_pochiVMContext->m_curModule->GetNextAvailableFnName("groupby_hashfn");
    {
        auto [fn, row] = NewFunction<HashFnPrototype>(hashFnName);
        std::map<SqlRow*, PochiVM::Variable<uintptr_t>*> bakRowMap;
        bakRowMap.swap(thread_queryCodegenContext.m_rowMap);

        thread_queryCodegenContext.m_rowMap[m_row] = &row;

        Function f = fn;
        Block initBlock;
        auto genHash = [&](SqlField* field) -> Value<size_t>
        {
            if (field->GetType().IsFloatingPoint())
            {
                ReleaseAssert(false && "cannot hash floating point");
            }
            else if (field->GetType().IsPrimitiveIntType())
            {
                return StaticCast<size_t>(field->Codegen());
            }
            else
            {
                TestAssert(field->GetType() == TypeId::Get<char*>());
                Value<char*> v(field->Codegen());
                auto i = f.NewVariable<char*>();
                auto hashValue = f.NewVariable<size_t>();
                initBlock.Append(Block(
                    Declare(i, v),
                    Declare(hashValue, static_cast<size_t>(0)),
                    While(*i != '\0').Do(
                        Assign(hashValue, hashValue * Literal<size_t>(100000000007ULL) + StaticCast<size_t>(*i)),
                        Assign(i, i + 1)
                    )
                ));
                return hashValue;
            }
        };

        std::function<Value<size_t>(size_t, size_t)> genExpr = [&](size_t seed, size_t n) -> Value<size_t>
        {
            Value<size_t> term = genHash(m_groupByFields[n]) * Literal<size_t>(seed);
            if (n == 0)
            {
                return term;
            }
            else
            {
                return genExpr(seed * 13331, n - 1) + term;
            }
        };

        TestAssert(m_groupByFields.size() > 0);
        fn.SetBody(initBlock, Return(genExpr(13331, m_groupByFields.size() - 1)));

        bakRowMap.swap(thread_queryCodegenContext.m_rowMap);
    }

    std::string cmpFnName = thread_pochiVMContext->m_curModule->GetNextAvailableFnName("groupby_cmpfn");
    {
        auto [fn, row1, row2] = NewFunction<CmpFnPrototype>(cmpFnName);
        std::map<SqlRow*, PochiVM::Variable<uintptr_t>*> bakRowMap;
        bakRowMap.swap(thread_queryCodegenContext.m_rowMap);

        thread_queryCodegenContext.m_rowMap[m_row] = &row1;
        std::vector<ValueVT> r1v;
        for (size_t i = 0; i < m_groupByFields.size(); i++)
        {
            r1v.push_back(m_groupByFields[i]->Codegen());
        }

        thread_queryCodegenContext.m_rowMap[m_row] = &row2;
        std::vector<ValueVT> r2v;
        for (size_t i = 0; i < m_groupByFields.size(); i++)
        {
            r2v.push_back(m_groupByFields[i]->Codegen());
        }

        for (size_t i = 0; i < m_groupByFields.size(); i++)
        {
            if (r1v[i].GetType().IsFloatingPoint())
            {
                ReleaseAssert(false && "cannot compare floating point");
            }
            else if (r1v[i].GetType().IsPrimitiveIntType())
            {
                fn.GetBody().Append(If(CreateComparisonExpr(r1v[i], r2v[i], AstComparisonExprType::NOT_EQUAL)).Then(Return(false)));
            }
            else
            {
                TestAssert(r1v[i].HasType(TypeId::Get<char*>()));
                TestAssert(r2v[i].HasType(TypeId::Get<char*>()));
                Value<char*> v1(r1v[i]);
                Value<char*> v2(r2v[i]);
                auto i1 = fn.NewVariable<char*>();
                auto i2 = fn.NewVariable<char*>();
                fn.GetBody().Append(Block(
                    Declare(i1, v1),
                    Declare(i2, v2),
                    While(*i1 != '\0' && *i1 == *i2).Do(
                        Assign(i1, i1 + 1),
                        Assign(i2, i2 + 1)
                    ),
                    If (*i1 != *i2).Then(
                        Return(false)
                    )
                ));
            }
        }

        fn.GetBody().Append(Return(true));

        bakRowMap.swap(thread_queryCodegenContext.m_rowMap);
    }

    if (x_use_cpp_data_structure)
    {
        m_var = new Variable<QEHashTable>(thread_queryCodegenContext.m_curFunction->NewVariable<QEHashTable>());
        Value<uintptr_t> hashFnPtr = GetGeneratedFunctionPointer(hashFnName);
        Value<uintptr_t> cmpFnPtr = GetGeneratedFunctionPointer(cmpFnName);
        return Block(Declare(*m_var, CallFreeFn::MiniDbBackend::CreateQEHashTable(hashFnPtr, cmpFnPtr)),
                     Declare(*m_alloc));
    }
    else
    {
        auto i = thread_queryCodegenContext.m_curFunction->NewVariable<int>();
        m_keys = new Variable<uintptr_t*>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t*>());
        m_values = new Variable<uintptr_t*>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t*>());
        m_tableSize = new Variable<size_t>(thread_queryCodegenContext.m_curFunction->NewVariable<size_t>());
        m_count = new Variable<size_t>(thread_queryCodegenContext.m_curFunction->NewVariable<size_t>());
        size_t initialSize = 32;
        m_cmpFnName = cmpFnName;
        m_hashFnName = hashFnName;
        return Block(Declare(*m_alloc),
                     Declare(*m_keys, ReinterpretCast<uintptr_t*>(m_alloc->Allocate(Literal<size_t>(8 * initialSize)))),
                     Declare(*m_values, ReinterpretCast<uintptr_t*>(m_alloc->Allocate(Literal<size_t>(8 * initialSize)))),
                     Declare(*m_tableSize, initialSize),
                     Declare(*m_count, static_cast<size_t>(0)),
                     For(Declare(i, 0), i < static_cast<int>(initialSize), Increment(i)).Do(
                         Assign((*m_keys)[i], Literal<uintptr_t>(0))
                     ));
    }
}

inline Block WARN_UNUSED GroupByHashTableContainer::EmitProbe(const Variable<uintptr_t>& input, const Variable<size_t>& output)
{
    TestAssert(!x_use_cpp_data_structure);
    auto quadratic_probe_factor = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
    return Block(
        Declare(output, Call<HashFnPrototype>(m_hashFnName, input) % (*m_tableSize)),
        Declare(quadratic_probe_factor, static_cast<size_t>(0)),
        While((*m_keys)[output] != Literal<size_t>(0) &&
              !Call<CmpFnPrototype>(m_cmpFnName, input, (*m_keys)[output])).Do(
            Increment(quadratic_probe_factor),
            Assign(output, (output + quadratic_probe_factor * quadratic_probe_factor) % (*m_tableSize))
        )
    );
}

inline Scope WARN_UNUSED QueryPlanPipelineStage::Codegen()
{
    TestAssert(thread_queryCodegenContext.m_rowMap.empty());
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

inline Scope WARN_UNUSED QueryPlanOrderByStage::Codegen()
{
    Scope ret;
    std::string cmpFnName = thread_pochiVMContext->m_curModule->GetNextAvailableFnName("orderby_cmpfn");
    {
        auto [fn, row1, row2] = NewFunction<bool(*)(uintptr_t, uintptr_t) noexcept>(cmpFnName);
        std::map<SqlRow*, PochiVM::Variable<uintptr_t>*> bakRowMap;
        bakRowMap.swap(thread_queryCodegenContext.m_rowMap);

        thread_queryCodegenContext.m_rowMap[m_row] = &row1;
        std::vector<ValueVT> r1v1, r1v2;
        for (size_t i = 0; i < m_orderByFields.size(); i++)
        {
            r1v1.push_back(m_orderByFields[i]->Codegen());
            r1v2.push_back(m_orderByFields[i]->Codegen());
        }

        thread_queryCodegenContext.m_rowMap[m_row] = &row2;
        std::vector<ValueVT> r2v1, r2v2;
        for (size_t i = 0; i < m_orderByFields.size(); i++)
        {
            r2v1.push_back(m_orderByFields[i]->Codegen());
            r2v2.push_back(m_orderByFields[i]->Codegen());
        }

        for (size_t i = 0; i < m_orderByFields.size(); i++)
        {
            if (!r1v1[i].GetType().IsPointerType())
            {
                fn.GetBody().Append(
                    If(!CreateComparisonExpr(r1v1[i], r2v1[i], AstComparisonExprType::EQUAL)).Then(
                        Return(CreateComparisonExpr(r1v2[i], r2v2[i], AstComparisonExprType::LESS_THAN))));
            }
            else
            {
                TestAssert(r1v1[i].HasType(TypeId::Get<char*>()));
                TestAssert(r2v1[i].HasType(TypeId::Get<char*>()));
                Value<char*> v1(r1v1[i]);
                Value<char*> v2(r2v1[i]);
                auto i1 = fn.NewVariable<char*>();
                auto i2 = fn.NewVariable<char*>();
                fn.GetBody().Append(Block(
                    Declare(i1, v1),
                    Declare(i2, v2),
                    While(*i1 != '\0' && *i1 == *i2).Do(
                        Assign(i1, i1 + 1),
                        Assign(i2, i2 + 1)
                    ),
                    If (*i1 != *i2).Then(
                        Return(*i1 < *i2)
                    )
                ));
            }
        }

        fn.GetBody().Append(Return(false));

        bakRowMap.swap(thread_queryCodegenContext.m_rowMap);
    }

    Variable<std::vector<uintptr_t>>& v = *m_container->m_var;
    Value<uintptr_t> cmpFnPtr = GetGeneratedFunctionPointer(cmpFnName);
    return Scope(CallFreeFn::MiniDbBackend::SortRows(v.data(), v.size(), cmpFnPtr));
}

inline Scope WARN_UNUSED SqlTableRowGenerator::Codegen(Scope insertPoint)
{
    Scope ret;
    Variable<std::vector<uintptr_t>*>& v = *m_src->m_var;
    Variable<uintptr_t>* row = new Variable<uintptr_t>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>());
    TestAssert(!thread_queryCodegenContext.m_rowMap.count(m_dst));
    thread_queryCodegenContext.m_rowMap[m_dst] = row;
    auto length = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
    auto i = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
    auto data = thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t*>();
    insertPoint.Append(Block(
        Declare(data, v->data()),
        Declare(length, v->size()),
        For(Declare(i, Literal<size_t>(0)), i < length, Increment(i)).Do(
            Declare(*row, data[i]),
            ret
        )
    ));
    return ret;
}

inline Scope WARN_UNUSED AggregationRowGenerator::Codegen(Scope insertPoint)
{
    Variable<uintptr_t>* v = new Variable<uintptr_t>(*m_src->m_var);
    thread_queryCodegenContext.m_rowMap[m_dst] = v;
    return insertPoint;
}

inline Scope WARN_UNUSED GroupByHashTableGenerator::Codegen(Scope insertPoint)
{
    Variable<uintptr_t>* row = new Variable<uintptr_t>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>());
    auto tmp = thread_queryCodegenContext.m_curFunction->NewVariable<std::vector<uintptr_t>>();
    thread_queryCodegenContext.m_rowMap[m_dst] = row;
    if (x_use_cpp_data_structure)
    {
        Variable<QEHashTable>& v = *m_container->m_var;
        // Variable<QEHashTable::iterator> it = thread_queryCodegenContext.m_curFunction->NewVariable<QEHashTable::iterator>();
        // Variable<QEHashTable::iterator> end = thread_queryCodegenContext.m_curFunction->NewVariable<QEHashTable::iterator>();
        auto length = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
        auto i = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
        auto data = thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t*>();
        Scope ret;
        insertPoint.Append(Block(
            Declare(tmp),
            CallFreeFn::MiniDbBackend::DumpHashTable(v, tmp),
            Declare(data, tmp.data()),
            Declare(length, tmp.size()),
            For(Declare(i, Literal<size_t>(0)), i < length, Increment(i)).Do(
                Declare(*row, data[i]),
                ret
            )
            // Declare(end, v.end()),
            // For(Declare(it, v.begin()), it != end, it++).Do(
            //     Declare(*row, it->second()),
            //     ret
            // )
        ));
        return ret;
    }
    else
    {
        Scope ret;
        auto i = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
        insertPoint.Append(Block(
            For(Declare(i, static_cast<size_t>(0)), i < (*m_container->m_tableSize), Increment(i)).Do(
                If((*m_container->m_keys)[i] != static_cast<uintptr_t>(0)).Then(
                    Declare(*row, (*m_container->m_values)[i]),
                    ret
                )
            )
        ));
        return ret;
    }
}

inline Scope WARN_UNUSED TempTableRowGenerator::Codegen(Scope insertPoint)
{
    Variable<std::vector<uintptr_t>>& v = *m_container->m_var;
    Variable<uintptr_t>* row = new Variable<uintptr_t>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>());
    thread_queryCodegenContext.m_rowMap[m_dst] = row;

    Scope ret;
    auto length = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
    auto i = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
    auto data = thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t*>();
    insertPoint.Append(Block(
        Declare(data, v.data()),
        Declare(length, v.size()),
        For(Declare(i, Literal<size_t>(0)), i < length, Increment(i)).Do(
            Declare(*row, data[i]),
            ret
        )
    ));
    return ret;
}

inline void TempTableRowOutputter::Codegen(Scope insertPoint)
{
    Variable<std::vector<uintptr_t>>& v = *m_dst->m_var;
    Variable<uintptr_t>& row = thread_queryCodegenContext.GetRow(m_src);
    insertPoint.Append(v.push_back(row));
}

inline void AggregationOutputter::Codegen(Scope insertPoint)
{
    Variable<uintptr_t>* v = new Variable<uintptr_t>(*m_container->m_var);
    thread_queryCodegenContext.m_rowMap[m_row] = v;
    insertPoint.Append(m_row->EmitUpdate());
}

inline void GroupByHashTableOutputter::Codegen(Scope insertPoint)
{
    Variable<uintptr_t>& inputRow = thread_queryCodegenContext.GetRow(m_inputRow);
    auto aggRowAddr = thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t*>();
    if (x_use_cpp_data_structure)
    {
        Variable<QEHashTable>& ht = *m_container->m_var;
        insertPoint.Append(Declare(aggRowAddr, ht[inputRow].Addr()));
    }
    else
    {
        auto slot = thread_queryCodegenContext.m_curFunction->NewVariable<size_t>();
        insertPoint.Append(Block(
            m_container->EmitProbe(inputRow, slot),
            If((*m_container->m_keys)[slot] == Literal<size_t>(0)).Then(
                Assign((*m_container->m_keys)[slot], inputRow),
                Assign((*m_container->m_values)[slot], Literal<size_t>(0)),
                Increment(*m_container->m_count)
            ),
            Declare(aggRowAddr, (*m_container->m_values) + slot)
        ));
    }
    Block initBlock, updateBlock;
    insertPoint.Append(Block(
        If(*aggRowAddr == Literal<uintptr_t>(0)).Then(
            initBlock
        ),
        updateBlock
    ));

    {
        auto newAggRow = thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>();
        initBlock.Append(Declare(newAggRow, m_container->m_alloc->Allocate(Literal<size_t>(m_aggregationRow->GetRowSize()))));
        TestAssert(!thread_queryCodegenContext.m_rowMap.count(m_aggregationRow));
        thread_queryCodegenContext.m_rowMap[m_aggregationRow] = &newAggRow;
        initBlock.Append(Assign(*aggRowAddr, newAggRow));
        initBlock.Append(m_aggregationRow->EmitInit());
        // initBlock.Append(m_aggregationRow->EmitUpdate());
        thread_queryCodegenContext.m_rowMap.erase(thread_queryCodegenContext.m_rowMap.find(m_aggregationRow));
    }

    {
        auto newAggRow = thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>();
        updateBlock.Append(Declare(newAggRow, *aggRowAddr));
        TestAssert(!thread_queryCodegenContext.m_rowMap.count(m_aggregationRow));
        thread_queryCodegenContext.m_rowMap[m_aggregationRow] = &newAggRow;
        updateBlock.Append(m_aggregationRow->EmitUpdate());
        thread_queryCodegenContext.m_rowMap.erase(thread_queryCodegenContext.m_rowMap.find(m_aggregationRow));
    }
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

inline Scope WARN_UNUSED SqlProjectionProcessor::Codegen(Scope insertPoint)
{
    Scope ret;
    Variable<uintptr_t>* row = new Variable<uintptr_t>(thread_queryCodegenContext.m_curFunction->NewVariable<uintptr_t>());
    auto alloc = thread_queryCodegenContext.m_curFunction->NewVariable<QueryExecutionTempAllocator>();

    thread_queryCodegenContext.m_rowMap[m_output] = row;

    thread_queryCodegenContext.m_fnStartBlock->Append(Declare(alloc));
    insertPoint.Append(Block(
        Declare(*row, alloc.Allocate(Literal<size_t>(m_output->GetRowSize()))),
        m_output->Codegen(),
        ret
    ));
    return ret;
}

inline void QueryPlan::CodeGen()
{
    TestAssert(thread_queryCodegenContext.m_sqlResultPrinter == nullptr);
    TestAssert(thread_queryCodegenContext.m_rowMap.empty());
    TestAssert(thread_queryCodegenContext.m_curFunction == nullptr);
    TestAssert(thread_queryCodegenContext.m_fnStartBlock == nullptr);
    auto [fn, printer] = NewFunction<void(*)(SqlResultPrinter*)>("execute_query");
    thread_queryCodegenContext.m_curFunction = &fn;
    thread_queryCodegenContext.m_sqlResultPrinter = &printer;
    Scope body;
    fn.SetBody(body);
    Block startBlock;
    thread_queryCodegenContext.m_fnStartBlock = &startBlock;
    body.Append(startBlock);
    for (SqlRowContainer* container : m_neededContainers)
    {
        body.Append(container->EmitDeclaration());
    }
    for (QueryPlanPipelineStageBase* stage : m_stages)
    {
        body.Append(stage->Codegen());
    }
    thread_queryCodegenContext.m_sqlResultPrinter = nullptr;
    TestAssert(thread_queryCodegenContext.m_rowMap.empty());
    thread_queryCodegenContext.m_curFunction = nullptr;
    thread_queryCodegenContext.m_fnStartBlock = nullptr;
}

}   // namespace MiniDbBackend
