\ Constants inside a constraint expression, in every shape the reader has
\ to fold: after the terms, before them, on both ends of a range, and more
\ than one in a row. The objective's own constants still go to the offset.
Minimize
 obj: x + y + 7
Subject To
 c1: x + 5 <= 10
 c2: -3 + 2 y >= 7
 c3: 3 <= x + y + 1 <= 8
 c4: x + 2 + 3 = 10
Bounds
 x >= 0
 y >= 0
End
