#pragma once

#include "tpch_ddl.h"
#include "query_plan_ast.h"

namespace MiniDbBackend
{

// We use a struct for table schema only for the simplicity of loading database.
// The runtime does not access the definition of those structs, as is the case in a real database.
//
inline TpchSqlTable<TpchCustomerTableRow> x_tpchtable_customer;
inline TpchSqlTable<TpchLineItemTableRow> x_tpchtable_lineitem;
inline TpchSqlTable<TpchNationTableRow> x_tpchtable_nation;
inline TpchSqlTable<TpchOrdersTableRow> x_tpchtable_orders;
inline TpchSqlTable<TpchPartTableRow> x_tpchtable_part;
inline TpchSqlTable<TpchPartSuppTableRow> x_tpchtable_partsupp;
inline TpchSqlTable<TpchRegionTableRow> x_tpchtable_region;
inline TpchSqlTable<TpchSupplierTableRow> x_tpchtable_supplier;

inline TpchSqlTable<TestTable1Row> x_unittesttable_table1;

inline void TpchLoadDatabase()
{
    x_tpchtable_customer.LoadData("tpch_data/customer.txt");
    x_tpchtable_lineitem.LoadData("tpch_data/lineitem.txt");
    x_tpchtable_nation.LoadData("tpch_data/nation.txt");
    x_tpchtable_orders.LoadData("tpch_data/orders.txt");
    x_tpchtable_part.LoadData("tpch_data/part.txt");
    x_tpchtable_partsupp.LoadData("tpch_data/partsupp.txt");
    x_tpchtable_region.LoadData("tpch_data/region.txt");
    x_tpchtable_supplier.LoadData("tpch_data/supplier.txt");
    x_unittesttable_table1.LoadData("tpch_data/unittest_table1.txt");
}

inline void TpchLoadUnitTestDatabase()
{
    x_unittesttable_table1.LoadData("tpch_data/unittest_table1.txt");
}

inline __attribute__((__used__)) std::vector<uintptr_t>* GetTpchTableHelper(int table)
{
    if (table == TPCH_CUSTOMER) { return &x_tpchtable_customer.m_data; }
    if (table == TPCH_LINEITEM) { return &x_tpchtable_lineitem.m_data; }
    if (table == TPCH_NATION) { return &x_tpchtable_nation.m_data; }
    if (table == TPCH_ORDERS) { return &x_tpchtable_orders.m_data; }
    if (table == TPCH_PART) { return &x_tpchtable_part.m_data; }
    if (table == TPCH_PARTSUPP) { return &x_tpchtable_partsupp.m_data; }
    if (table == TPCH_REGION) { return &x_tpchtable_region.m_data; }
    if (table == TPCH_SUPPLIER) { return &x_tpchtable_supplier.m_data; }
    if (table == UNITTEST_TABLE1) { return &x_unittesttable_table1.m_data; }
    ReleaseAssert(false);
}

}   // namespace MiniDbBackend
