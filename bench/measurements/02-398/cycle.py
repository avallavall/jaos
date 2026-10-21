# Reads a netqp.py model and HiGHS's solution file, and looks for a cycle
# of up to six arcs bounded only above along which the objective still
# falls from HiGHS's point. Prints HiGHS's objective, the cycle, the slope
# and the curvature along it, and how much further the objective falls.
#
#   python3 cycle.py MODEL.mps HIGHS.sol
#
# SPDX-License-Identifier: Apache-2.0
import sys, itertools
def read(path):
    cols, sec = {}, None
    for ln in open(path):
        t = ln.split()
        if not t: continue
        if ln[0] != ' ':
            sec = t[0]; continue
        if sec == 'COLUMNS':
            c = cols.setdefault(t[0], {'obj': 0.0, 'rows': {}, 'mi': False, 'q': 0.0})
            if t[1] == 'obj': c['obj'] = float(t[2])
            else: c['rows'][t[1]] = float(t[2])
        elif sec == 'BOUNDS' and t[0] == 'MI':
            cols[t[2]]['mi'] = True
        elif sec == 'QUADOBJ':
            cols[t[0]]['q'] = float(t[2])
    return cols
def sol(path):
    x, on = {}, False
    for ln in open(path):
        if ln.startswith('# Columns'):
            if x: break
            on = True; continue
        if ln.startswith('# Rows'): on = False
        t = ln.split()
        if on and len(t) == 2: x[t[0]] = float(t[1])
    return x
cols, x = read(sys.argv[1]), sol(sys.argv[2])
edges = []
for n, c in cols.items():
    if not c['mi']: continue
    fr = [k for k, v in c['rows'].items() if v < 0][0]
    to = [k for k, v in c['rows'].items() if v > 0][0]
    g = c['obj'] + c['q'] * x.get(n, 0.0)
    edges.append((to, fr, n, g, c['q']))
best = None
nodes = sorted({e[0] for e in edges} | {e[1] for e in edges})
adj = {}
for e in edges: adj.setdefault(e[0], []).append(e)
def dfs(start, at, path, seen):
    global best
    for e in adj.get(at, []):
        if e[1] == start and len(path) + 1 >= 2:
            cyc = path + [e]
            G = sum(k[3] for k in cyc); Q = sum(k[4] for k in cyc)
            if G > 0:
                gain = G * G / (2 * Q)
                if best is None or gain > best[0]:
                    best = (gain, G, Q, [k[2] for k in cyc])
        elif e[1] not in seen and e[1] > start and len(path) < 5:
            dfs(start, e[1], path + [e], seen | {e[1]})
for s in nodes:
    dfs(s, s, [], {s})
obj = sum(c['obj'] * x.get(n, 0.0) + 0.5 * c['q'] * x.get(n, 0.0) ** 2 for n, c in cols.items())
if best:
    print('highs objective %.6e; cycle %s slope %.6e curvature %.6e, the objective falls by %.6e more' % (obj, best[3], -best[1], best[2], best[0]))
else:
    print('highs objective %.6e; no improving cycle of up to 6 arcs' % obj)
