# Parity rows of one MPS file: equality rows over integer columns with an
# even coefficient, their odd columns binaries; the rank of the system mod 2
# and the binaries Gaussian elimination fixes.  parity.py FILE.mps
#
# SPDX-License-Identifier: Apache-2.0
import os, sys, collections
f = sys.argv[1]
sec = None
rtype, rhs = {}, collections.defaultdict(float)
cols = collections.OrderedDict()
ints, ub, lb = set(), {}, {}
intmark = False
for line in open(f):
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
            cols[t[0]][t[k]] = float(t[k + 1])
    elif sec == 'RHS':
        for k in range(1, len(t), 2):
            rhs[t[k]] = float(t[k + 1])
    elif sec == 'BOUNDS':
        if t[0] == 'UP':
            ub[t[2]] = float(t[3])
        elif t[0] == 'LO':
            lb[t[2]] = float(t[3])
        elif t[0] == 'BV':
            ub[t[2]] = 1.0; ints.add(t[2])
rows = collections.defaultdict(list)
for c, d in cols.items():
    for r, v in d.items():
        rows[r].append((c, v))
binary = {c for c in ints if lb.get(c, 0.0) == 0.0 and ub.get(c) == 1.0}
par = []
for r, ty in rtype.items():
    if ty != 'E':
        continue
    b = rhs.get(r, 0.0)
    if b != int(b):
        continue
    ok = True
    vs = []
    even = False
    for c, v in rows[r]:
        if c not in ints or v != int(v):
            ok = False; break
        if int(v) % 2 == 0:
            even = True
            continue
        if c not in binary:
            ok = False; break
        vs.append(c)
    if ok and vs and (even or os.environ.get('ALLROWS')):
        par.append((vs, int(b) % 2))
print('equality rows', sum(1 for t in rtype.values() if t == 'E'), 'parity rows', len(par))
var = sorted({c for vs, _ in par for c in vs})
idx = {c: i for i, c in enumerate(var)}
print('binaries in parity rows', len(var))
mat = []
for vs, b in par:
    m = 0
    for c in vs:
        m ^= 1 << idx[c]
    mat.append([m, b])
rank = 0
pivots = []
nv = len(var)
for col in range(nv):
    piv = None
    for r in range(rank, len(mat)):
        if mat[r][0] >> col & 1:
            piv = r; break
    if piv is None:
        continue
    mat[rank], mat[piv] = mat[piv], mat[rank]
    for r in range(len(mat)):
        if r != rank and mat[r][0] >> col & 1:
            mat[r][0] ^= mat[rank][0]; mat[r][1] ^= mat[rank][1]
    pivots.append(col)
    rank += 1
incons = sum(1 for m, b in mat[rank:] if m == 0 and b == 1)
fixed = sum(1 for r in range(rank) if bin(mat[r][0]).count('1') == 1)
print('rank', rank, 'nullity', nv - rank, 'inconsistent', incons, 'fixed by elimination', fixed)
if fixed == nv:
    print('ones in the unique solution', sum(mat[r][1] for r in range(rank)))
