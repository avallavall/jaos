\ The pass-through path of tests/test_mip.c: three arcs from a source to a
\ sink that asks for 5, each arc open only with its binary. The two middle
\ nodes pass all their flow on, so the MIP presolve substitutes x2 and x3
\ out. The optimum opens all three arcs, worth 3 + 0.1 * 15 = 4.5.
Minimize
 obj: 0.1 x1 + 0.1 x2 + 0.1 x3 + y1 + y2 + y3
Subject To
 a: x1 - x2 = 0
 b: x2 - x3 = 0
 t: x3 = 5
 v1: x1 - 10 y1 <= 0
 v2: x2 - 10 y2 <= 0
 v3: x3 - 10 y3 <= 0
Bounds
 x1 <= 10
 x2 <= 10
 x3 <= 10
Binary
 y1 y2 y3
End
