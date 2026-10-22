* bench/measurements/02-431/permcone.c 2000 1, model 562 in the permuted
* order. C6 is semi-continuous in [1, 4], beside two rotated cones and a
* concave quadratic row. A rough leaf with every integer column fixed was
* set aside, because its semi-continuous column kept the fixed model from
* being the node, and the tree ended numerical_error; the other order
* ends optimal at -10.5. The node's own relaxation is now solved in full
* there, and the bound from its duals reads the rows' and cones' implied
* bounds.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
COLUMNS
    C1        COST      -3   R1        1
    MARKER    'MARKER'   'INTORG'
    C2        COST      -2   R1        3
    MARKER    'MARKER'   'INTEND'
    C3        COST      1
    C4        COST      1   R1        -2
    MARKER    'MARKER'   'INTORG'
    C5        COST      -2   R1        -2
    MARKER    'MARKER'   'INTEND'
    C6        COST      -5   R1        2
    C7        COST      -3
RHS
    RHS       R1        -1
RANGES
BOUNDS
 LO BND       C1        2
 UP BND       C1        3
 UP BND       C2        1
 UP BND       C3        6
 LO BND       C4        -2
 UP BND       C4        3
 PL BND       C5
 LO BND       C6        1
 SC BND       C6        4
 LO BND       C7        1
 UP BND       C7        3
QCMATRIX   R1
    C1        C1        -1.5
CSECTION   K1   0.0   RQUAD
    C1
    C5
    C3
CSECTION   K2   0.0   RQUAD
    C2
    C7
    C6
ENDATA
