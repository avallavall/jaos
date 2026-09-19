\ A square on the lower side of a row. JAOS reads it and refuses to solve
\ it, because x + y + x^2 >= 2 is not a convex set.
Minimize
 obj: x + y
Subject To
 c1: x + y + [ x ^ 2 ] >= 2
End
