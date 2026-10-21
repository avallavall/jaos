# Feasibility Jump (Luteberget and Sartor, Mathematical Programming
# Computation 2023), tried from outside the tool: a weighted local search
# for a first point. Every column starts at its lower bound and each row
# carries a weight of 1. A move sets one column, taken from the rows a
# point breaks (at most 25 sampled), to the value in its box that most
# lowers the weighted violation of its rows; ties go to the lower cost.
# When no move lowers it, every broken row's weight rises by 1 and the
# best column of a random broken row moves anyway. Prints, for each model
# and each of seeds 1, 2 and 3, the move that reached a point and its
# objective, or None:None after MOVES moves.
#
#   python3 fj.py MOVES MODEL.mps...
#
# SPDX-License-Identifier: Apache-2.0
import math, random, sys


def read(path):
    rows, kind, cols, integer, rhs, lo, up = [], {}, {}, {}, {}, {}, {}
    sec, inint, cost = None, False, {}
    for ln in open(path):
        t = ln.split()
        if not t:
            continue
        if ln[0] != ' ':
            sec = t[0]
            continue
        if sec == 'ROWS':
            if t[0] != 'N':
                kind[t[1]] = t[0]
                rows.append(t[1])
        elif sec == 'COLUMNS':
            if len(t) >= 3 and t[1] == "'MARKER'":
                inint = t[2] == "'INTORG'"
                continue
            j = t[0]
            if j not in cols:
                cols[j] = {}
                integer[j] = inint
                lo[j], up[j] = 0.0, math.inf
            for k in range(1, len(t) - 1, 2):
                if t[k] == 'obj':
                    cost[j] = float(t[k + 1])
                else:
                    cols[j][t[k]] = float(t[k + 1])
        elif sec == 'RHS':
            for k in range(1, len(t) - 1, 2):
                rhs[t[k]] = float(t[k + 1])
        elif sec == 'BOUNDS':
            if t[0] == 'UP':
                up[t[2]] = float(t[3])
            elif t[0] == 'LO':
                lo[t[2]] = float(t[3])
    return rows, kind, cols, integer, rhs, lo, up, cost


def fj(path, seed, max_moves):
    rows, kind, cols, integer, rhs, lo, up, cost = read(path)
    names = list(cols)
    n, m = len(names), len(rows)
    ri = {r: i for i, r in enumerate(rows)}
    colrows = [[(ri[r], a) for r, a in cols[j].items()] for j in names]
    rowcols = [[] for _ in range(m)]
    for j, ent in enumerate(colrows):
        for i, a in ent:
            rowcols[i].append((j, a))
    b = [rhs.get(r, 0.0) for r in rows]
    kd = [kind[r] for r in rows]
    L = [lo[j] for j in names]
    U = [up[j] for j in names]
    isint = [integer[j] for j in names]
    x = [L[j] for j in range(n)]
    act = [sum(a * x[j] for j, a in rowcols[i]) for i in range(m)]
    w = [1.0] * m
    rng = random.Random(seed)

    def viol(i, v):
        d = v - b[i]
        if kd[i] == 'E':
            return abs(d)
        if kd[i] == 'L':
            return d if d > 0 else 0.0
        return -d if d < 0 else 0.0

    def best_jump(j):
        cands = {L[j], U[j]}
        for i, a in colrows[j]:
            t = x[j] + (b[i] - act[i]) / a
            if isint[j]:
                cands.add(math.floor(t + 1e-9))
                cands.add(math.ceil(t - 1e-9))
            else:
                cands.add(t)
        base = sum(w[i] * viol(i, act[i]) for i, _ in colrows[j])
        best, bv = None, None
        for v in cands:
            if v < L[j] or v > U[j] or v == x[j] or math.isinf(v):
                continue
            f = sum(w[i] * viol(i, act[i] + a * (v - x[j])) for i, a in colrows[j])
            key = (base - f, -cost.get(names[j], 0.0) * (v - x[j]))
            if best is None or key > best:
                best, bv = key, v
        return best, bv

    for move in range(max_moves):
        bad = [i for i in range(m) if viol(i, act[i]) > 1e-9]
        if not bad:
            return move, sum(cost.get(names[j], 0.0) * x[j] for j in range(n))
        cand = sorted({j for i in bad for j, _ in rowcols[i]})
        if len(cand) > 25:
            cand = rng.sample(cand, 25)
        top, tj, tv = None, None, None
        for j in cand:
            sc, v = best_jump(j)
            if sc is not None and sc[0] > 1e-9 and (top is None or sc > top):
                top, tj, tv = sc, j, v
        if tj is None:
            for i in bad:
                w[i] += 1.0
            i = rng.choice(bad)
            top = None
            for j, _ in rowcols[i]:
                sc, v = best_jump(j)
                if sc is not None and (top is None or sc > top):
                    top, tj, tv = sc, j, v
            if tj is None:
                continue
        d = tv - x[tj]
        x[tj] = tv
        for i, a in colrows[tj]:
            act[i] += a * d
    return None, None


if __name__ == '__main__':
    moves = int(sys.argv[1])
    for path in sys.argv[2:]:
        res = [fj(path, s, moves) for s in (1, 2, 3)]
        print(path.split('/')[-1], ' '.join('%s:%s' % r for r in res), flush=True)
