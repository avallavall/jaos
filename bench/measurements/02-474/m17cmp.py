# SPDX-License-Identifier: Apache-2.0
import sys, re
def load(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(.*?)\s+rows=', ln)
        if not m: continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        kv['ref'] = kv.get('ref', '').split('[')[0]
        d[m.group(1)] = (m.group(2), kv)
    return d
def num(s):
    try: return float(s)
    except: return None
def score(st, kv):
    r = num(kv['ref']); s = max(1.0, abs(r))
    if st.startswith('optimal'): return 0.0, 0.0
    i = num(kv.get('inc', 'none')); b = num(kv.get('bound', ''))
    pg = 1.0 if i is None else min(1.0, abs(i - r) / s)
    dg = 1.0 if b is None else min(1.0, abs(r - b) / s)
    return pg, dg
runs = [(a.split('=', 1)[0], load(a.split('=', 1)[1])) for a in sys.argv[1:]]
base = runs[0][1]
names = [k for k in base]
tot = {n: [0.0, 0.0] for n, _ in runs}
print(f"{'name':22s} " + " ".join(f"{n:>26s}" for n, _ in runs))
for k in names:
    if not all(k in r for _, r in runs):
        continue
    line = f"{k:22s} "
    for n, r in runs:
        st, kv = r[k]
        pg, dg = score(st, kv)
        tot[n][0] += pg; tot[n][1] += dg
        tag = 'OPT' if st.startswith('optimal') else ''
        line += f"{pg:8.4f} {dg:8.4f} {tag:3s} {int(kv.get('nodes', 0)):6d}"
    print(line)
for n, _ in runs:
    print(f"{n:10s} primal {tot[n][0]:.4f} dual {tot[n][1]:.4f} sum {tot[n][0] + tot[n][1]:.4f}")
