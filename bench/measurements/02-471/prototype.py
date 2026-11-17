"""Searches over the items of partitioning rows with an LP over the rest,
the prototype behind 02-471.

Reads an MPS file whose free integer columns are binaries in exactly one
partitioning row, the items, or sit in no partitioning row. The other
columns, continuous or general integer, go into one LP over the rows they
sit in, with the items' activity moved into the row bounds. A move gives
one partitioning row another of its items, or trades the slots of two
rows' items, as in 02-469's prototype, and the LP prices it. Rows with no
LP column are hard and priced in closed form.

The first phase starts from the relaxation's largest item in each row, or
from a greedy pass in a seeded order, and walks on the violation: the hard
rows' violation plus the optimum of the LP with one elastic column per
finite row bound. A move that does not raise it is taken. At zero the
second phase takes the objective: threshold accepting, the first threshold
the median rise among 30 sampled moves, falling geometrically to 1e-3 of
it over the LPs that remain. The best assignment then has its items fixed
and the model solved, a MIP when general integers are left. Prints the LPs
each phase took, the work they cost, the search's objectives and that last
solve's.

    python3 prototype.py MPS LPS SEED START

START is relax or greedy. Needs the JAOS library (make shared) for the LPs.

SPDX-License-Identifier: Apache-2.0
"""
import os
import random
import sys

INF = float('inf')
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..', '..')
CENSUS = os.path.join(HERE, '..', '02-463', 'census.py')
sys.path.insert(0, os.path.join(ROOT, 'python'))
os.environ.setdefault('JAOS_LIBRARY',
                      os.path.join(ROOT, 'build', 'release', 'libjaos.so'))
import jaos


def read_mps(path):
    src = open(CENSUS).read().replace('\nmain()\n', '\n')
    g = {'__name__': 'census'}
    exec(compile(src, CENSUS, 'exec'), g)
    return g['read_mps'](path)


