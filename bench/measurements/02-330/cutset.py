import sys, subprocess, collections, os

mps, jaos, work = sys.argv[1], sys.argv[2], sys.argv[3]
rounds = int(sys.argv[4]) if len(sys.argv) > 4 else 30

sec = None
rtype, rhs = {}, collections.defaultdict(float)
cols = collections.OrderedDict()
ub = {}
for line in open(mps):
    if not line.strip() or line.startswith('*'):
        continue
    if not line[0].isspace():
        sec = line.split()[0]
        continue
    t = line.split()
    if sec == 'ROWS':
        rtype[t[1]] = t[0]
    elif sec == 'COLUMNS':
        if "'MARKER'" in t:
            continue
        cols.setdefault(t[0], {})
        for k in range(1, len(t), 2):
            cols[t[0]][t[k]] = float(t[k + 1])
    elif sec == 'RHS':
        for k in range(1, len(t), 2):
            rhs[t[k]] = float(t[k + 1])
    elif sec == 'BOUNDS':
        if t[0] in ('UP', 'BV'):
            ub[t[2]] = float(t[3]) if len(t) > 3 else 1.0

obj = [r for r, ty in rtype.items() if ty == 'N'][0]
node_rows = [r for r, ty in rtype.items() if ty == 'E']
nidx = {r: i for i, r in enumerate(node_rows)}
vubrow = {}
for c, d in cols.items():
    for r, v in d.items():
        if rtype.get(r) == 'L':
            vubrow.setdefault(r, []).append((c, v))
arcs = []
ybin = {}
for r, lst in vubrow.items():
    if len(lst) != 2:
        continue
    (c1, v1), (c2, v2) = lst
    if v1 > 0 and v2 < 0:
        ybin[c1] = (c2, -v2)
    elif v2 > 0 and v1 < 0:
        ybin[c2] = (c1, -v1)
for c, d in cols.items():
    if c not in ybin:
        continue
    head = tail = None
    for r, v in d.items():
        if r in nidx:
            if v > 0:
                head = nidx[r]
            else:
                tail = nidx[r]
    arcs.append((c, tail, head, ybin[c][0], ybin[c][1]))
n = len(node_rows)
dem = [rhs.get(r, 0.0) for r in node_rows]
print('nodes', n, 'arcs', len(arcs), 'supply nodes', sum(1 for d in dem if d < 0), 'demand nodes', sum(1 for d in dem if d > 0))

names = {}
for k, c in enumerate(cols):
    names[c] = 'v%d' % k

cuts = []

def write_lp(path):
    with open(path, 'w') as f:
        f.write('Minimize\n obj:')
        for c, d in cols.items():
            if obj in d and d[obj] != 0:
                f.write(' %+.17g %s' % (d[obj], names[c]))
        f.write('\nSubject To\n')
        rows = collections.defaultdict(list)
        for c, d in cols.items():
            for r, v in d.items():
                if r != obj:
                    rows[r].append((c, v))
        for r, lst in rows.items():
            op = {'E': '=', 'L': '<=', 'G': '>='}[rtype[r]]
            f.write(' r%s:' % names_r(r) + ''.join(' %+.17g %s' % (v, names[c]) for c, v in lst) + ' %s %.17g\n' % (op, rhs.get(r, 0.0)))
        for k, (cut, d) in enumerate(cuts):
            f.write(' cut%d:' % k + ''.join(' %+.17g %s' % (cf, names[v]) for v, cf in cut) + ' >= %.17g\n' % d)
        f.write('Bounds\n')
        for c in cols:
            u = ub.get(c)
            if u is not None:
                f.write(' 0 <= %s <= %.17g\n' % (names[c], u))
        f.write('End\n')

rn = {}
def names_r(r):
    if r not in rn:
        rn[r] = str(len(rn))
    return rn[r]

def maxflow_cut(cap_arcs, s, t, nn):
    adj = [[] for _ in range(nn)]
    to, capv = [], []
    def add(u, v, c):
        adj[u].append(len(to)); to.append(v); capv.append(c)
        adj[v].append(len(to)); to.append(u); capv.append(0.0)
    for (u, v, c) in cap_arcs:
        add(u, v, c)
    flow = 0.0
    while True:
        par = [-1] * nn
        par[s] = -2
        q = collections.deque([s])
        while q and par[t] == -1:
            u = q.popleft()
            for e in adj[u]:
                if capv[e] > 1e-12 and par[to[e]] == -1:
                    par[to[e]] = e
                    q.append(to[e])
        if par[t] == -1:
            break
        f = float('inf')
        v = t
        while v != s:
            e = par[v]; f = min(f, capv[e]); v = to[e ^ 1]
        v = t
        while v != s:
            e = par[v]; capv[e] -= f; capv[e ^ 1] += f; v = to[e ^ 1]
        flow += f
        if flow > 1.0:
            break
    reach = set(i for i in range(nn) if par[i] != -1)
    return flow, reach

for it in range(rounds):
    write_lp(work + '/d.lp')
    out = subprocess.run([jaos, 'solve', work + '/d.lp', '--write-point', work + '/d.pt'], capture_output=True, text=True).stdout
    objv = [l for l in out.splitlines() if l.startswith('objective')]
    xv = {}
    for l in open(work + '/d.pt'):
        if l.startswith('#') or not l.strip():
            continue
        a, b = l.split()
        xv[a] = float(b)
    print('round', it, objv, 'cuts', len(cuts), flush=True)
    S = n
    new = 0
    seen = set()
    for t in range(n):
        if dem[t] <= 0:
            continue
        cap = []
        flowterm = os.environ.get('FLOW') is not None
        for (c, tail, head, y, u) in arcs:
            yv = xv.get(names[y], 0.0)
            if flowterm:
                yv = min(yv, xv.get(names[c], 0.0) / dem[t])
            a = tail if tail is not None else S
            b = head if head is not None else S
            cap.append((a, b, yv))
        for i in range(n):
            if dem[i] < 0:
                cap.append((S, i, 1e9))
        flow, reach = maxflow_cut(cap, S, t, n + 1)
        if flow < 1.0 - 1e-6:
            sink = frozenset(i for i in range(n) if i not in reach)
            if sink in seen:
                continue
            seen.add(sink)
            cut = []
            for (c, tail, head, y, u) in arcs:
                if head in sink and (tail is None or tail not in sink):
                    yv = xv.get(names[y], 0.0)
                    xa = xv.get(names[c], 0.0)
                    if flowterm and xa / dem[t] < yv:
                        cut.append((c, 1.0))
                    else:
                        cut.append((y, dem[t]))
            cuts.append((cut, dem[t]))
            new += 1
    if new == 0:
        break
