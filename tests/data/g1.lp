\ Golden LP 1: labels, all three relations, objective constant,
\ bounds forms l <= x <= u, free, and defaults.
Minimize
 obj: 3x + 2y - z + 5
Subject To
 c1: x + y <= 10
 c2: 2x - y >= -3
 c3: x + y + z = 7
Bounds
 -1 <= y <= 8
 z free
End
