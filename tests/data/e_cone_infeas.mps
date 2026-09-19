* (t, x) in the second-order cone with t <= 1 and x = 2 has no point.
NAME          ECONE
ROWS
 N  obj
COLUMNS
    t         obj       0
    x         obj       0
BOUNDS
 MI bnd       t
 UP bnd       t         1
 FX bnd       x         2
CSECTION      K1        0.0       QUAD
    t
    x
ENDATA
