import sys, re
def load(p):
    d = {}
    for ln in open(p):
        f = ln.split()
        if not f: continue
        kv = dict(x.split('=', 1) for x in f[2:] if '=' in x)
        d[f[0]] = (f[1], kv)
    return d
def num(s):
    try: return float(s)
    except: return None
def gap(st, kv):
    inc, bd = num(kv.get('inc', '')), num(kv.get('bound', ''))
    if st == 'optimal': return 0.0
    if inc is None or bd is None: return 1.0
    g = abs(inc - bd) / max(abs(inc), abs(bd), 1.0)
    return min(g, 1.0)
a, b = load(sys.argv[1]), load(sys.argv[2])
sa = sb = 0.0; na = nb = 0; ia = ib = 0; wa = wb = 0
diff = []
for k in sorted(a):
    if k not in b: continue
    (s1, k1), (s2, k2) = a[k], b[k]
    g1, g2 = gap(s1, k1), gap(s2, k2)
    sa += g1; sb += g2
    na += s1 == 'optimal'; nb += s2 == 'optimal'
    ia += num(k1.get('inc', '')) is not None; ib += num(k2.get('inc', '')) is not None
    if (s1, k1.get('inc'), k1.get('bound')) != (s2, k2.get('inc'), k2.get('bound')):
        diff.append((k, s1, k1.get('inc'), k1.get('bound'), g1, s2, k2.get('inc'), k2.get('bound'), g2))
print(f"optimal {na} -> {nb}; incumbents {ia} -> {ib}; gap sum {sa:.4f} -> {sb:.4f} ({sb/sa if sa else 0:.3f}x)")
for d in diff:
    print("%-24s %-10s inc=%-14s bd=%-14s g=%.3g | %-10s inc=%-14s bd=%-14s g=%.3g" % tuple(str(x)[:14] if isinstance(x,str) else x for x in d))
