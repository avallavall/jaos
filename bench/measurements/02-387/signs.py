# SPDX-License-Identifier: Apache-2.0
# python3 signs.py MODEL.mps: equality rows by the signs of their entries
import collections
import sys

rows, cols, sec, rhs = {}, collections.defaultdict(dict), None, {}
lo = {}
for ln in open(sys.argv[1]):
    if not ln.strip() or ln.startswith("*"):
        continue
    if not ln[0].isspace():
        sec = ln.split()[0]
        continue
    p = ln.split()
    if sec == "ROWS":
        rows[p[1]] = p[0]
    elif sec == "COLUMNS" and p[1] != "'MARKER'":
        for i in range(1, len(p) - 1, 2):
            cols[p[0]][p[i]] = float(p[i + 1])
    elif sec == "RHS":
        for i in range(1, len(p) - 1, 2):
            rhs[p[i]] = float(p[i + 1])
rowcols = collections.defaultdict(list)
for c, ent in cols.items():
    for r, v in ent.items():
        if rows.get(r) != "N":
            rowcols[r].append((c, v))
kind = collections.Counter()
for r, ent in rowcols.items():
    if rows[r] != "E":
        continue
    pos = sum(1 for _, v in ent if v > 0)
    neg = sum(1 for _, v in ent if v < 0)
    b = rhs.get(r, 0.0)
    if pos == 0 or neg == 0:
        kind[("one sign", "rhs 0" if b == 0 else "rhs %s" % ("same sign" if (b > 0) == (pos > 0) else "opposite"), len(ent))] += 1
    else:
        kind[("both signs", len(ent) == 2)] += 1
for k, v in sorted(kind.items(), key=lambda kv: -kv[1])[:12]:
    print(k, v)
