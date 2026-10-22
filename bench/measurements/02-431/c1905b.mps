* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 G  R1
COLUMNS
    C1        R1        -1
    C2        COST      1   R1        -1
    C3        COST      -1
    C4        COST      -4   R1        -2
    C5        COST      -5
    C6        COST      1
RHS
RANGES
BOUNDS
 LO BND       C1        -2
 UP BND       C1        3
 UP BND       C2        4
 UP BND       C4        4
 UP BND       C5        2
 UP BND       C6        1
QCMATRIX   R1
    C3        C3        -1.5
    C6        C6        -0.5
CSECTION   K1   0.0   RQUAD
    C2
    C1
    C6
CSECTION   K2   0.0   QUAD
    C3
    C4
    C5
ENDATA
