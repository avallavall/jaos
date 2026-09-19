* minimise -x - y over x^2 + y^2 <= 2: the optimum is (1, 1) at -2, and the
* row's dual is -1/2. QCMATRIX holds the whole matrix of x'Qx, unhalved.
NAME          GQCP
ROWS
 N  obj
 L  ball
COLUMNS
    x         obj       -1
    y         obj       -1
RHS
    rhs       ball      2
BOUNDS
 FR bnd       x
 FR bnd       y
QCMATRIX      ball
    x         x         1
    y         y         1
ENDATA
