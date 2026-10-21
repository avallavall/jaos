* bench/measurements/02-365/netqp.py 12 30, seed 1374. The rows sum to
* zero, so the duals share a free shift, and the barrier leaves them near
* 1.26e9: every reduced cost is a difference at about 2e-7 precision and
* every push fails. The third push takes the shift out of the duals first
* and settles.
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
    x2 n2 -1
    x2 n3 1
    x3 obj -9044.65243197
    x3 n3 -1
    x3 n4 1
    x4 obj -6235.41482237
    x4 n4 -1
    x4 n5 1
    x5 obj -8955.63820685
    x5 n5 -1
    x5 n6 1
    x6 obj -5221.18432399
    x6 n6 -1
    x6 n7 1
    x7 n7 -1
    x7 n8 1
    x8 obj -4748.83519471
    x8 n8 -1
    x8 n9 1
    x9 n9 -1
    x9 n10 1
    x10 obj 7530.26849421
    x10 n10 -1
    x10 n11 1
    x11 obj 8227.74253238
    x11 n11 -1
    x11 n0 1
    x12 n0 -1
    x12 n10 1
    x13 obj -7593.48757599
    x13 n6 -1
    x13 n9 1
    x14 n10 -1
    x14 n8 1
    x15 n0 -1
    x15 n4 1
    x16 n10 -1
    x16 n3 1
    x17 n4 -1
    x17 n10 1
    x18 n1 -1
    x18 n9 1
    x19 n10 -1
    x19 n6 1
    x20 obj 2227.5949289
    x20 n2 -1
    x20 n10 1
    x21 obj 641.852526758
    x21 n10 -1
    x21 n5 1
    x22 obj 9478.85586095
    x22 n1 -1
    x22 n10 1
    x23 n7 -1
    x23 n9 1
    x24 n4 -1
    x24 n5 1
    x25 obj 1261.1113512
    x25 n5 -1
    x25 n1 1
    x26 n11 -1
    x26 n7 1
    x27 n11 -1
    x27 n2 1
    x28 n3 -1
    x28 n1 1
    x29 obj -9901.95617129
    x29 n2 -1
    x29 n9 1
RHS
BOUNDS
 MI BND x0
 UP BND x0 0
 MI BND x1
 UP BND x1 0
 MI BND x2
 UP BND x2 0
 MI BND x3
 UP BND x3 0
 MI BND x4
 UP BND x4 0
 MI BND x5
 UP BND x5 0
 LO BND x6 2804.46566255
 UP BND x6 88512.9040693
 LO BND x7 0.0406115508716
 UP BND x7 1.59149734394
 LO BND x8 2624.2569996
 UP BND x8 35678.1510995
 LO BND x9 2661.55499644
 UP BND x9 4068.05865148
 MI BND x10
 UP BND x10 0
 MI BND x11
 UP BND x11 0
 LO BND x12 126.695087037
 UP BND x12 1462.4118605
 MI BND x13
 UP BND x13 0
 LO BND x14 2241.23545326
 UP BND x14 8080.58576553
 LO BND x15 7.98687439935
 UP BND x15 76.5120376996
 MI BND x16
 UP BND x16 0
 MI BND x17
 UP BND x17 0
 MI BND x18
 UP BND x18 0
 LO BND x19 0.0513890395021
 UP BND x19 0.365484590076
 MI BND x20
 UP BND x20 0
 MI BND x21
 UP BND x21 0
 MI BND x22
 UP BND x22 0
 LO BND x23 1777.30058032
 UP BND x23 9054.19787004
 LO BND x24 201.815200451
 UP BND x24 440.305256971
 MI BND x25
 UP BND x25 0
 MI BND x26
 UP BND x26 0
 LO BND x27 1770.33435646
 UP BND x27 62323.8701548
 MI BND x28
 UP BND x28 0
 MI BND x29
 UP BND x29 0
QUADOBJ
    x0 x0 4.03711358752e-05
    x1 x1 0.00301556701561
    x2 x2 2
    x3 x3 0.0191745252284
    x4 x4 2
    x5 x5 1.88936868063e-05
    x6 x6 0.00500114621146
    x7 x7 2
    x8 x8 0.00517806352385
    x9 x9 1.22515768395e-05
    x10 x10 2
    x11 x11 0.331573485036
    x12 x12 2
    x13 x13 2.6657154993e-11
    x14 x14 0.44192425908
    x15 x15 1.96526571364e-11
    x16 x16 2
    x17 x17 2
    x18 x18 2.05187827768e-05
    x19 x19 0.000471490003634
    x20 x20 2
    x21 x21 2.36240237688e-06
    x22 x22 1.1781483597e-07
    x23 x23 2
    x24 x24 2
    x25 x25 3.18157397897e-11
    x26 x26 0.089827121127
    x27 x27 2
    x28 x28 0.00240589644703
    x29 x29 2
ENDATA
