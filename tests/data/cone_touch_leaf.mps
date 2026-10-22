* bench/measurements/02-425/warmcone.c 300 1 8 misocp, model 257 after
* its first edit. A leaf fixes C1 = 1 and C7 = 2, the heads of the
* rotated cone K1, which then holds C17^2 + C14^2 <= 4 and meets the
* rows at C17 = 2, C14 = 0 only. Its relaxation stops rough, and the tree
* ended numerical_error with the incumbent at -53.6013 and the bound at
* -53.6279. The rough node's own duals now bound it.
NAME          JAOS
ROWS
 N  COST
 E  R1
 G  R2
 E  R3
 E  R4
 G  R5
 L  R6
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      2   R1        3
    C1        R3        2   R5        1
    C2        COST      4   R2        -2
    C2        R3        -2   R5        1
    C2        R6        -3
    C3        COST      -2   R1        2
    C3        R3        -2   R4        -2
    C3        R6        1
    MARKER    'MARKER'   'INTEND'
    C4        COST      6   R3        1
    C4        R4        -1   R5        -1
    C4        R6        -2
    MARKER    'MARKER'   'INTORG'
    C5        COST      -7   R2        1
    C5        R3        -2   R4        -3
    C5        R5        -1
    C6        COST      3   R1        3
    C6        R2        -2   R5        3
    C7        COST      -5   R1        2
    C7        R5        -3   R6        1
    C8        COST      6   R1        -1
    C8        R2        -3   R3        3
    C9        COST      -3   R3        3
    C9        R4        3   R5        -1
    C9        R6        1
    MARKER    'MARKER'   'INTEND'
    C10       COST      -4   R4        -1
    C10       R6        -1
    C11       COST      2   R1        3
    C11       R2        -1   R5        1
    C11       R6        3
    C12       COST      -7   R1        1
    C12       R2        2   R5        -3
    C12       R6        2
    C13       COST      8   R2        3
    C13       R3        -3   R5        -3
    C14       COST      -6   R2        1
    C14       R4        -2
    MARKER    'MARKER'   'INTORG'
    C15       COST      1   R2        3
    C15       R3        -3   R4        1
    C15       R5        1   R6        -2
    MARKER    'MARKER'   'INTEND'
    C16       COST      3   R1        1
    C16       R3        1   R4        3
    C16       R6        3
    C17       COST      -6   R1        2
    C17       R5        3   R6        -3
    MARKER    'MARKER'   'INTORG'
    C18       COST      2   R5        2
    C18       R6        -1
    MARKER    'MARKER'   'INTEND'
    C19       R1        1   R4        3
    C19       R5        1
    MARKER    'MARKER'   'INTORG'
    C20       COST      -4   R1        1
    C20       R2        2   R4        1
    C20       R6        1
    MARKER    'MARKER'   'INTEND'
    C21       COST      -3   R1        -1
    C22       COST      6   R1        1
    C22       R2        2   R3        -1
    C22       R5        2   R6        -2
    MARKER    'MARKER'   'INTORG'
    C23       R5        -2
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       COST      -1
    RHS       R1        12
    RHS       R2        -23
    RHS       R3        15
    RHS       R4        3
    RHS       R5        6
    RHS       R6        1
RANGES
BOUNDS
 UP BND       C1        1
 UP BND       C2        2
 UP BND       C3        3
 UP BND       C4        7
 UP BND       C5        3
 UP BND       C6        7
 UP BND       C7        2
 UP BND       C8        6
 UP BND       C9        5
 UP BND       C10       2
 UP BND       C11       10
 UP BND       C12       5
 UP BND       C13       2
 UP BND       C14       2
 UP BND       C15       1
 UP BND       C16       6
 UP BND       C17       9
 UP BND       C18       1
 UP BND       C19       5
 UP BND       C20       3
 UP BND       C21       6
 UP BND       C22       7
 UP BND       C23       4
CSECTION   K1   0.0   RQUAD
    C1
    C7
    C17
    C14
ENDATA
