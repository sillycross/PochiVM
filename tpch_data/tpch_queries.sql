-- Query 1

SELECT
    l_returnflag,
    l_linestatus,
    sum(l_quantity) as sum_qty,
    sum(l_extendedprice) as sum_base_price,
    sum(l_extendedprice * (1 - l_discount)) as sum_disc_price,
    sum(l_extendedprice * (1 - l_discount) * (1 + l_tax)) as sum_charge,
    avg(l_quantity) as avg_qty,
    avg(l_extendedprice) as avg_price,
    avg(l_discount) as avg_disc,
    count(*) as count_order
FROM
    lineitem
WHERE
    l_shipdate <= 904719600 -- date '1998-12-01' - interval '90' day
GROUP BY
    l_returnflag,
    l_linestatus
ORDER BY
    l_returnflag,
    l_linestatus; 
    
-- +--------------+--------------+-------------+----------------+------------------+--------------------+-----------+--------------+----------+-------------+
-- | l_returnflag | l_linestatus | sum_qty     | sum_base_price | sum_disc_price   | sum_charge         | avg_qty   | avg_price    | avg_disc | count_order |
-- +--------------+--------------+-------------+----------------+------------------+--------------------+-----------+--------------+----------+-------------+
-- | A            | F            | 11799894.00 | 17694594440.49 | 16809747534.0709 | 17481727589.532706 | 25.510857 | 38254.943185 | 0.050024 |      462544 |
-- | N            | F            |   307146.00 |   461415894.15 |   438370222.5784 |   456021204.722029 | 25.557164 | 38393.733912 | 0.050182 |       12018 |
-- | N            | O            | 23232475.00 | 34817796765.83 | 33077127289.6018 | 34400024394.850034 | 25.529154 | 38259.759511 | 0.049991 |      910037 |
-- | R            | F            | 11788252.00 | 17672163035.45 | 16787827349.3684 | 17459618619.351637 | 25.495168 | 38220.660065 | 0.050049 |      462372 |
-- +--------------+--------------+-------------+----------------+------------------+--------------------+-----------+--------------+----------+-------------+

-- Query 3

SELECT
    l_orderkey,
    sum(l_extendedprice * (1 - l_discount)) as revenue,
    o_orderdate,
    o_shippriority
FROM
    customer,
    orders,
    lineitem
WHERE
    c_mktsegment = 'BUILDING'
    AND c_custkey = o_custkey
    AND l_orderkey = o_orderkey
    AND o_orderdate < 795254400 -- date '1995-03-15'
    AND l_shipdate > 795254400 -- date '1995-03-15'
GROUP BY
    l_orderkey,
    o_orderdate,
    o_shippriority
ORDER BY
    revenue desc,
    o_orderdate
LIMIT 20;

-- +------------+-------------+-------------+----------------+
-- | l_orderkey | revenue     | o_orderdate | o_shippriority |
-- +------------+-------------+-------------+----------------+
-- |   56515460 | 386191.2899 | 1995-02-06  |              0 |
-- |   38559075 | 340093.9172 | 1995-01-29  |              0 |
-- |   28666663 | 301296.4547 | 1995-02-12  |              0 |
-- |   53469703 | 281069.9016 | 1995-03-03  |              0 |
-- |   45613187 | 266925.3961 | 1995-02-14  |              0 |
-- |   52263431 | 254077.8476 | 1995-03-06  |              0 |
-- |   26894336 | 246784.1367 | 1995-02-25  |              0 |
-- |   50551300 | 224597.1293 | 1995-03-01  |              0 |
-- |   43752419 | 220553.8837 | 1995-01-07  |              0 |
-- |   19974213 | 185434.4491 | 1995-02-19  |              0 |
-- |   22083299 | 183986.8480 | 1995-02-14  |              0 |
-- |   37017408 | 183120.6386 | 1995-02-11  |              0 |
-- |   26112001 | 180333.3156 | 1995-01-18  |              0 |
-- |    4143395 | 178770.1012 | 1995-02-02  |              0 |
-- |   31777029 | 177249.8452 | 1995-01-23  |              0 |
-- |   37973732 | 174125.6169 | 1995-02-05  |              0 |
-- |   33717218 | 171613.5989 | 1995-02-05  |              0 |
-- |   31945287 | 167527.5350 | 1995-02-19  |              0 |
-- |   14591234 | 162619.1652 | 1995-03-11  |              0 |
-- |   10786852 | 161931.2401 | 1995-02-13  |              0 |
-- +------------+-------------+-------------+----------------+

-- Query 5

SELECT
    n_name,
    sum(l_extendedprice * (1 - l_discount)) as revenue
FROM
    customer,
    orders,
    lineitem,
    supplier,
    nation,
    region
WHERE
    c_custkey = o_custkey
    AND l_orderkey = o_orderkey
    AND l_suppkey = s_suppkey
    AND c_nationkey = s_nationkey
    AND s_nationkey = n_nationkey
    AND n_regionkey = r_regionkey
    AND r_name = 'AFRICA'
    AND o_orderdate >= 757411200 -- date '1994-01-01'
    AND o_orderdate < 1072944000 -- date '1994-01-01' + interval '10' year
