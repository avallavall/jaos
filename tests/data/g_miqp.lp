\ A mixed-integer QP: min x^2 + y^2 - 5.2 x - 2.6 y with x + y <= 3,
\ x and y integer in [0, 5]. The relaxation sits at (2.6, 1.3) with
\ x + y = 3.9 > 3; the integer optimum is (2, 1) with objective -8.
Minimize
 obj: - 5.2 x - 2.6 y + [ 2 x ^ 2 + 2 y ^ 2 ] / 2
Subject To
 c1: x + y <= 3
Bounds
 x <= 5
 y <= 5
General
 x y
End
