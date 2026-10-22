* bench/measurements/02-435/mirror.c 20000 2 miqp, model 4764. Coefficient
* tightening reads R4's largest activity from implied bounds that carry
* the primal tolerance's widening, and so turned C5's coefficient 1 into
* 8.67e-7 where exact arithmetic gives 0. That entry forced scale factors
* from 2^-11 to 2^9, the barrier's push on the root relaxation did not
* settle, and the tree ended numerical_error. A tightening whose new
* coefficient is no larger than that widening is now skipped; the model
* ends optimal at -11.
NAME          JAOS
ROWS
 N  COST
 G  R1
 G  R2
 G  R3
 L  R4
 L  R5
COLUMNS
    C1        COST      2   R4        -3
    C2        COST      -1   R1        2
    C2        R3        1   R5        2
    C3        COST      3   R3        -3
    C3        R4        -2
    C4        COST      -1   R1        -1
    C4        R3        2   R5        -2
    MARKER    'MARKER'   'INTORG'
    C5        COST      4   R2        -1
    C5        R4        1   R5        -1
    MARKER    'MARKER'   'INTEND'
    C6        COST      2   R2        -3
    C6        R3        3
RHS
    RHS       R1        -2
    RHS       R2        4
    RHS       R4        5
    RHS       R5        2
RANGES
    RNG       R1        1
    RNG       R3        2
BOUNDS
 UP BND       C1        4
 FR BND       C3
 UP BND       C5        1
 LO BND       C6        -2
 UP BND       C6        3
QUADOBJ
    C2        C2        2
INDICATORS
 IF R2        C5        0
ENDATA
