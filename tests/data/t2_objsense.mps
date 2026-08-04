* Golden instance 2: free-style layout, OBJSENSE MAX on its own data line,
* an E row with a negative range, the negative-UP-bound wart, MI then UP.
NAME t2
OBJSENSE
    MAX
ROWS
 N obj
 E balance
COLUMNS
 x obj 3.0 balance 1.0
 y obj 1.0 balance 2.0
RHS
 r balance 4.0
RANGES
 rng balance -1.0
BOUNDS
 UP b x -2.0
 MI b y
 UP b y 5.0
ENDATA
