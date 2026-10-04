import re, sys
# picksum.py picks.txt : per factorization where ND was tried, ops_md / nnz(A) and ops_nd / ops_md
rows = []
last_a = {}
lines = open(sys.argv[1]).read().splitlines()
for i, ln in enumerate(lines):
    m = re.match(r'(\S+) CHOLPICK n (\d+) nd (\S+) md (\S+) (\w+)', ln)
    if not m:
        continue
    name, n, nd, md, pick = m.group(1), int(m.group(2)), float(m.group(3)), float(m.group(4)), m.group(5)
    a = None
    for ln2 in lines[i + 1:i + 3]:
        m2 = re.match(r'(\S+) CHOLLOG n (\d+) a (\d+) l (\d+) ops (\S+)', ln2)
        if m2 and m2.group(1) == name and int(m2.group(2)) == n:
            a = int(m2.group(3))
            break
    if nd == float('inf') or a is None:
        continue
    mdv = md if md != float('inf') else None
    rows.append((name, n, a, mdv, nd, pick))
seen = set()
for name, n, a, md, nd, pick in sorted(rows, key=lambda r: (r[3] or 1e300) / r[2]):
    if (name, n) in seen:
        continue
    seen.add((name, n))
    ratio = (md / a) if md else float('inf')
    rel = (nd / md) if md else 0.0
    print(f'{name:14s} n {n:8d} a {a:10d} md/a {ratio:10.2f} nd/md {rel:8.3f} {pick}')
