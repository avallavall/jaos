* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 G  R1
 L  R2
COLUMNS
    C1        COST      3
    C2        COST      3
    C3        COST      -5   R2        1
    C4        COST      -1   R2        -1
    C5        COST      -3
RHS
    RHS       R1        -2
    RHS       R2        -2
RANGES
BOUNDS
 FR BND       C1
 FR BND       C3
 LO BND       C4        2
 UP BND       C4        4
 UP BND       C5        1
QCMATRIX   R1
    C5        C5        -1.5
CSECTION   K1   0.0   QUAD
    C5
    C3
CSECTION   K2   0.0   QUAD
    C2
    C4
    C1
ENDATA
