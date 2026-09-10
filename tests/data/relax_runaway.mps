* Over the columns alone the elastic copy frees every column, and a free
* integer column gives the tree an unbounded space. These rows admit no
* integer point: C3 = 2 from R3, C4 = 3 C2 - 1 from R2, and R1 then asks
* 3 C1 + 2 C4 = -9.5, which no pair of integers gives. Proving that needs
* the whole space, so `relax --cols` on this model needs a work limit.
NAME runaway
ROWS
 N obj
 E r1
 E r2
 E r3
COLUMNS
    MARKER    'MARKER'   'INTORG'
    x1 obj 1 r1 -2
    MARKER    'MARKER'   'INTEND'
    x2 obj -1 r1 2
    x2 r2 -3
    x3 obj 2 r2 3
    x3 r3 -1
    MARKER    'MARKER'   'INTORG'
    x4 r1 -2 r2 1
    MARKER    'MARKER'   'INTEND'
RHS
    RHS r1 7
    RHS r2 5
    RHS r3 -2
BOUNDS
 UP BND x1 3
 UP BND x2 4
 UP BND x3 6
 UP BND x4 2
ENDATA
