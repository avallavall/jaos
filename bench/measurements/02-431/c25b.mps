* written by JAOS 0.5.0
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
 L  R2
 E  R3
 G  R4
 E  R5
 L  R6
COLUMNS
    C1        COST      -1   R2        -2
    C1        R3        2   R4        -3
    C1        R6        -3
    C2        R1        3   R3        3
    C2        R5        3   R6        -3
    C3        R2        2   R3        -1
    C3        R6        -1
    C4        COST      -5   R4        -1
    C4        R5        2
    C5        COST      2   R1        1
    C5        R5        1   R6        -3
RHS
    RHS       R1        -3
    RHS       R3        -2
    RHS       R5        -2
    RHS       R6        4
RANGES
BOUNDS
 UP BND       C1        1
 LO BND       C2        -2
 UP BND       C2        3
 LO BND       C3        -2
 UP BND       C3        3
 UP BND       C4        3
 LO BND       C5        -2
 UP BND       C5        3
QCMATRIX   R4
    C3        C3        -0.5
    C4        C4        -0.5
ENDATA
