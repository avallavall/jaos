* bench/measurements/02-425/warmcone.c 300 3 8 misocp, the start model of
* the seed's endless solve. A node with C18 at 0, C19 in [3, 4] and C20 in
* [0, 1] stops rough with C19 at 2.95 and C20 at 1.28. The branch on C20
* kept [0, 1] for the down child and left the up child empty, so the same
* node came back 757,645 levels deep. The point is now held in the box.
NAME          JAOS
OBJSENSE      MIN
ROWS
 N  COST
 E  R1
 L  R2
 L  R3
 L  R4
 G  R5
 G  R6
 L  R7
 L  R8
 E  R9
 E  R10
COLUMNS
    MARKER    'MARKER'    'INTORG'
    C1    COST    5.0
    C1    R5    3.0
    C1    R7    -2.0
    C1    R9    1.0
    C1    R10    1.0
    C2    COST    -7.0
    C2    R1    3.0
    C2    R3    2.0
    C2    R4    1.0
    C2    R6    3.0
    C2    R7    -2.0
    C2    R8    -2.0
    C2    R10    -1.0
    C3    COST    4.0
    C3    R3    1.0
    C3    R4    -1.0
    C3    R5    -1.0
    C3    R7    -3.0
    C3    R9    1.0
    C3    R10    3.0
    C4    COST    8.0
    C4    R2    -1.0
    C4    R3    -1.0
    C4    R5    -3.0
    C4    R8    -3.0
    C4    R9    -1.0
    MARKER    'MARKER'    'INTEND'
    C5    COST    3.0
    C5    R2    3.0
    C5    R3    -3.0
    C5    R6    -1.0
    C5    R7    1.0
    C5    R8    -1.0
    C6    COST    -9.0
    C6    R3    -2.0
    C6    R4    3.0
    C6    R7    -2.0
    C6    R10    1.0
    MARKER    'MARKER'    'INTORG'
    C7    COST    1.0
    C7    R1    1.0
    C7    R3    -1.0
    C7    R4    1.0
    C8    COST    -2.0
    C8    R1    3.0
    C8    R2    -3.0
    C8    R3    1.0
    C8    R4    -3.0
    C8    R5    3.0
    C8    R6    -2.0
    C8    R7    -1.0
    C8    R8    -1.0
    C8    R9    2.0
    C8    R10    -1.0
    MARKER    'MARKER'    'INTEND'
    C9    COST    -1.0
    C9    R3    -1.0
    C9    R4    -2.0
    C9    R5    -3.0
    C9    R7    2.0
    C9    R9    1.0
    MARKER    'MARKER'    'INTORG'
    C10    COST    -5.0
    C10    R1    1.0
    C10    R2    -3.0
    C10    R4    1.0
    C10    R5    -1.0
    C11    COST    4.0
    C11    R2    1.0
    C11    R3    -1.0
    C11    R4    3.0
    C11    R6    -3.0
    C11    R7    3.0
    C11    R8    2.0
    C11    R10    3.0
    MARKER    'MARKER'    'INTEND'
    C12    COST    -1.0
    C12    R7    1.0
    C13    COST    3.0
    C13    R4    1.0
    C13    R5    -3.0
    C13    R9    -3.0
    C13    R10    1.0
    C14    COST    8.0
    C14    R1    2.0
    C14    R4    1.0
    C14    R5    -3.0
    C14    R6    1.0
    C14    R8    -3.0
    MARKER    'MARKER'    'INTORG'
    C15    COST    4.0
    C15    R2    -2.0
    C15    R3    -1.0
    C15    R10    1.0
    MARKER    'MARKER'    'INTEND'
    C16    COST    5.0
    C16    R5    -2.0
    C16    R7    -1.0
    C16    R9    -2.0
    C16    R10    -1.0
    C17    COST    -9.0
    C17    R2    -1.0
    C17    R3    1.0
    C17    R8    -1.0
    C17    R9    1.0
    C17    R10    2.0
    MARKER    'MARKER'    'INTORG'
    C18    COST    -3.0
    C18    R2    -1.0
    C18    R6    2.0
    C18    R7    3.0
    C18    R8    1.0
    C18    R10    -2.0
    C19    COST    -1.0
    C19    R1    -2.0
    C19    R3    -3.0
    C19    R4    2.0
    C19    R5    1.0
    C19    R8    3.0
    C19    R10    -1.0
    C20    COST    -4.0
    C20    R4    3.0
    C20    R6    3.0
    C20    R9    1.0
    C20    R10    3.0
    MARKER    'MARKER'    'INTEND'
    C21    COST    0.0
    C21    R1    -1.0
    C21    R2    -1.0
    C21    R3    -3.0
    C21    R4    3.0
    C21    R5    1.0
    C21    R6    1.0
    C21    R7    -2.0
    MARKER    'MARKER'    'INTORG'
    C22    COST    -9.0
    C22    R1    1.0
    C22    R2    -2.0
    C22    R3    2.0
    C22    R4    -3.0
    C22    R6    -1.0
    C22    R8    -1.0
    C22    R10    -1.0
    C23    COST    4.0
    C23    R2    -3.0
    C23    R3    3.0
    C23    R5    1.0
    C23    R7    2.0
    C23    R9    -3.0
    C23    R10    -2.0
    MARKER    'MARKER'    'INTEND'
    C24    COST    6.0
    C24    R6    3.0
    C24    R10    3.0
RHS
    RHS    COST    2.0
    RHS    R1    68.0
    RHS    R2    -27.0
    RHS    R3    11.0
    RHS    R4    45.0
    RHS    R5    4.0
    RHS    R6    35.0
    RHS    R7    -37.0
    RHS    R8    -7.0
    RHS    R9    -31.0
    RHS    R10    42.0
RANGES
BOUNDS
 LO BND    C1    0.0
 PL BND    C1
 LO BND    C2    0.0
 PL BND    C2
 LO BND    C3    0.0
 UP BND    C3    8.0
 LO BND    C4    0.0
 UP BND    C4    1.0
 UP BND    C5    1.0
 UP BND    C6    2.0
 LO BND    C7    0.0
 UP BND    C7    4.0
 LO BND    C8    0.0
 UP BND    C8    8.0
 UP BND    C9    1.0
 LO BND    C10    0.0
 UP BND    C10    10.0
 LO BND    C11    0.0
 PL BND    C11
 UP BND    C12    3.0
 UP BND    C13    8.0
 UP BND    C14    4.0
 LO BND    C15    0.0
 PL BND    C15
 UP BND    C17    7.0
 LO BND    C18    0.0
 UP BND    C18    3.0
 LO BND    C19    0.0
 UP BND    C19    4.0
 LO BND    C20    0.0
 PL BND    C20
 LO BND    C22    0.0
 UP BND    C22    2.0
 LO BND    C23    0.0
 UP BND    C23    3.0
 UP BND    C24    2.0
CSECTION   K1   0.0   RQUAD
    C10
    C18
    C23
    C14
ENDATA
