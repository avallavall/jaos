* Both columns are integer and pinned at zero, and the row asks
* 5 x - 7 y = 1. The nearest integer point is (3, 2), a total move of 5.
* The elastic copy's LP puts the move at 1/7, so the first box round the
* freed columns is one wide and holds no integer point; it doubles to 2,
* then 4, where (3, 2) fits but costs more than the box is wide, then 8.
NAME rounds
ROWS
 N obj
 E r1
COLUMNS
    MARKER    'MARKER'   'INTORG'
    x obj 0 r1 5
    y obj 0 r1 -7
    MARKER    'MARKER'   'INTEND'
RHS
    RHS r1 1
BOUNDS
 FX BND x 0
 FX BND y 0
ENDATA
