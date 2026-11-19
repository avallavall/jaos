# SPDX-License-Identifier: Apache-2.0
import sys, re, math
def load(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(\S.*?)\s+rows=', ln)
        if not m: continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        d[m.group(1)] = (m.group(2), int(kv['work']), int(kv['nodes']))
    return d
base_p = sys.argv[1]
base = load(base_p)
others = [(a.split('=',1)[0], load(a.split('=',1)[1])) for a in sys.argv[2:]]
print(f"{'name':12s} {'base':>12s} {'nodes':>7s}" + ''.join(f" {n:>16s}" for n, _ in others))
for k in base:
    row = f"{k:12s} {base[k][1]:12d} {base[k][2]:7d}"
    for n, d in others:
        if k in d:
            row += f"  {d[k][1]/base[k][1]:6.3f}x {d[k][2]:7d}"
        else:
            row += "  " + "-"*14
    print(row)
for n, d in others:
    ks = [k for k in base if k in d]
    g = math.exp(sum(math.log(d[k][1]/base[k][1]) for k in ks)/len(ks))
    t = sum(d[k][1] for k in ks)/sum(base[k][1] for k in ks)
    w = max(ks, key=lambda k: d[k][1]/base[k][1])
    b = min(ks, key=lambda k: d[k][1]/base[k][1])
    st = sum(1 for k in ks if not d[k][0].startswith('optimal'))
    print(f"{n}: geo {g:.4f}x total {t:.4f}x worst {d[w][1]/base[w][1]:.3f}x {w} best {d[b][1]/base[b][1]:.3f}x {b} notopt {st}")
