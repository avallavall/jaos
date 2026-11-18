* Pugh's example (The Omega test, 1991): 27 <= 11 x + 13 y <= 45 and
* -10 <= 7 x - 9 y <= 4, written as four one-sided rows. Each pair has
* integer values between its sides, and the four rows together hold real
* points and no integer point. There is no equality row and no ranged row,
* so `relax --cols` proves it from the rows together once the boxes stop:
* the real shadow of x has integer points, its dark shadow has none, and
* none of the 16 splinters has one. With r3 at -11 the point (1, 2) fits.
NAME pugh
ROWS
 N obj
 G r1
 L r2
 G r3
 L r4
COLUMNS
    MARKER    'MARKER'   'INTORG'
    x obj 1 r1 11
    x r2 11 r3 7
    x r4 7
    y obj 1 r1 13
    y r2 13 r3 -9
    y r4 -9
    MARKER    'MARKER'   'INTEND'
RHS
    RHS r1 27 r2 45
    RHS r3 -10 r4 4
BOUNDS
 UP BND x 1
 UP BND y 1
ENDATA
