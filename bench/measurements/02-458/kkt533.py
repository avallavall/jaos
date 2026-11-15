"""KKT points of 02-255 seed 4's model 533 (the second brute-force solve),
by active set, in 60-digit decimal arithmetic. C4 and C6 are fixed at -3.

Variables x = (C1, C2, C3, C5). Every constraint is written g(x) <= 0.
"""
from decimal import Decimal as D, getcontext
import itertools

getcontext().prec = 60

C4 = D(-3)
C6 = D(-3)

c = {1: D('-0.08204502383448031'), 2: D('-0.4621651937993658'),
     3: D('0.8798350975300651'), 5: D('-0.30509074113704693')}
q = {1: D('1.1781838132898408'), 2: D('0.5250919743684503'),
     3: D('0.5518908579957371'), 5: D('0.46170218191091794')}
const = (D('3.376957379169766') + D('-0.8567564462027899') * C4
         + D('0.6429691811105451') * C6
         + D('1.350582026337865') * C4 * C4 / 2
         + D('0.6611001314051954') * C6 * C6 / 2)

# R4's x'Qx over (C1, C3, C4, C5), both halves.
Q4 = {(1, 1): D('0.8892321242196557'), (1, 3): D('-0.09995344769639633'),
      (1, 4): D('-0.4217305612012978'), (1, 5): D('-0.3529512771867152'),
      (3, 3): D('0.7436546052711871'), (3, 4): D('0.07648696727451453'),
      (3, 5): D('0.20302603637700262'), (4, 4): D('0.6346754617754906'),
      (4, 5): D('0.37437651271456546'), (5, 5): D('1.0321069538161665')}
a4 = {1: D('1.4501931014561573'), 3: D('1.2905312222147995'),
      4: D('-0.11794028151271574'), 5: D('-0.8954910454651146')}
R4_up = D('3.1709701597523967')
R1_lo = D('0.20522689819335938')
R1_up = R1_lo + D('2.130162239074707')
a1_4, a1_5 = D('0.31398508157598926'), D('0.5223245834228978')
R2_lo = D('2.7425107955932617')
R3_up = D('2.3239153394500054')

IDX = {1: 0, 2: 1, 3: 2, 5: 3}


def val(x, j):
    return C4 if j == 4 else x[IDX[j]]


def f(x):
    return sum(c[j] * val(x, j) + q[j] * val(x, j) ** 2 / 2 for j in c) + const


def grad_f(x):
    return [c[j] + q[j] * val(x, j) for j in (1, 2, 3, 5)]


def hess_f():
    h = [[D(0)] * 4 for _ in range(4)]
    for j in (1, 2, 3, 5):
        h[IDX[j]][IDX[j]] = q[j]
    return h


def r4(x):
    s = sum(a4[j] * val(x, j) for j in a4)
    for (i, j), v in Q4.items():
        m = 1 if i == j else 2
        s += m * v * val(x, i) * val(x, j)
    return s - R4_up


def grad_r4(x):
    g = [D(0)] * 4
    for j in (1, 3, 5):
        g[IDX[j]] += a4[j]
    for (i, j), v in Q4.items():
        m = 1 if i == j else 2
        if i != 4:
            g[IDX[i]] += m * v * val(x, j)
        if j != 4:
            g[IDX[j]] += m * v * val(x, i)
    return g


def hess_r4():
    h = [[D(0)] * 4 for _ in range(4)]
    for (i, j), v in Q4.items():
        if i == 4 or j == 4:
            continue
        m = 1 if i == j else 2
        h[IDX[i]][IDX[j]] += m * v
        h[IDX[j]][IDX[i]] += m * v
    return h


