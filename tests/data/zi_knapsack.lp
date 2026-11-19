\ The four knapsack rows of tests/test_mip.c's ZI test (knapsack_rows(4, 20, 7),
\ 02-499): twenty general integers in small boxes. ZI rounding finds the first
\ incumbent, -1518, at node 1, and four in all; the optimum is -1582.
Minimize
 obj: - 79 x0 - 70 x1 - 34 x2 - 64 x3 - 29 x4 - 82 x5 - 54 x6 - 75 x7 - 52 x8 - 67 x9 - 65 x10 - 98 x11 - 71 x12 - 96 x13 - 58 x14 - 77 x15 - 78 x16 - 91 x17 - 89 x18 - 91 x19
Subject To
 r0: 29 x0 + 20 x1 + 6 x2 + 6 x3 + 8 x4 + 12 x5 + 3 x6 + 22 x7 + 8 x8 + 1 x9 + 24 x10 + 28 x11 + 10 x12 + 1 x13 + 6 x14 + 15 x15 + 9 x16 + 23 x17 + 12 x18 + 20 x19 <= 257
 r1: 19 x0 + 29 x1 + 8 x2 + 29 x3 + 1 x4 + 17 x5 + 13 x6 + 22 x7 + 20 x8 + 17 x9 + 18 x10 + 19 x11 + 14 x12 + 30 x13 + 22 x14 + 18 x15 + 30 x16 + 11 x17 + 22 x18 + 17 x19 <= 382
 r2: 8 x0 + 8 x1 + 8 x2 + 4 x3 + 6 x4 + 25 x5 + 5 x6 + 20 x7 + 1 x8 + 14 x9 + 14 x10 + 20 x11 + 26 x12 + 23 x13 + 10 x14 + 14 x15 + 4 x16 + 30 x17 + 29 x18 + 12 x19 <= 299
 r3: 11 x0 + 10 x1 + 3 x2 + 8 x3 + 9 x4 + 13 x5 + 18 x6 + 2 x7 + 5 x8 + 19 x9 + 2 x10 + 20 x11 + 6 x12 + 30 x13 + 6 x14 + 23 x15 + 30 x16 + 21 x17 + 12 x18 + 25 x19 <= 301
Bounds
 0 <= x0 <= 3
 0 <= x1 <= 1
 0 <= x2 <= 2
 0 <= x3 <= 5
 0 <= x4 <= 5
 0 <= x5 <= 1
 0 <= x6 <= 4
 0 <= x7 <= 3
 0 <= x8 <= 2
 0 <= x9 <= 3
 0 <= x10 <= 3
 0 <= x11 <= 3
 0 <= x12 <= 6
 0 <= x13 <= 5
 0 <= x14 <= 2
 0 <= x15 <= 6
 0 <= x16 <= 3
 0 <= x17 <= 4
 0 <= x18 <= 1
 0 <= x19 <= 1
General
 x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 x10 x11 x12 x13 x14 x15 x16 x17 x18 x19
End
