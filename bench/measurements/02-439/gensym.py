"""Writes small integer programs with symmetric column groups as MPS files.

Each model has 2 to 4 groups of 2 to 6 integer columns. The columns of a
group share their cost, their upper bound and their coefficients in 2 to 4
covering rows, so the symmetry search finds them interchangeable; the
groups' costs differ, so their colours do. Model s is drawn from seed s.

Usage: python3 gensym.py OUT COUNT

SPDX-License-Identifier: Apache-2.0
"""
import os
import random
import sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
for s in range(int(sys.argv[2])):
    r = random.Random(s)
    g = r.randint(2, 4)
    sizes = [r.randint(2, 6) for _ in range(g)]
    ubs = [r.randint(1, 3) for _ in range(g)]
    costs = r.sample(range(1, 20), g)
    nrow = r.randint(2, 4)
    coef = [[r.randint(1, 9) for _ in range(g)] for _ in range(nrow)]
    cols = []
    for k in range(g):
        for t in range(sizes[k]):
            cols.append(k)
    lines = ["NAME sym%d" % s, "ROWS", " N obj"]
    for i in range(nrow):
        lines.append(" G r%d" % i)
    lines.append("COLUMNS")
    lines.append("    M1 'MARKER' 'INTORG'")
    for j, k in enumerate(cols):
        lines.append("    x%d obj %d" % (j, costs[k]))
        for i in range(nrow):
            lines.append("    x%d r%d %d" % (j, i, coef[i][k]))
    lines.append("    M2 'MARKER' 'INTEND'")
    lines.append("RHS")
    for i in range(nrow):
        tot = sum(coef[i][k] for k in cols)
        lines.append("    rhs r%d %d" % (i, r.randint(tot // 4, tot // 2)))
    lines.append("BOUNDS")
    for j, k in enumerate(cols):
        lines.append(" UP bnd x%d %d" % (j, ubs[k]))
    lines.append("ENDATA")
    with open(os.path.join(out, "sym%04d.mps" % s), "w") as f:
        f.write("\n".join(lines) + "\n")
