select 
c_custkey, 
replace(to_base64(c_name), '\n', ''),
replace(to_base64(c_address), '\n', ''),
c_nationkey, 
replace(to_base64(c_phone), '\n', ''),
c_acctbal, 
replace(to_base64(c_mktsegment), '\n', ''),
replace(to_base64(c_comment), '\n', '') 
from customer order by c_custkey into outfile '/var/lib/mysql-files/customer.txt';

select
l_orderkey,
l_partkey,
l_suppkey,
l_linenumber,
l_quantity,
l_extendedprice,
l_discount,
l_tax,
replace(to_base64(l_returnflag), '\n', ''),
replace(to_base64(l_linestatus), '\n', ''),
UNIX_TIMESTAMP(l_shipdate),
UNIX_TIMESTAMP(l_commitdate),
UNIX_TIMESTAMP(l_receiptdate),
replace(to_base64(l_shipinstruct), '\n', ''),
replace(to_base64(l_shipmode), '\n', ''),
replace(to_base64(l_comment), '\n', '')
from lineitem order by l_orderkey into outfile '/var/lib/mysql-files/lineitem.txt';

select
n_nationkey,
replace(to_base64(n_name), '\n', ''),
n_regionkey,
replace(to_base64(n_comment), '\n', '')
from nation order by n_nationkey into outfile '/var/lib/mysql-files/nation.txt';

select
o_orderkey,
o_custkey,
replace(to_base64(o_orderstatus), '\n', ''),
o_totalprice,
UNIX_TIMESTAMP(o_orderdate),
replace(to_base64(o_orderpriority), '\n', ''),
replace(to_base64(o_clerk ), '\n', ''),
o_shippriority,
replace(to_base64(o_comment), '\n', '')
from orders order by o_orderkey into outfile '/var/lib/mysql-files/orders.txt';

select
p_partkey,
replace(to_base64(p_name), '\n', ''),   
replace(to_base64(p_mfgr), '\n', ''),  
replace(to_base64(p_brand), '\n', ''), 
replace(to_base64(p_type), '\n', ''), 
p_size,
replace(to_base64(p_container), '\n', ''), 
p_retailprice,
replace(to_base64(p_comment), '\n', '')
from part order by p_partkey into outfile '/var/lib/mysql-files/part.txt';

select
ps_partkey, 
ps_suppkey, 
ps_availqty,
ps_supplycost,
replace(to_base64(ps_comment), '\n', '')
from partsupp order by ps_partkey into outfile '/var/lib/mysql-files/partsupp.txt';

select
r_regionkey,
replace(to_base64(r_name), '\n', ''), 
replace(to_base64(r_comment), '\n', '')
from region order by r_regionkey into outfile '/var/lib/mysql-files/region.txt';

select
s_suppkey,
replace(to_base64(s_name), '\n', ''), 
replace(to_base64(s_address), '\n', ''), 
s_nationkey,
replace(to_base64(s_phone), '\n', ''), 
s_acctbal,
replace(to_base64(s_comment ), '\n', '')
from supplier order by s_suppkey into outfile '/var/lib/mysql-files/supplier.txt';

