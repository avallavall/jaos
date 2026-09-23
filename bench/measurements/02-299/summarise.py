import math, os, re, sys
def load(path):
    out = {}
    for line in open(path):
        if line.startswith('#') or '=' not in line: continue
        f = line.split()
        if len(f) < 3: continue
        name, verdict = f[0], f[1]
        w = re.search(r' work=(\d+)', line); o = re.search(r' obj=(\S+)', line); r = re.search(r' ref=([-0-9.eE+]+)', line)
        out[name] = (verdict, int(w.group(1)) if w else None, float(o.group(1)) if o else None, float(r.group(1)) if r else None)
    return out
base_dir, sets, vals = sys.argv[1], ['netlib', 'netlib-infeas', 'netlib-kennington'], sys.argv[2:]
for v in vals:
    logs, agree, total, worst, errs = [], 0, 0, (0, ''), []
    for s in sets:
        b = load(f'{base_dir}/bench/results/{s}.txt')
        c = load(os.path.expanduser(f'~/jaos-rf-{v}')+f'/bench/results/{s}.txt')
        for n, (vb, wb, ob, rb) in b.items():
            if n not in c: continue
            vc, wc, oc, rc = c[n]
            total += 1; agree += vc == vb
            if wb and wc:
                r = wc / wb; logs.append(math.log(r))
                if r > worst[0]: worst = (r, n)
            if oc is not None and rc is not None and vc == 'optimal':
                errs.append((abs(oc - rc) / max(1.0, abs(rc)), n))
    errs.sort(reverse=True)
    print(f'{v}: work geomean vs 64 {math.exp(sum(logs)/len(logs)):.4f} over {len(logs)}, verdicts agree {agree}/{total}, worst {worst[0]:.3f} {worst[1]}, largest rel obj error {errs[0][0]:.3g} {errs[0][1]}')
