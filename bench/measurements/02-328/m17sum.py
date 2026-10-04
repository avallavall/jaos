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
for a in sys.argv[1:]:
    n, p = a.split('=', 1)
    d = load(p)
    solved = sum(1 for st, kv in d.values() if st.startswith('optimal'))
    pg = dg = 0.0; inc = 0
    for st, kv in d.values():
        r = num(kv['ref']); s = max(1.0, abs(r))
        if st.startswith('optimal'): inc += 1; continue
        i = num(kv.get('inc', 'none')); b = num(kv.get('bound', ''))
        if i is None: pg += 1
        else: inc += 1; pg += min(1.0, abs(i - r) / s)
        dg += 1.0 if b is None else min(1.0, abs(r - b) / s)
    print(f"{n:10s} solved {solved} incumbents {inc} primal {pg:.4f} dual {dg:.4f} sum {pg+dg:.4f}")
