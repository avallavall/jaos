"""Energetic reasoning over the decoded jobs of a time-indexed scheduling
model, for 02-472.

For each interval [a, b) of chain positions, the energy the jobs must spend
inside it whatever their starts (each job's least overlap with it, times its
usage) against the capacity there. Prints the tightest intervals.

    python3 -I energy.py MPS [DEADLINE]

SPDX-License-Identifier: Apache-2.0
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import decode  # noqa: E402


def main():
    d = decode.decode(sys.argv[1])
    dl = int(sys.argv[2]) if len(sys.argv) > 2 else None
    jobs = {}
    for j, c, p, q, a in d['info']:
        if dl is not None and q > dl:
            continue
        jobs.setdefault(j, []).append((p, q, a))
    J = []
    for j, it in jobs.items():
        es = min(p for p, q, a in it)
        ls = max(p for p, q, a in it)
        dur = it[0][1] - it[0][0]
        J.append((es, ls, dur, it[0][2]))
    cap = d['cap']
    H = len(cap)
    pts = sorted(set([e for e, l, p, r in J] + [l + p for e, l, p, r in J] +
                     [l for e, l, p, r in J] + [e + p for e, l, p, r in J]))
    res = []
    for a in pts:
        for b in pts:
            if b <= a or b > H:
                continue
            need = 0.0
            for e, l, p, r in J:
                ov = min(p, b - a, e + p - a, b - l)
                if ov > 0:
                    need += r * ov
            have = sum(cap[a:b])
            res.append((need / have if have > 0 else 9, a, b, need, have))
    res.sort(reverse=True)
    for x in res[:12]:
        print('ratio %.4f [%d,%d) need %g have %g' % x)


if __name__ == '__main__':
    main()
