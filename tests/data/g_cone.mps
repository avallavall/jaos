* minimise t over (t, x, y) in the second-order cone, with x = 3 and y = 4:
* t = 5, and the cone's dual is (1, -0.6, -0.8).
NAME          GCONE
ROWS
 N  obj
 E  r1
 E  r2
COLUMNS
    t         obj       1
    x         r1        1
    y         r2        1
RHS
    rhs       r1        3
    rhs       r2        4
BOUNDS
 FR bnd       t
 FR bnd       x
 FR bnd       y
CSECTION      K1        0.0       QUAD
    t
    x
    y
ENDATA
