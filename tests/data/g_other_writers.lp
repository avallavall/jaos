\ what other tools' LP files carry, from the HiGHS, SCIP and
\ MathOptInterface test instances read on 2026-09-16
Maximize
 obj: + x#1 + 2 x#2 - 0.1 x3 - 3 x4
Subject To
 + x#1 + x#2 <= 5
 - x#1 + 3 x#2 >= 0
 -x3 + x4 >= -1
 con4: = 0
 end: 3 <= x3 + x4 <= 9
Bounds
 x3 >= 1.1
 x3 <= 10
 x4 <= 8
 V8 <= -3
Semi-continuous
 x3
Binary
 V8
General
 x3
End
