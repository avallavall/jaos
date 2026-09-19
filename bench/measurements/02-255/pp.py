# SPDX-License-Identifier: Apache-2.0
"""The continuous optimum of a CBLIB pp instance, in closed form.

usage: pp.py FILE.cbf.gz

The pp family (Ziegler 1982) is min sum c_j x_j + d_j t_j with
2 x_j t_j >= w_j^2 in a rotated cone and one row sum a_j x_j <= b, so
t_j = w_j^2 / (2 x_j) at an optimum and x_j = sqrt(e_j / (c_j + l a_j))
for the row's multiplier l, found by bisection. The script checks that
the file has that shape before it trusts the formula, and rounds the
point to an integer one that keeps the row.
"""
import gzip
import math
import sys


def sections(path):
    with gzip.open(path, "rt") as f:
        lines = [ln.strip() for ln in f]
    out, i = {}, 0
    while i < len(lines):
        ln = lines[i]
        if ln and ln.isalpha() and ln.isupper():
            name, body = ln, []
            i += 1
            while i < len(lines) and lines[i] != "":
                body.append(lines[i])
                i += 1
            out[name] = body
        i += 1
    return out


s = sections(sys.argv[1])
doms = [d.split() for d in s["VAR"][1:]]
n = int(doms[0][1])
assert doms[0][0] == "L+" and doms[1] == ["F", str(n)]
assert all(d == ["QR", "3"] for d in doms[2:]) and len(doms) - 2 == n
assert s["CON"][1:] == ["L- 1", "L= " + str(3 * n)]
obj = {}
for ln in s["OBJACOORD"][1:]:
    j, v = ln.split()
    obj[int(j)] = float(v)
rows = {}
for ln in s["ACOORD"][1:]:
    i, j, v = ln.split()
    rows.setdefault(int(i), []).append((int(j), float(v)))
bc = {}
for ln in s["BCOORD"][1:]:
    i, v = ln.split()
    bc[int(i)] = float(v)
a = [0.0] * n
for j, v in rows[0]:
    assert j < n
    a[j] = v
b = -bc.get(0, 0.0)
link = {}
for i in range(1, 3 * n + 1):
    ent = rows.get(i, [])
    minus = [j for j, v in ent if v == -1.0 and j >= 2 * n]
    plus = [(j, v) for j, v in ent if j < 2 * n]
    assert len(minus) == 1 and len(ent) == len(minus) + len(plus)
    if plus:
        assert len(plus) == 1 and plus[0][1] == 1.0 and i not in bc
        link[minus[0]] = ("var", plus[0][0])
    else:
        link[minus[0]] = ("const", bc.get(i, 0.0))
c = [obj.get(j, 0.0) for j in range(n)]
e = [0.0] * n
for k in range(n):
    u, v, w = (link[2 * n + 3 * k + t] for t in range(3))
    assert u[0] == "var" and u[1] < n and v[0] == "var" and v[1] >= n
    assert w[0] == "const" and v[1] - n == u[1]
    e[u[1]] = obj.get(n + u[1], 0.0) * w[1] * w[1] / 2.0
assert all(t >= 0.0 for t in e + c + a)


def point(lam):
    return [math.sqrt(e[j] / (c[j] + lam * a[j])) if e[j] > 0.0 else 0.0
            for j in range(n)]


def load(x):
    return math.fsum(a[j] * x[j] for j in range(n))


def value(x):
    return math.fsum(c[j] * x[j] + (e[j] / x[j] if e[j] > 0.0 else 0.0)
                     for j in range(n))


lam = 0.0
if load(point(0.0)) > b:
    lo, hi = 0.0, 1.0
    while load(point(hi)) > b:
        hi *= 2.0
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if load(point(mid)) > b:
            lo = mid
        else:
            hi = mid
    lam = hi
x = point(lam)
print(f"n={n} multiplier={lam:.10g} continuous optimum={value(x):.10g}")


def nearest(j):
    if e[j] == 0.0:
        return 0
    k = max(1, math.floor(x[j]))
    cand = [k, k + 1] + ([k - 1] if k > 1 else [])
    return min(cand, key=lambda q: c[j] * q + e[j] / q + lam * a[j] * q)


xi = [nearest(j) for j in range(n)]
while load(xi) > b:
    best, bj = None, -1
    for j in range(n):
        if a[j] > 0.0 and xi[j] > 1:
            d = (c[j] * (xi[j] - 1) + e[j] / (xi[j] - 1)) - \
                (c[j] * xi[j] + e[j] / xi[j])
            if best is None or d / a[j] < best:
                best, bj = d / a[j], j
    xi[bj] -= 1
print(f"a rounded integer point: {value(xi):.10g}")
