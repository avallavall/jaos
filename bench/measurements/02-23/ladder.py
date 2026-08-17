#!/usr/bin/env python3
"""The pds and fome scaling ladders, read from the manifests and baselines.

Every number this prints has one owner, and it is not this file: dimensions
come from the manifests, iterations and work units from the committed
baselines. Nothing is copied by hand, so the ladder cannot drift away from the
record the way a table in prose does.

pds spans both sides of the fourth set: pds-02 through pds-20 have been in
bench/netlib-kennington.* since M1, and pds-30 through pds-100 arrived with
TODO.md section 4. Four of the six points were already here and nobody had
plotted them, because the family stopped at pds-20.

    python3 bench/measurements/02-23/ladder.py        # from the repo root

Instances missing from a baseline are skipped with a note rather than dropped
silently: a ladder with a hole in it that says so is usable, one that closes
the hole quietly is not.

**One row of the fome table is not a ladder step.** fome11, fome12 and fome13
double exactly in both dimensions; fome21 does not continue that sequence, it
is a different model. Its step line is printed anyway, and its iteration
exponent comes out negative, which is the output telling the reader that the
comparison is between two unrelated models rather than a step. Read the first
two fome steps and ignore the third.
"""
import math
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")

SETS = [
    ("pds", [("bench/netlib-kennington", ["pds-02", "pds-06", "pds-10", "pds-20"]),
             ("bench/plato-pds", ["pds-30", "pds-40", "pds-50", "pds-60",
                                  "pds-70", "pds-80", "pds-90", "pds-100"])]),
    ("fome", [("bench/plato-fome", ["fome11", "fome12", "fome13", "fome21"])]),
]


def read_table(path, cols):
    """name -> tuple of the requested 0-based columns, skipping comments."""
    out = {}
    full = os.path.join(ROOT, path)
    if not os.path.exists(full):
        return out
    with open(full) as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            f = line.split()
            if len(f) <= max(cols):
                continue
            out[f[0]] = tuple(f[c] for c in cols)
    return out


def main():
    for family, sources in SETS:
        rows = []
        missing = []
        for stem, names in sources:
            dims = read_table(stem + ".manifest", [2, 3])       # rows, cols
            runs = read_table(stem + ".baseline", [7, 8])       # iters, work
            for n in names:
                if n not in dims or n not in runs:
                    missing.append(n)
                    continue
                rows.append((n, int(dims[n][0]), int(dims[n][1]),
                             int(runs[n][0]), int(runs[n][1])))

        if not rows:
            print(f"== {family}: nothing measured yet\n")
            continue

        print(f"== {family} ladder, {len(rows)} points")
        print(f"{'instance':<10} {'rows':>8} {'cols':>8} {'iters':>9} "
              f"{'work':>15} {'work/iter':>11}")
        for n, r, c, it, w in rows:
            print(f"{n:<10} {r:>8} {c:>8} {it:>9} {w:>15} {w // it:>11}")

        print(f"\n   step-by-step, exponent is log(ratio)/log(row ratio)")
        print(f"{'step':<20} {'rows':>7} {'iters':>7} {'exp':>6} "
              f"{'w/it':>7} {'exp':>6}")
        for (n0, r0, _, i0, w0), (n1, r1, _, i1, w1) in zip(rows, rows[1:]):
            rr = r1 / r0
            ir = i1 / i0
            pr = (w1 / i1) / (w0 / i0)
            e_i = math.log(ir) / math.log(rr)
            e_p = math.log(pr) / math.log(rr)
            print(f"{n0 + ' -> ' + n1:<20} {rr:>7.3f} {ir:>7.3f} {e_i:>6.2f} "
                  f"{pr:>7.3f} {e_p:>6.2f}")

        n0, r0, _, i0, w0 = rows[0]
        n1, r1, _, i1, w1 = rows[-1]
        rr = r1 / r0
        print(f"\n   end to end, {n0} -> {n1}: rows {rr:.2f}x, "
              f"iters {i1 / i0:.1f}x (n^{math.log(i1 / i0) / math.log(rr):.2f}), "
              f"work {w1 / w0:.0f}x "
              f"(n^{math.log(w1 / w0) / math.log(rr):.2f})")
        if missing:
            print(f"\n   NOT MEASURED, so absent from every figure above: "
                  f"{', '.join(missing)}")
        print()

    return 0


if __name__ == "__main__":
    sys.exit(main())
