* A ball and a half-space past it: x0^2 + x1^2 <= 1 as a quadratic row,
* and x0 + x1 >= 3. The ball reaches sqrt(2) along that direction, so the
* model has no point, and the proof rests on the ball's curvature: the
* certificate's columns reach a^2 / (-2h) and no further, where h is the
* multiplier times the row's own quadratic part.
NAME          GBALL
ROWS
 N  obj
 L  ball
 G  half
COLUMNS
    x0        obj       0   half      1
    x1        obj       0   half      1
RHS
    rhs       ball      1   half      3
BOUNDS
 FR bnd       x0
 FR bnd       x1
QCMATRIX   ball
    x0        x0        2
    x1        x1        2
ENDATA
