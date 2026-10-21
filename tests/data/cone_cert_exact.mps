* bench/measurements/02-255/misocp.c 1000 2, model 166, one brute-force
* solve with the integer columns fixed. It is infeasible. With no quadratic
* row the checker's certificate test is exact, and the walk's certificate
* fails it (the columns reach infinity). A solve over the multipliers finds
* one the checker takes.
NAME          JAOS
ROWS
 N  COST
 G  R1
 G  R2
 L  R3
COLUMNS
    C1        COST      0.03036232188152077   R2        0.015941995830715427
    C2        COST      0.07105325503596527   R2        -0.5546258116234677
    C2        R3        2.325746860369213
    C3        COST      0.7593756896265926   R1        -1.6472065520926076
    C3        R3        1.756320900628043
    C4        COST      -0.8480680356321051   R1        -1.048502412852287
    C5        COST      0.13925222431832984   R1        -2.3108002210583978
    C5        R2        -1.69810408284532
    C6        COST      0.9611065097230092   R3        -2.1802109692905716
RHS
    RHS       COST      1.7170355404814996
    RHS       R1        1.3316097259521484
    RHS       R2        2.288893699645996
    RHS       R3        6.68721866607666
RANGES
    RNG       R1        3.3425235748291016
BOUNDS
 LO BND       C1        0.20913669205524066
 MI BND       C2
 UP BND       C2        3.554088613679789
 FX BND       C3        2
 LO BND       C4        -1.217790423492252
 FX BND       C5        0
 LO BND       C6        -1.7060065674480998
QUADOBJ
    C1        C1        1.7068281458963204
    C2        C2        1.8411928382732479
    C3        C3        1.0635646965701813
    C4        C4        1.3724305554326797
    C5        C5        1.8055629069017318
    C6        C6        0.805716270443884
CSECTION   K1   0.0   QUAD
    C2
    C4
    C6
CSECTION   K2   0.0   QUAD
    C1
    C5
ENDATA
