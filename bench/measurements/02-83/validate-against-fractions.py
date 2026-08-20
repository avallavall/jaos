"""A second implementation of the exact sum, sharing no code with the first.

`exact-objective.c` adds the products c_j * x_j in a binary fixed-point
accumulator it carries itself. Every conclusion in this record rests on that
accumulator being exact, and its own self-test only checks it against values
this session wrote down. This checks it against Python's `fractions`, which
is exact by construction and which nobody here wrote.

A dev-time tool. Nothing builds it, nothing links it, and the library does
not depend on Python — the same standing as `bench/koch-refs.py`.

    exact-objective --terms bench/instances/finnis.mps > terms.txt
    python3 validate-against-fractions.py terms.txt <expected decimal>

Prints the exact sum and says whether the digits agree.
"""
import sys
from fractions import Fraction

TERMS, EXPECT = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else None)

total = Fraction(0)
n = 0
for line in open(TERMS, encoding="utf-8"):
    f = line.split()
    if f[0] == "offset":
        total += Fraction(float.fromhex(f[1]))
    elif f[0] == "term":
        # float.fromhex is exact, and Fraction of a float is exact, so the
        # product is the same rational the accumulator holds.
        total += Fraction(float.fromhex(f[1])) * Fraction(float.fromhex(f[2]))
        n += 1

# The same rendering the C prints: the integer part, a point, then digits of
# the fraction, with '...' when it has not terminated.
FRAC = 20
neg = total < 0
mag = -total if neg else total
whole = mag.numerator // mag.denominator
rest = mag - whole
digits = ""
for _ in range(FRAC):
    rest *= 10
    d = rest.numerator // rest.denominator
    digits += str(d)
    rest -= d
shown = ("-" if neg else "") + str(whole) + "." + digits + ("..." if rest else "")

print(f"terms {n}")
print(f"fractions {shown}")
if EXPECT is not None:
    print(f"accumulator {EXPECT}")
    print("AGREE" if shown == EXPECT else "DISAGREE")
    sys.exit(0 if shown == EXPECT else 1)
