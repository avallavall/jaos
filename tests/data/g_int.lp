\ Integer test model (D288): x general, y binary.
\   min x + y  s.t.  x + y >= 1.5, x >= 0: y binary, x integer -> x = 1,
\   y = 1 (x + y >= 1.5 needs 2 with integers) -> objective 2.
Minimize
 obj: x + y
Subject To
 c: x + y >= 1.5
General
 x
Binary
 y
End
