* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 G  R1
COLUMNS
    C1        COST      1   R1        -1
    C2        COST      -5
    C3        COST      -4   R1        -2
    C4        R1        -1
    C5        COST      -1
    C6        COST      1
RHS
RANGES
BOUNDS
 UP BND       C1        4
 UP BND       C2        2
 UP BND       C3        4
 LO BND       C4        -2
 UP BND       C4        3
 UP BND       C6        1
QCMATRIX   R1
    C5        C5        -1.5
    C6        C6        -0.5
CSECTION   K1   0.0   RQUAD
    C1
    C4
    C6
CSECTION   K2   0.0   QUAD
    C5
    C3
    C2
ENDATA
