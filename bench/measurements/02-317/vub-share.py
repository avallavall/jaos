import glob
import gzip
import os
import sys


def rows_of(path):
    op = gzip.open if path.endswith('.gz') else open
    rtype, entries, bounds, integer = {}, {}, {}, set()
    section, intmark = None, False
    rhs = {}
    for raw in op(path, 'rt', errors='replace'):
        if not raw.strip() or raw.startswith('*'):
            continue
        if not raw[0].isspace():
            section = raw.split()[0].upper()
            continue
        f = raw.split()
        if section == 'ROWS':
            rtype[f[1]] = f[0].upper()
        elif section == 'COLUMNS':
            if len(f) >= 3 and f[1].upper() == "'MARKER'":
                intmark = "'INTORG'" in raw.upper()
                continue
            col = f[0]
            if intmark:
                integer.add(col)
            for k in range(1, len(f) - 1, 2):
                r, v = f[k], float(f[k + 1])
                if rtype.get(r) == 'N':
                    continue
                entries.setdefault(r, []).append((col, v))
        elif section == 'RHS':
            for k in range(1, len(f) - 1, 2):
                rhs[f[k]] = float(f[k + 1])
        elif section == 'BOUNDS':
            t, col = f[0].upper(), f[2]
            v = float(f[3]) if len(f) > 3 else 0.0
            lo, hi = bounds.get(col, (0.0, None))
            if t == 'UP':
                hi = v
            elif t == 'LO':
                lo = v
            elif t == 'BV':
                lo, hi = 0.0, 1.0
                integer.add(col)
            elif t == 'FX':
                lo = hi = v
            bounds[col] = (lo, hi)
    return rtype, entries, bounds, integer, rhs


def vub_share(path):
    rtype, entries, bounds, integer, rhs = rows_of(path)
    n = len(entries)
    vub = 0
    for r, e in entries.items():
        if len(e) != 2 or rhs.get(r, 0.0) != 0.0:
            continue
        t = rtype[r]
        (c1, a1), (c2, a2) = e
        for (x, ax), (y, ay) in (((c1, a1), (c2, a2)), ((c2, a2), (c1, a1))):
            if y not in integer or x in integer:
                continue
            lo, hi = bounds.get(y, (0.0, None))
            if hi is None and y in integer:
                hi = 1.0 if y not in bounds else hi
            if (lo, hi) != (0.0, 1.0):
                continue
            s = 1.0 if t == 'L' else -1.0 if t == 'G' else 0.0
            if s != 0.0 and s * ax > 0 and s * ay < 0:
                vub += 1
                break
    return vub, n


for d in sys.argv[1:]:
    for p in sorted(glob.glob(os.path.join(d, '*.mps*'))):
        v, n = vub_share(p)
        name = os.path.basename(p).split('.mps')[0]
        print(f'{name:28s} vub {v:6d} of {n:7d} rows  {100.0 * v / max(n, 1):6.1f}%')
