# SPDX-License-Identifier: Apache-2.0
# python3 arcs.py MODEL.mps PRESOLVED.mps: which arcs of a fixed-charge
# network the presolved model dropped, and the degrees of their nodes
import collections
import sys


def load(path):
    rows, cols, sec, rhs = {}, collections.defaultdict(dict), None, {}
    for ln in open(path):
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
    return rows, cols, rhs


rows, cols, rhs = load(sys.argv[1])
_, pcols, _ = load(sys.argv[2])
obj = [r for r, t in rows.items() if t == "N"][0]
flows = [c for c, e in cols.items() if obj not in e]
binaries = [c for c, e in cols.items() if obj in e]
print("names kept:", sum(1 for c in cols if c in pcols), "of", len(cols))
arc = {}
for x in flows:
    nodes = sorted((r, v) for r, v in cols[x].items() if rows[r] == "E")
    cap = [r for r in cols[x] if rows[r] == "L"][0]
    y = [b for b in binaries if cap in cols[b]][0]
    tail = [r for r, v in nodes if v < 0]
    head = [r for r, v in nodes if v > 0]
    arc[x] = (tuple(tail), tuple(head), y, -cols[y][cap] / cols[x][cap], cols[y][obj])
gone = [x for x in flows if x not in pcols]
kept = [x for x in flows if x in pcols]
print("flows gone:", len(gone), "kept:", len(kept))
pair = collections.Counter()
for x, (t, h, y, u, f) in arc.items():
    pair[(t, h)] += 1
rev = lambda a: (a[1], a[0])
stats = collections.Counter()
for x in gone:
    t, h, y, u, f = arc[x]
    stats["parallel same way" if pair[(t, h)] > 1 else "single"] += 1
    stats["has reverse arc" if pair[rev((t, h))] > 0 else "no reverse"] += 1
    stats["one end missing" if not t or not h else "two ends"] += 1
print("gone:", dict(stats))
stats = collections.Counter()
for x in kept:
    t, h, y, u, f = arc[x]
    stats["parallel same way" if pair[(t, h)] > 1 else "single"] += 1
    stats["has reverse arc" if pair[rev((t, h))] > 0 else "no reverse"] += 1
print("kept:", dict(stats))
deg = collections.Counter()
for x, (t, h, y, u, f) in arc.items():
    for n in t + h:
        deg[n] += 1
dg = collections.Counter()
for x in gone:
    t, h, *_ = arc[x]
    dg[tuple(sorted(deg[n] for n in t + h))] += 1
print("gone arcs by their nodes' degrees:", dg.most_common(8))
