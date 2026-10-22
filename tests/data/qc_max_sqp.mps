* bench/measurements/02-433/flipcone.c 20000 1 continuous, model 7550, a
* maximisation with one concave quadratic row whose optimum the checker
* refuses after the settle. The quadratic programs on the row's
* linearisation ran only on a minimisation, so the solve ended
* numerical_error; the same model minimising the negated objective ended
* optimal after 7 of them. Both now end optimal at the same point,
* 14.000000031784593.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 G  R1
COLUMNS
    C1        COST      2
    C2        COST      2   R1        2
    C3        COST      -4
RHS
    RHS       R1        6
RANGES
BOUNDS
 UP BND       C1        4
 LO BND       C2        -2
 UP BND       C2        3
 LO BND       C3        -2
 UP BND       C3        3
QCMATRIX   R1
    C1        C1        -1
ENDATA
