* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 L  R1
 G  R2
 L  R3
 G  R4
 G  R5
 E  R6
COLUMNS
    C1        COST      4   R1        3
    C1        R2        -1   R3        -2
    C1        R6        -2
    C2        COST      5   R1        3
    C2        R3        3   R4        3
    C2        R5        -3   R6        -1
    C3        COST      -5   R1        -3
    C3        R2        -3   R3        -3
    C3        R6        1
    C4        COST      -4   R1        -1
    C4        R3        -2   R4        2
    C5        COST      3   R3        -3
    C6        COST      5   R1        2
    C6        R3        -1   R4        1
    C6        R6        -2
    C7        COST      -3   R3        -1
    C8        COST      1   R1        -1
    C8        R2        3   R4        1
RHS
    RHS       R1        3
    RHS       R2        -3
    RHS       R3        -3
    RHS       R4        4
    RHS       R5        -1
    RHS       R6        3
RANGES
BOUNDS
 FX BND       C1        1
 LO BND       C4        -2
 UP BND       C4        3
 UP BND       C5        1
 LO BND       C6        -2
 UP BND       C6        3
 UP BND       C7        1
 UP BND       C8        1
QCMATRIX   R1
    C1        C1        1.5
    C3        C3        1.5
QCMATRIX   R2
    C3        C3        -0.5
    C7        C7        -0.5
    C8        C8        -1.5
CSECTION   K1   0.0   RQUAD
    C4
    C3
    C2
    C6
ENDATA
