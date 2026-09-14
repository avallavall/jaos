* Model 108 of seed 1 in bench/measurements/02-230. Over the columns the
* elastic copy's tree lands C3, an integer column, at -0.99999999999999989,
* and a move read off that value moved C3's lower bound to the same number,
* which no integer point clears: the relaxed model was infeasible. The move
* of an integer column is snapped to the integer its value is at.
NAME          JAOS
ROWS
 N  COST
 E  R1
 E  R2
 E  R3
 G  R4
 E  R5
COLUMNS
    C1        R1        -2   R2        -3
    C1        R3        -1
    C2        COST      -2   R1        3
    C2        R2        1   R3        1
    C2        R5        1
    MARKER    'MARKER'   'INTORG'
    C3        COST      1   R3        -1
    C3        R4        3   R5        2
    MARKER    'MARKER'   'INTEND'
    C4        COST      -2   R1        3
    C4        R3        -2   R5        -2
    C5        COST      -2   R4        -3
RHS
    RHS       R1        -20
    RHS       R3        1
    RHS       R5        -4
RANGES
    RNG       R4        2
BOUNDS
 UP BND       C1        3
 UP BND       C2        1
 UP BND       C3        2
 UP BND       C4        4
 UP BND       C5        1
ENDATA
