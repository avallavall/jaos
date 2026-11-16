"""Row classes and structures of every instance in a MIP set.

Reads each MPS file of a manifest, classifies each row in the manner of
MIPLIB 2017's constraint classification, and reports the structures a
dedicated heuristic could aim at:
- cells: binaries in exactly two partitioning rows;
- items: binaries in exactly one partitioning row;
- flow: equality rows of unit coefficients over two or more continuous
  columns;
- bigm: rows whose binary coefficient is at least 1e3 times the median of
  the others;
- periodic: the most rows of one period T, two unit potentials of opposite
  sign and one general integer column with coefficient T.
With a results file of bench/run it adds what JAOS reached on each
instance.

    python3 -I census.py MANIFEST DIR [RESULTS] > census.txt

SPDX-License-Identifier: Apache-2.0
"""
import gzip
import math
import os
import sys

INF = float('inf')


def read_mps(path):
    opener = gzip.open if path.endswith('.gz') else open
    sense, rows, cols, order, rhs, rng = {}, [], {}, [], {}, {}
    lo, up, integer = {}, {}, set()
    obj = None
    sec = None
    intmark = False
    with opener(path, 'rt') as f:
        for line in f:
            if not line.strip() or line.startswith('*'):
                continue
            if not line[0].isspace():
                sec = line.split()[0]
                continue
            t = line.split()
            if sec == 'ROWS':
                sense[t[1]] = t[0]
                if t[0] == 'N':
                    if obj is None:
                        obj = t[1]
                else:
                    rows.append(t[1])
            elif sec == 'COLUMNS':
                if len(t) >= 3 and t[1] == "'MARKER'":
                    intmark = t[2] == "'INTORG'"
                    continue
                c = t[0]
                if c not in cols:
                    cols[c] = {}
                    order.append(c)
                    if intmark:
                        integer.add(c)
                for k in range(1, len(t) - 1, 2):
                    cols[c][t[k]] = float(t[k + 1])
            elif sec == 'RHS':
                for k in range(1, len(t) - 1, 2):
                    if t[k] in sense:
                        rhs[t[k]] = float(t[k + 1])
            elif sec == 'RANGES':
                for k in range(1, len(t) - 1, 2):
                    rng[t[k]] = float(t[k + 1])
            elif sec == 'BOUNDS':
                kind, c = t[0], t[2]
                v = float(t[3]) if len(t) > 3 else 0.0
                if kind == 'UP':
                    up[c] = v
                elif kind == 'LO':
                    lo[c] = v
                elif kind == 'FX':
                    lo[c] = up[c] = v
                elif kind == 'FR':
                    lo[c], up[c] = -INF, INF
                elif kind == 'MI':
                    lo[c] = -INF
                elif kind == 'PL':
                    up[c] = INF
                elif kind == 'BV':
                    lo[c], up[c] = 0.0, 1.0
                    integer.add(c)
                elif kind == 'LI':
                    lo[c] = v
                    integer.add(c)
                elif kind == 'UI':
                    up[c] = v
                    integer.add(c)
    for c in integer:
        if c not in up:
            up[c] = INF
    row_entries = {r: [] for r in rows}
    for c in order:
        for r, a in cols[c].items():
            if r in row_entries:
                row_entries[r].append((c, a))
    bounds = {}
    for r in rows:
        b = rhs.get(r, 0.0)
        s = sense[r]
        if s == 'E':
            lw, hi = b, b
            if r in rng:
                if rng[r] > 0:
                    hi = b + abs(rng[r])
                else:
                    lw = b - abs(rng[r])
        elif s == 'L':
            lw, hi = -INF, b
            if r in rng:
                lw = b - abs(rng[r])
        else:
            lw, hi = b, INF
            if r in rng:
                hi = b + abs(rng[r])
        bounds[r] = (lw, hi)
    colinfo = {}
    for c in order:
        l = lo.get(c, 0.0)
        u = up.get(c, INF)
        colinfo[c] = (c in integer, l, u)
    return rows, row_entries, bounds, colinfo, cols, obj


def is_bin(info):
    i, l, u = info
    return i and l == 0.0 and u == 1.0


def classify(ent, lw, hi, colinfo):
    n = len(ent)
    if n == 0:
        return 'empty'
    if lw == -INF and hi == INF:
        return 'free'
    if n == 1:
        return 'singleton'
    kinds = [colinfo[c] for c, _ in ent]
    nbin = sum(1 for k in kinds if is_bin(k))
    nint = sum(1 for k in kinds if k[0])
    if n == 2:
        if lw == hi:
            return 'aggregation'
        if nbin == 1 and nint < 2:
            return 'varbound'
        if nbin == 1 and nint == 2:
            return 'varbound'
        if ent[0][1] * ent[1][1] < 0 and (nint == 0 or nint == 2):
            return 'precedence'
    if nbin == n:
        a = [v for _, v in ent]
        if all(v == 1.0 for v in a) or all(v == -1.0 for v in a):
            s = 1.0 if a[0] == 1.0 else -1.0
            l, h = (lw, hi) if s > 0 else (-hi, -lw)
            if l == 1.0 and h == 1.0:
                return 'setpart'
            if h == 1.0 and l <= 0.0:
                return 'setpack'
            if l == 1.0 and h == INF:
                return 'setcover'
            if l == h:
                return 'cardinality'
            return 'invknap'
        if all(float(v).is_integer() for v in a):
            return 'eqknap' if lw == hi else 'knapsack'
        return 'binary'
    if nint == n:
        return 'intknap' if lw != hi else 'inteq'
    if nint > 0 and nint == nbin:
        return 'mixedbin'
    if nint > 0:
        return 'mixed'
    return 'continuous'


