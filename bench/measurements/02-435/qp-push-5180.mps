* written by JAOS 0.5.0
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 L  R1
 G  R2
 G  R3
 G  R4
 L  R5
COLUMNS
    C1        COST      3   R1        -1
    C1        R3        2   R5        2
    C2        COST      -5   R2        2
    C2        R4        1
    C3        COST      5   R1        -2
    C3        R3        3   R5        2
    C4        COST      -2   R1        2
    C4        R4        0.50000015
    C5        R3        1   R5        -1
    C6        COST      -5   R1        2
    C6        R2        -1
RHS
    RHS       R1        -4
    RHS       R2        3
    RHS       R3        7
    RHS       R4        1
    RHS       R5        3
RANGES
BOUNDS
 UP BND       C1        2
 UP BND       C2        4
 UP BND       C3        4
 UP BND       C4        1
 LO BND       C5        2
 UP BND       C5        3
 LO BND       C6        -2
 UP BND       C6        3
QUADOBJ
    C1        C1        -2
    C6        C6        -1
ENDATA
