* bench/measurements/02-427/warmqc.c 1000 8 8 miqcp, model 95 after its
* first edit. A rough node's leaf has quadratic rows, so the conic tree
* gave it no bound from its duals, and the tree ended numerical_error
* with the incumbent at -15.5000093 and the bound at -22.03. The duals now
* bound it through each column's smallest d x + h x^2 / 2 over its box.
NAME          JAOS
ROWS
 N  COST
 E  R1
 E  R2
 G  R3
 G  R4
 G  R5
 L  R6
 G  R7
 L  R8
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      -3   R2        3
    C1        R5        1   R6        -1
    C2        COST      4   R5        -1
    C2        R8        3
    MARKER    'MARKER'   'INTEND'
    C3        COST      -5   R4        -3
    C3        R5        2   R7        -1
    C4        COST      5   R1        1
    C4        R6        3
    C5        COST      2   R1        1
    C5        R3        1   R5        -3
    C5        R6        1   R7        3
    C6        COST      -4   R7        -3
    MARKER    'MARKER'   'INTORG'
    C7        COST      7   R7        3
    MARKER    'MARKER'   'INTEND'
    C8        COST      6   R2        -3
    C8        R4        1   R6        -3
    C8        R7        1
    MARKER    'MARKER'   'INTORG'
    C9        COST      -2   R1        1
    C9        R2        -2   R3        -2
    C9        R4        1   R6        -3
    C9        R7        -3
    C10       COST      -2   R2        3
    C10       R4        1   R5        1
    C10       R6        -3   R7        -3
    C10       R8        1
    MARKER    'MARKER'   'INTEND'
    C11       COST      -6   R2        -2
    C11       R6        -1
    C12       COST      8   R2        -2
    C12       R5        2   R7        1
    C13       COST      -3   R1        3
    C13       R2        1   R3        3
    MARKER    'MARKER'   'INTORG'
    C14       COST      2   R1        3
    C14       R3        -3   R6        1
    C14       R7        -1
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       COST      2
    RHS       R1        28
    RHS       R2        -4
    RHS       R3        -21
    RHS       R4        1
    RHS       R5        5
    RHS       R6        3
    RHS       R7        7
    RHS       R8        18
RANGES
BOUNDS
 LO BND       C1        4
 UP BND       C2        1
 UP BND       C3        2
 UP BND       C4        4
 UP BND       C5        9
 UP BND       C6        8
 UP BND       C7        3
 UP BND       C8        8
 UP BND       C9        3
 UP BND       C10       9
 UP BND       C11       9
 UP BND       C12       1
 UP BND       C13       2
 UP BND       C14       10
QCMATRIX   R7
    C12       C12       -0.5
QCMATRIX   R8
    C1        C1        0.5
    C10       C10       1.5
    C12       C12       1.5
ENDATA
