* bench/measurements/02-393/miqp.c 1000 1 with its equality sides rounded
* to 2^-20, model 500. R1 holds only the integers C1 and C3, and C1 = C3 = 1
* misses it by 4.1e-8. Propagation bounded C3 by 0.99999998, rounded that
* down to 0 with a window of 1e-9 and called the node infeasible, though
* the prune test keeps any row within 1e-7 of its terms. An integer bound
* now keeps that window too.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 E  R1
 L  R2
 G  R3
 L  R4
 E  R5
COLUMNS
    MARKER    'MARKER'   'INTORG'
    C1        COST      0.8816438609146757   R1        -0.6717301407349052
    C1        R2        0.8067470526566911   R3        -0.0286884357356465
    C1        R4        -1.7322346978053347   R5        2.6922388613367048
    MARKER    'MARKER'   'INTEND'
    C2        COST      -0.8914144815597811   R2        1.4017064333054927
    C2        R3        2.49181575048563   R4        -1.878236680909271
    C2        R5        1.6388763751341138
    MARKER    'MARKER'   'INTORG'
    C3        COST      -0.4480989057289402   R1        -2.1898888956426346
    C3        R2        -2.086388453473403   R3        0.8877616030123927
    C3        R5        -1.681432521924664
    MARKER    'MARKER'   'INTEND'
RHS
    RHS       COST      0.2770058076543114
    RHS       R1        -2.861618995666504
    RHS       R2        -0.8050212860107422
    RHS       R3        -0.9884252548217773
    RHS       R4        -0.13136768341064453
    RHS       R5        0.5106878280639648
RANGES
    RNG       R3        1.8425617218017578
BOUNDS
 LO BND       C1        -1
 UP BND       C1        3
 MI BND       C2
 UP BND       C2        1.084349521371161
 LO BND       C3        -1
 UP BND       C3        3
QUADOBJ
    C1        C1        -3.531981895121253
    C2        C1        0.47530372392076947
    C3        C1        0.10550714548362661
    C2        C2        -2.2803030726114937
    C3        C2        0.5234600919081072
    C3        C3        -1.6360253742314133
ENDATA
