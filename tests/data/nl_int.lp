\ The neighbour model of tests/test_mip.c (D290, D291): the relaxation sits
\ at (2.2, 1.4), which rounds to the optimum (2, 1), worth 3. The 2.2 and
\ 1.4 are rows, since a bound on an integer column is rounded inward
\ before the root (D292). With the cuts off the root stays fractional, so
\ a node limit of 1 stops after it.
Maximize
 obj: x + y
Subject To
 c1: x + y <= 3.6
 c2: x <= 2.2
 c3: y <= 1.4
General
 x y
End
