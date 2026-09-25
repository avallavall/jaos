\ min -x1 s.t. x1 - x2 <= 1 and SOS1 {x1, x2}: when x2 > 0, x1 = 0,
\ so x1 <= 1 and the optimum is -1, though the relaxation is unbounded.
Minimize
 obj: - x1
Subject To
 c: x1 - x2 <= 1
Bounds
 x1 >= 0
 x2 >= 0
SOS
 s1: S1:: x1:1 x2:2
End
