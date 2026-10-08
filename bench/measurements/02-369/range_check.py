# Reads range.c's lines, "VERDICT LO HI A1 ... AN", and recomputes with
# exact fractions whether a multiple of the coefficients' gcd lies in
# [LO, HI]; prints the counts and every disagreement.
#
#   ./range 4000 1 | python3 range_check.py
#
# SPDX-License-Identifier: Apache-2.0
import math, sys
from fractions import Fraction as F

agree = empty = full = 0
for line in sys.stdin:
    f = line.split()
    verdict = f[0] == "1"
    lo, hi = F(f[1]), F(f[2])
    a = [F(x) for x in f[3:]]
    den = 1
    for v in a + [lo, hi]:
        den = den * v.denominator // math.gcd(den, v.denominator)
    g = 0
    for v in a:
        g = math.gcd(g, int(v * den))
    lo_i, hi_i = lo * den, hi * den
    first = -((-lo_i) // g) * g
    expect = first > hi_i
    empty += expect
    full += not expect
    if expect == verdict:
        agree += 1
    else:
        print("DISAGREE", line.strip())
print("rows", agree + (empty + full - agree), "agree", agree,
      "empty", empty, "with a multiple", full)