GROUP BY
    n_name
ORDER BY
    revenue desc;

-- +---------+------------+
-- | n_name  | revenue    |
-- +---------+------------+
-- | ALGERIA | 52516.8000 |
-- +---------+------------+

-- Query 6

SELECT
    sum(l_extendedprice * l_discount) as revenue
FROM
    lineitem
WHERE
    l_shipdate >= 757411200 -- date '1994-01-01'
    AND l_shipdate < 788947200 -- date '1994-01-01' + interval '1' year
    AND l_discount between 0.06 - 0.01 AND 0.06 + 0.01
    AND l_quantity < 24;
    
-- +---------------+
-- | revenue       |
-- +---------------+
-- | 38212505.9147 |
-- +---------------+

-- Query 10


SELECT
    c_custkey,
    c_name,
    sum(l_extendedprice * (1 - l_discount)) as revenue,
    c_acctbal,
    n_name,
    c_address,
    c_phone,
    c_comment
FROM
    customer,
    orders,
    lineitem,
    nation
WHERE
    c_custkey = o_custkey
    AND l_orderkey = o_orderkey
    AND o_orderdate >= 749458800 -- date '1993-10-01'
    AND o_orderdate < 757411200 -- date '1993-10-01' + interval '3' month
    AND l_returnflag = 'R'
    AND c_nationkey = n_nationkey
GROUP BY
    c_custkey,
    c_name,
    c_acctbal,
    c_phone,
    n_name,
    c_address,
    c_comment
ORDER BY
    revenue desc
LIMIT 20;

-- +-----------+--------------------+-------------+-----------+---------+----------------------------------------+-----------------+---------------------------------------------------------------------------------------------------------------------+
-- | c_custkey | c_name             | revenue     | c_acctbal | n_name  | c_address                              | c_phone         | c_comment                                                                                                           |
-- +-----------+--------------------+-------------+-----------+---------+----------------------------------------+-----------------+---------------------------------------------------------------------------------------------------------------------+
-- |    463510 | Customer#000463510 | 209303.0655 |   5515.66 | ALGERIA | WvyLxW0HOhUzyGsZ1X0J                   | 10-886-559-3779 | ily. furiously final dolphins sleep final deposits. quickly pending foxes sleep fl                                  |
-- |    521620 | Customer#000521620 | 157110.9726 |   2127.64 | ALGERIA | xr9n 7WlIV7tjop2a0tY0                  | 10-827-271-6272 | boost about the unusual deposits. blithely even excuses are. blithely even foxes against the pe                     |
-- |    639649 | Customer#000639649 | 145055.7579 |   8816.62 | ALGERIA | 9MOLLfjIFxC6jumAmGFJHCOu9ZzU5          | 10-774-432-1353 | es. slyly regular ideas sleep quickly after th                                                                      |
-- |    638350 | Customer#000638350 | 140226.8264 |   3518.34 | ALGERIA | dw8S06MXpeCQX 0Fiw Cbu0sz7wN,fNGcC     | 10-668-433-1463 | g above the slyly bold instructions. pending pinto beans eat carefully across the final packages. e                 |
-- |    701903 | Customer#000701903 | 134877.2401 |   5017.74 | ALGERIA | WmdH8vJXChLKhXhM7hoHkEbToDKaV9q2vraXbK | 10-832-227-5806 | usly final accounts use furiously about the theodolites. requests wake blithely regular re                          |
-- |    579899 | Customer#000579899 | 130741.0532 |    872.13 | ALGERIA | oLE5L,duosliECu119bYx CHTQ3 b          | 10-776-291-7870 | al requests are. quickly ironic instructions sl                                                                     |
-- |   1075513 | Customer#001075513 | 126779.8800 |   8309.00 | ALGERIA | GjkVQxHUFAOkmlA5                       | 10-968-838-3385 | unts. slyly pending accounts detect furiously about the quickly ironic packages. carefully even deposits al         |
-- |   1294861 | Customer#001294861 | 109390.9928 |   2498.43 | ALGERIA | 4stLzw O2bSyPPdANHiF1BlLkUSLRxUr       | 10-496-804-3150 | ronic ideas. final packages use fluffily pending, unusual accounts.                                                 |
-- |    319955 | Customer#000319955 | 104879.2914 |   7165.89 | ALGERIA | 2oNsjQp3MHz2MP5GVfVzATSElGAEiCxczCi1   | 10-566-462-2241 | s. furiously ironic asymptotes breach silent, even deposits. slyly even packages n                                  |
-- |    553939 | Customer#000553939 |  91792.4280 |   7532.68 | ALGERIA | BOTXcsnOE9h7cL0C,5YJwP0MIOmoey         | 10-139-836-6635 | uickly ironic deposits alongside of the evenly ironic packages believe arou                                         |
-- |   1145489 | Customer#001145489 |  61757.3820 |   7135.32 | ALGERIA | LekyLfXFJ3c8HelAKoB4owNGXQS            | 10-348-967-5828 | fluffily blithely regular depths. blithely pending depths are carefully-- decoy                                     |
-- |    226939 | Customer#000226939 |  54124.1883 |   1561.11 | ALGERIA | lohl5sV97l5                            | 10-266-523-7291 | totes. carefully express requests sleep across the i                                                                |
-- |   1366306 | Customer#001366306 |  52389.2344 |   5532.93 | ALGERIA | NzrMTEghXXJTDoS4z4p                    | 10-689-829-9568 |  alongside of the carefully bold accounts haggle bold accounts. slyly special deposits acco                         |
-- |    501572 | Customer#000501572 |  50902.1271 |   2408.03 | ALGERIA | ppPglgpPKO                             | 10-830-582-6827 | carefully bold packages nag carefull                                                                                |
-- |   1114199 | Customer#001114199 |  34950.3000 |   -935.43 | ALGERIA | azCSIBJZND dYRzVGM4QDNZjU              | 10-503-648-2561 | lithely furiously pending frays. ironic asymptotes                                                                  |
-- |   1099228 | Customer#001099228 |  28067.5440 |   5859.98 | ALGERIA | STyiULA18QY9wpe79ibbVwk903p9Ff         | 10-919-516-9178 | the slyly ironic waters sleep at the somas. regular, ironic hockey players sleep. instructions sleep. re            |
-- |    334444 | Customer#000334444 |  11806.2945 |   9919.84 | ALGERIA | 4P3phxzpTbc4XZ7pTIfx9JNpBZ8Nm,ugh9     | 10-327-401-9058 | use. furiously bold platelets cajole. quickly express requests instead of the quickly close deposits wake fluffily  |
-- +-----------+--------------------+-------------+-----------+---------+----------------------------------------+-----------------+---------------------------------------------------------------------------------------------------------------------+

