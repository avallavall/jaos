\ The model of g_pair.qplib: the product x y in the objective and in a row.
Minimize
 obj: - 3 x - 3 y + [ 2 x ^ 2 + 2 x * y + 2 y ^ 2 ] / 2
Subject To
 c1: [ x ^ 2 + x * y + y ^ 2 ] <= 2
Bounds
 0 <= x <= 3
 0 <= y <= 3
End
