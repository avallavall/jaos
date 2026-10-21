* bench/measurements/02-255/misocp.c 1000 13, model 456, one brute-force
* solve. C7 is fixed at 0 and is a member of the rotated cone K2. The
* walk kept C7 in K2 and in an equality row, and it stopped after 208
* iterations with the gap open. A member held at 0 now leaves its cone.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 L  R1
 G  R2
 G  R3
COLUMNS
    C1        COST      0.448510812147882   R2        -1.8411275712320379
    C2        COST      -0.5157666365372933
    C3        COST      -0.6626229099372369   R2        2.2513365699193155
    C4        COST      -0.1185719164757506
    C5        COST      0.4865440607992333   R1        2.400567942661745
    C6        COST      0.8510614504913088   R3        0.32652904253910986
    C7        COST      -0.844917664059968   R3        1.5408000960618358
    C8        COST      -0.38322177346568176   R3        -0.6896826597203098
RHS
    RHS       COST      1.5634279030548806
    RHS       R1        1.4590892791748047
    RHS       R2        2.948794364929199
    RHS       R3        -4.581396102905273
RANGES
BOUNDS
 FX BND       C1        2
 LO BND       C2        -0.9328284641081561
 LO BND       C3        0.5087821830440828
 UP BND       C3        3.771372291852928
 MI BND       C4
 UP BND       C4        2.5305864945795684
 MI BND       C5
 UP BND       C5        0.9887756647972694
 LO BND       C6        -3.8645759251142255
 FX BND       C7        0
 FX BND       C8        1
QUADOBJ
    C1        C1        -1.9741952541738503
    C2        C2        -0.4663743080037104
    C3        C3        -0.592761300055324
    C4        C4        -0.29758217002707754
    C5        C5        -1.6603766773438098
    C6        C6        -1.6285196523467356
    C7        C7        -0.6703334685293643
    C8        C8        -1.9529010547483048
CSECTION   K1   0.0   QUAD
    C2
    C3
CSECTION   K2   0.0   RQUAD
    C4
    C5
    C7
ENDATA
