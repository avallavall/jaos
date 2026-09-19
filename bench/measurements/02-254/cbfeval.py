# SPDX-License-Identifier: Apache-2.0
"""Evaluate a point against a CBF file read independently of JAOS.

usage: cbfeval.py FILE.cbf.gz SOLUTION
SOLUTION is a JAOS solution file; its first nvar col records are x.
"""
import gzip
import math
import sys


def lines(path):
    op = gzip.open if path.endswith(".gz") else open
    with op(path, "rt") as f:
        for raw in f:
            s = raw.strip()
            if not s or s.startswith("#"):
                continue
            yield s


def parse(path):
    it = lines(path)
    d = {"obj": {}, "objb": 0.0, "a": [], "b": {}, "var": [], "con": []}
    for kw in it:
        if kw == "VER":
            next(it)
        elif kw == "OBJSENSE":
            d["sense"] = next(it)
        elif kw in ("VAR", "CON"):
            n, k = map(int, next(it).split())
            blocks = []
            for _ in range(k):
                t, dim = next(it).split()
                blocks.append((t, int(dim)))
            d["var" if kw == "VAR" else "con"] = blocks
            d["n" + kw.lower()] = n
        elif kw == "OBJACOORD":
            for _ in range(int(next(it))):
                j, v = next(it).split()
                d["obj"][int(j)] = float(v)
        elif kw == "OBJBCOORD":
            d["objb"] = float(next(it))
        elif kw == "ACOORD":
            for _ in range(int(next(it))):
                i, j, v = next(it).split()
                d["a"].append((int(i), int(j), float(v)))
        elif kw == "BCOORD":
            for _ in range(int(next(it))):
                i, v = next(it).split()
                d["b"][int(i)] = float(v)
        elif kw == "INT":
            for _ in range(int(next(it))):
                next(it)
        elif kw == "CHANGE":
            break
        else:
            raise SystemExit("unknown keyword " + kw)
    return d


def cone_viol(t, v):
    if t == "F":
        return 0.0
    if t == "L+":
        return max(0.0, -min(v))
    if t == "L-":
        return max(0.0, max(v))
    if t == "L=":
        return max(abs(x) for x in v)
    if t == "Q":
        return max(0.0, math.sqrt(sum(x * x for x in v[1:])) - v[0])
    if t == "QR":
        return max(0.0, sum(x * x for x in v[2:]) - 2 * v[0] * v[1],
                   -v[0], -v[1])
    raise SystemExit("cone " + t)


def main():
    d = parse(sys.argv[1])
    x = []
    with open(sys.argv[2]) as f:
        for s in f:
            p = s.split()
            if p and p[0] == "col":
                x.append(float(p[2]))
    nv = d["nvar"]
    x = x[:nv]
    obj = d["objb"] + sum(v * x[j] for j, v in d["obj"].items())
    g = [0.0] * d["ncon"]
    for i, j, v in d["a"]:
        g[i] += v * x[j]
    for i, v in d["b"].items():
        g[i] += v
    worst_var = worst_con = 0.0
    at = 0
    for t, dim in d["var"]:
        worst_var = max(worst_var, cone_viol(t, x[at:at + dim]))
        at += dim
    at = 0
    for t, dim in d["con"]:
        worst_con = max(worst_con, cone_viol(t, g[at:at + dim]))
        at += dim
    print("objective %.17g" % obj)
    print("worst variable-cone violation %.3e" % worst_var)
    print("worst constraint-cone violation %.3e" % worst_con)


main()
