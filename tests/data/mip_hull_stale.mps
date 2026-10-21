* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 G  R1
 E  R2
 E  R3
 G  R4
 G  R5
 L  R6
 G  R7
 G  R8
 L  R9
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      -1   R2        2
    C1        R3        1   R8        -1
    C2        COST      6   R2        -2
    C2        R3        2   R4        1
    C2        R6        2   R7        3
    C2        R9        1
    C3        COST      6   R4        -1
    C3        R5        1
    C4        COST      -3   R1        2
    C4        R2        -1   R5        1
    C4        R6        -1   R8        3
    C4        R9        3
    MARKER    'MARKER'   'INTEND'
    C5        COST      -2   R4        -2
    C5        R5        1   R6        -1
    C5        R7        2
    MARKER    'MARKER'   'INTORG'
    C6        COST      -1   R1        2
    C6        R2        2   R6        3
    C7        COST      2   R3        3
    C7        R5        -3   R7        -3
    C7        R8        -1
    C8        R3        -3   R4        1
    C8        R6        2   R8        2
    C8        R9        3
    C9        COST      -8   R2        1
    C9        R4        -1   R5        -1
    C9        R7        2   R9        2
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       R1        3
    RHS       R2        24
    RHS       R3        9
    RHS       R4        -5
    RHS       R5        -8
    RHS       R6        19
    RHS       R7        14
    RHS       R8        -6
    RHS       R9        21
RANGES
BOUNDS
 UP BND       C1        9
 UP BND       C2        2
 UP BND       C3        4
 UP BND       C4        3
 UP BND       C5        1
 UP BND       C6        3
 UP BND       C7        1
 UP BND       C8        6
 UP BND       C9        9
ENDATA
