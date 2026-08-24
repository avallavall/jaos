#!/usr/bin/env python3
"""What RSUB_FLOOR costs, and where the knee is.

Three questions, all answered from records already committed:

  1. How many of the 110 solves does the predicate watch at each floor?
  2. How far does `rsub` move under a change that is legitimate? D171 is the
     sample: it moved 88 of 94 digests, and 02-81/gate-diff.txt kept the
     before/after pair for every instance it touched.
  3. Do the two alternatives separate better than `rsub` does? One is
     `gap_positive` on its own, the other is `gap_positive` divided by the
     objective's own arithmetic floor, `eps * sum |c_j x_j|`. The traffic
     comes from 02-83, which measured it exactly.

Reads only committed files. Writes nothing.
"""
import re, sys

EPS = 2.0 ** -52
BANDS = [0, 1e-18, 1e-17, 1e-16, 1e-15, 1e-14, 1e-13, 1e-12,
         1e-11, 1e-10, 1e-9, 1e-8, 1e-6, 1e-4, 1.0]
FLOORS = (1e-9, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18)
FACTOR = 2.0


def read_record(path):
    """name -> rsub, from a bench/results/*.txt line."""
    out = {}
    for line in open(path):
        m = re.match(r'^(\S+)\s+optimal\b', line)
        if not m:
            continue
        r = re.search(r'\brsub=([^)]+)\)', line)
        if r:
            out[m.group(1)] = float(r.group(1))
    return out


def read_exact(path):
    """name -> (gappos, objtraf, refeps, cert), from 02-83."""
    out = {}
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        f = line.split()
        if len(f) < 16:
            continue
        out[f[0]] = (float(f[-4]), float(f[-6]), float(f[-5]), f[-2], f[-1])
    return out


def read_pairs(path):
    """(name, old, new) for every RSUB line in 02-81's captured diff."""
    out = []
    for line in open(path):
        m = re.match(r'\s+RSUB\s+(\S+)\s+(\S+)\s*->\s*(\S+)', line)
        if m:
            try:
                out.append((m.group(1), float(m.group(2)), float(m.group(3))))
            except ValueError:
                pass
    return out


def q1_coverage(records):
    print("== 1. what the floor watches ==")
    print()
    print("rsub, one line per decade, one column per set")
    names = list(records)
    print("  band".ljust(24) + "".join(f"{n:>20}" for n in names))
    for lo, hi in zip(BANDS, BANDS[1:]):
        row = f"  [{lo:.0e},{hi:.0e})".ljust(24)
        for n in names:
            row += f"{sum(1 for v in records[n].values() if lo <= v < hi):>20}"
        print(row)
    print()
    total = sum(len(v) for v in records.values())
    for fl in FLOORS:
        seen = sum(sum(1 for v in r.values() if v > fl) for r in records.values())
        per = "  ".join(f"{n}={sum(1 for v in records[n].values() if v > fl)}/{len(records[n])}"
                        for n in names)
        print(f"  floor {fl:.0e}: watches {seen:>3} of {total}   ({per})")
    print()
    for n, r in records.items():
        nz = [v for v in r.values() if v > 0]
        top = max(r, key=r.get)
        print(f"  {n}: max {r[top]:.3g} ({top})  min>0 {min(nz):.3g}  "
              f"baselines at exactly 0: {len(r) - len(nz)}")


def q2_control(pairs):
    print()
    print("== 2. the negative control: how far a legitimate change moves rsub ==")
    print()
    print(f"  D171, {len(pairs)} instances with a before and an after, "
          "88 of 94 digests moved")
    rat = [(new / old, n, old, new) for n, old, new in pairs if old > 0 and new > 0]
    rat.sort()
    print(f"  worst down {rat[0][0]:.4f}x ({rat[0][1]})   "
          f"worst up {rat[-1][0]:.4f}x ({rat[-1][1]})")
    print(f"  moved by {FACTOR}x or more, either way: "
          f"{sum(1 for r, *_ in rat if r >= FACTOR or r <= 1 / FACTOR)} of {len(rat)}")
    print()
    print("  would the predicate have called any of them a regression?")
    for fl in FLOORS + (0.0,):
        fired = [n for r, n, o, w in rat if w > fl and w > o * FACTOR]
        print(f"    floor {fl:.0e}: {len(fired)} false regressions {fired}")
    print()
    print("  and how much headroom is left under the factor, among the watched")
    for fl in FLOORS:
        w = [x for x in rat if x[3] > fl]
        if not w:
            print(f"    floor {fl:.0e}:   0 watched")
            continue
        print(f"    floor {fl:.0e}: {len(w):>3} watched   worst up {w[-1][0]:.3f}x "
              f"({w[-1][1]})   headroom to {FACTOR}x = {FACTOR / w[-1][0]:.2f}x")


def q3_alternatives(exact, records):
    print()
    print("== 3. the two alternatives, against the same ground truth ==")
    print()
    print("  ground truth is 02-83's refeps: the published point's distance from")
    print("  Koch's optimum, in units of the arithmetic floor. |refeps| > 1e4 is")
    print("  the four instances 02-83 named. pilot and pilot87 are the two the")
    print("  checker has any chance of seeing.")
    print()
    wrong = {'pilot', 'pilot87'}
    rows = []
    for name, (gappos, objtraf, refeps, cert, src) in exact.items():
        rsub = None
        for r in records.values():
            if name in r:
                rsub = r[name]
        if rsub is None:
            continue
        floor = EPS * objtraf
        rows.append(dict(name=name, rsub=rsub, gappos=gappos, cert=cert,
                         qeps=gappos / floor if floor > 0 else float('inf')))
    for key, label in (('rsub', 'rsub = gappos / (1 + |obj|), what the gate records'),
                       ('gappos', 'gappos on its own, no normalisation'),
                       ('qeps', 'gappos / (eps * sum|c_j x_j|), the traffic floor')):
        print(f"  {label}")
        for certonly in (False, True):
            pool = [x for x in rows if x['cert'] == 'yes' or not certonly]
            pos = [x for x in pool if x['name'] in wrong]
            neg = [x for x in pool if x['name'] not in wrong]
            if not pos:
                continue
            lo = min(x[key] for x in pos)
            hi = max(x[key] for x in neg)
            who = max(neg, key=lambda z: z[key])['name']
            print(f"    certified only={str(certonly):<5} "
                  f"catches {sorted(x['name'] for x in pos)}")
            print(f"      lowest of those {lo:.4g}   top clean instance "
                  f"{hi:.4g} ({who})   margin {lo / hi:.4g}x")
        print()


def main():
    records = {
        'netlib': read_record('bench/results/netlib.txt'),
        'kennington': read_record('bench/results/netlib-kennington.txt'),
    }
    # A record with no lines in it is what a campaign mid-write looks like,
    # and every count below would still print. It happened on the first run
    # of this script: `make netlib-kennington` was in flight, the set read 0
    # instances, and the coverage table came out looking finished.
    for name, r in records.items():
        if not r:
            print(f"{name}: no instances read. Either the set has not been run "
                  "or a campaign is writing it now. Refusing to take a "
                  "reading.", file=sys.stderr)
            return 2
    q1_coverage(records)
    q2_control(read_pairs('bench/measurements/02-81/gate-diff.txt'))
    exact = {}
    exact.update(read_exact('bench/measurements/02-83/exact-objective-netlib.txt'))
    exact.update(read_exact(
        'bench/measurements/02-83/exact-objective-netlib-kennington.txt'))
    q3_alternatives(exact, records)
    return 0


if __name__ == '__main__':
    sys.exit(main())
