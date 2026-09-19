\ A mixed-integer QP whose root rounds and solves for the rest: the
\ relaxation (0.3, 1.3, 0.1) rounded breaks the row, and solving for z
\ with x and y rounded gives the optimum (0, 1, 0.7) at 0.57.
Minimize
 obj: - 0.4 x - 2.4 y + 1.48 + [ 2 x ^ 2 + 2 y ^ 2 + 2 z ^ 2 ] / 2
Subject To
 r1: x + y + z = 1.7
Bounds
 0 <= x <= 3
 0 <= y <= 3
 -5 <= z <= 5
General
 x y
End