def main():
    path, lps, seed, start = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    rng = random.Random(seed)
    rows, ent, bnd, ci, cols, obj = read_mps(path)
    rid = {r: i for i, r in enumerate(rows)}
    nr = len(rows)
    cost = {c: cols[c].get(obj, 0.0) for c in cols}
    crow = {c: [(rid[r], a) for r, a in cols[c].items() if r in rid] for c in cols}
    lo = [bnd[r][0] for r in rows]
    hi = [bnd[r][1] for r in rows]
    fixed = {c for c in cols if ci[c][1] == ci[c][2]}
    part = [lo[i] == 1.0 and hi[i] == 1.0 and bool(ent[r]) and
            all(a == 1.0 and ci[c][0] and ci[c][1] >= 0 and ci[c][2] <= 1
                for c, a in ent[r]) for i, r in enumerate(rows)]
    act = [0.0] * nr
    covered = [False] * nr
    for c in fixed:
        for i, a in crow[c]:
            act[i] += a * ci[c][1]
            covered[i] = covered[i] or (part[i] and ci[c][1] == 1.0)
    items = [c for c in cols if c not in fixed and ci[c][0] and ci[c][1] == 0 and
             ci[c][2] == 1 and sum(1 for i, a in crow[c] if part[i]) == 1]
    iset = set(items)
    lpcols = [c for c in cols if c not in fixed and c not in iset]
    lprows = sorted({i for c in lpcols for i, a in crow[c]})
    pos = {i: k for k, i in enumerate(lprows)}
    hard = [i for i in range(nr) if not part[i] and i not in pos]
    opts = {}
    for c in items:
        p = next(i for i, a in crow[c] if part[i])
        if not covered[p]:
            opts.setdefault(p, []).append(c)
    P = sorted(opts)
    slot = {c: tuple(sorted(i for i, a in crow[c] if not part[i])) for c in items}
    byslot = {p: {slot[c]: c for c in opts[p]} for p in P}

    st, ix, va, cc, cl, cu = [0], [], [], [], [], []
    for c in lpcols:
        for i, a in crow[c]:
            ix.append(pos[i])
            va.append(a)
        st.append(len(ix))
        cc.append(cost[c])
        cl.append(ci[c][1])
        cu.append(ci[c][2])
    est, eix, eva = list(st), list(ix), list(va)
    ecc, ecl, ecu = [0.0] * len(lpcols), list(cl), list(cu)
    for k, i in enumerate(lprows):
        for b, s in ((lo[i], 1.0), (hi[i], -1.0)):
            if abs(b) < INF:
                eix.append(k)
                eva.append(s)
                est.append(len(eix))
                ecc.append(1.0)
                ecl.append(0.0)
                ecu.append(INF)
    rl = [lo[i] - act[i] for i in lprows]
    ru = [hi[i] - act[i] for i in lprows]
    feas_lp, viol_lp = jaos.Model(), jaos.Model()
    feas_lp.load(len(lpcols), len(lprows), cc, cl, cu, rl, ru, st, ix, va)
    viol_lp.load(len(ecc), len(lprows), ecc, ecl, ecu, rl, ru, est, eix, eva)
    stats = {'lps': 0, 'work': 0}
    basis = {}

    def solve(m, touched):
        for i in touched:
            m.set_row_bounds(pos[i], lo[i] - act[i], hi[i] - act[i])
        if id(m) in basis:
            m.set_basis(*basis[id(m)])
        m.solve()
        stats['lps'] += 1
        stats['work'] += m.work_units
        if m.status != jaos.SolveStatus.OPTIMAL:
            return None
        basis[id(m)] = m.basis()
        return m.objective()

    def viol(i):
        tol = 1e-6 * (1 + max(abs(lo[i]) if lo[i] > -INF else 0,
                              abs(hi[i]) if hi[i] < INF else 0))
        v = max(lo[i] - act[i], act[i] - hi[i], 0.0)
        return v if v > tol else 0.0

    choice = {}

    def shift(mv, sgn):
        touched, hit = set(), set()
        for p, co, cn in mv:
            for c, s in ((co, -1), (cn, 1)):
                for i, a in crow[c] if c else []:
                    if not part[i]:
                        act[i] += s * sgn * a
                        (touched if i in pos else hit).add(i)
        return touched, hit

    def trial(m, mv):
        hit = {i for p, co, cn in mv for c in (co, cn) if c
               for i, a in crow[c] if not part[i] and i not in pos}
        h0 = sum(viol(i) for i in hit)
        touched, _ = shift(mv, 1)
        dh = sum(viol(i) for i in hit) - h0
        z = solve(m, touched) if m is viol_lp or dh <= 0 else None
        return dh, z, touched

    def undo(m, mv, touched):
        shift(mv, -1)
        for i in touched:
            m.set_row_bounds(pos[i], lo[i] - act[i], hi[i] - act[i])

    relax = jaos.Model()
    relax.read_mps(path)
    for j in range(relax.num_col):
        relax.set_col_integer(j, False)
    relax.solve()
    xs = relax.solution().col_value
    cidx = {c: k for k, c in enumerate(cols)}
    if start == 'relax':
        for p in P:
            choice[p] = max(opts[p], key=lambda c: xs[cidx[c]])
            shift([(p, None, choice[p])], 1)
    else:
        order = P[:]
        rng.shuffle(order)
        solve(viol_lp, lprows)
        for p in order:
            best = None
            for c in sorted(opts[p], key=lambda c: -xs[cidx[c]]):
                dh, z, touched = trial(viol_lp, [(p, None, c)])
                v = sum(viol(i) for i in hard) + (z if z is not None else INF)
                if best is None or v < best[0] - 1e-9:
                    best = (v, c)
                undo(viol_lp, [(p, None, c)], touched)
            choice[p] = best[1]
            shift([(p, None, best[1])], 1)
    hv = sum(viol(i) for i in hard)
    z1 = solve(viol_lp, lprows)
    v = hv + z1
    first = stats['lps']

    def propose():
        p = rng.choice(P)
        co = choice[p]
        if rng.random() < 0.5:
            q = rng.choice(P)
            ko = choice[q]
            c, k = byslot[p].get(slot[ko]), byslot[q].get(slot[co])
            if q == p or c is None or k is None or c == co:
                return None
            return [(p, co, c), (q, ko, k)]
        c = rng.choice(opts[p])
        return None if c == co else [(p, co, c)]

    while v > 1e-6 and stats['lps'] < lps:
        mv = propose()
        if mv is None:
            continue
        dh, z, touched = trial(viol_lp, mv)
        if z is not None and hv + dh + z <= v:
            for p, co, cn in mv:
                choice[p] = cn
            hv += dh
            v = hv + z
        else:
            undo(viol_lp, mv, touched)
    print('start %s violation %.6g after %d LPs, ' % (start, v, stats['lps'] - first), end='')
    if v > 1e-6:
        print('no point')
        return
    zl = solve(feas_lp, lprows)
    if zl is None:
        print('the LP is infeasible at zero violation')
        return
    cur = zl + sum(cost[choice[p]] for p in P)
    best, kept = cur, dict(choice)
    print('first %.10g at LP %d, ' % (cur, stats['lps']), end='')
    rises = []
    for _ in range(30):
        mv = None
        while mv is None:
            mv = propose()
        dh, z, touched = trial(feas_lp, mv)
        if z is not None and dh <= 0:
            d = z + sum(cost[cn] - cost[co] for p, co, cn in mv) - zl
            if d > 0:
                rises.append(d)
        undo(feas_lp, mv, touched)
    rises.sort()
    t0 = rises[len(rises) // 2] if rises else 0.0
    l0 = stats['lps']
    while stats['lps'] < lps:
        t = t0 * 1e-3 ** ((stats['lps'] - l0) / max(1, lps - l0))
        mv = propose()
        if mv is None:
            continue
        dh, z, touched = trial(feas_lp, mv)
        if z is not None:
            d = z + sum(cost[cn] - cost[co] for p, co, cn in mv) - zl
            if d <= t:
                for p, co, cn in mv:
                    choice[p] = cn
                zl = z
                cur += d
                if cur < best - 1e-9 * (1 + abs(best)):
                    best, kept = cur, dict(choice)
                continue
        undo(feas_lp, mv, touched)
    full = jaos.Model()
    full.read_mps(path)
    chosen = set(kept.values())
    for p in P:
        for c in opts[p]:
            v = 1.0 if c in chosen else 0.0
            full.set_col_bounds(cidx[c], v, v)
    full.set_work_limit(100000000)
    full.solve()
    fin = full.objective() if full.status == jaos.SolveStatus.OPTIMAL else None
    print('T0 %.4g best %.10g after %d LPs, %d work units, finish %s' % (
        t0, best, stats['lps'], stats['work'], fin))


main()
