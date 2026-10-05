import sys, json, math, itertools
# cutwhy2.py MPS CUTS.json : the most violated SCIP c-MIR cuts at the prototype's final point, rebuilt from the rows holding their integers
mps, cj = sys.argv[1], sys.argv[2]
sys.argv = [sys.argv[0], mps, 'dist-look', '0', '0']
exec(open('aggproto.py').read().split("cuts = []\nobj, x = solve(rows)")[0])
x = json.load(open('protox.json'))['x']
d = json.load(open(cj))
rowof = {}
for i, (a, l, u) in enumerate(rows):
    for j in a:
        if isint[j]:
            rowof.setdefault(j, []).append(i)
names = [v.name for v in vs]
sc = []
for r in d['rows']:
    if 'cmir' not in r['name']:
        continue
    a = {idx[k]: v for k, v in r['coef'].items() if k in idx}
    if r['rhs'] < 1e19:
        rhs = r['rhs'] - r['const']; aa = a
    else:
        rhs = -(r['lhs'] - r['const']); aa = {j: -v for j, v in a.items()}
    act = sum(v * x[j] for j, v in aa.items()); nrm = math.sqrt(sum(v * v for v in aa.values()))
    sc.append(((act - rhs) / nrm, r['name'], aa, rhs))
sc.sort(key=lambda t: -t[0])
print('violated', sum(1 for t in sc if t[0] > 1e-6), 'of', len(sc))
for eff, nm, a, rhs in sc[:3]:
    src = sorted({i for j in a if isint[j] for i in rowof.get(j, [])})
    print(f'=== {nm} eff {eff:.4f} rhs {rhs:.4f} ints {sum(1 for j in a if isint[j])} conts {sum(1 for j in a if not isint[j])} src rows {len(src)}')
    print('   cut', {names[j]: round(v, 4) for j, v in a.items()})
    print('   x  ', {names[j]: (round(x[j], 3), lo[j], hi[j]) for j in a})
    for i in src:
        ar, l, u = rows[i]
        print('   row', i, {names[j]: v for j, v in ar.items()}, 'in', (l, u), ' x:', {names[j]: round(x[j], 3) for j in ar})
    arcs = {}
    for i in src:
        key = tuple(sorted(j for j in rows[i][0] if isint[j]))
        arcs.setdefault(key, []).append(i)
    groups = list(arcs.values())
    if len(groups) > 10:
        print('   too many arcs'); continue
    best = None
    for pick in itertools.product(*[[(i, s) for i in g for s in (0, 1)] for g in groups]):
        agg = {}; b = 0.0; ok = True
        for i, s in pick:
            ar, l, u = rows[i]
            if s == 0 and u < INF:
                for j, v in ar.items():
                    agg[j] = agg.get(j, 0.0) + v
                b += u
            elif s == 1 and l > -INF:
                for j, v in ar.items():
                    agg[j] = agg.get(j, 0.0) - v
                b -= l
            else:
                ok = False
        if not ok:
            continue
        agg = {j: v for j, v in agg.items() if abs(v) > 1e-12}
        res = mir(agg, b, x)
        if res and (best is None or res[0] > best[0]):
            best = (res[0], pick, res)
    if best:
        print(f'   mine best eff {best[0]:.4f} delta {best[2][3]} cut', {names[j]: round(v, 4) for j, v in best[2][1].items()}, 'rhs', round(best[2][2], 4))
    else:
        print('   mine: none')
