"""Reads the OBJCMP records and says whether the naive sum can pick wrong.

Two different quantities were both called "the worst margin" in the first
version of this file, and they disagree by five orders. They are separated
here and named apart, because a figure with two values and one name is how a
record misleads its own author (`numerics-reviewer`).

  spread  (|errc| + |errb|) / |sep| — how large each side's accumulation
          error is beside the gap the comparison is deciding. It says how
          close the arithmetic is to mattering.
  flip    |errc - errb| / |sep| — what a changed VERDICT actually needs,
          since a term common to both sides cancels inside `a < b`. It is
          small only while the two errors agree, which is a property of the
          instance and not of the method.
"""
import sys

recs = []
cur_set = "?"
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if line.startswith("== "):
        name = line[3:].strip()
        if not name.endswith("instances"):
            cur_set = name.split()[0]
        continue
    if "OBJCMP" not in line:
        continue
    f = line[line.index("OBJCMP"):].split()
    r = {"site": f[1], "set": cur_set}
    for kv in f[2:]:
        if kv == "FLIP":
            r["flip"] = True
        elif "=" in kv:
            k, _, v = kv.partition("=")
            try:
                r[k] = float(v)
            except ValueError:
                pass
    recs.append(r)

if not recs:
    print("no comparisons recorded on any set")
    sys.exit(0)

DUAL_TOL = 1e-7          # no instance of the three sets sets its own

sets = {}
for r in recs:
    sets.setdefault(r["set"], []).append(r)

print(f"comparisons recorded: {len(recs)}")
for name, rs in sorted(sets.items()):
    sites = {}
    for r in rs:
        sites[r["site"]] = sites.get(r["site"], 0) + 1
    print(f"  {name}: {len(rs)}  " +
          "  ".join(f"{k}={v}" for k, v in sorted(sites.items())))

flips = [r for r in recs if r.get("flip")]
print(f"\nverdict flips under a compensated sum: {len(flips)}")
for r in flips:
    print("  FLIP:", r)

# Only comparisons with both sides inside the dual tolerance are decided by
# the objective at all; the rest are settled by dual feasibility first.
decided = [r for r in recs
           if r.get("dv", 1.0) <= DUAL_TOL and r.get("bdv", 1.0) <= DUAL_TOL]
ties = [r for r in decided if r.get("sep") == 0.0]
# A tie whose two sides carry the same objective in BOTH arithmetics is one
# point compared with itself; no sum separates a thing from itself.
selfcmp = [r for r in ties
           if r.get("cur") == r.get("bst") and r.get("curc") == r.get("bstc")]
real = [r for r in decided if r.get("sep") != 0.0]

print(f"\ndecided by the objective (both sides dual-feasible): {len(decided)}")
print(f"  settled by dual feasibility instead: {len(recs) - len(decided)}")
print(f"  exact ties: {len(ties)}, of which one point against itself: "
      f"{len(selfcmp)}")
print(f"  two distinct points, which is the informative population: "
      f"{len(real)}")

if not real:
    print("\nno comparison anywhere separated two distinct points, so neither "
          "margin can be reported")
    sys.exit(0)


def margins(r):
    sep = abs(r["sep"])
    return ((abs(r.get("errc", 0.0)) + abs(r.get("errb", 0.0))) / sep,
            abs(r.get("errc", 0.0) - r.get("errb", 0.0)) / sep)


print("\nthe informative population, one line each:")
print(f"  {'set':<18} {'site':<10} {'sep':>13} {'spread':>10} {'flip':>12}")
for r in sorted(real, key=lambda r: -margins(r)[0]):
    sp, fl = margins(r)
    print(f"  {r['set']:<18} {r['site']:<10} {r['sep']:13.6g} "
          f"{sp:10.4g} {fl:12.4g}")

ws = max(margins(r)[0] for r in real)
wf = max(margins(r)[1] for r in real)
print(f"\nworst spread: {ws:.4g}   worst flip margin: {wf:.4g}")
print("A flip needs the flip margin to reach 1. The spread is what it would "
      "be if the two errors stopped agreeing.")