CONS = {
    'R1lo': (lambda x: R1_lo - (a1_4 * C4 + a1_5 * x[3]),
             lambda x: [D(0), D(0), D(0), -a1_5],
             lambda: [[D(0)] * 4 for _ in range(4)]),
    'R1up': (lambda x: a1_4 * C4 + a1_5 * x[3] - R1_up,
             lambda x: [D(0), D(0), D(0), a1_5],
             lambda: [[D(0)] * 4 for _ in range(4)]),
    'R2': (lambda x: R2_lo - (D('1.7932860499934744') * x[0]
                              + D('2.616854987782789') * x[2]
                              - D('0.7951869032655292') * x[3]
                              - D('2.72361293209309') * C6),
           lambda x: [-D('1.7932860499934744'), D(0), -D('2.616854987782789'),
                      D('0.7951869032655292')],
           lambda: [[D(0)] * 4 for _ in range(4)]),
    'R3': (lambda x: D('0.2873229801617665') * x[3]
           + D('0.17594220293784427') * x[3] ** 2 - R3_up,
           lambda x: [D(0), D(0), D(0), D('0.2873229801617665')
                      + 2 * D('0.17594220293784427') * x[3]],
           lambda: [[D(0)] * 3 + [D(0)], [D(0)] * 4, [D(0)] * 4,
                    [D(0)] * 3 + [2 * D('0.17594220293784427')]]),
    'R4': (r4, grad_r4, hess_r4),
    'cone': (lambda x: x[3] ** 2 + C6 ** 2 - 2 * x[0] * x[1],
             lambda x: [-2 * x[1], -2 * x[0], D(0), 2 * x[3]],
             lambda: [[D(0), D(-2), D(0), D(0)], [D(-2), D(0), D(0), D(0)],
                      [D(0)] * 4, [D(0), D(0), D(0), D(2)]]),
    'C3lo': (lambda x: D('-1.7299428488684743') - x[2],
             lambda x: [D(0), D(0), D(-1), D(0)],
             lambda: [[D(0)] * 4 for _ in range(4)]),
    'C5lo': (lambda x: D('-1.2803675666264676') - x[3],
             lambda x: [D(0), D(0), D(0), D(-1)],
             lambda: [[D(0)] * 4 for _ in range(4)]),
    'C5up': (lambda x: x[3] - D('3.8595862524944855'),
             lambda x: [D(0), D(0), D(0), D(1)],
             lambda: [[D(0)] * 4 for _ in range(4)]),
}


def solve(a, b):
    n = len(a)
    m = [row[:] + [b[i]] for i, row in enumerate(a)]
    for k in range(n):
        p = max(range(k, n), key=lambda i: abs(m[i][k]))
        m[k], m[p] = m[p], m[k]
        if m[k][k] == 0:
            raise ZeroDivisionError
        for i in range(k + 1, n):
            t = m[i][k] / m[k][k]
            for j in range(k, n + 1):
                m[i][j] -= t * m[k][j]
    x = [D(0)] * n
    for i in reversed(range(n)):
        x[i] = (m[i][n] - sum(m[i][j] * x[j] for j in range(i + 1, n))) / m[i][i]
    return x


def kkt(active, x0):
    x = x0[:]
    lam = [D(1)] * len(active)
    for _ in range(200):
        g = grad_f(x)
        h = hess_f()
        for k, name in enumerate(active):
            gi = CONS[name][1](x)
            hi = CONS[name][2]()
            for i in range(4):
                g[i] += lam[k] * gi[i]
                for j in range(4):
                    h[i][j] += lam[k] * hi[i][j]
        n = 4 + len(active)
        a = [[D(0)] * n for _ in range(n)]
        rhs = [D(0)] * n
        for i in range(4):
            for j in range(4):
                a[i][j] = h[i][j]
            rhs[i] = -g[i]
        for k, name in enumerate(active):
            gi = CONS[name][1](x)
            for i in range(4):
                a[i][4 + k] = gi[i]
                a[4 + k][i] = gi[i]
            rhs[4 + k] = -CONS[name][0](x)
        d = solve(a, rhs)
        x = [x[i] + d[i] for i in range(4)]
        lam = [lam[k] + d[4 + k] for k in range(len(active))]
        if max(abs(v) for v in d) < D('1e-45'):
            break
    return x, lam


def report(active, x, lam):
    worst = max((CONS[nm][0](x), nm) for nm in CONS if nm not in active)
    sign_ok = all(l >= 0 for l in lam)
    print(f"active {active}: objective {f(x):.15e}")
    print("   x  C1 %.20e  C2 %.20e  C3 %.20e  C5 %.20e" % tuple(x))
    print("   multipliers " + ", ".join(f"{n} {l:.6e}" for n, l in zip(active, lam)))
    print(f"   worst inactive constraint {worst[1]} at {worst[0]:.3e}; "
          f"multipliers {'all >= 0' if sign_ok else 'NOT all >= 0'}")
    return worst[0] <= 0 and sign_ok


C2g = D(732)
C5g = (R1_lo - a1_4 * C4) / a1_5
C1g = (C5g ** 2 + 9) / (2 * C2g)
C3g = D(0)
for _ in range(100):
    x = [C1g, C2g, C3g, C5g]
    C3g -= r4(x) / grad_r4(x)[2]
x0 = [C1g, C2g, C3g, C5g]

for active in (['R1lo', 'R4', 'cone'], ['R4', 'cone'], ['R1lo', 'cone']):
    try:
        x, lam = kkt(active, x0)
    except ZeroDivisionError:
        print(f"active {active}: singular")
        continue
    print("   KKT point" if report(active, x, lam) else "   not the optimum")
