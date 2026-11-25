import re, sys

def load(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(.*?)\s+rows=', ln)
        if not m:
            continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        d[m[1]] = (m[2], kv)
    return d

def short(s, n=12):
    return s[:n]

a, b = load(sys.argv[1]), load(sys.argv[2])
print('# instance status_a status_b | nodes a b | incumbent a b | bound a b | ref')
for n in sorted(a):
    if n not in b:
        continue
    sa, ka = a[n]
    sb, kb = b[n]
    inc = lambda k: k.get('obj', k.get('inc', '-'))
    print(f'{n:<22} {short(sa, 10):<10} {short(sb, 10):<10} | {ka.get("nodes", "-"):>7} {kb.get("nodes", "-"):>7} | '
          f'{short(inc(ka)):>12} {short(inc(kb)):>12} | {short(ka.get("bound", "-")):>12} {short(kb.get("bound", "-")):>12} | '
          f'{ka.get("ref", "-").split("[")[0][:12]}')
