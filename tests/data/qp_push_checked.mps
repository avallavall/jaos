* A circulation on 12 nodes and 30 arcs, curvatures from 1e-11 to 2
* (bench/measurements/02-365/netqp.py, seed 1007). The push settles, and
* its point breaks a row by 1.2e-7 of the row's traffic. A settled push was
* published as optimal without the checker; it is now checked, polished
* and passes.
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
    x2 obj -311.709772782
    x2 n2 -1
    x2 n3 1
    x3 obj -6694.55840141
    x3 n3 -1
    x3 n4 1
    x4 n4 -1
    x4 n5 1
    x5 n5 -1
    x5 n6 1
    x6 n6 -1
    x6 n7 1
    x7 obj -1250.39416084
    x7 n7 -1
    x7 n8 1
    x8 obj 5538.04817531
    x8 n8 -1
    x8 n9 1
    x9 obj -6682.28463167
    x9 n9 -1
    x9 n10 1
    x10 obj -8794.40676198
    x10 n10 -1
    x10 n11 1
    x11 obj -6597.98203388
    x11 n11 -1
    x11 n0 1
    x12 n0 -1
    x12 n9 1
    x13 n2 -1
    x13 n4 1
    x14 n2 -1
    x14 n3 1
    x15 obj -5355.18148569
    x15 n8 -1
    x15 n0 1
    x16 obj -6677.01097848
    x16 n10 -1
    x16 n3 1
    x17 obj 2386.60079879
    x17 n8 -1
    x17 n0 1
    x18 obj -5331.22756308
    x18 n7 -1
    x18 n9 1
    x19 obj 3141.6507102
    x19 n4 -1
    x19 n11 1
    x20 obj -265.213905366
    x20 n9 -1
    x20 n0 1
    x21 n0 -1
    x21 n6 1
    x22 n11 -1
    x22 n1 1
    x23 n1 -1
    x23 n10 1
    x24 n10 -1
    x24 n0 1
    x25 n4 -1
    x25 n11 1
    x26 obj -8405.28123276
    x26 n10 -1
    x26 n6 1
    x27 obj 9779.43806754
    x27 n3 -1
    x27 n10 1
    x28 obj 1474.88561315
    x28 n0 -1
    x28 n5 1
    x29 obj -3336.04619042
    x29 n9 -1
    x29 n5 1
RHS
BOUNDS
 LO BND x0 669.287444643
 UP BND x0 35027.6217172
 LO BND x1 1502.78880495
 UP BND x1 7916.81617955
 MI BND x2
 UP BND x2 0
 MI BND x3
 UP BND x3 0
 LO BND x4 11074.6844304
 UP BND x4 73499.0392353
 MI BND x5
 UP BND x5 0
 MI BND x6
 UP BND x6 0
 LO BND x7 0.0156702657483
 UP BND x7 0.0595462418281
 MI BND x8
 UP BND x8 0
 MI BND x9
 UP BND x9 0
 LO BND x10 2001.59793681
 UP BND x10 24278.9149881
 MI BND x11
 UP BND x11 0
 MI BND x12
 UP BND x12 0
 LO BND x13 0.0532760557465
 UP BND x13 2.56073022196
 LO BND x14 1192.42058172
 UP BND x14 31442.3450997
 LO BND x15 0.031805698364
 UP BND x15 1.08749670358
 MI BND x16
 UP BND x16 0
 MI BND x17
 UP BND x17 0
 MI BND x18
 UP BND x18 0
 MI BND x19
 UP BND x19 0
 MI BND x20
 UP BND x20 0
 MI BND x21
 UP BND x21 0
 LO BND x22 0.0871996946764
 UP BND x22 2.70450060006
 LO BND x23 0.0293369578497
 UP BND x23 0.310327941919
 MI BND x24
 UP BND x24 0
 LO BND x25 0.101039229204
 UP BND x25 1.61493415244
 MI BND x26
 UP BND x26 0
 MI BND x27
 UP BND x27 0
 MI BND x28
 UP BND x28 0
 MI BND x29
 UP BND x29 0
QUADOBJ
    x0 x0 4.59220836399e-06
    x1 x1 2
    x2 x2 7.16345264388e-08
    x3 x3 1.43795655593e-11
    x4 x4 1.5061911196e-06
    x5 x5 2
    x6 x6 2.57884167602e-08
    x7 x7 2
    x8 x8 2.08225621232e-08
    x9 x9 2
    x10 x10 2
    x11 x11 2
    x12 x12 0.0023177919227
    x13 x13 2
    x14 x14 9.61484627382e-08
    x15 x15 6.18347533988e-07
    x16 x16 1.51785848638e-09
    x17 x17 2
    x18 x18 0.00113386578058
    x19 x19 2
    x20 x20 2
    x21 x21 0.00116573339465
    x22 x22 2
    x23 x23 2
    x24 x24 3.75639793911e-09
    x25 x25 2
    x26 x26 2
    x27 x27 1.51549713842e-07
    x28 x28 1.04141664429e-09
    x29 x29 0.000414776447326
ENDATA
