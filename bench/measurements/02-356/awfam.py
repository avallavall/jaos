import sys, collections
# awfam.py RELAX.mps OUT.mps KA KB KC KD KY : add KA a + KB b + KC c + KD d <= KY y to every bin of awhea's LP
src, dst = sys.argv[1], sys.argv[2]
ka, kb, kc, kd, ky = map(float, sys.argv[3:8])
extra = collections.defaultdict(list)
for i in range(475):
    y, a, b, c, d = (f'C{i + 1:04d}', f'C{476 + i:04d}', f'C{951 + i:04d}',
                     f'C{1426 + i:04d}', f'C{1901 + i:04d}')
    for col, k in ((a, ka), (b, kb), (c, kc), (d, kd), (y, -ky)):
        if k != 0.0:
            extra[col].append(f'    {col}  K{i:04d}  {k:.17g}')
lines = open(src).read().splitlines()
out = []
sec = None
prev = None
for ln in lines:
    if ln and not ln[0].isspace():
        if sec == 'ROWS':
            for i in range(475):
                out.append(f' L  K{i:04d}')
        if sec == 'COLUMNS' and prev is not None:
            out.extend(extra.pop(prev, []))
        sec = ln.split()[0]
        out.append(ln)
        continue
    if sec == 'COLUMNS':
        col = ln.split()[0]
        if prev is not None and col != prev:
            out.extend(extra.pop(prev, []))
        prev = col
    out.append(ln)
assert not extra, list(extra)[:3]
open(dst, 'w').write('\n'.join(out) + '\n')
