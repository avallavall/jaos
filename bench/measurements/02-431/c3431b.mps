* written by JAOS 0.5.0
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
COLUMNS
    C1        COST      0
    C2        COST      -5   R1        -2
    C3        COST      2   R1        1
    C4        COST      5   R1        -1
    C5        COST      4
    C6        COST      4   R1        -1
    C7        COST      -1
RHS
    RHS       R1        -2
RANGES
    RNG       R1        1
BOUNDS
 UP BND       C1        1
 UP BND       C2        1
 LO BND       C4        1
 UP BND       C4        4
 FR BND       C6
 UP BND       C7        1
CSECTION   K1   0.0   RQUAD
    C6
    C1
    C3
    C2
CSECTION   K2   0.0   QUAD
    C7
    C4
    C5
ENDATA
