\ The neighbour model of tests/test_mip.c (D290, D291): the relaxation sits
\ at (2.2, 1.4), which rounds to the optimum (2, 1), worth 3. With the cuts
\ off the root stays fractional, so a node limit of 1 stops after it.
Maximize
 obj: x + y
Subject To
 c1: x + y <= 3.6
Bounds
 x <= 2.2
 y <= 1.4
General
 x y
End
