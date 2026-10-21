* bench/measurements/02-255/misocp.c 1000 2, model 698, one brute-force
* solve with the integer columns fixed. C1 and C6 are fixed at 2 and 3 and
* carry quadratic objective terms; C3 has a quadratic row. With those terms
* inside the walk, tau and kappa both went to zero and the walk stopped
* after 206 iterations. With them moved to the linear terms it takes 6.
NAME          JAOS
OBJSENSE      MAX
ROWS
 N  COST
 L  R1
COLUMNS
    C1        COST      0.8269573193594622
    C2        COST      -0.42125115775467004
    C3        COST      -0.04430658757034811   R1        0.41719035564712303
    C4        COST      0.2408926756990566
    C5        COST      -0.09823581454355024
    C6        COST      -0.3333631736368996
    C7        COST      0.8933323938731519
    C8        COST      0.2851845392797583
RHS
    RHS       COST      0.1868518929763816
    RHS       R1        0.6921930337428233
RANGES
BOUNDS
 FX BND       C1        2
 FX BND       C2        0
 LO BND       C3        -0.501317208763308
 MI BND       C4
 UP BND       C4        0.030926745683065082
 LO BND       C5        -2.3836549614015903
 FX BND       C6        3
 FR BND       C7
 LO BND       C8        0.01461842499624355
 UP BND       C8        4.290586243200206
QUADOBJ
    C1        C1        -1.6644986922759635
    C2        C2        -1.5577502598388813
    C3        C3        -1.8298563778481012
    C4        C4        -1.770863039181258
    C5        C5        -0.5950657703437103
    C6        C6        -1.6870916867982362
    C7        C7        -1.2434701103020198
    C8        C8        -1.945789312018242
QCMATRIX   R1
    C3        C3        0.6791715362697657
ENDATA
