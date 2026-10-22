* bench/measurements/02-425/warmcone.c 300 2 8 misocp, model 208 after
* its fifth edit. A leaf's box fixes every integer column, and its fixed
* model ends numerical_error at a point the checker passes as primal
* feasible, at -155.6. The tree set the leaf aside and ended
* numerical_error with the incumbent at -150. The point is now an
* incumbent, within the gap of the leaf's bound.
NAME          JAOS
ROWS
 N  COST
 L  R1
 G  R2
 G  R3
 G  R4
 L  R5
 L  R6
 L  R7
 E  R8
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      1   R3        2
    C1        R5        3   R7        2
    C1        R8        -1
    MARKER    'MARKER'   'INTEND'
    C2        COST      -9   R2        2
    C2        R4        -3   R5        2
    C2        R6        3   R8        1
    MARKER    'MARKER'   'INTORG'
    C3        COST      -3   R1        2
    C3        R3        1   R4        -2
    C3        R6        1   R7        -1
    MARKER    'MARKER'   'INTEND'
    C4        COST      1   R5        -2
    C5        COST      -9   R1        -3
    C5        R2        2   R3        -1
    C5        R6        1   R7        2
    C5        R8        -2
    C6        R1        3   R2        -3
    C6        R3        -1   R4        3
    C6        R5        -1   R6        1
    C6        R8        2
    MARKER    'MARKER'   'INTORG'
    C7        COST      -2   R3        1
    C7        R5        -3   R6        1
    C8        COST      7   R1        -1
    C8        R2        3   R4        1
    C8        R5        -2
    C9        COST      -4   R2        3
    C9        R4        3   R5        -1
    C9        R7        -3
    C10       COST      1   R1        1
    C10       R3        1   R4        1
    C10       R6        1   R8        -1
    C11       COST      9   R1        1
    C11       R2        -1   R4        -3
    MARKER    'MARKER'   'INTEND'
    C12       COST      8   R2        -3
    C12       R3        1   R4        -3
    C12       R5        -1   R7        2
    C12       R8        3
    MARKER    'MARKER'   'INTORG'
    C13       COST      -3   R2        -1
    C13       R3        1   R4        1
    C13       R6        -1
    C14       COST      -3   R1        -2
    C14       R2        -3   R3        1
    C14       R4        -1
    MARKER    'MARKER'   'INTEND'
    C15       COST      2   R2        -1
    C15       R3        -2   R5        -1
    C15       R8        1
    MARKER    'MARKER'   'INTORG'
    C16       COST      9   R1        3
    C16       R2        -3   R3        -2
    C16       R4        3
    C17       COST      -6   R2        -2
    C17       R4        -3   R6        -3
    C17       R7        -2   R8        3
    MARKER    'MARKER'   'INTEND'
    C18       COST      -9   R2        1
    C18       R7        -2   R8        1
    C19       COST      -6   R1        3
    C19       R2        1   R5        -3
    MARKER    'MARKER'   'INTORG'
    C20       R7        2   R8        2
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       COST      2
    RHS       R1        -15
    RHS       R2        -23
    RHS       R3        41
    RHS       R4        -9
    RHS       R6        21
    RHS       R7        13
    RHS       R8        19
RANGES
    RNG       R4        4
BOUNDS
 PL BND       C1
 UP BND       C2        2
 UP BND       C3        4
 UP BND       C4        2
 UP BND       C5        6
 UP BND       C6        1
 UP BND       C7        4
 UP BND       C8        1
 UP BND       C9        10
 PL BND       C10
 UP BND       C11       6
 LO BND       C12       5
 UP BND       C12       7
 UP BND       C13       7
 PL BND       C14
 UP BND       C15       8
 UP BND       C16       1
 UP BND       C17       4
 UP BND       C18       7
 UP BND       C19       9
 FX BND       C20       3
CSECTION   K1   0.0   QUAD
    C6
    C4
    C18
    C17
ENDATA
