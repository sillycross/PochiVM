#pragma once

#include "pochivm/pochivm.h"

namespace MiniDbBackend
{

constexpr bool x_use_cpp_data_structure = false;

class SqlValueBase
{
public:
    SqlValueBase(PochiVM::TypeId typeId) : m_typeId(typeId) { }
    virtual ~SqlValueBase() { }

    PochiVM::TypeId GetType() const { return m_typeId; }

    virtual PochiVM::ValueVT WARN_UNUSED Codegen() = 0;

private:
    PochiVM::TypeId m_typeId;
};

class DbTableBase
{
public:
    DbTableBase(size_t tableRowSize)
        : m_tableRowSize(tableRowSize)
    { }

    std::vector<uintptr_t> m_data;
    size_t m_tableRowSize;
};

enum class SqlRowType
{
    TABLE_ROW,
    PROJECTION_ROW,
    AGGREGATION_ROW
};

class SqlRow
{
public:
    SqlRow(SqlRowType rowType)
        : m_rowType(rowType)
        , m_rowSize(static_cast<size_t>(-1))
    { }

    virtual ~SqlRow() { }

    SqlRowType GetRowType() const { return m_rowType; }

    size_t GetRowSize() const
    {
        TestAssert(m_rowSize != static_cast<size_t>(-1));
        return m_rowSize;
    }

    void SetRowSize(size_t rowSize)
    {
        TestAssert(m_rowSize == static_cast<size_t>(-1) && rowSize != static_cast<size_t>(-1));
        m_rowSize = rowSize;
    }

    PochiVM::Variable<uintptr_t>& GetAddress();

private:
    SqlRowType m_rowType;
    size_t m_rowSize;
};

class SqlTableRow : public SqlRow
{
public:
    SqlTableRow(DbTableBase* owner)
        : SqlRow(SqlRowType::TABLE_ROW)
        , m_owner(owner)
    {
        SetRowSize(owner->m_tableRowSize);
    }

    DbTableBase* m_owner;
};

// Represents a value of a field in a sql row
//
class SqlField : public SqlValueBase
{
public:
    SqlField(PochiVM::TypeId typeId, SqlRow* owner, size_t offset)
        : SqlValueBase(typeId), m_owner(owner), m_offset(offset)
    { }

    virtual PochiVM::ValueVT WARN_UNUSED Codegen() override final;
    PochiVM::ReferenceVT WARN_UNUSED CodegenForWrite();

    SqlRow* m_owner;
    size_t m_offset;
};

// Only used for loading data and constructing SQL query plan.
//
template<typename RowDef>
class TpchSqlTableRow : public SqlTableRow
{
public:
    TpchSqlTableRow(DbTableBase* owner)
        : SqlTableRow(owner)
    { }

    template<typename FieldType>
    SqlField* GetSqlField(FieldType RowDef::* memPtr)
    {
        RowDef* rowPtr = reinterpret_cast<RowDef*>(0x1000000);
        size_t offset = reinterpret_cast<uintptr_t>(&(rowPtr->*memPtr)) - reinterpret_cast<uintptr_t>(rowPtr);
        return new SqlField(PochiVM::TypeId::Get<std::decay_t<FieldType>>(), this, offset);
    }
};

// Only used for loading data and constructing SQL query plan.
//
template<typename RowDef>
class TpchSqlTable : public DbTableBase
{
public:
    TpchSqlTable()
        : DbTableBase(sizeof(RowDef))
        , m_dataLoaded(false)
    { }

    void LoadData(const std::string& filepath)
    {
        if (m_dataLoaded) { return; }
        m_dataLoaded = true;
        m_data.clear();
        m_alloc.Reset();

        FILE* fp = fopen(filepath.c_str(), "r");
        if (fp == nullptr)
        {
            int err = errno;
            fprintf(stderr, "[DbTableReader] Failed to open file %s, error %d(%s)\n.",
                    filepath.c_str(), err, strerror(err));
            abort();
        }
        Auto(fclose(fp));

        while (true)
        {
            RowDef* row = new (m_alloc) RowDef();
            if (!row->LoadRow(fp))
            {
                break;
            }
            m_data.push_back(reinterpret_cast<uintptr_t>(row));
        }
    }

