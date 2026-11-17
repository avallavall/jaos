"""Constraint search over the starts of a time-indexed scheduling model, the
prototype behind 02-472.

Decodes the jobs (partitioning rows), the chain of profile columns and each
item's interval and usage on it with decode.py. A job's domain is the set of
its items still allowed.

Propagation (PROP): `table` takes the compulsory part of every job, the
overlap of its items' intervals, into the profile; `placed` only that of the
jobs with one item left. An item that would lift the profile, less its own
job's part, past a position's capacity leaves its domain, to a fixpoint.

Branching (BRANCH): `settimes` takes the job whose earliest item starts
first, ties by the relaxation's mean start, then by the latest start; it
places that item, and on backtrack removes it. `job` takes the job with the
fewest items left and tries them by the relaxation's value, the k-th costing
k-1 discrepancies. Either runs within a limited discrepancy search.

Rounds: each later round drops the items that end past the last point's
makespan less one, which is what bounding the objective by the last point
does on a makespan model.

    python3 -I sched.py MPS LPPOINT NODES ROUNDS OUT

LPPOINT is a file of NAME VALUE lines. NODES caps each round. Each round's
point goes to OUT-k.point as the line `point NAME...`. Prints, per round,
the nodes and the work (positions and items read by the propagation) to
each discrepancy limit, and the point's makespan on the chain.

SPDX-License-Identifier: Apache-2.0
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import decode  # noqa: E402


class Model:
    def __init__(self, d, lp):
        self.H = len(d['chain'])
        self.cap = list(d['cap'])
        nj = len(d['jobs'])
        self.all = [[] for _ in range(nj)]
        for j, c, p, q, a in d['info']:
            self.all[j].append((p, q, a, c))
        for j in range(nj):
            self.all[j].sort()
        val = {}
        for line in open(lp):
            t = line.split()
            if len(t) == 2 and not line.startswith('#'):
                val[t[0]] = float(t[1])
        self.mean = []
        for j in range(nj):
            s = sum(val.get(c, 0.0) for p, q, a, c in self.all[j])
            m = sum(val.get(c, 0.0) * p for p, q, a, c in self.all[j])
            self.mean.append(m / s if s > 0 else 0.0)
        self.val = val
        self.nj = nj
        self.ops = 0

    def restrict(self, deadline):
        self.items = [[e for e in self.all[j] if deadline is None or e[1] <= deadline]
                      for j in range(self.nj)]


def comp(m, j, dom):
    if not dom:
        return None
    lo = max(m.items[j][i][0] for i in dom)
    hi = min(m.items[j][i][1] for i in dom)
    if lo >= hi:
        return None
    return lo, hi, min(m.items[j][i][2] for i in dom)


def propagate(m, doms, mode):
    nj = m.nj
    prof = [0.0] * (m.H + 1)
    cp = [None] * nj
    for j in range(nj):
        if mode == 'placed' and len(doms[j]) > 1:
            continue
        cp[j] = comp(m, j, doms[j])
        if cp[j]:
            lo, hi, a = cp[j]
            for k in range(lo, hi):
                prof[k] += a
            m.ops += hi - lo
    changed = True
    while changed:
        changed = False
        for j in range(nj):
            dj = doms[j]
            if not dj:
                return False
            own = cp[j]
            lo = m.items[j][dj[0]][0]
            hi = max(m.items[j][i][1] for i in dj)
            m.ops += (min(hi, m.H) - lo) + len(dj)
            keep = []
            for i in dj:
                p, q, a, c = m.items[j][i]
                ok = True
                for k in range(p, min(q, m.H)):
                    use = prof[k] + a
                    if own and own[0] <= k < own[1]:
                        use -= own[2]
                    if use > m.cap[k] + 1e-9:
                        ok = False
                        break
                if ok:
                    keep.append(i)
            if len(keep) != len(dj):
                if not keep:
                    return False
                doms[j] = keep
                if mode == 'placed' and len(keep) > 1:
                    continue
                nc = comp(m, j, keep)
                if own:
                    for k in range(own[0], own[1]):
                        prof[k] -= own[2]
                if nc:
                    for k in range(nc[0], nc[1]):
                        prof[k] += nc[2]
                cp[j] = nc
                changed = True
    return True


class Search:
    def __init__(self, m, limit, branch, mode):
        self.m = m
        self.nodes = 0
        self.limit = limit
        self.found = None
        self.branch = branch
        self.mode = mode

    def dfs(self, doms, disc):
        if self.nodes >= self.limit:
            return False
        self.nodes += 1
        if not propagate(self.m, doms, self.mode):
            return False
        it = self.m.items
        open_ = [j for j in range(self.m.nj) if len(doms[j]) > 1]
        if not open_:
            self.found = [it[j][doms[j][0]] for j in range(self.m.nj)]
            return True
        if self.branch == 'settimes':
            j = min(open_, key=lambda j: (it[j][doms[j][0]][0], self.m.mean[j],
                                          it[j][doms[j][-1]][0], j))
            first = doms[j][0]
            nd = [list(x) for x in doms]
            nd[j] = [first]
            if self.dfs(nd, disc):
                return True
            if disc == 0 or self.nodes >= self.limit:
                return False
            nd = [list(x) for x in doms]
            nd[j] = doms[j][1:]
            return self.dfs(nd, disc - 1)
        j = min(open_, key=lambda j: (len(doms[j]), self.m.mean[j], j))
        mean = self.m.mean[j]
        order = sorted(doms[j], key=lambda i: (-round(self.m.val.get(it[j][i][3], 0.0), 9),
                                               abs(it[j][i][0] - mean), i))
        for k, i in enumerate(order):
            if k > disc:
                break
            nd = [list(x) for x in doms]
            nd[j] = [i]
            if self.dfs(nd, disc - k):
                return True
            if self.nodes >= self.limit:
                return False
        return False


def main():
    path, lp, nodes, rounds, out = sys.argv[1:6]
    branch = os.environ.get('BRANCH', 'settimes')
    mode = os.environ.get('PROP', 'table')
    d = decode.decode(path)
    m = Model(d, lp)
    deadline = None
    for r in range(1, int(rounds) + 1):
        m.restrict(deadline)
        s = Search(m, int(nodes), branch, mode)
        doms = [list(range(len(m.items[j]))) for j in range(m.nj)]
        line = 'round %d deadline %s:' % (r, deadline if deadline else '-')
        m.ops = 0
        if not propagate(m, doms, mode):
            print(line, 'infeasible at the root')
            break
        for disc in range(0, 50):
            before = s.nodes
            ok = s.dfs([list(x) for x in doms], disc)
            line += ' d%d=%d' % (disc, s.nodes - before)
            if ok or s.nodes >= s.limit:
                break
        line += ' nodes %d work %d' % (s.nodes, m.ops)
        if not s.found:
            print(line, 'none')
            break
        ends = max(q for p, q, a, c in s.found)
        print(line, 'makespan', ends)
        with open('%s-%d.point' % (out, r), 'w') as f:
            f.write('point ' + ' '.join(c for p, q, a, c in s.found) + '\n')
        deadline = ends - 1


if __name__ == '__main__':
    main()
