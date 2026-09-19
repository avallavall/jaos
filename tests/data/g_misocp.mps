* minimise t over t >= ||(x - 1.6, y - 2.3)|| with x and y integer in
* [0, 5]: the nearest integer point is (2, 2), at t = 0.5.
NAME          GMISOCP
ROWS
 N  obj
 E  r1
 E  r2
COLUMNS
    t         obj       1
    MARKER    'MARKER'  'INTORG'
    x         r1        1
    y         r2        1
    MARKER    'MARKER'  'INTEND'
    u         r1        -1
    v         r2        -1
RHS
    rhs       r1        1.6
    rhs       r2        2.3
BOUNDS
 UP bnd       x         5
 UP bnd       y         5
 FR bnd       u
 FR bnd       v
CSECTION      K1        0.0       QUAD
    t
    u
    v
ENDATA
