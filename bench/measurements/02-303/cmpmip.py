import math, re, sys
def load(p):
    out = {}
    for line in open(p):
        f = line.split()
        if len(f) < 3 or '=' not in line or line.startswith('#'): continue
        w = re.search(r' work=(\d+)', line); n = re.search(r' nodes=(\d+)', line); o = re.search(r' obj=(\S+)', line)
        if not w: continue
        out[f[0]] = (f[1], int(w.group(1)), int(n.group(1)) if n else -1, o.group(1) if o else '')
    return out
b, c = load(sys.argv[1]), load(sys.argv[2])
logs = []
rows = []
for k in b:
    if k not in c: continue
    sb, wb, nb, ob = b[k]; sc, wc, ncn, oc = c[k]
    r = wc / wb if wb else float('nan')
    if sb == 'optimal' and sc == 'optimal': logs.append(math.log(r))
    rows.append((r, k, sb, sc, nb, ncn, ob == oc))
rows.sort()
for r, k, sb, sc, nb, ncn, same in rows:
    print(f'{k:12s} {r:7.3f} {sb:>10s}->{sc:<10s} nodes {nb}->{ncn} {"" if same else "obj differs"}')
print(f'geomean over {len(logs)} optimal in both: {math.exp(sum(logs)/len(logs)):.4f}; past 2x: {sum(1 for x in logs if x > math.log(2))}')
