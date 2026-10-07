Minimize
 obj: s0 + s1
Subject To
 one0: x0_0 + x0_1 = 1
 one1: x1_0 + x1_1 = 1
 one2: x2_0 + x2_1 = 1
 one3: x3_0 + x3_1 = 1
 cap0: 3 x0_0 + 5 x1_0 + 8 x2_0 + 4 x3_0 - s0 <= 11.5
 cap1: 3 x0_1 + 5 x1_1 + 8 x2_1 + 4 x3_1 - s1 <= 8.5
Binary
 x0_0 x0_1 x1_0 x1_1 x2_0 x2_1 x3_0 x3_1
End
