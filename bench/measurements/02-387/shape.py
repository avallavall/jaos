# SPDX-License-Identifier: Apache-2.0
# python3 shape.py MODEL.mps: rows by type, length and side; columns by rows touched
import collections
import sys

rows, cols, sec, intmark = {}, collections.defaultdict(dict), None, False
integer, rhs, bounds = set(), {}, {}
for ln in open(sys.argv[1]):
    if not ln.strip() or ln.startswith("*"):
        continue
    if not ln[0].isspace():
        sec = ln.split()[0]
        continue
    p = ln.split()
    if sec == "ROWS":
        rows[p[1]] = p[0]
    elif sec == "COLUMNS":
        if len(p) >= 3 and p[1] == "'MARKER'":
            intmark = p[2] == "'INTORG'"
            continue
        if intmark:
            integer.add(p[0])
        for i in range(1, len(p) - 1, 2):
            cols[p[0]][p[i]] = float(p[i + 1])
    elif sec == "RHS":
        for i in range(1, len(p) - 1, 2):
            rhs[p[i]] = float(p[i + 1])
    elif sec == "BOUNDS":
        bounds.setdefault(p[2], []).append((p[0], p[3] if len(p) > 3 else None))
obj = [r for r, t in rows.items() if t == "N"][0]
rowcols = collections.defaultdict(list)
for c, ent in cols.items():
    for r, v in ent.items():
        if r != obj:
            rowcols[r].append((c, v))
kinds = collections.Counter()
for r, ent in rowcols.items():
    ni = sum(1 for c, _ in ent if c in integer)
    kinds[(rows[r], len(ent), ni, rhs.get(r, 0.0) == 0.0)] += 1
print("rows by (type, length, integer members, rhs zero):")
for k, v in sorted(kinds.items(), key=lambda kv: -kv[1])[:12]:
    print("  ", k, v)
deg = collections.Counter()
for c, ent in cols.items():
    deg[(c in integer, sum(1 for r in ent if r != obj), obj in ent)] += 1
print("columns by (integer, rows touched, in objective):")
for k, v in sorted(deg.items(), key=lambda kv: -kv[1])[:10]:
    print("  ", k, v)
bt = collections.Counter(tuple(sorted(t for t, _ in b)) for b in bounds.values())
print("bound kinds:", dict(bt), "columns without bounds:", len(cols) - len(bounds))
