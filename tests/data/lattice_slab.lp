\ The thin knapsack slab of tests/test_mip.c (02-476): six general integers
\ in 0..20 whose row allows one activity, 354323. The tree over the columns
\ takes 26261 nodes; after 20000 the lattice branching starts it again on a
\ reduced basis and closes it in a few. The optimum is 426.
Maximize
 obj: 3 x1 + 2 x2 + 5 x3 + 3 x4 + 6 x5 + 6 x6
Subject To
 c1: 354322.5 <= 4046 x1 + 4520 x2 + 3642 x3 + 2564 x4 + 2511 x5 + 4740 x6 <= 354323
Bounds
 0 <= x1 <= 20
 0 <= x2 <= 20
 0 <= x3 <= 20
 0 <= x4 <= 20
 0 <= x5 <= 20
 0 <= x6 <= 20
General
 x1 x2 x3 x4 x5 x6
End