    TpchSqlTableRow<RowDef>* GetTableRow()
    {
        return new TpchSqlTableRow<RowDef>(this);
    }

private:
    PochiVM::TempArenaAllocator m_alloc;
    bool m_dataLoaded;
};

// A SQL projection row
//
class SqlProjectionRow : public SqlRow
{
public:
    SqlProjectionRow(std::initializer_list<SqlValueBase*> initList)
        : SqlRow(SqlRowType::PROJECTION_ROW)
        , m_values(initList)
    {
        size_t offset = 0;
        for (SqlValueBase* value : m_values)
        {
            size_t size = value->GetType().Size();
            size_t alignment = 8;
            alignment = std::min(alignment, size);
            offset = (offset + alignment - 1) / alignment * alignment;
            SqlField* pf = new SqlField(value->GetType(), this, offset);
            offset += size;
            m_fields.push_back(pf);
        }
        SetRowSize(offset);
    }

    SqlField* GetSqlProjectionField(size_t ordinal)
    {
        TestAssert(ordinal < m_fields.size());
        return m_fields[ordinal];
    }

    PochiVM::Block WARN_UNUSED Codegen();

private:
    std::vector<SqlValueBase*> m_values;
    std::vector<SqlField*> m_fields;
};

// A SQL aggregation row
//
class SqlAggregationRow : public SqlRow
{
public:
    SqlAggregationRow(std::initializer_list<SqlValueBase*> initList)
        : SqlRow(SqlRowType::AGGREGATION_ROW)
        , m_initialValues(initList)
    {
        size_t offset = 0;
        for (SqlValueBase* value : m_initialValues)
        {
            size_t size = value->GetType().Size();
            size_t alignment = 8;
            alignment = std::min(alignment, size);
            offset = (offset + alignment - 1) / alignment * alignment;
            SqlField* pf = new SqlField(value->GetType(), this, offset);
            offset += size;
            m_fields.push_back(pf);
            m_updateExprs.push_back(nullptr);
        }
        SetRowSize(offset);
    }

    SqlField* GetSqlProjectionField(size_t ordinal)
    {
        TestAssert(ordinal < m_fields.size());
        return m_fields[ordinal];
    }

    void SetUpdateExpr(size_t ordinal, SqlValueBase* value)
    {
        TestAssert(ordinal < m_updateExprs.size() && m_updateExprs[ordinal] == nullptr);
        m_updateExprs[ordinal] = value;
    }

    PochiVM::Block WARN_UNUSED EmitInit();
    PochiVM::Block WARN_UNUSED EmitUpdate();

private:
    std::vector<SqlValueBase*> m_initialValues;
    std::vector<SqlValueBase*> m_updateExprs;
    std::vector<SqlField*> m_fields;
};

// A SQL literal value
//
class SqlLiteral : public SqlValueBase
{
public:
    template<typename T>
    SqlLiteral(T value)
        : SqlValueBase(PochiVM::TypeId::Get<T>())
    {
        TestAssert(!GetType().IsPointerType() && !GetType().IsVoid());
        static_assert(sizeof(T) <= 8);
        memcpy(m_value, &value, sizeof(T));
    }

    virtual PochiVM::ValueVT WARN_UNUSED Codegen() override final;

private:
    alignas(8) char m_value[8];
};

class SqlArithmeticOperator : public SqlValueBase
{
public:
    SqlArithmeticOperator(PochiVM::TypeId typeId,
                          PochiVM::AstArithmeticExprType arithType,
                          SqlValueBase* lhs,
                          SqlValueBase* rhs)
        : SqlValueBase(typeId)
        , m_arithType(arithType)
        , m_lhs(lhs)
        , m_rhs(rhs)
    {
        TestAssert(!GetType().IsPointerType() && !GetType().IsVoid());
        TestAssert(lhs->GetType() == GetType());
        TestAssert(rhs->GetType() == GetType());
    }

    virtual PochiVM::ValueVT WARN_UNUSED Codegen() override final;

private:
    PochiVM::AstArithmeticExprType m_arithType;
    SqlValueBase* m_lhs;
    SqlValueBase* m_rhs;
};

class SqlComparisonOperator : public SqlValueBase
{
public:
    SqlComparisonOperator(PochiVM::AstComparisonExprType comparisonType,
                          SqlValueBase* lhs,
                          SqlValueBase* rhs)
        : SqlValueBase(PochiVM::TypeId::Get<bool>())
        , m_comparisonType(comparisonType)
        , m_lhs(lhs)
        , m_rhs(rhs)
    {
        TestAssert(!GetType().IsPointerType() && !GetType().IsVoid());
        TestAssert(lhs->GetType() == rhs->GetType());
    }

