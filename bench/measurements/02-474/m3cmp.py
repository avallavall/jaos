# SPDX-License-Identifier: Apache-2.0
import sys, re, math
def base(p):
    d = {}
    for ln in open(p):
        if ln.startswith('#') or not ln.strip():
            continue
        t = ln.split()
        d[t[0]] = (int(t[8]), int(t[10]))
    return d
def res(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(.*?)\s+rows=', ln)
        if not m:
            continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        d[m.group(1)] = (m.group(2), int(kv.get('work', 0)), int(kv.get('nodes', 0)), kv.get('objective', '?'))
    return d
b = base(sys.argv[1])
runs = [(a.split('=', 1)[0], res(a.split('=', 1)[1])) for a in sys.argv[2:]]
print(f"{'name':12s} {'base work':>12s} {'nodes':>7s} " + " ".join(f"{n:>22s}" for n, _ in runs))
logs = {n: 0.0 for n, _ in runs}
tot = {n: 0 for n, _ in runs}
btot = 0
worst = {n: (0, '') for n, _ in runs}
cnt = 0
for k, (w, nd) in b.items():
    line = f"{k:12s} {w:12d} {nd:7d} "
    btot += w
    cnt += 1
    for n, r in runs:
        if k not in r:
            line += f"{'-':>22s} "
            continue
        st, w2, nd2, ob = r[k]
        ratio = w2 / w
        logs[n] += math.log(ratio)
        tot[n] += w2
        if ratio > worst[n][0]:
            worst[n] = (ratio, k)
        flag = '' if st.startswith('optimal') else '!' + st[:4]
        line += f"{ratio:7.3f}x {nd2:8d}{flag:>6s} "
    print(line)
for n, _ in runs:
    print(f"{n}: geo {math.exp(logs[n]/cnt):.3f}x total {tot[n]/btot:.3f}x worst {worst[n][0]:.2f}x {worst[n][1]}")
