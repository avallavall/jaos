# SPDX-License-Identifier: Apache-2.0
import gzip
import sys

from pyscipopt import Model, quicksum


def tokens(path):
    opener = gzip.open if path.endswith(".gz") else open
    with opener(path, "rt") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                yield line.split()


def read_cbf(path):
    it = tokens(path)
    cbf = {"sense": "MIN", "var": [], "con": [], "int": [], "obja": [],
           "objb": 0.0, "a": [], "b": []}
    for t in it:
        key = t[0]
        if key == "OBJSENSE":
            cbf["sense"] = next(it)[0]
        elif key in ("VAR", "CON"):
            _, k = map(int, next(it))
            cbf[key.lower()] = [(d[0], int(d[1])) for d in (next(it) for _ in range(k))]
        elif key == "INT":
            n = int(next(it)[0])
            cbf["int"] = [int(next(it)[0]) for _ in range(n)]
        elif key == "OBJACOORD":
            n = int(next(it)[0])
            cbf["obja"] = [(int(r[0]), float(r[1])) for r in (next(it) for _ in range(n))]
        elif key == "OBJBCOORD":
            cbf["objb"] = float(next(it)[0])
        elif key == "ACOORD":
            n = int(next(it)[0])
            cbf["a"] = [(int(r[0]), int(r[1]), float(r[2])) for r in (next(it) for _ in range(n))]
        elif key == "BCOORD":
            n = int(next(it)[0])
            cbf["b"] = [(int(r[0]), float(r[1])) for r in (next(it) for _ in range(n))]
        elif key == "VER":
            next(it)
        else:
            raise ValueError(f"CBF section {key} is not read here")
    return cbf


def cone(m, kind, e):
    if kind == "F":
        return
    if kind == "L+":
        for v in e:
            m.addCons(v >= 0)
    elif kind == "L-":
        for v in e:
            m.addCons(v <= 0)
    elif kind == "L=":
        for v in e:
            m.addCons(v == 0)
    elif kind == "Q":
        m.addCons(e[0] >= 0)
        m.addCons(e[0] * e[0] >= quicksum(v * v for v in e[1:]))
    elif kind == "QR":
        m.addCons(e[0] >= 0)
        m.addCons(e[1] >= 0)
        m.addCons(2 * e[0] * e[1] >= quicksum(v * v for v in e[2:]))
    else:
        raise ValueError(f"cone {kind} is not read here")


def build(cbf):
    m = Model()
    n = sum(size for _, size in cbf["var"])
    ints = set(cbf["int"])
    x = [m.addVar(lb=None, vtype="I" if j in ints else "C") for j in range(n)]
    at = 0
    for kind, size in cbf["var"]:
        cone(m, kind, x[at:at + size])
        at += size
    nrow = sum(size for _, size in cbf["con"])
    rows = [[] for _ in range(nrow)]
    for i, j, a in cbf["a"]:
        rows[i].append((j, a))
    b = [0.0] * nrow
    for i, v in cbf["b"]:
        b[i] = v
    expr = [quicksum(a * x[j] for j, a in rows[i]) + b[i] for i in range(nrow)]
    at = 0
    for kind, size in cbf["con"]:
        cone(m, kind, expr[at:at + size])
        at += size
    obj = quicksum(a * x[j] for j, a in cbf["obja"]) + cbf["objb"]
    m.setObjective(obj, "maximize" if cbf["sense"] == "MAX" else "minimize")
    return m


m = build(read_cbf(sys.argv[1]))
m.hideOutput()
m.setParam("limits/time", float(sys.argv[2]))
m.setParam("limits/gap", 1e-6)
m.setParam("parallel/maxnthreads", 1)
m.optimize()
obj = m.getObjVal() if m.getNSols() > 0 else float("nan")
print(f"{m.getStatus()}\t{obj!r}\t{m.getNNodes()}\t{m.getSolvingTime():.3f}")
