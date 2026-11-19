import sys, re, math
d = sys.argv[1]


def res(p):
    r = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(\S+)\s+rows=', ln)
        if not m:
            continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        r[m.group(1)] = int(kv['work'])
    return r


ks = [0, 100, 200, 300]
B = {k: res(f"{d}/m3n-base-{k}.txt") for k in ks}
C = {k: res(f"{d}/m3n-land9-{k}.txt") for k in ks}
names = sorted(B[0])
print(f"{'k':>4} {'geo':>7} {'total':>7}  base total  change total")
for k in ks:
    g = math.exp(sum(math.log(C[k][n] / B[k][n]) for n in names) / len(names))
    tb = sum(B[k].values()); tc = sum(C[k].values())
    print(f"{k:>4} {g:7.3f} {tc / tb:7.3f}  {tb:11d} {tc:12d}")
gm = {n: math.exp(sum(math.log(B[k][n]) for k in ks) / len(ks)) for n in names}
cm = {n: math.exp(sum(math.log(C[k][n]) for k in ks) / len(ks)) for n in names}
g = math.exp(sum(math.log(cm[n] / gm[n]) for n in names) / len(names))
tb = sum(sum(B[k].values()) for k in ks); tc = sum(sum(C[k].values()) for k in ks)
print(f"over the four: geometric of each model's mean {g:.3f}, total {tc / tb:.3f}")
for n in names:
    q = cm[n] / gm[n]
    lo_b = min(B[k][n] for k in ks); hi_b = max(B[k][n] for k in ks)
    lo_c = min(C[k][n] for k in ks); hi_c = max(C[k][n] for k in ks)
    if abs(math.log(q)) > 0.02 or hi_b > 1.05 * lo_b:
        print(f"{n:10s} {q:6.3f}x  base {lo_b}-{hi_b}  change {lo_c}-{hi_c}")
