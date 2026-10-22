* written by JAOS 0.5.0
NAME          JAOS
ROWS
 N  COST
 L  R1
COLUMNS
    C1        COST      -1
    C2        COST      -1
    C3        COST      -5
RHS
RANGES
BOUNDS
 LO BND       C1        1
 SC BND       C1        4
 UP BND       C2        5
QCMATRIX   R1
    C2        C2        1
CSECTION   K1   0.0   RQUAD
    C1
    C2
    C3
ENDATA
