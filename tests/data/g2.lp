\ Golden LP 2: Maximize, exponent numbers, repeated variable summing,
\ line-wrapped constraint, archaic =< form, unlabeled constraints,
\ glued coefficient, upper bound only.
Maximize
 2.5e1 x1 + x1
   + 3x2
Subject To
 x1 +
   x2 =< 4
 c: x1 - x2 >= -1e0
Bounds
 x2 <= 1.5
End
