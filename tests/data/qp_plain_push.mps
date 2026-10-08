* A circulation on 12 nodes and 30 arcs, every arc bounded below by -1e6
* (bench/measurements/02-367/, netqpb.py seed 163). The push's start pins
* every variable within a window scaled by the largest bound, the rows
* cannot then be met, and it cycles; a push from the variables the
* barrier reads at a bound settles.
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
    x0 obj 3614.62240058
    x0 n0 -1
    x0 n1 1
    x1 n1 -1
    x1 n2 1
    x2 obj 8882.69892188
    x2 n2 -1
    x2 n3 1
    x3 n3 -1
    x3 n4 1
    x4 obj 3539.48491171
    x4 n4 -1
    x4 n5 1
    x5 obj -5965.1645349
    x5 n5 -1
    x5 n6 1
    x6 n6 -1
    x6 n7 1
    x7 n7 -1
    x7 n8 1
    x8 n8 -1
    x8 n9 1
    x9 obj 7324.71538648
    x9 n9 -1
    x9 n10 1
    x10 n10 -1
    x10 n11 1
    x11 n11 -1
    x11 n0 1
    x12 obj -2704.88998011
    x12 n8 -1
    x12 n9 1
    x13 n7 -1
    x13 n9 1
    x14 obj 9914.6252054
    x14 n2 -1
    x14 n7 1
    x15 obj -6710.33333215
    x15 n4 -1
    x15 n3 1
    x16 obj -3221.82480712
    x16 n5 -1
    x16 n3 1
    x17 obj 6222.42961005
    x17 n5 -1
    x17 n2 1
    x18 n9 -1
    x18 n0 1
    x19 obj -3363.45080007
    x19 n11 -1
    x19 n1 1
    x20 n5 -1
    x20 n2 1
    x21 obj 186.005279954
    x21 n0 -1
    x21 n11 1
    x22 obj 6817.64978127
    x22 n11 -1
    x22 n1 1
    x23 obj -2066.19276015
    x23 n7 -1
    x23 n8 1
    x24 n1 -1
    x24 n9 1
    x25 obj -7537.20673435
    x25 n8 -1
    x25 n4 1
    x26 obj 3645.04835248
    x26 n7 -1
    x26 n2 1
    x27 obj -2494.10292457
    x27 n5 -1
    x27 n8 1
    x28 n2 -1
    x28 n11 1
    x29 n1 -1
    x29 n5 1
RHS
BOUNDS
 LO BND x0 0.0361591824834
 UP BND x0 2.50905107833
 LO BND x1 0.0480711000673
 UP BND x1 0.0582912715423
 LO BND x2 0.0296067798563
 UP BND x2 0.0754977078928
 LO BND x3 37520.383268
 UP BND x3 453294.960468
 LO BND x4 0.0487853729879
 UP BND x4 4.58202256913
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
 LO BND x12 0.0341943231931
 UP BND x12 4.15910191878
 LO BND x13 -1e6
 UP BND x13 0
 LO BND x14 439.600989412
 UP BND x14 51773.4734029
 LO BND x15 45605.1319645
 UP BND x15 6809659.98887
 LO BND x16 -1e6
 UP BND x16 0
 LO BND x17 -1e6
 UP BND x17 0
 LO BND x18 0.0300886910873
 UP BND x18 0.419292088741
 LO BND x19 -1e6
 UP BND x19 0
 LO BND x20 -1e6
 UP BND x20 0
 LO BND x21 -1e6
 UP BND x21 0
 LO BND x22 -1e6
 UP BND x22 0
 LO BND x23 0.0226440515287
 UP BND x23 0.407189053485
 LO BND x24 -1e6
 UP BND x24 0
 LO BND x25 -1e6
 UP BND x25 0
 LO BND x26 978.937144294
 UP BND x26 9902.20024402
 LO BND x27 0.105426038118
 UP BND x27 0.374637979856
 LO BND x28 -1e6
 UP BND x28 0
 LO BND x29 -1e6
 UP BND x29 0
QUADOBJ
    x0 x0 4.61127721287e-09
    x1 x1 0.00435874416067
    x2 x2 1.21197268072e-11
    x3 x3 6.34279456118e-06
    x4 x4 2
    x5 x5 0.000609016590454
    x6 x6 0.000213753377687
    x7 x7 2
    x8 x8 2
    x9 x9 0.155698922628
    x10 x10 5.76997629716e-09
    x11 x11 1.45318323685e-07
    x12 x12 2
    x13 x13 2
    x14 x14 1.1220492135e-08
    x15 x15 3.52816044116e-09
    x16 x16 1.73204654745
    x17 x17 2
    x18 x18 2
    x19 x19 0.01824726615
    x20 x20 7.76616400031e-06
    x21 x21 1.6648085925e-11
    x22 x22 2
    x23 x23 8.28521554942e-10
    x24 x24 0.429502703542
    x25 x25 2
    x26 x26 2
    x27 x27 2.9430561001e-07
    x28 x28 2
    x29 x29 1.28590891118
ENDATA