def structures(rows, row_entries, bounds, colinfo, cols):
    part = set()
    flow = 0
    bigm = 0
    for r in rows:
        ent = row_entries[r]
        lw, hi = bounds[r]
        cl = classify(ent, lw, hi, colinfo)
        if cl == 'setpart':
            part.add(r)
        if lw == hi and len(ent) >= 3 and all(abs(a) == 1.0 for _, a in ent):
            if sum(1 for c, _ in ent if not colinfo[c][0]) >= 2:
                flow += 1
        bins = [abs(a) for c, a in ent if is_bin(colinfo[c])]
        rest = [abs(a) for c, a in ent if not is_bin(colinfo[c])]
        if bins and rest:
            med = sorted(rest)[len(rest) // 2]
            if med > 0 and max(bins) >= 1e3 * med:
                bigm += 1
    cells = 0
    items = 0
    nbin = 0
    for c, info in colinfo.items():
        if not is_bin(info):
            continue
        nbin += 1
        k = sum(1 for r in cols[c] if r in part)
        if k == 2:
            cells += 1
        elif k == 1:
            items += 1
    periods = {}
    for r in rows:
        ent = row_entries[r]
        if len(ent) != 3:
            continue
        unit = [(c, a) for c, a in ent if abs(a) == 1.0 and not is_bin(colinfo[c])]
        big = [(c, a) for c, a in ent if abs(a) >= 2.0 and colinfo[c][0]
               and not is_bin(colinfo[c]) and float(a).is_integer()]
        if len(unit) == 2 and len(big) == 1 and unit[0][1] * unit[1][1] < 0:
            t = abs(big[0][1])
            periods[t] = periods.get(t, 0) + 1
    periodic = max(periods.values()) if periods else 0
    return part, cells, items, nbin, flow, bigm, periodic


def results(path):
    out = {}
    if path is None or not os.path.exists(path):
        return out
    for line in open(path):
        if line.startswith('#') or ' rows=' not in line:
            continue
        name = line.split()[0]
        f = dict(t.split('=', 1) for t in line.split() if '=' in t
                 and not t.startswith('['))
        st = 'optimal' if ' optimal ' in line else 'open'
        out[name] = (st, f.get('nodes'), f.get('inc', f.get('obj')),
                     f.get('bound'), f.get('ref'), f.get('first'))
    return out


def main():
    man, d = sys.argv[1], sys.argv[2]
    res = results(sys.argv[3] if len(sys.argv) > 3 else None)
    names = []
    for line in open(man):
        if line.startswith('#') or not line.strip():
            continue
        names.append(line.split()[0])
    order = ['setpart', 'setpack', 'setcover', 'cardinality', 'invknap',
             'eqknap', 'knapsack', 'binary', 'intknap', 'inteq', 'mixedbin',
             'mixed', 'continuous', 'varbound', 'precedence', 'aggregation',
             'singleton', 'free', 'empty']
    for name in names:
        path = None
        for ext in ('.mps', '.mps.gz'):
            p = os.path.join(d, name + ext)
            if os.path.exists(p):
                path = p
                break
        if path is None:
            print(f'{name:20s} missing')
            continue
        rows, ent, bnd, info, cols, obj = read_mps(path)
        count = {}
        for r in rows:
            cl = classify(ent[r], *bnd[r], info)
            count[cl] = count.get(cl, 0) + 1
        part, cells, items, nbin, flow, bigm, periodic = structures(
            rows, ent, bnd, info, cols)
        nint = sum(1 for v in info.values() if v[0])
        ncont = len(info) - nint
        mix = ' '.join(f'{k}={count[k]}' for k in order if k in count)
        r = res.get(name)
        st = ''
        if r is not None:
            st = f' | {r[0]} nodes={r[1]} inc={r[2]} bound={r[3]} ref={r[4]}'
            if r[5] is not None:
                st += f' first={r[5]}'
        print(f'{name:20s} rows={len(rows)} cols={len(info)} bin={nbin} '
              f'gint={nint - nbin} cont={ncont} cells={cells} items={items} '
              f'flow={flow} bigm={bigm} periodic={periodic} | {mix}{st}')


main()
