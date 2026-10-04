\ nl_int.lp with y's cost at 0.999: the costs share no step the tree can
\ round a bound to, so the root's bound of 3.5986 stays above the
\ rounding's 2.999 and a node limit of 1 stops after the root.
Maximize
 obj: x + 0.999 y
Subject To
 c1: x + y <= 3.6
 c2: x <= 2.2
 c3: y <= 1.4
General
 x y
End
