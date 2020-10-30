#include "gtest/gtest.h"

#include "mini_db_backend/tpch_ddl.h"

using namespace MiniDbBackend;

namespace {

template<typename RowType>
int TryLoadTable(std::string fileName)
{
    FILE* fp = fopen(fileName.c_str(), "r");
    ReleaseAssert(fp != nullptr);
    RowType row;
    int count = 0;
    while (row.LoadRow(fp))
    {
        count++;
    }
    fclose(fp);
    return count;
}

}   // anonymous namespace

TEST(MiniDbBackendUnitTest, LoadDatabase)
{
    ReleaseAssert(TryLoadTable<TpchCustomerTableRow>("tpch_data/customer.txt") == 46611);
    ReleaseAssert(TryLoadTable<TpchLineItemTableRow>("tpch_data/lineitem.txt") == 1873428);
    ReleaseAssert(TryLoadTable<TpchNationTableRow>("tpch_data/nation.txt") == 1);
    ReleaseAssert(TryLoadTable<TpchOrdersTableRow>("tpch_data/orders.txt") == 468418);
    ReleaseAssert(TryLoadTable<TpchPartTableRow>("tpch_data/part.txt") == 61884);
    ReleaseAssert(TryLoadTable<TpchPartSuppTableRow>("tpch_data/partsupp.txt") == 247536);
    ReleaseAssert(TryLoadTable<TpchRegionTableRow>("tpch_data/region.txt") == 1);
    ReleaseAssert(TryLoadTable<TpchSupplierTableRow>("tpch_data/supplier.txt") == 3170);
}
