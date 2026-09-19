* A generated model of bench/measurements/02-253 (seed 1, model 448), kept
* because the walk stops after 25 iterations with nothing to answer
* from: its objective runs away while tau falls and kappa holds. The
* feasibility solve and the direction solve of 02-265 end it unbounded.
NAME          JAOS
ROWS
 N  COST
 L  R1
COLUMNS
    C1        COST      -0.03509757944983449
    C2        COST      0.22919179016153057
    C3        COST      0.6353125056539324
    C4        COST      0.8204801695543646
    C5        COST      -0.9165963254263465
    C6        COST      0.8837777623640444   R1        0.6890807166477892
    C7        COST      -0.2958821925889452   R1        1.5662061236778388
    C8        COST      0.8327940167803798   R1        0.5247609520187382
RHS
    RHS       COST      -0.15794151148603497
    RHS       R1        7.560497237400734
RANGES
BOUNDS
 FR BND       C1
 MI BND       C2
 UP BND       C2        2.6740611171165285
 LO BND       C3        -3.8374955951106937
 FR BND       C4
 MI BND       C5
 UP BND       C5        -0.1661590550891201
 FR BND       C6
 LO BND       C7        -3.6906550418127537
 LO BND       C8        -0.8308407279601102
 UP BND       C8        3.084692440753678
QCMATRIX   R1
    C6        C6        0.835246156478682
    C7        C6        -0.502216674588142
    C6        C7        -0.502216674588142
    C8        C6        -0.1417360091159669
    C6        C8        -0.1417360091159669
    C7        C7        0.6948548210452667
    C8        C7        -0.24758156080231794
    C7        C8        -0.24758156080231794
    C8        C8        0.9008661806399483
CSECTION   K1   0.0   RQUAD
    C1
    C2
    C3
CSECTION   K2   0.0   QUAD
    C4
    C5
    C8
ENDATA
