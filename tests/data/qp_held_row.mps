* bench/measurements/02-365/netqp.py 12 30, seed 1019. Row n11 holds x10 and
* x11 alone, and x11 sits on its lower bound, so x10 = x11 lies 0.0138
* above x10's own lower bound. The barrier's dual slack on that bound is
* larger, so the push and the polish read x10 at it, and n11 broke with
* both its columns held. A row whose held columns break it now frees the
* one that moved the most.
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
    x0 n0 -1
    x0 n1 1
    x1 n1 -1
    x1 n2 1
    x2 obj 9008.1238565
    x2 n2 -1
    x2 n3 1
    x3 n3 -1
    x3 n4 1
    x4 n4 -1
    x4 n5 1
    x5 obj -2537.13430502
    x5 n5 -1
    x5 n6 1
    x6 obj 537.479504613
    x6 n6 -1
    x6 n7 1
    x7 obj 1313.14333419
    x7 n7 -1
    x7 n8 1
    x8 n8 -1
    x8 n9 1
    x9 obj 3575.6788475
    x9 n9 -1
    x9 n10 1
    x10 n10 -1
    x10 n11 1
    x11 n11 -1
    x11 n0 1
    x12 obj 5194.38426783
    x12 n0 -1
    x12 n5 1
    x13 n3 -1
    x13 n1 1
    x14 obj 2386.21538697
    x14 n3 -1
    x14 n7 1
    x15 n9 -1
    x15 n8 1
    x16 n10 -1
    x16 n3 1
    x17 n4 -1
    x17 n6 1
    x18 obj 5022.194555
    x18 n6 -1
    x18 n4 1
    x19 obj -5468.36896163
    x19 n4 -1
    x19 n3 1
    x20 n4 -1
    x20 n1 1
    x21 n10 -1
    x21 n5 1
    x22 n6 -1
    x22 n0 1
    x23 n2 -1
    x23 n7 1
    x24 obj 1851.56061543
    x24 n4 -1
    x24 n7 1
    x25 obj 1665.07107156
    x25 n10 -1
    x25 n7 1
    x26 n0 -1
    x26 n1 1
    x27 obj 2657.42942855
    x27 n7 -1
    x27 n2 1
    x28 n9 -1
    x28 n3 1
    x29 obj 9648.33010189
    x29 n8 -1
    x29 n10 1
RHS
BOUNDS
 LO BND x0 1.10073178236
 UP BND x0 1.72287047084
 MI BND x1
 UP BND x1 0
 MI BND x2
 UP BND x2 0
 MI BND x3
 UP BND x3 0
 LO BND x4 3293.62715949
 UP BND x4 21145.0102099
 MI BND x5
 UP BND x5 0
 MI BND x6
 UP BND x6 0
 MI BND x7
 UP BND x7 0
 MI BND x8
 UP BND x8 0
 MI BND x9
 UP BND x9 0
 LO BND x10 0.56821787988
 UP BND x10 18.0765187651
 LO BND x11 0.582040998707
 UP BND x11 27.0434161312
 MI BND x12
 UP BND x12 0
 LO BND x13 3797.64929653
 UP BND x13 6337.93896062
 LO BND x14 0.0488917791504
 UP BND x14 1.94057656817
 MI BND x15
 UP BND x15 0
 MI BND x16
 UP BND x16 0
 MI BND x17
 UP BND x17 0
 MI BND x18
 UP BND x18 0
 LO BND x19 4108.0420545
 UP BND x19 13962.9902629
 MI BND x20
 UP BND x20 0
 MI BND x21
 UP BND x21 0
 MI BND x22
 UP BND x22 0
 LO BND x23 2022.33759206
 UP BND x23 17646.8335518
 MI BND x24
 UP BND x24 0
 LO BND x25 5037.09400481
 UP BND x25 485794.613199
 MI BND x26
 UP BND x26 0
 LO BND x27 5844.07319844
 UP BND x27 34388.9182791
 LO BND x28 0.466185356483
 UP BND x28 3.5323537678
 MI BND x29
 UP BND x29 0
QUADOBJ
    x0 x0 2.00305581418e-05
    x1 x1 0.0122068130657
    x2 x2 5.21078707861e-09
    x3 x3 2
    x4 x4 2
    x5 x5 2
    x6 x6 2
    x7 x7 1.79492564818e-06
    x8 x8 1.09043582162e-11
    x9 x9 0.000136320488689
    x10 x10 2
    x11 x11 3.45483654735e-10
    x12 x12 2
    x13 x13 2
    x14 x14 1.34994740062e-11
    x15 x15 5.15322220714e-06
    x16 x16 0.0369012436556
    x17 x17 0.00132860161317
    x18 x18 0.00310461689248
    x19 x19 2
    x20 x20 0.0879710626478
    x21 x21 1.30757892822e-07
    x22 x22 2
    x23 x23 2
    x24 x24 2
    x25 x25 2
    x26 x26 2.95363701573e-10
    x27 x27 1.84961393011e-05
    x28 x28 2
    x29 x29 6.34379169e-08
ENDATA
