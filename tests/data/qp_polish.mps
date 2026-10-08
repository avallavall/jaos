* A circulation on 12 nodes and 30 arcs with separable curvatures from
* 1e-11 to 2, from bench/measurements/02-365/netqp.py (seed 33). The
* barrier's point leaves rows and columns a little past the checker's
* windows and the first push does not settle; the push from the
* barrier's own active set does (bench/measurements/02-367/).
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
    x3 n3 -1
    x3 n4 1
    x4 n4 -1
    x4 n5 1
    x5 obj 7853.1398983
    x5 n5 -1
    x5 n6 1
    x6 n6 -1
    x6 n7 1
    x7 obj -47.9890611035
    x7 n7 -1
    x7 n8 1
    x8 n8 -1
    x8 n9 1
    x9 obj -8042.4995215
    x9 n9 -1
    x9 n10 1
    x10 n10 -1
    x10 n11 1
    x11 obj -375.567146427
    x11 n11 -1
    x11 n0 1
    x12 obj -8427.01075239
    x12 n9 -1
    x12 n2 1
    x13 n10 -1
    x13 n3 1
    x14 obj 1451.55359537
    x14 n4 -1
    x14 n7 1
    x15 n10 -1
    x15 n8 1
    x16 obj 2900.14355591
    x16 n8 -1
    x16 n2 1
    x17 n10 -1
    x17 n9 1
    x18 n8 -1
    x18 n5 1
    x19 obj -8448.21962033
    x19 n10 -1
    x19 n8 1
    x20 obj 7662.50413933
    x20 n4 -1
    x20 n10 1
    x21 obj -3771.86378436
    x21 n10 -1
    x21 n1 1
    x22 obj 9017.09666529
    x22 n4 -1
    x22 n6 1
    x23 obj -8028.1300293
    x23 n4 -1
    x23 n8 1
    x24 n7 -1
    x24 n0 1
    x25 obj 8225.76316822
    x25 n11 -1
    x25 n9 1
    x26 obj 8933.19495356
    x26 n11 -1
    x26 n10 1
    x27 n6 -1
    x27 n4 1
    x28 n5 -1
    x28 n8 1
    x29 obj 5809.76157883
    x29 n10 -1
    x29 n3 1
RHS
BOUNDS
 LO BND x0 6040.14193006
 UP BND x0 843590.658031
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
 MI BND x6
 UP BND x6 0
 MI BND x7
 UP BND x7 0
 MI BND x8
 UP BND x8 0
 LO BND x9 0.0264546215223
 UP BND x9 0.324565476106
 LO BND x10 4140.04349855
 UP BND x10 19142.6583086
 LO BND x11 5355.67396759
 UP BND x11 163430.668374
 MI BND x12
 UP BND x12 0
 MI BND x13
 UP BND x13 0
 MI BND x14
 UP BND x14 0
 LO BND x15 0.0221628098698
 UP BND x15 0.070102561853
 MI BND x16
 UP BND x16 0
 MI BND x17
 UP BND x17 0
 LO BND x18 0.0431466592202
 UP BND x18 0.299186280256
 MI BND x19
 UP BND x19 0
 LO BND x20 0.0595920366126
 UP BND x20 1.72737830437
 MI BND x21
 UP BND x21 0
 MI BND x22
 UP BND x22 0
 MI BND x23
 UP BND x23 0
 MI BND x24
 UP BND x24 0
 LO BND x25 0.0519057209924
 UP BND x25 0.393463051489
 MI BND x26
 UP BND x26 0
 MI BND x27
 UP BND x27 0
 LO BND x28 0.0470396467526
 UP BND x28 0.583544547458
 LO BND x29 0.861064799977
 UP BND x29 13.7547691352
QUADOBJ
    x0 x0 2
    x1 x1 0.22787282205
    x2 x2 0.00658930993438
    x3 x3 2
    x4 x4 1.00612907037e-10
    x5 x5 2
    x6 x6 2
    x7 x7 2
    x8 x8 2
    x9 x9 8.45682664108e-11
    x10 x10 3.55787631211e-10
    x11 x11 1.83904321645e-11
    x12 x12 0.00207846466728
    x13 x13 5.89882937805e-05
    x14 x14 3.00498536321e-11
    x15 x15 2
    x16 x16 0.0142121519078
    x17 x17 2
    x18 x18 2
    x19 x19 1.7925094496e-09
    x20 x20 2.22237932174e-07
    x21 x21 0.000355910122467
    x22 x22 3.26256678116e-07
    x23 x23 2.94154970668e-09
    x24 x24 5.28804467538e-08
    x25 x25 2
    x26 x26 2
    x27 x27 2
    x28 x28 4.28901430243e-05
    x29 x29 1.26901210069e-05
ENDATA
