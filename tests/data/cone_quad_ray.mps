* bench/measurements/02-255/misocp.c 1000 11, model 381, one brute-force
* solve with the integer columns fixed. It is unbounded: C1 alone may grow.
* In the solve over directions, R2's diagonal term holds C2's direction at
* 0, but through a row, so the rotated cone K1 stayed on a degenerate face
* and that solve failed. Read as a bound, it lets the cone leave.
NAME          JAOS
ROWS
 N  COST
 G  R1
 L  R2
COLUMNS
    C1        COST      -0.9103211106990603
    C2        COST      -0.9637322168343472   R2        -1.4424781518485337
    C3        COST      0.9160254970894399   R2        1.1677458546008133
    C4        COST      -0.19788723569661903   R1        1.9206749034945307
    C4        R2        0.7610254986982588
    C5        COST      -0.8577981977471663   R2        -1.4041606411844723
    C6        COST      0.019302085676749625   R1        1.8620029570772703
RHS
    RHS       COST      0.7052650209195637
    RHS       R1        -1.0700979232788086
    RHS       R2        3.2450246126479003
RANGES
BOUNDS
 FR BND       C1
 FR BND       C2
 FX BND       C3        1
 LO BND       C4        -1.1013839944622548
 UP BND       C4        1.9037772327835278
 LO BND       C5        -0.6636785082880667
 FR BND       C6
QCMATRIX   R2
    C2        C2        0.28152488804060305
    C3        C3        0.5863538277037389
    C4        C4        0.685573756585832
CSECTION   K1   0.0   RQUAD
    C1
    C2
    C3
    C5
ENDATA
