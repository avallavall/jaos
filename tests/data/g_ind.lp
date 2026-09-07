\ indicator: c1 holds only while z = 1
\   max x - 3 z  s.t.  z = 1 -> x <= 2, 0 <= x <= 10, z binary
\   -> z = 0, x = 10, objective 10
Maximize
 obj: x - 3 z
Subject To
 c1: z = 1 -> x <= 2
Bounds
 x <= 10
Binary
 z
End
