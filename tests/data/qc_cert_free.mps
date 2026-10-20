* bench/measurements/02-253/conic.c 1000 2, model 924: the ball R4 and
* the half-space R5 past it. The walk's certificate gives a free column with
* no curvature (C5 or C7) a coefficient that is not zero, so the checker
* says the columns reach infinity. The searches that move one multiplier at
* a time do not bring it to zero. A solve over the multipliers finds a
* certificate the checker takes.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 E  R1
 G  R2
 G  R3
 L  R4
 G  R5
COLUMNS
    C1        COST      0.8769302411274407   R5        1
    C2        COST      -0.061853110194498795   R1        1.6289040072285017
    C2        R5        1
    C3        COST      0.03758352297012468   R2        -2.3870295252732836
    C3        R5        1
    C4        COST      -0.4925714404796695   R1        -0.21426180790002247
    C4        R2        -0.9486580430739728   R5        1
    C5        COST      -0.7572389317677748
    C6        COST      0.84721823852108   R3        -2.408519316311498
    C7        COST      0.38713129024180826   R2        2.4466013469456955
    C8        COST      0.31655204798857417   R1        -2.539223344338381
    C8        R2        -2.7733096850417285
    C9        COST      0.7016357249559575   R3        0.6785488691592159
RHS
    RHS       COST      -4.8242275194327675
    RHS       R1        1.53457119825669
    RHS       R2        -4.647502018566351
    RHS       R3        2.8462874860433804
    RHS       R4        1
    RHS       R5        3
RANGES
BOUNDS
 FR BND       C1
 LO BND       C2        -1.161789824594262
 FR BND       C3
 LO BND       C4        -1.9202020729818887
 FR BND       C5
 LO BND       C6        -3.6023425960218822
 UP BND       C6        0.12933577821795317
 FR BND       C7
 LO BND       C8        -0.8488861662248193
 MI BND       C9
 UP BND       C9        2.834355849395885
QCMATRIX   R4
    C1        C1        1
    C2        C2        1
    C3        C3        1
    C4        C4        1
CSECTION   K1   0.0   RQUAD
    C1
    C5
    C7
CSECTION   K2   0.0   QUAD
    C3
    C4
    C8
ENDATA
