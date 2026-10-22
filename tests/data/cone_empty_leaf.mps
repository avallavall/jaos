* bench/measurements/02-431/permcone.c 20000 1, model 14354 in the permuted
* order. C2 is fixed at 0 and heads the rotated cone 2*C2*C4 >= C3^2, so
* C3 must be 0, and C3 = 0 turns on the indicator row R1. A leaf with
* C3 = 1 holds no point, but points with C2 near 0 and C4 large come
* arbitrarily close, so its solve fails with no certificate, and the tree
* set it aside and ended numerical_error. The bounds the cone implies
* empty that leaf's box, which closes it; the tree ends optimal at
* -4.7320508075688776, as the other order does.
NAME          JAOS
ROWS
 N  COST
 G  R1
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      2   R1        3
    MARKER    'MARKER'   'INTEND'
    C2        R1        -2
    MARKER    'MARKER'   'INTORG'
    C3        COST      0
    MARKER    'MARKER'   'INTEND'
    C4        COST      -1   R1        3
RHS
    RHS       R1        3
RANGES
BOUNDS
 UP BND       C1        1
 FX BND       C2        0
 UP BND       C3        1
QCMATRIX   R1
    C2        C2        -0.5
    C4        C4        -0.5
CSECTION   K1   0.0   RQUAD
    C2
    C4
    C3
INDICATORS
 IF R1        C3        0
ENDATA
