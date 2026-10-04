import sys, collections

src, dst = sys.argv[1], sys.argv[2]
sec = None
rtype = collections.OrderedDict()
rhs = collections.defaultdict(float)
cols = collections.OrderedDict()
ints = set()
intmark = False
lo, up = {}, {}
for line in open(src):
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
            intmark = "'INTORG'" in t
            continue
        cols.setdefault(t[0], {})
        if intmark:
            ints.add(t[0])
        for k in range(1, len(t), 2):
            cols[t[0]][t[k]] = cols[t[0]].get(t[k], 0.0) + float(t[k + 1])
    elif sec == 'RHS':
        for k in range(1, len(t), 2):
            rhs[t[k]] = float(t[k + 1])
    elif sec == 'BOUNDS':
        b, c = t[0], t[2]
        v = float(t[3]) if len(t) > 3 else None
        if b == 'UP': up[c] = v
        elif b == 'LO': lo[c] = v
        elif b == 'FX': lo[c] = up[c] = v
        elif b == 'BV': lo[c] = 0.0; up[c] = 1.0; ints.add(c)
        elif b == 'MI': lo[c] = -float('inf')
        elif b == 'PL': up[c] = float('inf')
obj = [r for r, ty in rtype.items() if ty == 'N'][0]

def rows_of():
    rr = collections.defaultdict(dict)
    for c, d in cols.items():
        for r, v in d.items():
            rr[r][c] = v
    return rr

removed_rows = set()
contracted = 0
changed = True
while changed:
    changed = False
    rr = rows_of()
    for r, ty in rtype.items():
        if ty != 'E' or r in removed_rows:
            continue
        ent = rr.get(r, {})
        if len(ent) != 2 or rhs.get(r, 0.0) != 0.0:
            continue
        (a, va), (b, vb) = sorted(ent.items())
        if a in ints or b in ints or va != -vb or abs(va) != 1.0:
            continue
        keep, drop = a, b
        for rr2, v in cols[drop].items():
            if rr2 == r:
                continue
            cols[keep][rr2] = cols[keep].get(rr2, 0.0) + v
        del cols[keep][r]
        lo[keep] = max(lo.get(keep, 0.0), lo.get(drop, 0.0))
        up[keep] = min(up.get(keep, float('inf')), up.get(drop, float('inf')))
        del cols[drop]
        removed_rows.add(r)
        contracted += 1
        changed = True
        break
print('contracted', contracted, 'rows', len(rtype) - 1 - len(removed_rows), 'cols', len(cols))
with open(dst, 'w') as f:
    f.write('NAME contracted\nROWS\n')
    for r, ty in rtype.items():
        if r not in removed_rows:
            f.write(' %s %s\n' % (ty, r))
    f.write('COLUMNS\n')
    inint = False
    for c, d in cols.items():
        if (c in ints) != inint:
            f.write("    M%d 'MARKER' '%s'\n" % (contracted, 'INTORG' if c in ints else 'INTEND'))
            inint = c in ints
        for r, v in d.items():
            if r in removed_rows or v == 0.0:
                continue
            f.write('    %s %s %.17g\n' % (c, r, v))
    if inint:
        f.write("    MEND 'MARKER' 'INTEND'\n")
    f.write('RHS\n')
    for r, v in rhs.items():
        if r not in removed_rows and v != 0.0:
            f.write('    RHS %s %.17g\n' % (r, v))
    f.write('BOUNDS\n')
    for c in cols:
        l, u = lo.get(c, 0.0), up.get(c, float('inf'))
        if l != 0.0:
            f.write(' LO BND %s %.17g\n' % (c, l))
        if u != float('inf'):
            f.write(' UP BND %s %.17g\n' % (c, u))
    f.write('ENDATA\n')
