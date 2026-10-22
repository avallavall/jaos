* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 L  R1
 E  R2
 L  R3
 G  R4
 G  R5
 G  R6
COLUMNS
    C1        COST      3   R1        -3
    C2        COST      5   R1        -1
    C2        R2        -2   R3        2
    C2        R6        1
    C3        COST      -5   R1        -3
    C3        R2        1   R3        -3
    C3        R5        -3
    C4        COST      5   R1        3
    C4        R2        -1   R3        3
    C4        R4        -3   R6        3
    C5        COST      4   R1        -2
    C5        R2        -2   R3        3
    C5        R5        -1
    C6        COST      -4   R1        -2
    C6        R3        -1   R6        2
    C7        COST      -3   R1        -1
    C8        COST      1   R3        -1
    C8        R5        3   R6        1
RHS
    RHS       R1        -3
    RHS       R2        3
    RHS       R3        3
    RHS       R4        -1
    RHS       R5        -3
    RHS       R6        4
RANGES
BOUNDS
 UP BND       C1        1
 LO BND       C2        -2
 UP BND       C2        3
 FX BND       C5        1
 LO BND       C6        -2
 UP BND       C6        3
 UP BND       C7        1
 UP BND       C8        1
QCMATRIX   R3
    C3        C3        1.5
    C5        C5        1.5
QCMATRIX   R5
    C3        C3        -0.5
    C7        C7        -0.5
    C8        C8        -1.5
CSECTION   K1   0.0   RQUAD
    C6
    C3
    C4
    C2
ENDATA
