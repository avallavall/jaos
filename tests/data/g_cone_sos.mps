* minimise t over (t, u, v) in the second-order cone, with u = x1 - 3,
* v = x2 - 4 and at most one of x1, x2 nonzero: x1 = 0, x2 = 4, t = 3.
NAME          GCONESOS
ROWS
 N  obj
 E  r1
 E  r2
COLUMNS
    t         obj       1
    u         r1        1
    v         r2        1
    x1        r1        -1
    x2        r2        -1
RHS
    rhs       r1        -3
    rhs       r2        -4
BOUNDS
 FR bnd       t
 FR bnd       u
 FR bnd       v
 UP bnd       x1        10
 UP bnd       x2        10
SOS
 S1 SOS s1
    x1        1
    x2        2
CSECTION      K1        0.0       QUAD
    t
    u
    v
ENDATA
