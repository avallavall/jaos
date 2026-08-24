#!/usr/bin/env python3
"""What the warm repair knows before the solve, against what the solve cost.

    warm-probe.py <diag-stderr> <warm-stdout>

The verdict column comes from THIS run's own work ratios, never from a table
in another measurement directory. 02-60's per-instance detail was taken at
D151 and 24 commits to `src/` have landed since; two of its four WORSE rows
no longer reproduce.
"""
import re, sys


def read_diag(path):
    cur, rows = None, {}
    for line in open(path):
        m = re.match(r'DIAG-INSTANCE (\S+)', line)
        if m:
            cur = m.group(1)
            rows[cur] = {'name': cur, 'guards': []}
            continue
        m = re.match(r'DIAG-REPAIR (.*)', line)
        if m and cur:
            for k, v in re.findall(r'(\w+)=(-?\d+)', m.group(1)):
                rows[cur][k] = int(v)
            continue
        m = re.match(r'DIAG-GUARD warm=(\d) violation=(\S+) iters=(\d+)', line)
        if m and cur:
            rows[cur]['guards'].append(
                (int(m.group(1)), float(m.group(2)), int(m.group(3))))
    return rows


def read_verdicts(path):
    out = {}
    for line in open(path):
        m = re.match(r'^(\S+)\s+ok\s+.*warm=(\d+)/(\d+) cold=(\d+)/(\d+)', line)
        if m:
            n, iw, ww, ic, wc = m.group(1), *map(int, m.groups()[1:])
            out[n] = dict(iters_w=iw, work_w=ww, iters_c=ic, work_c=wc,
                          ratio=ww / wc if wc else float('inf'))
    return out


def main():
    diag = read_diag(sys.argv[1])
    verd = read_verdicts(sys.argv[2])

    rows = []
    for n, d in diag.items():
        v = verd.get(n)
        if v is None or 'short' not in d:
            continue
        wg = [g for g in d['guards'] if g[0] == 1]
        d['viol'] = wg[0][1] if wg else float('nan')
        d['guard'] = bool(wg) and wg[0][1] != 0.0
        d.update(v)
        d['loses'] = v['ratio'] > 1.0
        rows.append(d)
    rows.sort(key=lambda r: -r['ratio'])

    print("== what the repair did, and what the solve then cost ==")
    print()
    print("guard=YES means the settled warm point was not dual feasible, so the")
    print("trajectory was thrown away and the solve restarted cold. That is the")
    print("only row where warm iterations equal cold by construction.")
    print()
    h = (f"{'instance':<10}{'nrow':>6}{'ncol':>6}{'S':>3}{'uncov':>6}"
         f"{'byUnc':>6}{'byOrd':>6}{'wantcol':>8}{'wantlog':>8}"
         f"{'warm-viol':>11}{'guard':>6}{'itersW':>8}{'itersC':>8}{'work x':>9}")
    print(h)
    print('-' * len(h))
    for r in rows:
        print(f"{r['name']:<10}{r['nrow']:>6}{r['ncol']:>6}{r['short']:>3}"
              f"{r['uncovered_rows']:>6}{r['by_uncovered']:>6}{r['by_order']:>6}"
              f"{r['wantcol']:>8}{r['wantlog']:>8}{r['viol']:>11.4g}"
              f"{'YES' if r['guard'] else 'no':>6}"
              f"{r['iters_w']:>8}{r['iters_c']:>8}{r['ratio']:>9.4f}")

    losers = [r for r in rows if r['loses']]
    winners = [r for r in rows if not r['loses']]
    print()
    print(f"{len(losers)} cost more work warm than cold: "
          f"{sorted(r['name'] for r in losers)}")
    print(f"{len(winners)} cost less: {len(winners)} instances")

    print()
    print("== does anything the repair knows BEFORE the solve separate them? ==")
    print()
    print("A column separates only if no winner's value falls inside the range")
    print("the losers span. A range that is merely sparse is not a predictor:")
    print("a rule read off two instances out of twenty is D46's warning.")
    print()
    keys = ('short', 'uncovered_rows', 'by_uncovered', 'by_order',
            'nrow', 'ncol', 'wantcol', 'wantlog')
    for k in keys:
        d = sorted(r[k] for r in losers)
        lo, hi = min(d), max(d)
        inside = sorted(r['name'] for r in winners if lo <= r[k] <= hi)
        verdict = "SEPARATES" if not inside else f"{len(inside)} winners inside"
        print(f"  {k:<16} losers {d}   {verdict}")
    print()
    for name, f in (('S/nrow', lambda r: r['short'] / r['nrow']),
                    ('wantlog/nrow', lambda r: r['wantlog'] / r['nrow']),
                    ('ncol/nrow', lambda r: r['ncol'] / r['nrow'])):
        d = sorted(f(r) for r in losers)
        lo, hi = min(d), max(d)
        inside = sorted(r['name'] for r in winners if lo <= f(r) <= hi)
        verdict = "SEPARATES" if not inside else f"{len(inside)} winners inside"
        print(f"  {name:<16} losers {[round(x, 4) for x in d]}   {verdict}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
