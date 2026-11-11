* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 G  R1
 E  R2
 L  R3
 G  R4
 L  R5
 G  R6
 G  R7
 G  R8
COLUMNS
    C1        COST      -7   R2        -2
    C1        R6        -1
    C2        COST      9   R1        -1
    C2        R2        -3   R4        -1
    C2        R5        1
    C3        COST      9   R3        2
    C3        R5        -1   R7        -2
    C4        COST      -4   R1        3
    C4        R4        -3
    C5        COST      -1   R1        3
    C5        R2        2   R3        1
    C5        R6        -2
    C6        COST      8   R1        3
    C6        R2        1   R3        -1
    C7        R2        1   R3        1
    C7        R5        3   R6        1
    C8        COST      -2   R1        -1
    C8        R4        -1   R7        2
    C9        R2        -1   R3        1
    C9        R4        1   R6        1
    C10       COST      4   R1        1
    C10       R2        3   R3        2
    C10       R5        -3
    C11       COST      8   R1        -2
    C11       R3        1   R4        2
    C11       R5        -2   R6        -1
    C12       COST      9   R1        2
    C12       R2        2   R3        3
    C12       R6        -2   R7        3
    C12       R8        -1
    C13       COST      5   R1        -1
    C13       R3        1   R4        -3
    C13       R5        1
    C14       COST      -3   R3        3
    C14       R7        -2
    C15       COST      -3   R2        -3
    C15       R4        1   R5        -2
    C16       COST      -6   R4        -3
    C16       R5        2   R6        -2
    C16       R7        -3
RHS
    RHS       COST      -3
    RHS       R1        4
    RHS       R2        -3
    RHS       R3        67
    RHS       R4        -74
    RHS       R5        1
    RHS       R6        -14
    RHS       R7        -5
RANGES
    RNG       R1        2
BOUNDS
 UP BND       C1        3
 UP BND       C2        3
 UP BND       C3        3
 UP BND       C4        8
 UP BND       C5        2
 UP BND       C6        7
 UP BND       C7        9
 UP BND       C8        2
 UP BND       C9        3
 UP BND       C11       6
 UP BND       C12       1
 UP BND       C13       6
 LO BND       C15       -2
 UP BND       C15       3
 LO BND       C16       -2
 UP BND       C16       2
QCMATRIX   R7
    C9        C9        -1
QCMATRIX   R8
    C9        C9        -1
    C10       C10       -1.5
    C13       C13       -1.5
ENDATA
