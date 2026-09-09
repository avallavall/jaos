* The QP of g_quad.lp in MPS with a QUADOBJ section:
*   min x + y + (2 x^2 + 2 y^2) / 2   s.t.  x + y >= 2,  x, y <= 10
* Optimum x = y = 1, objective 4.
NAME          GQUAD
ROWS
 N  obj
 G  c1
COLUMNS
    x         obj          1.0   c1           1.0
    y         obj          1.0   c1           1.0
RHS
    RHS       c1           2.0
BOUNDS
 UP BND       x           10.0
 UP BND       y           10.0
QUADOBJ
    x         x            2.0
    y         y            2.0
ENDATA
