"""Searches over the items of partitioning rows, the prototype behind 02-469.

Reads an MPS file whose free integer columns are binaries in exactly one
partitioning row and whose continuous columns sit in one row each. A move
gives one partitioning row another of its items, or trades the slots of
two rows' items, where an item's slot is the set of the other rows it sits
in. Each row prices its continuous columns in closed form: the cheapest
way to cover what the items leave of its bounds. Rows with no continuous
column are hard. Prints the first threshold and the best objective, less
the model's constant terms, after STEPS moves from a greedy start in a
seeded order.

    python3 -I prototype.py MPS STEPS SEED RULE SWAPS

RULE is anneal (a rise d is taken with probability exp(-d/T), T falling
geometrically to 1e-3 of the first), linear (threshold accepting: a rise
below T is taken, T falling linearly to 0) or geometric (threshold
accepting, T falling geometrically to 1e-3 of the first). SWAPS is 1 to
draw half the moves as trades, 0 for none. The first T is the median rise
among 2000 sampled moves that leave the hard rows as they are.

SPDX-License-Identifier: Apache-2.0
"""
import math
import os
import random
import sys

INF = float('inf')
HERE = os.path.dirname(os.path.abspath(__file__))
CENSUS = os.path.join(HERE, '..', '02-463', 'census.py')


def read_mps(path):
    src = open(CENSUS).read().replace('\nmain()\n', '\n')
    g = {'__name__': 'census'}
    exec(compile(src, CENSUS, 'exec'), g)
    return g['read_mps'](path)


def main():
    path, steps, seed, rule, swaps = sys.argv[1:6]
    steps, seed, swaps = int(steps), int(seed), int(swaps) == 1
    rng = random.Random(seed)
    rows, ent, bnd, ci, cols, obj = read_mps(path)
    rid = {r: i for i, r in enumerate(rows)}
    nr = len(rows)
    cost = {c: cols[c].get(obj, 0.0) for c in cols}
    crow = {c: [(rid[r], a) for r, a in cols[c].items() if r in rid] for c in cols}
    part = [bnd[r] == (1.0, 1.0) and bool(ent[r]) and
            all(a == 1.0 and ci[c][0] and ci[c][1] == 0 and ci[c][2] == 1
                for c, a in ent[r]) for r in rows]
    items = [c for c in cols if ci[c][0] and ci[c][1] != ci[c][2]]
    conts = [c for c in cols if not ci[c][0]]
    if any(sum(1 for i, a in crow[c] if part[i]) != 1 for c in items) or \
            any(len(crow[c]) > 1 for c in conts):
        sys.exit('not the shape')
    lo = [bnd[r][0] for r in rows]
    hi = [bnd[r][1] for r in rows]
    up = [[] for _ in range(nr)]
    dn = [[] for _ in range(nr)]
    base = [0.0] * nr
    anchor = [0.0] * nr
    soft = [False] * nr
    free = [False] * nr
    for c in conts:
        if not crow[c]:
            continue
        i, a = crow[c][0]
        soft[i] = True
        l, u = ci[c][1], ci[c][2]
        L, U = (a * l, a * u) if a > 0 else (a * u, a * l)
        r = cost[c] / a
        if L == -INF and U == INF:
            free[i] = True
        elif r > 0 or (r == 0 and L > -INF):
            anchor[i] += L
            base[i] += r * L
            up[i].append((r, U - L))
        else:
            anchor[i] += U
            base[i] += r * U
            dn[i].append((-r, U - L))
    for i in range(nr):
        up[i].sort()
        dn[i].sort()

    def rowcost(i, act):
        if free[i]:
            return 0.0, 0.0
        s, need_lo, need_hi = anchor[i], lo[i] - act, hi[i] - act
        if need_lo <= s <= need_hi:
            return base[i], 0.0
        d, segs = (need_lo - s, up[i]) if s < need_lo else (s - need_hi, dn[i])
        c = base[i]
        for r, cap in segs:
            t = min(d, cap)
            c += r * t
            d -= t
            if d <= 0:
                break
        return c, max(d, 0.0)

    opts = {}
    for c in items:
        p = next(i for i, a in crow[c] if part[i])
        opts.setdefault(p, []).append(c)
    P = sorted(opts)
    slot = {c: tuple(sorted(i for i, a in crow[c] if not part[i])) for c in items}
    byslot = {p: {slot[c]: c for c in opts[p]} for p in P}
    choice = {}
    act = [0.0] * nr

    def delta(moves):
        touched = {}
        for co, cn in moves:
            for i, a in crow[co] if co else []:
                touched[i] = touched.get(i, 0.0) - a
            for i, a in crow[cn]:
                touched[i] = touched.get(i, 0.0) + a
        dz = sum(cost[cn] - (cost[co] if co else 0.0) for co, cn in moves)
        dv = 0.0
        for i, da in touched.items():
            if part[i]:
                continue
            c1, v1 = rowcost(i, act[i] + da)
            c0, v0 = rowcost(i, act[i])
            dz += c1 - c0
            dv += v1 - v0
        return dz, dv

    def apply(moves):
        for p, co, cn in moves:
            for i, a in crow[co] if co else []:
                act[i] -= a
            for i, a in crow[cn]:
                act[i] += a
            choice[p] = cn

    order = P[:]
    rng.shuffle(order)
    for p in order:
        best = min(opts[p], key=lambda c: tuple(reversed(delta([(None, c)]))))
        apply([(p, None, best)])

    def propose():
        p = rng.choice(P)
        co = choice[p]
        if swaps and rng.random() < 0.5:
            q = rng.choice(P)
            ko = choice[q]
            c, k = byslot[p].get(slot[ko]), byslot[q].get(slot[co])
            if q == p or c is None or k is None or c == co:
                return None
            return [(p, co, c), (q, ko, k)]
        c = rng.choice(opts[p])
        return None if c == co else [(p, co, c)]

    def total():
        z = sum(cost[choice[p]] for p in P)
        v = 0.0
        for i in range(nr):
            if not part[i]:
                c, w = rowcost(i, act[i])
                z += c
                v += w
        return z, v

    rises = []
    for _ in range(2000):
        mv = propose()
        if mv is not None:
            dz, dv = delta([(co, cn) for _, co, cn in mv])
            if dz > 0 and dv == 0:
                rises.append(dz)
    rises.sort()
    T0 = rises[len(rises) // 2] if rises else 0.0
    z, v = total()
    best = z if v == 0 else INF
    for s in range(steps):
        f = s / steps
        T = T0 * (1.0 - f) if rule == 'linear' else T0 * 1e-3 ** f
        mv = propose()
        if mv is None:
            continue
        dz, dv = delta([(co, cn) for _, co, cn in mv])
        if dv > 0:
            continue
        if dv == 0 and dz > 0:
            if rule == 'anneal':
                if T <= 0 or rng.random() >= math.exp(-dz / T):
                    continue
            elif dz > T:
                continue
        apply(mv)
        z += dz
        v += dv
        if v <= 1e-9 and z < best - 1e-9:
            best = z
    print('T0 %.4g best %.6g' % (T0, best))


main()
