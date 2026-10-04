import sys, re
def load(p):
    d = {}
    for ln in open(p):
        if not ln.strip() or ln[0].isspace(): continue
        f = ln.split()
        m = re.match(r'^(\S+)\s+(.*?)\s+rows=', ln)
        if not m: continue
        kv = dict(x.split('=', 1) for x in f if '=' in x)
        d[m.group(1)] = (m.group(2), kv)
    return d
arms = [(a.split('=')[0], load(a.split('=')[1])) for a in sys.argv[1:]]
names = list(arms[0][1].keys())
def g(s):
    try: return float(s)
    except: return None
print(f"{'instance':22s}" + ''.join(f"| {n:>32s} " for n, _ in arms))
for k in names:
    row = f"{k:22s}"
    for n, d in arms:
        if k not in d: row += f"| {'-':>32s} "; continue
        st, kv = d[k]
        tag = 'OPT ' if st.startswith('optimal') else ''
        inc = kv.get('inc', kv.get('obj', ''))
        row += f"| {tag}{inc[:12]:>12s} / {kv.get('bound','')[:12]:>12s} "
    print(row)
for n, d in arms:
    print(n, 'solved', sum(1 for st, kv in d.values() if st.startswith('optimal')))
