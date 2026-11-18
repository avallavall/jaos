\ Two one-sided rows over integers: 2x - 2y >= 1 and x - y <= 0.75 pinch
\ x - y between 0.5 and 0.75. Neither is an equality or a ranged row.
Minimize
 obj: x + y
Subject To
 r1: 2 x - 2 y >= 1
 r2: x - y <= 0.75
Bounds
 x = 0
 y = 0
General
 x
 y
End
