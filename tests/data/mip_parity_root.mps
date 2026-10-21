* bench/measurements/02-235/certs.c 2000 21, model 1113. The tree's parity
* test proves that no integer point exists before any node is solved, and
* the relaxation as loaded is infeasible too, so the MIP carries the
* relaxation's certificate.
NAME          JAOS
ROWS
 N  COST
 G  R1
 E  R2
 G  R3
 E  R4
 E  R5
 E  R6
 E  R7
 G  R8
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      3   R1        1
    C1        R4        2   R5        1
    C1        R7        1   R8        -3
    C2        COST      -3   R1        1
    C2        R2        3   R7        -3
    C2        R8        1
    C3        COST      6   R3        1
    C3        R6        2   R7        -1
    C4        COST      -9   R3        1
    C4        R7        1   R8        2
    C5        COST      8   R6        3
    C6        COST      -7   R3        -3
    C6        R5        3   R7        -1
    C6        R8        -1
    C7        COST      6   R1        -2
    C7        R2        -2   R3        2
    C7        R5        1   R6        -3
    C7        R7        -2
    C8        COST      1   R1        -3
    C8        R2        2   R7        2
    C8        R8        2
    MARKER    'MARKER'   'INTEND'
    C9        COST      -1   R3        3
    C9        R5        -2   R8        1
    MARKER    'MARKER'   'INTORG'
    C10       COST      6   R1        1
    C10       R2        -3   R3        -2
    C10       R4        1   R5        -2
    C10       R7        2
    C11       COST      8   R1        1
    C11       R2        -2   R4        -2
    C11       R5        3
    C12       COST      9   R4        -1
    C12       R6        3   R7        3
    C13       COST      -2   R1        -3
    C13       R3        2   R5        3
    C13       R7        1
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       COST      -2
    RHS       R1        -32
    RHS       R2        21
    RHS       R3        3
    RHS       R4        -41
    RHS       R5        72
    RHS       R6        54
    RHS       R7        16
    RHS       R8        15
RANGES
    RNG       R1        2
    RNG       R3        1
BOUNDS
 UP BND       C1        7
 PL BND       C2
 PL BND       C3
 UP BND       C4        4
 UP BND       C5        1
 PL BND       C6
 UP BND       C7        3
 UP BND       C8        6
 UP BND       C9        7
 UP BND       C10       1
 UP BND       C11       3
 PL BND       C12
 UP BND       C13       3
ENDATA
