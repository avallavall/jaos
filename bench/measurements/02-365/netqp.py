# Generates a circulation QP on N nodes and M arcs: a ring plus random
# arcs, a flow planted as random cycles with magnitudes from 1e-2 to 1e5,
# arcs that carry no positive flow bounded in (-inf, 0], the others in a
# box around their flow, random costs, and separable curvatures, log-uniform
# from 1e-11 to 2 on six arcs in ten and 2 on the rest.
#
#   python3 netqp.py N M SEED OUT.mps
#
# SPDX-License-Identifier: Apache-2.0
import random, sys


def gen(seed, n, m, path):
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
        mag = 10 ** rnd.uniform(-2, 5)
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
    lines = ["NAME netqp", "ROWS", " N obj"]
    lines += [" E n%d" % i for i in range(n)]
    lines.append("COLUMNS")
    bounds, quad = [], []
    for k, (a, b) in enumerate(arcs):
        c = rnd.choice([0.0, rnd.uniform(-1e4, 1e4)])
        col = "x%d" % k
        ent = [("n%d" % a, -1.0), ("n%d" % b, 1.0)]
        if c != 0.0:
            ent.insert(0, ("obj", c))
        for r, v in ent:
            lines.append("    %s %s %.12g" % (col, r, v))
        f = flow[k]
        if f <= 0.0:
            bounds.append(" MI BND %s" % col)
            bounds.append(" UP BND %s 0" % col)
        else:
            lo = f * rnd.uniform(0.3, 1.0)
            hi = f * 10 ** rnd.uniform(0, 2)
            bounds.append(" LO BND %s %.12g" % (col, lo))
            bounds.append(" UP BND %s %.12g" % (col, hi))
        q = 10 ** rnd.uniform(-11, 0.3) if rnd.random() < 0.6 else 2.0
        quad.append("    %s %s %.12g" % (col, col, q))
    lines.append("RHS")
    lines.append("BOUNDS")
    lines += bounds
    lines.append("QUADOBJ")
    lines += quad
    lines.append("ENDATA")
    open(path, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    gen(int(sys.argv[3]), int(sys.argv[1]), int(sys.argv[2]), sys.argv[4])