-- Query 12

SELECT
    l_shipmode,
    sum(case
        when o_orderpriority = '1-URGENT'
            OR o_orderpriority = '2-HIGH'
            then 1
        else 0
    end) as high_line_count,
    sum(case
        when o_orderpriority <> '1-URGENT'
            AND o_orderpriority <> '2-HIGH'
            then 1
        else 0
    end) AS low_line_count
FROM
    orders,
    lineitem
WHERE
    o_orderkey = l_orderkey
    AND l_shipmode in ('MAIL', 'SHIP')
    AND l_commitdate < l_receiptdate
    AND l_shipdate < l_commitdate
    AND l_receiptdate >= 757411200 -- date '1994-01-01'
    AND l_receiptdate < 788947200 -- date '1994-01-01' + interval '1' year
GROUP BY
    l_shipmode
ORDER BY
    l_shipmode;
    
-- +------------+-----------------+----------------+
-- | l_shipmode | high_line_count | low_line_count |
-- +------------+-----------------+----------------+
-- | MAIL       |            1917 |           2942 |
-- | SHIP       |            1948 |           2890 |
-- +------------+-----------------+----------------+

-- Query 14

SELECT
    100.00 * sum(case
        when p_type like 'PROMO%'
            then l_extendedprice * (1 - l_discount)
        else 0
    end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
FROM
    lineitem,
    part
WHERE
    l_partkey = p_partkey
    AND l_shipdate >= 809938800 -- date '1995-09-01'
    AND l_shipdate < 812530800; -- date '1995-09-01' + interval '1' month;
    
-- +---------------+
-- | promo_revenue |
-- +---------------+
-- | 17.9321376392 |
-- +---------------+

-- Query 19

SELECT
    sum(l_extendedprice* (1 - l_discount)) as revenue
FROM
    lineitem,
    part
WHERE
    (
        p_partkey = l_partkey
        AND p_brand = 'Brand#12'
        AND p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
        AND l_quantity >= 1 AND l_quantity <= 1 + 10
        AND p_size between 1 AND 5
        AND l_shipmode in ('AIR', 'AIR REG')
        AND l_shipinstruct = 'DELIVER IN PERSON'
    )
    OR
    (
        p_partkey = l_partkey
        AND p_brand = 'Brand#23'
        AND p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
        AND l_quantity >= 10 AND l_quantity <= 10 + 10
        AND p_size between 1 AND 10
        AND l_shipmode in ('AIR', 'AIR REG')
        AND l_shipinstruct = 'DELIVER IN PERSON'
    )
    OR
    (
        p_partkey = l_partkey
        AND p_brand = 'Brand#34'
        AND p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
        AND l_quantity >= 20 AND l_quantity <= 20 + 10
        AND p_size between 1 AND 15
        AND l_shipmode in ('AIR', 'AIR REG')
        AND l_shipinstruct = 'DELIVER IN PERSON'
    );
    
-- +------------+
-- | revenue    |
-- +------------+
-- | 18654.3585 |
-- +------------+

