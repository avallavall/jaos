* min -x - y  s.t.  x - y <= 1,  x, y >= 0
*
* Unbounded along (1, 1): the row holds the difference and the objective
* falls forever. Two columns rather than one on purpose, so no presolve
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
