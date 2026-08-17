# S1c: build the model whose postsolve recovery error exceeds the margin.
#
# One equality row R: S + V*(Y1 + ... + Yn) == b, minimize
#   cost(S) = 1e-9 (nonzero, so the cost-0 singleton family cannot take it),
#   cost(Yk) = -1  (so every Yk sits at its upper bound 1 at the optimum),
#   Yk in [0, 1], S in [L, +inf).
#
# All n terms are the SAME double V, so the seeded accumulation of
# sol_row[R] is order-independent: E = fl(V+...+V) - n*V is one exact,
# computable number. The implied lower bound on S is ilo = b - n*V; L sits
# delta below it with margin < delta < E, so the family fires and the
# recovered S = b - fl_sum lands below L by about E - delta.
import sys
from fractions import Fraction

N = 2000
EPS = 2.0 ** -52

def sim(V, n):
    s = 0.0
    for _ in range(n):
        s += V
    return s, Fraction(s) - Fraction(V) * n

# fl_sum must land ABOVE the exact sum (E > 0) so the recovered S lands low.
best = None
for m in range(1, 6000):
    for num_off in (1, 3, 5):
        V = float(Fraction(150000 * m + 2 * m + num_off, 3 * m))  # ~5e4
        if not (4.7e4 < V < 5.3e4):
            continue
        s, E = sim(V, N)
        if E > 0 and (best is None or E > best[2]):
            best = (V, s, E)
if best is None:
    sys.exit("no candidate with positive accumulation error found")
V, s, E = best

exactsum = Fraction(V) * N
b = float(exactsum)                 # rhs as the double nearest the exact sum
ilo = Fraction(b) - exactsum        # exact implied lower bound on S
traffic = N * V + abs(b)
margin_est = 8 * EPS * max(1.0, abs(b), traffic)

delta_f = Fraction(5, 2) * Fraction(margin_est)
if Fraction(E) - delta_f < 5 * Fraction(margin_est):
    sys.exit("E=%.3e too small against margin=%.3e; widen the scan" % (float(E), margin_est))
L = float(ilo - delta_f)

S_pub = b - s                       # exactly what the replay computes
viol_pred = float(Fraction(L) - Fraction(S_pub))

print("V=%.17g" % V)
print("b=%.17g" % b)
print("L=%.17g" % L)
print("E=%.6e" % float(E))
print("margin_est=%.6e" % margin_est)
print("delta=%.6e" % float(delta_f))
print("S_pub_pred=%.17g" % S_pub)
print("viol_pred=%.6e" % viol_pred)

with open(sys.argv[1], "w") as f:
    w = f.write
    w("NAME          IFRECOV\n")
    w("ROWS\n")
    w(" N  COST\n")
    w(" E  R\n")
    w("COLUMNS\n")
    w("    S         COST      1.0e-9     R         1.0\n")
    for k in range(N):
        w("    Y%-8d COST      -1.0       R         %.17g\n" % (k, V))
    w("RHS\n")
    w("    RHS       R         %.17g\n" % b)
    w("BOUNDS\n")
    w(" LO BND       S         %.17g\n" % L)
    for k in range(N):
        w(" UP BND       Y%-8d 1.0\n" % k)
    w("ENDATA\n")
