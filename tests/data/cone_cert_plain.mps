* bench/measurements/02-255/misocp.c 1000 29, model 903, one brute-force
* solve. R2 holds only C1, fixed at -3, outside R2's range: one row and
* one bound prove it infeasible. C1 also sits in the quadratic row R5, so
* the first certificate program held C1's coefficient at 0; the second,
* with the quadratic rows left out, finds the one-row certificate.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 L  R1
 G  R2
 L  R3
 L  R4
 L  R5
COLUMNS
    C1        COST      0.7649663998086733   R1        1.1721297502215018
    C1        R2        -2.675722999139243   R5        1.3012463500245053
    C2        COST      0.3810271621294099
    C3        COST      0.13704246572883094   R1        0.492579222691238
    C3        R3        2.1785308620031385   R5        -0.932260875903069
    C4        COST      -0.40465364054170117   R3        -0.01840451701764989
    C5        COST      -0.8751997513579888   R1        -0.19051612284175068
    C5        R3        1.6381106973584174   R5        1.0880106562243843
    C6        COST      0.7691017565502485   R1        -1.64246868811008
    C6        R4        1.742471072340889   R5        -0.8390832657609248
    C7        COST      -0.09927410098417622   R3        -0.943237915685883
RHS
    RHS       COST      4.562895466556794
    RHS       R1        -0.3558769226074219
    RHS       R2        2.320643424987793
    RHS       R3        -2.973395347595215
    RHS       R4        1.1298892284120172
    RHS       R5        2.3843832564152896
RANGES
    RNG       R2        1.1401948928833008
BOUNDS
 FX BND       C1        -3
 LO BND       C2        -1.6884372149077875
 LO BND       C3        -3.293788157852431
 MI BND       C4
 UP BND       C4        -0.509544330348231
 FR BND       C5
 LO BND       C6        -2.3963700925098905
 LO BND       C7        -1.070104414488291
QCMATRIX   R4
    C6        C6        0.18653643555736688
QCMATRIX   R5
    C1        C1        0.30834097463658866
    C3        C3        0.6868114207717421
    C5        C5        0.6387621690726591
CSECTION   K1   0.0   RQUAD
    C2
    C6
    C7
ENDATA
