* The badly scaled box of TODO.md row C5: fifteen bound-only columns of
* QPLIB_9002 with a separable Q, two of them near 1e9 against Q entries
* of 4e-11, and one free column alone in a cone of one member, beside a
* cone the walk cannot leave out (t >= |u| with u at 1 and t costed), so
* the walk runs. The fifteen touch no row, no cone and no other column,
* so each takes the minimiser of its own term and the walk solves the
* rest. With two of them in a row the walk sees the box and fails.
NAME          GBADBOX
ROWS
 N  obj
COLUMNS
    x0        obj       0
    x1        obj       0
    x2        obj       0
    x3        obj       0
    x4        obj       0
    x5        obj       0
    x6        obj       0
    x7        obj       0
    x8        obj       0
    x9        obj       0
    x10       obj       0
    x11       obj       0
    x12       obj       0
    x13       obj       0
    x14       obj       0
    x15       obj       0
    t         obj       1
    u         obj       0
RHS
BOUNDS
 FR bnd       x0
 MI bnd       x1
 UP bnd       x1        0
 MI bnd       x2
 UP bnd       x2        0
 MI bnd       x3
 UP bnd       x3        0
 MI bnd       x4
 UP bnd       x4        0
 MI bnd       x5
 UP bnd       x5        0
 MI bnd       x6
 UP bnd       x6        0
 MI bnd       x7
 UP bnd       x7        0
 MI bnd       x8
 UP bnd       x8        0
 MI bnd       x9
 UP bnd       x9        0
 MI bnd       x10
 UP bnd       x10       0
 LO bnd       x11       1736510
 UP bnd       x11       21706300
 MI bnd       x12
 UP bnd       x12       0
 LO bnd       x13       1838820000
 UP bnd       x13       22985200000
 MI bnd       x14
 UP bnd       x14       0
 MI bnd       x15
 UP bnd       x15       0
 FR bnd       t
 FX bnd       u         1
QUADOBJ
    x1        x1        2
    x2        x2        2
    x3        x3        2
    x4        x4        2
    x5        x5        2
    x6        x6        2
    x7        x7        2
    x8        x8        2
    x9        x9        2
    x10       x10       2
    x11       x11       4.606946e-08
    x12       x12       2
    x13       x13       4.350616e-11
    x14       x14       2
    x15       x15       2
CSECTION      K1        0.0       QUAD
    x0
CSECTION      K2        0.0       QUAD
    t
    u
ENDATA
