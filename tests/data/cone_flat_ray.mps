* bench/measurements/02-255/misocp.c 1000 4, model 625, one brute-force
* solve with the integer columns fixed. It is unbounded: C2 alone may grow.
* In the solve over directions, C4's direction is fixed at 0 and C1's
* cannot leave 0, so both rotated cones sit on a degenerate face and that
* solve ended numerical_error. With those cones turned into bounds it is
* an LP, and its direction passes the ray checker.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
COLUMNS
    C1        COST      0.4740217288704236
    C2        COST      0.12128970918734328
    C3        COST      -0.675495065791013
    C4        COST      -0.5207088023421049
    C5        COST      0.5461264544643174
    C6        COST      -0.8511952951769788
    C7        COST      0.07708017158152858
    C8        COST      -0.9070079646974003
RHS
    RHS       COST      2.1159959399266572
RANGES
BOUNDS
 MI BND       C1
 UP BND       C1        2.021288531442253
 LO BND       C2        -1.8398960571746361
 LO BND       C3        -0.007952631674555066
 LO BND       C4        0.9047088275057726
 UP BND       C4        3.8421365934387337
 MI BND       C5
 UP BND       C5        1.364603096785077
 FX BND       C6        -2
 LO BND       C7        -1.0580838049464614
 MI BND       C8
 UP BND       C8        2.1221162409613608
CSECTION   K1   0.0   RQUAD
    C1
    C3
    C5
    C6
CSECTION   K2   0.0   RQUAD
    C2
    C4
    C7
    C8
ENDATA
