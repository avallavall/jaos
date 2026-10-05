import sys, math
import numpy as np
from scipy.optimize import linprog
from pyscipopt import Model

# aggproto.py MPS VARIANT ROUNDS STEPS : MIR on aggregated rows, the bound after each round
# VARIANT words: dist (pick the most interior continuous column, else the largest coefficient),
#                stop (stop a start row's walk at its first violated cut), half (try delta/2,/4,/8),
#                short (pick the row with fewest entries, else the first), rank2 (cuts are rows too),
#                look (pick the row that leaves the least coefficient-weighted bound distance), low and
#                lowall (substitute at the lower bound unless the column sits at its upper one),
#                slack (look also counts |lambda| times the added row's slack), tight (start only from
#                row sides within 0.1 of tight)
# A fifth argument, a JSON file of SCIP's cut rows, also reports which of them the final point violates.
f, variant, rounds, steps = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
scip_cuts = sys.argv[5] if len(sys.argv) > 5 else None
m = Model(); m.hideOutput(); m.readProblem(f)
vs = m.getVars(); n = len(vs)
idx = {v.name: i for i, v in enumerate(vs)}
c = np.array([v.getObj() for v in vs])
lo = np.array([v.getLbOriginal() for v in vs]); hi = np.array([v.getUbOriginal() for v in vs])
isint = np.array([v.vtype() in ('INTEGER', 'BINARY') for v in vs])
rows = []
for cons in m.getConss():
    a = {idx[k]: v for k, v in m.getValsLinear(cons).items() if v != 0.0}
    rows.append((a, m.getLhs(cons), m.getRhs(cons)))
INF = 1e19

def solve(rows):
    ub, bub, eq, beq = [], [], [], []
    for a, l, u in rows:
        r = np.zeros(n)
        for j, v in a.items():
            r[j] = v
        if l > -INF and u < INF and l == u:
            eq.append(r); beq.append(u)
        else:
            if u < INF:
                ub.append(r); bub.append(u)
            if l > -INF:
                ub.append(-r); bub.append(-l)
    res = linprog(c, A_ub=np.array(ub) if ub else None, b_ub=np.array(bub) if ub else None,
                  A_eq=np.array(eq) if eq else None, b_eq=np.array(beq) if eq else None, bounds=list(zip(lo, hi)), method='highs')
    return res.fun, res.x

def mir(a, b, x):
    sb = b; terms = {}; cand = []
    for j, aj in a.items():
        if aj == 0.0:
            continue
        if math.isinf(lo[j]) and math.isinf(hi[j]):
            return None
        if ('low' in variant and not isint[j]) or 'lowall' in variant:
            up = math.isinf(lo[j]) or (not math.isinf(hi[j]) and x[j] - lo[j] > 0.9999 * (hi[j] - lo[j]))
        else:
            up = (not math.isinf(hi[j])) and (math.isinf(lo[j]) or hi[j] - x[j] < x[j] - lo[j])
        sb -= aj * (hi[j] if up else lo[j])
        terms[j] = (-aj if up else aj, up)
        if isint[j] and x[j] > lo[j] + 1e-6 and x[j] < hi[j] - 1e-6:
            cand.append(abs(aj))
    def build(d):
        beta = sb / d; f0 = beta - math.floor(beta)
        if f0 < 1e-2 or f0 > 1 - 1e-2:
            return None
        cut = {}; rhs = math.floor(beta)
        for j, (cj, up) in terms.items():
            g = cj / d
            if isint[j]:
                k = math.floor(g) + max(0.0, (g - math.floor(g)) - f0) / (1 - f0)
            else:
                k = g / (1 - f0) if g < 0 else 0.0
            if k == 0.0:
                continue
            if up:
                cut[j] = cut.get(j, 0.0) - k; rhs -= k * hi[j]
            else:
                cut[j] = cut.get(j, 0.0) + k; rhs += k * lo[j]
        nrm = math.sqrt(sum(v * v for v in cut.values()))
        if nrm == 0:
            return None
        act = sum(v * x[j] for j, v in cut.items())
        return ((act - rhs) / nrm, cut, rhs, d)
    best = None
    for d in sorted(set(cand)):
        r = build(d)
        if r and (best is None or r[0] > best[0]):
            best = r
    if best and 'half' in variant:
        d0 = best[3]
        for q in (2, 4, 8):
            r = build(d0 / q)
            if r and r[0] > best[0]:
                best = r
    return best

def colrows(rowlist):
    cr = {}
    for i, (a, l, u) in enumerate(rowlist):
        for j in a:
            cr.setdefault(j, []).append(i)
    return cr