    virtual PochiVM::ValueVT WARN_UNUSED Codegen() override final;

private:
    PochiVM::AstComparisonExprType m_comparisonType;
    SqlValueBase* m_lhs;
    SqlValueBase* m_rhs;
};

class SqlBinaryLogicalOperator : public SqlValueBase
{
public:
    SqlBinaryLogicalOperator(bool isAnd, SqlValueBase* lhs, SqlValueBase* rhs)
        : SqlValueBase(PochiVM::TypeId::Get<bool>())
        , m_isAnd(isAnd)
        , m_lhs(lhs)
        , m_rhs(rhs)
    { }

    virtual PochiVM::ValueVT WARN_UNUSED Codegen() override final;

private:
    bool m_isAnd;
    SqlValueBase* m_lhs;
    SqlValueBase* m_rhs;
};

class SqlCastOperator : public SqlValueBase
{
public:
    SqlCastOperator(PochiVM::TypeId dstType, SqlValueBase* operand)
        : SqlValueBase(dstType)
        , m_operand(operand)
    { }

    virtual PochiVM::ValueVT WARN_UNUSED Codegen() override final;

private:
    SqlValueBase* m_operand;
};

class SqlRowContainer
{
public:
    virtual ~SqlRowContainer() { }
    virtual PochiVM::Block WARN_UNUSED EmitDeclaration() = 0;
};

class SqlTableContainer : public SqlRowContainer
{
public:
    SqlTableContainer(const std::string& tableName)
        : m_tableName(tableName)
        , m_var(nullptr)
    { }

    virtual PochiVM::Block WARN_UNUSED EmitDeclaration() override final;

    std::string m_tableName;
    PochiVM::Variable<std::vector<uintptr_t>*>* m_var;
};

class TempTableContainer : public SqlRowContainer
{
public:
    TempTableContainer()
        : m_var(nullptr)
    { }

    virtual PochiVM::Block WARN_UNUSED EmitDeclaration() override final;

    PochiVM::Variable<std::vector<uintptr_t>>* m_var;
};

class AggregatedRowContainer : public SqlRowContainer
{
public:
    AggregatedRowContainer(SqlAggregationRow* row) : m_var(nullptr), m_row(row) { }

    virtual PochiVM::Block WARN_UNUSED EmitDeclaration() override final;

    PochiVM::Variable<uintptr_t>* m_var;
    SqlAggregationRow* m_row;
};

class GroupByHashTableContainer : public SqlRowContainer
{
public:
    GroupByHashTableContainer()
        : m_var(nullptr)
    { }

    virtual PochiVM::Block WARN_UNUSED EmitDeclaration() override final;

    PochiVM::Block WARN_UNUSED EmitProbe(const PochiVM::Variable<uintptr_t>& input, const PochiVM::Variable<size_t>& output);

    using CmpFnPrototype = bool(*)(uintptr_t, uintptr_t) noexcept;
    using HashFnPrototype = size_t(*)(uintptr_t) noexcept;

    std::vector<SqlField*> m_groupByFields;
    PochiVM::Variable<QueryExecutionTempAllocator>* m_alloc;
    // If use cpp data structure
    //
    PochiVM::Variable<QEHashTable>* m_var;
    // If not use cpp data structure
    //
    PochiVM::Variable<uintptr_t*>* m_keys;
    PochiVM::Variable<uintptr_t*>* m_values;
    PochiVM::Variable<size_t>* m_tableSize;
    PochiVM::Variable<size_t>* m_count;
    std::string m_cmpFnName;
    std::string m_hashFnName;

    SqlRow* m_row;
};

// A piece of logic which creates a stream of SQL rows from some source
//
class QueryPlanRowGenerator
{
public:
    virtual ~QueryPlanRowGenerator() { }

    // Returns a scope where processor logic can be inserted
    //
    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) = 0;
};

// A piece of logic which takes in one or more SQL rows, and (optionally) output a SQL row to an Outputter
//
class QueryPlanRowProcessor
{
public:
    virtual ~QueryPlanRowProcessor() { }

