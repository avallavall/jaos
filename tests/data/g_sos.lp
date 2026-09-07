\ SOS2 over three unit columns: at most two adjacent nonzero
\   max x1 + x2 + x3  s.t. x1 + x2 + x3 <= 10, 0 <= xi <= 1 -> 2
\ and an unnamed SOS1 over x3, x1 with weights 0.5 and 4
Maximize
 obj: x1 + x2 + x3
Subject To
 c: x1 + x2 + x3 <= 10
Bounds
 x1 <= 1
 x2 <= 1
 x3 <= 1
SOS
 s1: S2:: x1:1 x2:2 x3:3
 S1:: x3:0.5 x1:4
End
