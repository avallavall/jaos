* Integer test model (D288): X integer through a MARKER pair, Y continuous,
* Z binary through a BV bound.
*   min X + Y + Z  s.t.  X + Y >= 2.5,  Z >= 0.5,  X, Y >= 0
* Z must be 1. X + Y = 2.5 with X integer costs 2.5 whichever way the
* fraction falls on Y, so the optimum is 3.5; the relaxation is the same
* value, which is why the LP round trip below checks the marks and not the
* objective alone.
NAME          T4INT
ROWS
 N  COST
 G  R1
 G  R2
COLUMNS
    MARKER    'MARKER'   'INTORG'
    X         COST         1.0   R1           1.0
    MARKER    'MARKER'   'INTEND'
    Y         COST         1.0   R1           1.0
    Z         COST         1.0   R2           1.0
RHS
    RHS       R1           2.5   R2           0.5
BOUNDS
 BV BND       Z
ENDATA
