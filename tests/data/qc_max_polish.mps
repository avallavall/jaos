* bench/measurements/02-433/flipcone.c 20000 1 continuous, model 446, a
* maximisation with one concave quadratic row. The Newton finish lets go
* of each column whose reduced cost pushes it off its bound; that test
* read the published reduced cost without the sense, so on a
* maximisation it let go of the right columns and the solve ended
* numerical_error. The same model minimising the negated objective ended
* optimal. Both now end optimal at the same point, 40.000000419175734.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
COLUMNS
    C1        COST      5
    C2        COST      5   R1        1
    C3        COST      4
    C4        COST      2
    C5        COST      -4   R1        2
    C6        COST      5
    C7        COST      -1
    C8        COST      -1
RHS
    RHS       R1        3
RANGES
BOUNDS
 LO BND       C1        -2
 UP BND       C1        3
 LO BND       C2        1
 UP BND       C2        2
 UP BND       C3        1
 FR BND       C4
 UP BND       C5        1
 LO BND       C6        1
 UP BND       C6        3
 UP BND       C7        1
QCMATRIX   R1
    C4        C4        -1.5
    C5        C5        -1
ENDATA
