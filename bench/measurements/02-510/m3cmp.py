import math, re, sys

def load(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(\S+)\s+rows=', ln)
        if not m:
            continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        d[m[1]] = (m[2], int(kv.get('work', '0')), int(kv.get('nodes', '0')))
    return d

base = load(sys.argv[1])
for p in sys.argv[2:]:
    d = load(p)
    logs, tb, ta, moved = [], 0, 0, []
    for n in sorted(base):
        if n not in d:
            continue
        sb, wb, nb = base[n]
        sa, wa, na = d[n]
        if wb > 0 and wa > 0 and sb == 'optimal' and sa == 'optimal':
            logs.append(math.log(wa / wb))
            tb += wb
            ta += wa
        if (wa, na, sa) != (wb, nb, sb):
            moved.append(f'{n} {sb}->{sa} nodes {nb}->{na} work {wa / wb if wb else 0:.3f}x')
    print(f'== {p}: {len(moved)} moved, work geometric {math.exp(sum(logs) / len(logs)):.4f}x, total {ta / tb:.4f}x over {len(logs)}')
    for x in moved:
        print('  ' + x)
