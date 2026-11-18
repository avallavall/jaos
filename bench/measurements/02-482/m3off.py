# SPDX-License-Identifier: Apache-2.0
import sys, re, math
def res(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(\S+)\s+rows=', ln)
        if not m:
            continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        d[m.group(1)] = (m.group(2), int(kv['work']), int(kv['nodes']), kv['objective'])
    return d
ref = res(sys.argv[1])
runs = [(a.split('=', 1)[0], res(a.split('=', 1)[1])) for a in sys.argv[2:]]
print(f"{'name':12s} {'off work':>12s} {'nodes':>7s} " + " ".join(f"{n:>24s}" for n, _ in runs))
logs = {n: 0.0 for n, _ in runs}
tot = {n: 0 for n, _ in runs}
worst = {n: (0.0, '') for n, _ in runs}
bad = {n: [] for n, _ in runs}
rtot = 0
for k, (st, w, nd, ob) in sorted(ref.items()):
    rtot += w
    line = f"{k:12s} {w:12d} {nd:7d} "
    for n, r in runs:
        st2, w2, nd2, ob2 = r[k]
        q = w2 / w
        logs[n] += math.log(q)
        tot[n] += w2
        if q > worst[n][0]:
            worst[n] = (q, k)
        if st2 != st or ob2 != 'ok':
            bad[n].append(f"{k} {st2} objective={ob2}")
        line += f"{q:9.3f}x {nd2:8d} {ob2:>5s} "
    print(line)
for n, _ in runs:
    print(f"{n}: geo {math.exp(logs[n] / len(ref)):.3f}x total {tot[n] / rtot:.3f}x "
          f"worst {worst[n][0]:.3f}x {worst[n][1]}" + (f"; wrong: {'; '.join(bad[n])}" if bad[n] else ""))
