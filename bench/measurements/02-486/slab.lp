\ A continuous column c between two slabs: x + y + c in [0.5, 0.75] and
\ c - y in [0, 0.125], all four rows one-sided. Projecting c out leaves
\ x + 2y in [0.375, 0.75], which no pair of integers reaches.
Minimize
 obj: x + y
Subject To
 r1: x + y + c >= 0.5
 r2: x + y + c <= 0.75
 r3: - y + c >= 0
 r4: - 2 y + 2 c <= 0.25
Bounds
 x = 0
 y = 0
 c = 0
General
 x
 y
End
