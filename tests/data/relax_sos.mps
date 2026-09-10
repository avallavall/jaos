NAME relaxsos
ROWS
 N obj
 G r1
 G r2
COLUMNS
    x1 obj 1 r1 1
    x2 obj 1 r2 1
RHS
    RHS r1 2
    RHS r2 2
BOUNDS
 UP BND x1 10
 UP BND x2 10
SOS
 S1 SOS s1
    x1 1
    x2 2
ENDATA
