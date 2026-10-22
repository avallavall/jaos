* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 L  R1
 G  R2
COLUMNS
    C1        COST      -1   R1        -1
    C2        COST      3
    C3        COST      3
    C4        COST      -3
    C5        COST      -5   R1        1
RHS
    RHS       R1        -2
    RHS       R2        -2
RANGES
BOUNDS
 LO BND       C1        2
 UP BND       C1        4
 FR BND       C3
 UP BND       C4        1
 FR BND       C5
QCMATRIX   R2
    C4        C4        -1.5
CSECTION   K1   0.0   QUAD
    C4
    C5
CSECTION   K2   0.0   QUAD
    C2
    C1
    C3
ENDATA