    // Returns a scope where outputter logic can be inserted
    //
    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) = 0;
};

// A piece of logic which takes in a SQL row, and store the row into some destination.
//
class QueryPlanRowOutputter
{
public:
    virtual ~QueryPlanRowOutputter() { }

    virtual void Codegen(PochiVM::Scope insertPoint) = 0;
};

class QueryPlanPipelineStageBase
{
public:
    virtual ~QueryPlanPipelineStageBase() { }
    virtual PochiVM::Scope WARN_UNUSED Codegen() = 0;
};

// A function executing a single stage of query plan
// It takes two or more parameters, which is the input container and output container,
// and optionally a list of other context variables used
//
class QueryPlanPipelineStage : public QueryPlanPipelineStageBase
{
public:
    virtual PochiVM::Scope WARN_UNUSED Codegen() override final;

    QueryPlanRowGenerator* m_generator;
    std::vector<QueryPlanRowProcessor*> m_processor;
    QueryPlanRowOutputter* m_outputter;
};

class QueryPlanOrderByStage : public QueryPlanPipelineStageBase
{
public:
    virtual PochiVM::Scope WARN_UNUSED Codegen() override final;

    TempTableContainer* m_container;
    std::vector<SqlField*> m_orderByFields;
    SqlRow* m_row;
};

class SqlTableRowGenerator : public QueryPlanRowGenerator
{
public:
    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) override final;

    SqlTableContainer* m_src;
    SqlRow* m_dst;
};

class AggregationRowGenerator : public QueryPlanRowGenerator
{
public:
    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) override final;

    AggregatedRowContainer* m_src;
    SqlRow* m_dst;
};

class GroupByHashTableGenerator : public QueryPlanRowGenerator
{
public:
    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) override final;

    GroupByHashTableContainer* m_container;
    SqlRow* m_dst;
};

class TempTableRowGenerator : public QueryPlanRowGenerator
{
public:
    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) override final;
    TempTableContainer* m_container;
    SqlRow* m_dst;
};

class TempTableRowOutputter : public QueryPlanRowOutputter
{
public:
    virtual void Codegen(PochiVM::Scope insertPoint) override final;

    SqlRow* m_src;
    TempTableContainer* m_dst;
};

class AggregationOutputter : public QueryPlanRowOutputter
{
public:
    virtual void Codegen(PochiVM::Scope insertPoint) override final;

    SqlAggregationRow* m_row;
    AggregatedRowContainer* m_container;
};

class GroupByHashTableOutputter : public QueryPlanRowOutputter
{
public:
    virtual void Codegen(PochiVM::Scope insertPoint) override final;

    SqlRow* m_inputRow;
    SqlAggregationRow* m_aggregationRow;
    GroupByHashTableContainer* m_container;
};

class SqlResultOutputter : public QueryPlanRowOutputter
{
public:
    virtual void Codegen(PochiVM::Scope insertPoint) override final;

    std::vector<SqlValueBase*> m_projections;
};

class SqlFilterProcessor : public QueryPlanRowProcessor
{
public:
    SqlFilterProcessor(SqlValueBase* condition)
        : m_condition(condition)
    { }

    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) override final;

    SqlValueBase* m_condition;
};

class SqlProjectionProcessor : public QueryPlanRowProcessor
{
public:
    virtual PochiVM::Scope WARN_UNUSED Codegen(PochiVM::Scope insertPoint) override final;

    SqlProjectionRow* m_output;
};

class QueryPlan
{
public:
    std::vector<SqlRowContainer*> m_neededContainers;
    std::vector<QueryPlanPipelineStageBase*> m_stages;

    void CodeGen();
};

struct QueryCodegenContext
{
    std::map<SqlRow*, PochiVM::Variable<uintptr_t>*> m_rowMap;
    PochiVM::Variable<SqlResultPrinter*>* m_sqlResultPrinter;
    PochiVM::Function* m_curFunction;
    PochiVM::Block* m_fnStartBlock;

    PochiVM::Variable<uintptr_t>& GetRow(SqlRow* which)
    {
        TestAssert(m_rowMap.count(which));
        return *m_rowMap[which];
    }
};

inline thread_local QueryCodegenContext thread_queryCodegenContext;

}   // namespace MinIDbBackend1

