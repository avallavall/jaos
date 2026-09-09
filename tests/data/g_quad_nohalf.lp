\ The same QP as g_quad.lp with the block not divided by 2, and the
\ squares written as products: x * x is x ^ 2.
Minimize
 obj: x + y + [ x * x + y ^ 2 ]
Subject To
 c1: x + y >= 2
Bounds
 x <= 10
 y <= 10
End
