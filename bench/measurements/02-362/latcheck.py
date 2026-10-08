# Eliminates the continuous columns of an LP file's equality rows with
# exact fractions and prints the integer rows left and their gcd.
#
#   python3 latcheck.py miss-1.lp miss-2.lp
#
# SPDX-License-Identifier: Apache-2.0
import re, sys
from fractions import Fraction as F
from math import gcd


def parse(path):
    txt = open(path).read()
    body = txt.split("Subject To")[1].split("Bounds")[0]
    rows = []
    for chunk in re.split(r"\n R\d+:", "\n" + body.strip()):
        chunk = chunk.replace("\n", " ").strip()
        if not chunk:
            continue
        lhs, rhs = chunk.split("=")
        terms = re.findall(r"([+-]?)\s*([0-9.]+)\s+C(\d+)", lhs)
        row = {}
        for s, v, c in terms:
            row[int(c)] = F(v) * (-1 if s == "-" else 1)
        rows.append((row, F(rhs.strip())))
    gen = txt.split("General")[1].split("End")[0].split()
    ints = {int(g[1:]) for g in gen}
    return rows, ints


def solve(path):
    rows, ints = parse(path)
    cols = sorted({c for r, _ in rows for c in r})
    cont = [c for c in cols if c not in ints]
    rows = [(dict(r), b) for r, b in rows]
    for c in cont:
        piv = next((i for i, (r, b) in enumerate(rows) if r.get(c, 0) != 0), None)
        if piv is None:
            continue
        pr, pb = rows.pop(piv)
        new = []
        for r, b in rows:
            f = r.get(c, 0)
            if f != 0:
                k = f / pr[c]
                for cc, v in pr.items():
                    r[cc] = r.get(cc, 0) - k * v
                b = b - k * pb
                r = {cc: v for cc, v in r.items() if v != 0}
            new.append((r, b))
        rows = new
    print(path)
    for r, b in rows:
        den = 1
        for v in list(r.values()) + [b]:
            den = den * v.denominator // gcd(den, v.denominator)
        ri = {c: int(v * den) for c, v in r.items()}
        bi = int(b * den)
        print("  integer row:", ri, "=", bi)
        g = 0
        for v in ri.values():
            g = gcd(g, v)
        print("  gcd", g, "rhs mod gcd", bi % g if g else bi)


for p in sys.argv[1:]:
    solve(p)
