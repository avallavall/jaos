#!/usr/bin/env python3
"""How much of DUAL_TOL 1e-9's cost is `can_move`'s half?

    attribute.py <variant record> <shipping record>

The variant holds `can_move`'s threshold at 1e-7 while every other reader of
`dual_tol` goes to 1e-9, so the difference between the two is that one site.
"""
import math, re, sys


def read(path):
    out = {}
    for line in open(path):
        m = re.match(r'^(\S+)\s+optimal\b', line)
        if not m:
            continue
        out[m.group(1)] = dict(
            work=int(re.search(r'\bwork=(\d+)', line).group(1)),
            iters=int(re.search(r'\biters=(\d+)', line).group(1)),
            obj=float(re.search(r'\bobj=(\S+)', line).group(1)),
            ref=float(re.search(r'\bref=([^\[]+)\[', line).group(1)),
            digest=re.search(r'\bdigest=(\S+)', line).group(1))
    return out


def main():
    v, s = read(sys.argv[1]), read(sys.argv[2])
    if not v or not s:
        print("  a record read zero instances; nothing below means anything")
        return 2
    names = sorted(set(v) & set(s))
    print(f"    {len(names)} instances compared "
          f"({len(v)} variant, {len(s)} shipping)")

    same = sum(1 for n in names if v[n]['digest'] == s[n]['digest'])
    print(f"    {same} of {len(names)} publish the same digest, so `can_move`'s "
          f"threshold changes {len(names) - same} answers")

    r = [v[n]['work'] / s[n]['work'] for n in names if s[n]['work'] > 0]
    gm = math.exp(sum(math.log(x) for x in r) / len(r))
    print(f"    work geometric mean, variant against shipping: {gm:.4f}x")
    print(f"    (below 1.000 means holding can_move at 1e-7 is CHEAPER, so the "
          f"tightening at that site costs)")

    worst = sorted(((v[n]['work'] / s[n]['work'], n) for n in names
                    if s[n]['work'] > 0), reverse=True)
    print(f"    worst {worst[0][0]:.3f}x ({worst[0][1]}), "
          f"best {worst[-1][0]:.3f}x ({worst[-1][1]})")

    print("    the four instances the tolerance was changed for:")
    for n in ('pilot', 'pilot87', 'scsd6', 'etamacro'):
        if n not in names:
            continue
        ev = abs(v[n]['obj'] - v[n]['ref'])
        es = abs(s[n]['obj'] - s[n]['ref'])
        print(f"      {n:<10} shipping gap {es:.4g}   variant gap {ev:.4g}   "
              f"work {v[n]['work'] / s[n]['work']:.4f}x")
    return 0


if __name__ == '__main__':
    sys.exit(main())
