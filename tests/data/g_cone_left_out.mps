* minimise c + 2a over a + c >= 1, with (h, a, b) in a cone whose head has
* an upper bound of 0, and (g, u, v) in a cone whose head g is free, costs
* nothing and sits in no row, with u = 1 and v = 2. The first cone holds
* h, a and b at 0, so c = 1; g ends at the norm of (1, 2). A third cone,
* t >= |w| with w at 1 and t costed, is one the walk cannot leave out, so
* the walk runs and the objective is 2.
NAME          GLEFTOUT
ROWS
 N  obj
 G  r1
 E  r2
 E  r3
COLUMNS
    h         obj       0
    a         obj       2
    a         r1        1
    b         obj       0
    c         obj       1
    c         r1        1
    g         obj       0
    u         r2        1
    v         r3        1
    t         obj       1
    w         obj       0
RHS
    rhs       r1        1
    rhs       r2        1
    rhs       r3        2
BOUNDS
 LO bnd       h         -1
 UP bnd       h         0
 FR bnd       a
 FR bnd       b
 UP bnd       c         2
 FR bnd       g
 FR bnd       u
 FR bnd       v
 FR bnd       t
 FX bnd       w         1
CSECTION      K1        0.0       QUAD
    h
    a
    b
CSECTION      K2        0.0       QUAD
    g
    u
    v
CSECTION      K3        0.0       QUAD
    t
    w
ENDATA
