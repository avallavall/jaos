* Model 1 of seed 1 in bench/measurements/02-235: a MIP whose relaxation
* is infeasible. The tree's root proves it with a Farkas ray, and since
* 2026-09-14 that ray is the model's certificate, the same one an LP with
* these rows would publish; before, a MIP published none.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
 E  R2
 G  R3
 L  R4
 E  R5
 L  R6
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      -8   R2        -1
    MARKER    'MARKER'   'INTEND'
    C2        COST      -9   R1        1
    C2        R2        1   R3        -1
    C2        R5        2   R6        3
    MARKER    'MARKER'   'INTORG'
    C3        COST      -3   R2        -1
    C3        R3        2   R4        3
    C3        R6        2
    C4        R2        3   R3        3
    C4        R4        -2   R6        1
    C5        COST      -9   R1        -1
    C5        R4        3   R6        -2
    MARKER    'MARKER'   'INTEND'
    C6        COST      -8
    C7        COST      -3   R1        -1
    C7        R5        1   R6        2
    C8        COST      6   R3        1
    C8        R4        -3   R6        -3
    C9        COST      -8   R2        1
    C9        R3        -2   R6        3
    C10       COST      2   R1        2
    C10       R2        1   R3        2
    C10       R4        -1   R5        1
    C10       R6        1
    MARKER    'MARKER'   'INTORG'
    C11       COST      7   R1        3
    C11       R3        1   R4        -3
    C11       R6        3
    C12       COST      -5   R2        -3
    C12       R4        2
    MARKER    'MARKER'   'INTEND'
    C13       COST      3   R3        3
    C13       R4        -3   R5        -2
    C14       COST      2   R1        2
    C14       R5        -1
    MARKER    'MARKER'   'INTORG'
    C15       R2        1   R3        1
    C15       R4        1   R5        -3
    C16       COST      3   R1        3
    C16       R2        3
    MARKER    'MARKER'   'INTEND'
    C17       COST      2   R1        -2
    C17       R3        -1   R6        -2
    C18       R1        2   R5        3
    MARKER    'MARKER'   'INTORG'
    C19       COST      5   R1        3
    C19       R4        -2   R5        -2
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       COST      -1
    RHS       R1        77
    RHS       R2        -13
    RHS       R3        72
    RHS       R4        -59
    RHS       R5        -17
    RHS       R6        -14
RANGES
BOUNDS
 UP BND       C1        2
 UP BND       C2        5
 UP BND       C3        1
 UP BND       C4        9
 UP BND       C5        10
 UP BND       C6        6
 UP BND       C7        4
 UP BND       C8        10
 UP BND       C9        5
 UP BND       C10       9
 UP BND       C12       7
 UP BND       C13       10
 UP BND       C16       8
 UP BND       C17       1
 UP BND       C18       6
 UP BND       C19       4
ENDATA
