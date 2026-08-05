* A small model the slack basis is dual feasible for: every cost is
* nonnegative and every column has a lower bound to sit at.
*   min  2 X1 + 3 X2 + 4 X3
*   s.t. X1 + X2 + X3 >= 10   (DEMAND)
*        X1           <=  4   (CAP1)
*             X2      <=  3   (CAP2)
*        0 <= Xj <= 100
* Cheapest fill: X1=4, X2=3, X3=3  ->  8 + 9 + 12 = 29
NAME          SOLVE1
ROWS
 N  COST
 G  DEMAND
 L  CAP1
 L  CAP2
COLUMNS
    X1        COST         2.0   DEMAND       1.0
    X1        CAP1         1.0
    X2        COST         3.0   DEMAND       1.0
    X2        CAP2         1.0
    X3        COST         4.0   DEMAND       1.0
RHS
    RHS       DEMAND      10.0   CAP1         4.0
    RHS       CAP2         3.0
BOUNDS
 UP BND       X1         100.0
 UP BND       X2         100.0
 UP BND       X3         100.0
ENDATA