cuts = []
obj, x = solve(rows)
print(f'round 0 bound {obj:.2f}', flush=True)
for r in range(rounds):
    src = rows + (cuts if 'rank2' in variant else [])
    cr = colrows(src)
    new = {}
    for i0, (a0, l0, u0) in enumerate(src):
        for side in (0, 1):
            bnd = u0 if side == 0 else l0
            if abs(bnd) >= INF:
                continue
            sg = 1.0 if side == 0 else -1.0
            if 'tight' in variant:
                act0 = sum(v * x[j] for j, v in a0.items())
                if sg * (bnd - act0) > 0.1:
                    continue
            agg = {j: sg * v for j, v in a0.items()}; b = sg * bnd
            used = {i0}; picked = set()
            for s in range(steps + 1):
                res = mir(agg, b, x)
                if res and res[0] > 1e-6:
                    key = tuple(sorted((j, round(v, 9)) for j, v in res[1].items()))
                    if key not in new:
                        new[key] = res
                    if 'stop' in variant:
                        break
                if s == steps:
                    break
                pick, pv = -1, -1.0
                for j, aj in agg.items():
                    if aj == 0.0 or isint[j] or j in picked:
                        continue
                    d = min(x[j] - lo[j], hi[j] - x[j])
                    if d <= 1e-6:
                        continue
                    v = d if 'dist' in variant else abs(aj)
                    if v > pv:
                        pv, pick = v, j
                if pick < 0:
                    break
                rr = -1; rlen = 10**9
                for ri in cr.get(pick, []):
                    if ri in used:
                        continue
                    ar, lr, ur = src[ri]
                    lam = agg[pick] / ar[pick]
                    bb = lr if lam > 0 else ur
                    if abs(bb) >= INF:
                        continue
                    if 'look' in variant:
                        mass = 0.0
                        tmp = dict(agg)
                        for j2, v2 in ar.items():
                            tmp[j2] = tmp.get(j2, 0.0) - lam * v2
                        for j2, v2 in tmp.items():
                            if isint[j2] or abs(v2) < 1e-12:
                                continue
                            mass += abs(v2) * max(0.0, min(x[j2] - lo[j2], hi[j2] - x[j2]))
                        if 'slack' in variant:
                            act = sum(v2 * x[j2] for j2, v2 in ar.items())
                            sl = (act - lr) if lam > 0 else (ur - act)
                            mass += abs(lam) * max(0.0, sl)
                        if mass < rlen:
                            rr, rlen = ri, mass
                    elif 'short' in variant:
                        if len(ar) < rlen:
                            rr, rlen = ri, len(ar)
                    else:
                        rr = ri; break
                if rr < 0:
                    break
                ar, lr, ur = src[rr]
                lam = agg[pick] / ar[pick]
                for j, v in ar.items():
                    agg[j] = agg.get(j, 0.0) - lam * v
                    if abs(agg[j]) < 1e-12:
                        agg[j] = 0.0
                b -= lam * (lr if lam > 0 else ur)
                used.add(rr); picked.add(pick)
    if not new:
        print(f'round {r+1} no cuts'); break
    for eff, cut, rhs, d in new.values():
        cuts.append((cut, -1e20, rhs))
    obj, x = solve(rows + cuts)
    print(f'round {r+1} cuts {len(new)} total {len(cuts)} bound {obj:.2f}', flush=True)

import json
names = [v.name for v in vs]
if scip_cuts:
    d = json.load(open(scip_cuts))
    sc = []
    for r in d['rows']:
        if 'cmir' not in r['name'] and 'flow' not in r['name'] and 'knap' not in r['name']:
            continue
        a = {idx[k]: v for k, v in r['coef'].items() if k in idx}
        sc.append((a, r['lhs'] - r['const'] if r['lhs'] > -1e19 else -1e20, r['rhs'] - r['const'] if r['rhs'] < 1e19 else 1e20))
    o1, x1 = solve(rows + sc)
    print('LP with SCIP cuts only', round(o1, 2), 'cuts', len(sc))
    viol = []
    for a, l, u in sc:
        act = sum(v * x[j] for j, v in a.items())
        nrm = math.sqrt(sum(v * v for v in a.values()))
        e = max(act - u if u < 1e19 else -1, l - act if l > -1e19 else -1) / nrm
        viol.append(e)
    viol.sort(reverse=True)
    print('SCIP cuts violated at my final point:', sum(1 for e in viol if e > 1e-6), 'top', [round(e, 4) for e in viol[:8]])
    o2, x2 = solve(rows + cuts + sc)
    print('LP with both', round(o2, 2))
json.dump({'x': list(map(float, x))}, open('protox.json', 'w'))
