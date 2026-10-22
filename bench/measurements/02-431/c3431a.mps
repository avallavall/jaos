* written by JAOS 0.5.0
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
COLUMNS
    C1        COST      -1
    C2        COST      4
    C3        COST      2   R1        1
    C4        COST      -5   R1        -2
    C5        COST      0
    C6        COST      4   R1        -1
    C7        COST      5   R1        -1
RHS
    RHS       R1        -2
RANGES
    RNG       R1        1
BOUNDS
 UP BND       C1        1
 UP BND       C4        1
 UP BND       C5        1
 FR BND       C6
 LO BND       C7        1
 UP BND       C7        4
CSECTION   K1   0.0   RQUAD
    C6
    C5
    C3
    C4
CSECTION   K2   0.0   QUAD
    C1
    C7
    C2
ENDATA
