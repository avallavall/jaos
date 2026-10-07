import sys, re
# gapdiff.py BASE ARM : per-model primal/dual gap (m17sum rules) where they differ
def num(s):
    try:
        return float(s)
    except ValueError:
        return None
def load(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(.*?)\s+rows=', ln)
        if not m:
            continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        r = num(kv.get('ref', '').split('[')[0])
        if r is None:
            continue
        s = max(1.0, abs(r))
        if m.group(2).startswith('optimal'):
            d[m.group(1)] = (0.0, 0.0, 'OPT', '', kv.get('work'))
            continue
        i = num(kv.get('inc', 'none'))
        b = num(kv.get('bound', ''))
        p = 1.0 if i is None else min(1.0, abs(i - r) / s)
        q = 1.0 if b is None else min(1.0, abs(r - b) / s)
        d[m.group(1)] = (p, q, kv.get('inc', ''), kv.get('bound', ''), kv.get('nodes'))
    return d
a, b = load(sys.argv[1]), load(sys.argv[2])
for k in a:
    if k in b and a[k][:4] != b[k][:4]:
        print(f'{k:20s} p {a[k][0]:.4f}->{b[k][0]:.4f} d {a[k][1]:.4f}->{b[k][1]:.4f}  inc {a[k][2][:12]}->{b[k][2][:12]}  bnd {a[k][3][:12]}->{b[k][3][:12]}')
print('sum', round(sum(v[0] + v[1] for v in a.values()), 4), '->', round(sum(v[0] + v[1] for v in b.values()), 4))
