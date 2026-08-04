* Golden instance 1: fixed-style layout, all row types, RANGES on a G row,
* objective constant via RHS on the COST row, a Fortran D exponent, LO and
* UP bounds. Expected model spelled out in tests/test_mps.c.
NAME          T1
ROWS
 N  COST
 L  LIM1
 G  LIM2
 E  EQ1
COLUMNS
    X1        COST         1.0   LIM1         1.0
    X1        LIM2         1.0
    X2        COST         2.0   LIM1         1.0
    X2        EQ1         -1.0
    X3        COST        -1.0D0
    X3        LIM2         1.0   EQ1          1.0
RHS
    RHS       LIM1         4.0   LIM2         1.0
    RHS       EQ1          7.0   COST         3.5
RANGES
    RNG       LIM2         2.5
BOUNDS
 UP BND       X1           4.0
 LO BND       X2          -1.0
ENDATA
