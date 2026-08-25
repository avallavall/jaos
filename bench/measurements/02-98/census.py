#!/usr/bin/env python3
"""How many mapped bases arrive long, short or exact.

    census.py <stderr file> [<stderr file> ...]

Reads the DIAG-MAPPED and DIAG-REFUSE lines 02-90's probe emits, keyed to the
DIAG-INSTANCE line before them. `delta = nbasic - nrow`: negative is short,
positive is long, zero is exact.
"""
import re, sys, os


def read(path):
    cur, out = None, []
    for line in open(path):
        m = re.match(r'DIAG-INSTANCE (\S+)', line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r'DIAG-MAPPED nrow=(\d+) nvar=(\d+) nbasic=(\d+) delta=(-?\d+)',
                     line)
        if m and cur:
            out.append((cur, int(m.group(1)), int(m.group(2)),
                        int(m.group(3)), int(m.group(4))))
    return out


def main():
    grand = {}
    for path in sys.argv[1:]:
        name = os.path.basename(path).replace('census-', '').replace('.txt', '')
        rows = read(path)
        grand[name] = rows
        if not rows:
            print(f"{name}: no DIAG-MAPPED lines; nothing to report")
            continue
        long_ = [r for r in rows if r[4] > 0]
        short = [r for r in rows if r[4] < 0]
        exact = [r for r in rows if r[4] == 0]
        print(f"== {name}: {len(rows)} calls to build_warm_basis ==")
        print(f"   exact  {len(exact):>4}")
        print(f"   short  {len(short):>4}"
              + (f"   worst {min(r[4] for r in short)}"
                 f" ({min(short, key=lambda r: r[4])[0]})" if short else ""))
        print(f"   LONG   {len(long_):>4}"
              + (f"   worst +{max(r[4] for r in long_)}"
                 f" ({max(long_, key=lambda r: r[4])[0]})" if long_ else ""))
        if long_:
            print("   every long map, which is what the refusal says does not exist:")
            for n, nr, nv, nb, d in sorted(long_, key=lambda r: -r[4]):
                print(f"     {n:<12} nrow={nr:<7} nbasic={nb:<7} over by +{d}")
        else:
            print("   the refusal's premise holds on this set: no map arrives long")
        # how many of the short ones the cap already covers
        if short:
            within = [r for r in short if -r[4] <= 4]
            print(f"   of the short ones, {len(within)} are within "
                  f"WARM_REPAIR_MAX_SHORT = 4 and get repaired; "
                  f"{len(short) - within.__len__()} fall back to cold")
        print()
    total_long = sum(len([r for r in v if r[4] > 0]) for v in grand.values())
    total = sum(len(v) for v in grand.values())
    print(f"OVER BOTH SETS: {total_long} long maps in {total} calls")
    print("The refusal in build_warm_basis says a demotion rule would be "
          "'a constant fitted to nothing'." )
    print("That premise " + ("HAS EXPIRED." if total_long else "HOLDS.")
          + (" A demotion rule now has a measured population."
             if total_long else " There is nothing for one to act on."))
    return 0


if __name__ == '__main__':
    sys.exit(main())
