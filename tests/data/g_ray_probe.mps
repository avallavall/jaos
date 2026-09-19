* A generated model of bench/measurements/02-253 (seed 1, model 550), kept
* because the walk finds an improving direction the ray checker refuses:
* it pushes past a row side by 2.1e-06. The direction solve of 02-265
* finds one the checker takes, and the model ends unbounded.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 L  R1
 E  R2
 G  R3
 G  R4
 L  R5
COLUMNS
    C1        COST      0.24901573772826224   R1        -0.8449129880303934
    C1        R2        2.6555583103491545   R3        -1.8907139038518315
    C1        R4        -1.6102898580850287
    C2        COST      -0.9427930809206508   R3        2.2157274810283907
    C3        COST      0.9585135130889451
    C4        COST      0.257302367551997   R3        1.5834530546447443
    C5        COST      0.6773629057684984
    C6        COST      0.4119169482792986   R1        1.6572843202646634
    C6        R2        -0.43599388930388105   R4        -0.46428031147381477
    C6        R5        0.6564334720505149
    C7        COST      -0.16316770996387553   R5        0.04955838668395929
    C8        COST      0.3441518959571219   R2        1.9576531841112752
    C8        R3        -1.9569652286748545
RHS
    RHS       COST      -0.8321593946858288
    RHS       R1        4.278290873289626
    RHS       R2        -0.2500395232579783
    RHS       R3        2.045722307370591
    RHS       R4        -0.788749019970663
    RHS       R5        3.671242618297623
RANGES
BOUNDS
 MI BND       C1
 UP BND       C1        0.2889992526817844
 MI BND       C2
 UP BND       C2        3.2080710837464026
 MI BND       C3
 UP BND       C3        1.3703532079227179
 LO BND       C4        -0.4758480265775029
 UP BND       C4        1.850138647910801
 MI BND       C5
 UP BND       C5        3.3475615488874757
 MI BND       C6
 UP BND       C6        3.0332948038291594
 MI BND       C7
 UP BND       C7        1.3048623714204188
 MI BND       C8
 UP BND       C8        3.1293957396726135
QCMATRIX   R5
    C6        C6        0.34613618648563566
CSECTION   K1   0.0   QUAD
    C4
    C5
ENDATA
