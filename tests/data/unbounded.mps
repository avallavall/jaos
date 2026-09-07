* min -x - y  s.t.  x - y <= 1,  x, y >= 0
*
* Unbounded: the row holds only the difference of the two columns, so
* raising either without lowering the other runs forever and the
* objective falls with it. (0, 1) and (1, 1) are both rays, which is
* why the checker judges what the solve produced rather than a ray
* named here. Two columns rather than one on purpose, so no presolve
* family settles it by itself and the simplex is what answers.
NAME          UNB1
ROWS
 N  COST
 L  LIM1
COLUMNS
    X         COST      -1.0       LIM1       1.0
    Y         COST      -1.0       LIM1      -1.0
RHS
    RHS       LIM1       1.0
BOUNDS
ENDATA
