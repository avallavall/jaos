#!/usr/bin/env python3
"""Which instances change verdict, digest or work across the sweep.

    compare-sweep.py <dir> <setting> [<setting> ...]

Reads record-netlib-<setting>.txt written by run-refactor-sweep.sh. Three
questions, in the order they matter:

  1. Does any instance's OBJECTIVE or CHECKER verdict differ between settings?
     That is a defect: the interval changes the trajectory and must not change
     whether the answer is right.
  2. How far does the objective itself move? A verdict that holds while the
     value walks says how much margin the window has.
  3. How much does the work move? That is the sweep's own canary — if the
     settings all read the same, they are one binary measured N times.
"""
import os, re, sys


def read(path):
    out = {}
    for line in open(path):
        m = re.match(r'^(\S+)\s+optimal\b', line)
        if not m:
            continue
        n = m.group(1)
        out[n] = dict(
            obj=float(re.search(r'\bobj=(\S+)', line).group(1)),
            ref=float(re.search(r'\bref=([^\[]+)\[', line).group(1)),
            objv=re.search(r'\bobjective=(\S+)', line).group(1),
            chk=re.search(r'\bchecker=(\S+)', line).group(1),
            det=re.search(r'\bdet=(\S+)', line).group(1),
            work=int(re.search(r'\bwork=(\d+)', line).group(1)),
            iters=int(re.search(r'\biters=(\d+)', line).group(1)),
            digest=re.search(r'\bdigest=(\S+)', line).group(1))
    return out


def main():
    d, settings = sys.argv[1], sys.argv[2:]
    rec, missing = {}, []
    for s in settings:
        p = os.path.join(d, f'record-netlib-{s}.txt')
        if not os.path.exists(p):
            missing.append(s); continue
        rec[s] = read(p)
    if missing:
        print(f"  settings with no record: {missing}")
    if len(rec) < 2:
        print("  fewer than two settings read; nothing to compare")
        return 2

    names = sorted(set().union(*(set(v) for v in rec.values())))
    print(f"  {len(names)} instances over {len(rec)} settings: {sorted(rec)}")

    # 1. verdicts
    flips = []
    for n in names:
        vs = {s: rec[s][n] for s in rec if n in rec[s]}
        if len(vs) < len(rec):
            flips.append((n, 'absent from some setting'))
            continue
        for key in ('objv', 'chk', 'det'):
            got = {v[key] for v in vs.values()}
            if len(got) > 1:
                flips.append((n, f"{key}: " + ", ".join(
                    f"{s}={vs[s][key]}" for s in sorted(vs, key=int))))
    print()
    if flips:
        print(f"  *** {len(flips)} VERDICT DIFFERENCES ***")
        for n, why in flips:
            print(f"    {n:<12} {why}")
    else:
        print("  no instance changes objective, checker or determinism verdict "
              "at any setting")

    # 2. how far the objective walks while the verdict holds
    print()
    print("  the ten instances whose objective moves most across the sweep")
    walk = []
    for n in names:
        vs = [rec[s][n] for s in rec if n in rec[s]]
        if len(vs) < 2:
            continue
        objs = [v['obj'] for v in vs]
        ref = vs[0]['ref']
        span = max(objs) - min(objs)
        scale = max(abs(ref), 1.0)
        walk.append((span / scale, n, span, min(objs), max(objs)))
    walk.sort(reverse=True)
    for rel, n, span, lo, hi in walk[:10]:
        print(f"    {n:<12} relative span {rel:.4g}   {lo:.17g} .. {hi:.17g}")
    print(f"  the gate's own window is 1e-6 relative, so the worst above uses "
          f"{walk[0][0] / 1e-6:.4g} of it")

    # 3. the canary
    print()
    same = 0
    for n in names:
        works = {rec[s][n]['work'] for s in rec if n in rec[s]}
        same += len(works) == 1
    print(f"  {same} of {len(names)} instances report identical work at every "
          f"setting")
    print("  (a sweep where that is all of them has measured one binary N "
          "times — D82)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
