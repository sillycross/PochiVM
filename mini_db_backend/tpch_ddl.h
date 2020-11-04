#pragma once

#include "pochivm/common.h"
#include "db_dump_reader.h"

namespace MiniDbBackend
{

// These table row structs are used only for the purpose of loading table.
// We don't use them in codegen, since the assumption is that table schema is completely runtime-known.
//
struct TpchCustomerTableRow
{
    int c_custkey;
    char c_name[26];
    char c_address[41];
    int c_nationkey;
    char c_phone[16];
    double c_acctbal;
    char c_mktsegment[11];
    char c_comment[118];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt32(fp, c_custkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, c_name, 26));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, c_address, 41));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, c_nationkey));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, c_phone, 16));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, c_acctbal));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, c_mktsegment, 11));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, c_comment, 118));
        return true;
    }
};

struct TpchLineItemTableRow
{
    int64_t l_orderkey;
    int l_partkey;
    int l_suppkey;
    int l_linenumber;
    double l_quantity;
    double l_extendedprice;
    double l_discount;
    double l_tax;
    char l_returnflag[2];
    char l_linestatus[2];
    uint32_t l_shipdate;
    uint32_t l_commitdate;
    uint32_t l_receiptdate;
    char l_shipinstruct[26];
    char l_shipmode[11];
    char l_comment[45];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt64(fp, l_orderkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, l_partkey));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, l_suppkey));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, l_linenumber));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, l_quantity));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, l_extendedprice));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, l_discount));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, l_tax));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, l_returnflag, 2));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, l_linestatus, 2));
        ReleaseAssert(TpchDbDumpReader::LoadUInt32(fp, l_shipdate));
        ReleaseAssert(TpchDbDumpReader::LoadUInt32(fp, l_commitdate));
        ReleaseAssert(TpchDbDumpReader::LoadUInt32(fp, l_receiptdate));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, l_shipinstruct, 26));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, l_shipmode, 11));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, l_comment, 45));
        return true;
    }
};

struct TpchNationTableRow
{
    int n_nationkey;
    char n_name[26];
    int n_regionkey;
    char n_comment[153];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt32(fp, n_nationkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, n_name, 26));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, n_regionkey));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, n_comment, 153));
        return true;
    }
};

struct TpchOrdersTableRow
{
    int64_t o_orderkey;
    int o_custkey;
    char o_orderstatus[2];
    double o_totalprice;
    uint32_t o_orderdate;
    char o_orderpriority[16];
    char o_clerk[16];
    int o_shippriority;
    char o_comment[80];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt64(fp, o_orderkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, o_custkey));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, o_orderstatus, 2));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, o_totalprice));
        ReleaseAssert(TpchDbDumpReader::LoadUInt32(fp, o_orderdate));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, o_orderpriority, 16));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, o_clerk, 16));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, o_shippriority));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, o_comment, 80));
        return true;
    }
};

struct TpchPartTableRow
{
    int p_partkey;
    char p_name[56];
    char p_mfgr[26];
    char p_brand[11];
    char p_type[26];
    int p_size;
    char p_container[11];
    double p_retailprice;
    char p_comment[24];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt32(fp, p_partkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, p_name, 56));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, p_mfgr, 26));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, p_brand, 11));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, p_type, 26));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, p_size));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, p_container, 11));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, p_retailprice));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, p_comment, 24));
        return true;
    }
};

struct TpchPartSuppTableRow
{
    int ps_partkey;
    int ps_suppkey;
    int ps_availqty;
    double ps_supplycost;
    char ps_comment[200];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt32(fp, ps_partkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, ps_suppkey));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, ps_availqty));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, ps_supplycost));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, ps_comment, 200));
        return true;
    }
};

struct TpchRegionTableRow
{
    int r_regionkey;
    char r_name[26];
    char r_comment[153];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt32(fp, r_regionkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, r_name, 26));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, r_comment, 153));
        return true;
    }
};

struct TpchSupplierTableRow
{
    int s_suppkey;
    char s_name[26];
    char s_address[41];
    int s_nationkey;
    char s_phone[16];
    double s_acctbal;
    char s_comment[102];

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt32(fp, s_suppkey)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, s_name, 26));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, s_address, 41));
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, s_nationkey));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, s_phone, 16));
        ReleaseAssert(TpchDbDumpReader::LoadDouble(fp, s_acctbal));
        ReleaseAssert(TpchDbDumpReader::LoadString(fp, s_comment, 102));
        return true;
    }
};

struct TestTable1Row
{
    int a;
    int b;

    bool WARN_UNUSED LoadRow(FILE* fp)
    {
        if (!TpchDbDumpReader::LoadInt32(fp, a)) { return false; }
        ReleaseAssert(TpchDbDumpReader::LoadInt32(fp, b));
        return true;
    }
};

}   // namespace MiniDbBackend
