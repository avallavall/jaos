* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 L  R1
 G  R2
 E  R3
COLUMNS
    C1        COST      1   R1        -2
    C1        R3        -3
    MARKER    'MARKER'   'INTORG'
    C2        R1        -3
    C3        COST      3   R2        3
    C3        R3        -3
    C4        COST      1   R3        2
    C5        COST      -5   R2        3
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       R2        3
    RHS       R3        -3
RANGES
BOUNDS
 LO BND       C1        -2
 UP BND       C1        3
 PL BND       C2
 FR BND       C3
 FR BND       C4
 UP BND       C5        1
QCMATRIX   R1
    C3        C3        0.5
    C5        C5        0.5
INDICATORS
 IF R1        C5        1
ENDATA
