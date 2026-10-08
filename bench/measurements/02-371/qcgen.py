# Generates a QCQP on a circulation: N nodes, M arcs, a flow planted as
# random cycles with magnitudes from 1e-2 to 10**QC_TOP (default 3), every
# arc bounded around its flow, random costs, a separable quadratic
# objective on half the arcs, and NBALL rows sum(x_k^2) <= R over random
# arcs, R set so the planted point holds.
#
#   python3 qcgen.py N M NBALL SEED OUT.mps
#
# SPDX-License-Identifier: Apache-2.0
import os, random, sys


def gen(seed, n, m, nball, path):
    rnd = random.Random(seed)
    arcs = [(i, (i + 1) % n) for i in range(n)]
    while len(arcs) < m:
        a, b = rnd.randrange(n), rnd.randrange(n)
        if a != b:
            arcs.append((a, b))
    flow = [0.0] * len(arcs)
    out = {}
    for k, (a, b) in enumerate(arcs):
        out.setdefault(a, []).append((k, 1.0, b))
        out.setdefault(b, []).append((k, -1.0, a))
    for _ in range(n // 2):
        mag = 10 ** rnd.uniform(-2, float(os.environ.get("QC_TOP", "3")))
        start = rnd.randrange(n)
        at, walk = start, []
        for _ in range(4 * n):
            k, sg, nxt = rnd.choice(out[at])
            walk.append((k, sg))
            at = nxt
            if at == start:
                break
        if at == start:
            for k, sg in walk:
                flow[k] += sg * mag
    balls = []
    for b in range(nball):
        members = rnd.sample(range(len(arcs)), rnd.randint(3, max(3, len(arcs) // 3)))
        r = sum(flow[k] ** 2 for k in members) * rnd.uniform(1.0, 2.0) + 1.0
        balls.append((members, r))
    lines = ["NAME qcnet", "ROWS", " N obj"]
    lines += [" E n%d" % i for i in range(n)]
    lines += [" L b%d" % b for b in range(nball)]
    lines.append("COLUMNS")
    bounds, quad = [], []
    for k, (a, b) in enumerate(arcs):
        c = rnd.uniform(-1e3, 1e3)
        col = "x%d" % k
        for r, v in (("obj", c), ("n%d" % a, -1.0), ("n%d" % b, 1.0)):
            lines.append("    %s %s %.12g" % (col, r, v))
        f = flow[k]
        if f <= 0.0:
            bounds.append(" LO BND %s %.12g" % (col, -abs(f) * 10 - 10))
            bounds.append(" UP BND %s 0" % col)
        else:
            bounds.append(" LO BND %s %.12g" % (col, f * rnd.uniform(0.3, 1.0)))
            bounds.append(" UP BND %s %.12g" % (col, f * 10 ** rnd.uniform(0, 2)))
        if rnd.random() < 0.5:
            quad.append("    %s %s %.12g" % (col, col, 10 ** rnd.uniform(-6, 0)))
    lines.append("RHS")
    for b, (members, r) in enumerate(balls):
        lines.append("    RHS b%d %.12g" % (b, r))
    lines.append("BOUNDS")
    lines += bounds
    if quad:
        lines.append("QUADOBJ")
        lines += quad
    for b, (members, r) in enumerate(balls):
        lines.append("QCMATRIX b%d" % b)
        for k in members:
            lines.append("    x%d x%d 1" % (k, k))
    lines.append("ENDATA")
    open(path, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    gen(int(sys.argv[4]), int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), sys.argv[5])
