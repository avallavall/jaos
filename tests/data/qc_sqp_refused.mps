* bench/measurements/02-371/qcgen.py 12 30 1 88: a circulation with one
* ball row. The conic walk nearly converges, and the checker refuses its
* duals by 1.2e-3. A quadratic program on the ball row's linearisation,
* with the walk's duals as its curvature, finishes it.
NAME qcnet
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
 L b0
COLUMNS
    x0 obj -219.181762507
    x0 n0 -1
    x0 n1 1
    x1 obj -637.545071911
    x1 n1 -1
    x1 n2 1
    x2 obj -201.870760674
    x2 n2 -1
    x2 n3 1
    x3 obj 398.851504703
    x3 n3 -1
    x3 n4 1
    x4 obj -268.071651574
    x4 n4 -1
    x4 n5 1
    x5 obj 376.633877108
    x5 n5 -1
    x5 n6 1
    x6 obj -377.5715501
    x6 n6 -1
    x6 n7 1
    x7 obj -396.715812447
    x7 n7 -1
    x7 n8 1
    x8 obj 470.701794068
    x8 n8 -1
    x8 n9 1
    x9 obj 763.516134316
    x9 n9 -1
    x9 n10 1
    x10 obj -744.548335419
    x10 n10 -1
    x10 n11 1
    x11 obj 473.631854163
    x11 n11 -1
    x11 n0 1
    x12 obj -587.875203562
    x12 n6 -1
    x12 n3 1
    x13 obj -95.7406318802
    x13 n5 -1
    x13 n2 1
    x14 obj -673.781951133
    x14 n10 -1
    x14 n0 1
    x15 obj 357.487450884
    x15 n8 -1
    x15 n11 1
    x16 obj -773.248489101
    x16 n0 -1
    x16 n5 1
    x17 obj -325.439444677
    x17 n10 -1
    x17 n7 1
    x18 obj 389.333000916
    x18 n2 -1
    x18 n3 1
    x19 obj 706.050923236
    x19 n1 -1
    x19 n11 1
    x20 obj 547.06424676
    x20 n2 -1
    x20 n9 1
    x21 obj -210.16050796
    x21 n2 -1
    x21 n7 1
    x22 obj 652.952801704
    x22 n6 -1
    x22 n5 1
    x23 obj -165.372662988
    x23 n10 -1
    x23 n6 1
    x24 obj -554.706556799
    x24 n2 -1
    x24 n4 1
    x25 obj 642.526369492
    x25 n2 -1
    x25 n1 1
    x26 obj 775.901903202
    x26 n7 -1
    x26 n5 1
    x27 obj 638.739840744
    x27 n0 -1
    x27 n2 1
    x28 obj -79.1035500961
    x28 n3 -1
    x28 n7 1
    x29 obj -381.309075872
    x29 n2 -1
    x29 n0 1
RHS
    RHS b0 7902.89274222
BOUNDS
 LO BND x0 -10.1668057651
 UP BND x0 0
 LO BND x1 -10.1668057651
 UP BND x1 0
 LO BND x2 -258.010343255
 UP BND x2 0
 LO BND x3 0.00788388125
 UP BND x3 0.902537572073
 LO BND x4 0.00867129777715
 UP BND x4 0.966237575144
 LO BND x5 46.2824103681
 UP BND x5 3311.19860463
 LO BND x6 -10
 UP BND x6 0
 LO BND x7 -258.010343255
 UP BND x7 0
 LO BND x8 -10
 UP BND x8 0
 LO BND x9 -10
 UP BND x9 0
 LO BND x10 -10
 UP BND x10 0
 LO BND x11 -10
 UP BND x11 0
 LO BND x12 -10
 UP BND x12 0
 LO BND x13 0.00655389208198
 UP BND x13 0.321015810618
 LO BND x14 -10
 UP BND x14 0
 LO BND x15 -258.010343255
 UP BND x15 0
 LO BND x16 -10
 UP BND x16 0
 LO BND x17 -10.1668057651
 UP BND x17 0
 LO BND x18 0.00794704357686
 UP BND x18 0.0668788199494
 LO BND x19 14.1492750224
 UP BND x19 170.624643158
 LO BND x20 -10
 UP BND x20 0
 LO BND x21 -10
 UP BND x21 0
 LO BND x22 67.5740528008
 UP BND x22 340.871260295
 LO BND x23 0.013714437135
 UP BND x23 0.486147235284
 LO BND x24 -10
 UP BND x24 0
 LO BND x25 11.6146543891
 UP BND x25 96.3047923792
 LO BND x26 -10.1668057651
 UP BND x26 0
 LO BND x27 -168.760117077
 UP BND x27 0
 LO BND x28 -258.010343255
 UP BND x28 0
 LO BND x29 -168.926922842
 UP BND x29 0
QUADOBJ
    x3 x3 0.328692984168
    x4 x4 0.000453649651219
    x5 x5 0.000232525864608
    x6 x6 1.27343886616e-05
    x7 x7 0.675977701607
    x9 x9 0.0913759793139
    x10 x10 3.71062293329e-06
    x11 x11 0.0114781988309
    x13 x13 0.051470086471
    x14 x14 0.0243525253525
    x16 x16 0.313238646629
    x19 x19 0.00146797227508
    x22 x22 0.0547611018405
    x23 x23 0.000373992671775
    x24 x24 0.0350863696803
    x26 x26 9.12814887624e-05
    x27 x27 0.0787218079933
    x28 x28 7.81050329968e-05
    x29 x29 1.96395779235e-05
QCMATRIX b0
    x27 x27 1
    x10 x10 1
    x5 x5 1
ENDATA
