* bench/measurements/02-434/flip.c 20000 7 miqp, model 7087 with its
* objective negated and its sense the other one: a maximisation with a
* concave quadratic objective, two integer columns and an indicator row.
* The feasibility pump copies the root relaxation and minimises its
* distance there; the copy kept the concave term as it was, so on a
* maximisation its programs were not convex and the pump found nothing,
* while the same model as a minimisation took the pump's point. Both now
* end at the same point.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 L  R1
 G  R2
 G  R3
 L  R4
COLUMNS
    C1        COST      -5   R1        3
    C1        R2        1
    C2        COST      1   R1        -3
    C2        R3        -1
    MARKER    'MARKER'   'INTORG'
    C3        COST      -5   R1        -2
    C3        R2        2   R3        3
    C3        R4        1
    C4        R2        2   R3        3
    C4        R4        -2
    MARKER    'MARKER'   'INTEND'
    C5        COST      -1   R1        3
    C5        R2        -3
RHS
    RHS       R1        -1
    RHS       R2        -3
    RHS       R3        -1
    RHS       R4        3
RANGES
    RNG       R2        1
    RNG       R3        2
BOUNDS
 LO BND       C2        -2
 UP BND       C2        3
 LO BND       C3        -2
 UP BND       C3        3
 UP BND       C4        1
 LO BND       C5        1
 UP BND       C5        4
QUADOBJ
    C3        C3        -2
    C5        C5        -1
INDICATORS
 IF R2        C4        0
ENDATA
