* written by JAOS 0.5.0
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
 G  R2
 G  R3
COLUMNS
    C1        COST      -1   R2        3
    C2        COST      1   R1        2
    C2        R3        1
    C3        COST      -5   R3        1
    C4        COST      -3   R1        1
    C4        R3        -1
    C5        COST      1   R2        -2
    C5        R3        2
    C6        COST      3   R1        -2
    C6        R2        -1
RHS
    RHS       R1        8
    RHS       R2        -3
    RHS       R3        2
RANGES
BOUNDS
 LO BND       C1        1
 UP BND       C1        3
 UP BND       C2        4
 UP BND       C3        1
 UP BND       C4        1
 UP BND       C5        3
 FR BND       C6
CSECTION   K1   0.0   QUAD
    C4
    C6
    C1
    C5
ENDATA
