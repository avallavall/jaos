"""Decodes a time-indexed scheduling model by its rows, for 02-472.

Finds the partitioning rows (equality, right-hand side 1, binaries with
coefficient 1), the chain of profile columns (continuous columns with +1 in
one equality row and -1 in the next), and each item's interval and usage on
the chain. Prints the jobs, their windows, durations and usages, the bounds
of the chain's columns and the energy the jobs need.

    python3 -I decode.py MPS

SPDX-License-Identifier: Apache-2.0
"""
import os
import sys

INF = float('inf')
HERE = os.path.dirname(os.path.abspath(__file__))
CENSUS = os.path.join(HERE, '..', '02-463', 'census.py')


def read_mps(path):
    src = open(CENSUS).read().replace('\nmain()\n', '\n')
    g = {'__name__': 'census'}
    exec(compile(src, CENSUS, 'exec'), g)
    return g['read_mps'](path)


def decode(path):
    rows, ent, bnd, ci, cols, obj = read_mps(path)
    rid = {r: i for i, r in enumerate(rows)}
    crow = {c: [(rid[r], a) for r, a in cols[c].items() if r in rid] for c in cols}
    cost = {c: cols[c].get(obj, 0.0) for c in cols}
    isbin = {c: ci[c][0] and ci[c][1] == 0.0 and ci[c][2] == 1.0 for c in cols}
    part = [bnd[r] == (1.0, 1.0) and len(ent[r]) > 1 and
            all(a == 1.0 and isbin[c] for c, a in ent[r]) for r in rows]
    eq = [bnd[r][0] == bnd[r][1] for r in rows]
    nxt = {}
    prv = {}
    link = {}
    for c in cols:
        if ci[c][0] or len(crow[c]) != 2:
            continue
        (i, a), (k, b) = crow[c]
        if not (eq[i] and eq[k]) or a != -b or abs(a) != 1.0:
            continue
        p, q = (i, k) if a > 0 else (k, i)
        if p in nxt or q in prv:
            continue
        nxt[p] = q
        prv[q] = p
        link[p] = c
    heads = [p for p in nxt if p not in prv]
    best = []
    for h in heads:
        path = [h]
        while path[-1] in nxt:
            path.append(nxt[path[-1]])
        if len(path) > len(best):
            best = path
    pos = {r: k for k, r in enumerate(best)}
    H = len(best)
    zlo = [0.0] * H
    zhi = [0.0] * H
    for k, p in enumerate(best):
        if p in link:
            zlo[k], zhi[k] = ci[link[p]][1], ci[link[p]][2]
        else:
            for c in cols:
                if not ci[c][0] and crow[c] == [(p, 1.0)]:
                    zlo[k], zhi[k] = ci[c][1], ci[c][2]
    run = 0.0
    cap = [0.0] * H
    for k, p in enumerate(best):
        run += bnd[rows[p]][0]
        cap[k] = zhi[k] - run
    items = [c for c in cols if isbin[c] and ci[c][1] < ci[c][2]]
    jobs = []
    for i, r in enumerate(rows):
        if part[i]:
            jobs.append(i)
    jix = {i: k for k, i in enumerate(jobs)}
    info = []
    bad = 0
    for c in items:
        js = [i for i, a in crow[c] if part[i]]
        ch = sorted((pos[i], a) for i, a in crow[c] if i in pos)
        if len(js) != 1:
            bad += 1
            continue
        if len(ch) == 2 and ch[0][1] == -ch[1][1]:
            info.append((jix[js[0]], c, ch[0][0], ch[1][0], -ch[0][1]))
        elif len(ch) == 1:
            info.append((jix[js[0]], c, ch[0][0], len(best), -ch[0][1]))
        else:
            bad += 1
    return dict(rows=rows, ent=ent, bnd=bnd, ci=ci, cols=cols, obj=obj,
                crow=crow, cost=cost, part=part, chain=best, link=link, cap=cap,
                jobs=jobs, info=info, bad=bad, rid=rid)


def main():
    d = decode(sys.argv[1])
    rows, bnd, ci, chain, link = d['rows'], d['bnd'], d['ci'], d['chain'], d['link']
    print('rows', len(rows), 'cols', len(d['cols']), 'partitioning', len(d['jobs']),
          'chain', len(chain), 'items', len(d['info']), 'undecoded', d['bad'])
    zb = [ci[link[p]][1:] for p in chain[:-1]]
    print('chain columns bounds', sorted(set(zb)))
    print('chain rows rhs', sorted(set(bnd[rows[p]] for p in chain)))
    head = rows[chain[0]]
    print('chain head', head, bnd[head], d['ent'][head])
    by = {}
    for j, c, p, q, a in d['info']:
        by.setdefault(j, []).append((p, q, a, c))
    energy = 0
    lens = []
    for j in sorted(by):
        it = sorted(by[j])
        durs = sorted(set(q - p for p, q, a, c in it))
        uses = sorted(set(a for p, q, a, c in it))
        energy += durs[0] * uses[0]
        lens.append(len(it))
        print('job %2d %-8s starts %3d..%3d n=%3d dur=%s use=%s cost=%s' % (
            j, rows[d['jobs'][j]], it[0][0], it[-1][0], len(it), durs, uses,
            sorted(set(d['cost'][c] for p, q, a, c in it))))
    print('energy', energy, 'capacity*chain', 25 * (len(chain) - 1))
    other = [r for i, r in enumerate(rows) if not d['part'][i] and i not in set(chain)]
    print('other rows', len(other))
    for r in other[:4]:
        print(' ', r, bnd[r], d['ent'][r][:6])


if __name__ == '__main__':
    main()
