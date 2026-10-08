* A circulation on 12 nodes and 30 arcs, every arc bounded below by -1e6,
* curvatures from 1e-11 to 2 (bench/measurements/02-366/, seed 291). The
* push pins x12 on its upper bound 0 with a reduced cost of +1.2e-8, the
* wrong sign by less than the push's threshold; its lower bound is 1e6
* away, so the checker's gap reads 1.18e-7 until the push frees it.
NAME netqp
ROWS
 N obj
 E n0
 E n1
 E n2
 E n3
 E n4
 E n5
 E n6
 E n7
 E n8
 E n9
 E n10
 E n11
COLUMNS
    x0 obj -1245.12637943
    x0 n0 -1
    x0 n1 1
    x1 n1 -1
    x1 n2 1
    x2 obj 3528.62654387
    x2 n2 -1
    x2 n3 1
    x3 n3 -1
    x3 n4 1
    x4 n4 -1
    x4 n5 1
    x5 n5 -1
    x5 n6 1
    x6 obj -2890.02481578
    x6 n6 -1
    x6 n7 1
    x7 n7 -1
    x7 n8 1
    x8 n8 -1
    x8 n9 1
    x9 obj -5661.10530772
    x9 n9 -1
    x9 n10 1
    x10 n10 -1
    x10 n11 1
    x11 obj -1089.81867391
    x11 n11 -1
    x11 n0 1
    x12 n10 -1
    x12 n7 1
    x13 n11 -1
    x13 n10 1
    x14 n8 -1
    x14 n1 1
    x15 n6 -1
    x15 n10 1
    x16 n10 -1
    x16 n8 1
    x17 n7 -1
    x17 n1 1
    x18 n11 -1
    x18 n9 1
    x19 n4 -1
    x19 n11 1
    x20 obj -3225.28141433
    x20 n3 -1
    x20 n4 1
    x21 obj 9631.20175577
    x21 n9 -1
    x21 n10 1
    x22 obj -1891.05172833
    x22 n10 -1
    x22 n9 1
    x23 obj -3447.98087201
    x23 n4 -1
    x23 n5 1
    x24 n6 -1
    x24 n1 1
    x25 obj 3901.79962184
    x25 n2 -1
    x25 n11 1
    x26 n11 -1
    x26 n9 1
    x27 n3 -1
    x27 n4 1
    x28 n1 -1
    x28 n11 1
    x29 obj 2769.67154684
    x29 n5 -1
    x29 n1 1
RHS
BOUNDS
 LO BND x0 -1e6
 UP BND x0 0
 LO BND x1 2.57022954446
 UP BND x1 13.5910644143
 LO BND x2 -1e6
 UP BND x2 0
 LO BND x3 0.144402014905
 UP BND x3 0.358135490702
 LO BND x4 -1e6
 UP BND x4 0
 LO BND x5 -1e6
 UP BND x5 0
 LO BND x6 -1e6
 UP BND x6 0
 LO BND x7 -1e6
 UP BND x7 0
 LO BND x8 -1e6
 UP BND x8 0
 LO BND x9 -1e6
 UP BND x9 0
 LO BND x10 -1e6
 UP BND x10 0
 LO BND x11 -1e6
 UP BND x11 0
 LO BND x12 -1e6
 UP BND x12 0
 LO BND x13 -1e6
 UP BND x13 0
 LO BND x14 -1e6
 UP BND x14 0
 LO BND x15 -1e6
 UP BND x15 0
 LO BND x16 -1e6
 UP BND x16 0
 LO BND x17 -1e6
 UP BND x17 0
 LO BND x18 -1e6
 UP BND x18 0
 LO BND x19 -1e6
 UP BND x19 0
 LO BND x20 -1e6
 UP BND x20 0
 LO BND x21 5.61299823702
 UP BND x21 298.138628273
 LO BND x22 -1e6
 UP BND x22 0
 LO BND x23 0.28517931167
 UP BND x23 12.8929994427
 LO BND x24 14.907806635
 UP BND x24 31.8855236695
 LO BND x25 13.9921238
 UP BND x25 34.0117812273
 LO BND x26 5.61469429602
 UP BND x26 686.697777982
 LO BND x27 -1e6
 UP BND x27 0
 LO BND x28 -1e6
 UP BND x28 0
 LO BND x29 -1e6
 UP BND x29 0
QUADOBJ
    x0 x0 2.7323366617e-07
    x1 x1 2
    x2 x2 2.45154685064e-10
    x3 x3 1.39821052323e-06
    x4 x4 3.21720983652e-11
    x5 x5 1.14203148288e-07
    x6 x6 1.11355917336e-09
    x7 x7 2
    x8 x8 0.000863070205041
    x9 x9 2
    x10 x10 0.000333232477521
    x11 x11 1.86076508173
    x12 x12 0.000182660013387
    x13 x13 0.00676560305544
    x14 x14 1.84657330579e-06
    x15 x15 2
    x16 x16 3.86437786189e-09
    x17 x17 0.00412110955188
    x18 x18 0.000153992426182
    x19 x19 9.10253718614e-06
    x20 x20 1.10862695038
    x21 x21 0.00392703675014
    x22 x22 2
    x23 x23 2
    x24 x24 0.000123809679964
    x25 x25 2
    x26 x26 9.17021088019e-10
    x27 x27 1.88195879864
    x28 x28 2
    x29 x29 2
ENDATA
