import math, os, re, sys
def load(path):
    out = {}
    for line in open(path):
        if line.startswith('#') or '=' not in line: continue
        f = line.split()
        if len(f) < 3: continue
        g = lambda k: (lambda m: m.group(1) if m else None)(re.search(r' ' + k + r'=([-0-9.eE+]+)', line))
        out[f[0]] = dict(verdict=f[1], work=int(g('work')) if g('work') else None,
                         obj=float(g('obj')) if g('obj') else None, ref=float(g('ref')) if g('ref') else None,
                         rsub=float(g('rsub')) if g('rsub') else None, iters=int(g('iters')) if g('iters') else None)
    return out
base, sets = sys.argv[1], ['netlib', 'netlib-infeas', 'netlib-kennington']
for tree in sys.argv[2:]:
    logs, agree, total, worst, best, errs, moved, subs = [], 0, 0, (0, ''), (9, ''), [], 0, []
    for s in sets:
        b = load(f'{base}/bench/results/{s}.txt')
        p = os.path.expanduser(tree) + f'/bench/results/{s}.txt'
        if not os.path.exists(p): continue
        c = load(p)
        for n, x in b.items():
            if n not in c: continue
            y = c[n]; total += 1; agree += x['verdict'] == y['verdict']
            if x['work'] and y['work']:
                r = y['work'] / x['work']; logs.append(math.log(r)); moved += r != 1.0
                if r > worst[0]: worst = (r, n)
                if r < best[0]: best = (r, n)
            if y['obj'] is not None and y['ref'] is not None and y['verdict'] == 'optimal':
                errs.append((abs(y['obj'] - y['ref']) / max(1.0, abs(y['ref'])), n))
            if x['rsub'] is not None and y['rsub'] is not None:
                subs.append((max(y['rsub'], 1e-17) / max(x['rsub'], 1e-17), n, x['rsub'], y['rsub']))
    errs.sort(reverse=True); subs.sort(reverse=True)
    gm = math.exp(sum(logs) / len(logs)) if logs else float('nan')
    p87 = [e for e in errs if e[1] == 'pilot87']
    print(f"{os.path.basename(tree)}: work geomean {gm:.4f} over {len(logs)} ({moved} moved), verdicts {agree}/{total}, "
          f"worst {worst[0]:.3f} {worst[1]}, best {best[0]:.3f} {best[1]}, pilot87 rel obj error {p87[0][0] if p87 else 'n/a'}, "
          f"largest rel obj error {errs[0][0]:.3g} {errs[0][1]}, largest rsub rise {subs[0][0]:.3g}x {subs[0][1]} ({subs[0][2]:.3g} -> {subs[0][3]:.3g})")
