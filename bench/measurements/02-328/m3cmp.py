import sys, math, re
def load(f):
    d = {}
    for line in open(f):
        m = re.match(r'^(\S+)\s+(.*?)\s+rows=.*work=(\d+).*nodes=(\d+)', line)
        if m: d[m.group(1)] = (m.group(2), int(m.group(3)), int(m.group(4)))
    return d
a, b = load(sys.argv[1]), load(sys.argv[2])
lg = []; sa = sb = 0; worse = []
for k in a:
    if k not in b: continue
    (sta, wa, na), (stb, wb, nb) = a[k], b[k]
    r = wb / wa if wa else 1.0
    lg.append(math.log(r)); sa += wa; sb += wb
    flag = '' if sta == stb else f'  [{sta} -> {stb}]'
    if abs(r - 1) > 0.02 or flag: print(f'{k:12s} {r:7.3f}x  nodes {na} -> {nb}{flag}')
print(f'geomean {math.exp(sum(lg)/len(lg)):.4f}x  sum {sb/sa:.4f}x  over {len(lg)}')
