#!/usr/bin/env python3
"""Joins D277's before and after halves by instance and says what moved.

The before half is HEAD's `long double` checker; the after half is the
compensated `double` one. Both judge the same published point, because the
solve is untouched by the change.

Three questions, in the order they matter:

  1. Did any VERDICT move? `pfeas`, `dfeas`, `gcert` on an optimum,
     `cert` on a certificate. A moved verdict is a finding, not a detail.
  2. Which figures moved at all, and on how many instances.
  3. How far the largest move was, relative to the figure's own size.

The control is inside this script and it is the first thing it checks: the
two files must report a DIFFERENT `long double` count in their headers. If
they agree, the two halves were built from the same source and the whole
comparison is measuring the machine's repeatability rather than the change.
Exit 2 when that happens, because a clean-looking diff would otherwise be
read as "nothing moved".

Usage, from anywhere:
    python3 bench/measurements/02-182/compare-before-after.py

SPDX-License-Identifier: Apache-2.0
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BEFORE = os.path.join(HERE, "before-dual-cover.txt")
AFTER = os.path.join(HERE, "dual-cover.txt")

VERDICTS = ("pfeas", "dfeas", "gcert", "cert")
INTEGERS = ("dropped", "rays")


def read(path):
    """-> (header_long_double_count, [(instance, status, {field: text})])

    A LIST and not a dict, because the instance names are not unique:
    `greenbea.mps` exists in both `bench/instances` and
    `bench/instances-infeas` and they are different models. Keyed by name,
    one of the two silently overwrites the other and the comparison quietly
    drops an instance. Both halves ran the same glob in the same order, so
    position is the identity here, and `main` checks the two lists agree on
    the name at every position before comparing anything.
    """
    ld = None
    rows = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# check.c long double uses:"):
                ld = int(line.split(":")[1])
                continue
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            name, status = parts[0], parts[1]
            fields = {}
            for p in parts[2:]:
                if "=" in p:
                    k, v = p.split("=", 1)
                    fields[k] = v
            rows.append((name, status, fields))
    return ld, rows


def relative(a, b):
    """|a - b| / max(1, |a|, |b|). Non-finite pairs read as 0 or inf."""
    try:
        fa, fb = float(a), float(b)
    except ValueError:
        return float("inf")
    if fa == fb:
        return 0.0
    scale = max(1.0, abs(fa), abs(fb))
    d = abs(fa - fb)
    return d / scale if d == d else float("inf")


def main():
    for p in (BEFORE, AFTER):
        if not os.path.exists(p):
            sys.stderr.write("missing %s\n" % p)
            return 2

    ld_before, before = read(BEFORE)
    ld_after, after = read(AFTER)

    print("D277 -- the checker's dual half, before and after")
    print()
    print("control: `long double` uses in src/check.c")
    print("  before: %s" % ld_before)
    print("  after:  %s" % ld_after)
    if ld_before is None or ld_after is None:
        sys.stderr.write("a header is missing its long double count\n")
        return 2
    if ld_before == ld_after:
        sys.stderr.write(
            "CONTROL FAILED: both halves report %d long double uses, so they\n"
            "were built from the same source. Nothing below means anything.\n"
            % ld_before)
        return 2
    print("  -> the two halves are different binaries")
    print()

    if len(before) != len(after):
        sys.stderr.write("the halves hold %d and %d instances; not comparable\n"
                         % (len(before), len(after)))
        return 2
    for k, (b, a) in enumerate(zip(before, after)):
        if b[0] != a[0]:
            sys.stderr.write("line %d names %s in the before half and %s in "
                             "the after half; the two runs did not read the "
                             "same instances in the same order\n"
                             % (k + 1, b[0], a[0]))
            return 2

    print("instances compared: %d" % len(before))
    dups = sorted({n for n, _, _ in before
                   if sum(1 for m, _, _ in before if m == n) > 1})
    if dups:
        print("names that are not unique across the three sets: %s"
              % ", ".join(dups))
        print("  (compared by position, so both readings are counted)")

    status_moved = [b[0] for b, a in zip(before, after) if b[1] != a[1]]
    print("status changed:     %d %s" % (len(status_moved), status_moved or ""))

    verdict_moved = []
    field_moved = {}
    worst = {}
    instances_moved = set()

    for b, a in zip(before, after):
        n, fb, fa = b[0], b[2], a[2]
        for k in sorted(set(fb) | set(fa)):
            vb, va = fb.get(k), fa.get(k)
            if vb is None or va is None:
                field_moved.setdefault(k, []).append(n)
                instances_moved.add(n)
                continue
            if vb == va:
                continue
            field_moved.setdefault(k, []).append(n)
            instances_moved.add(n)
            if k in VERDICTS:
                verdict_moved.append((n, k, vb, va))
            elif k in INTEGERS:
                worst.setdefault(k, []).append((abs(int(va) - int(vb)), n,
                                                vb, va))
            else:
                worst.setdefault(k, []).append((relative(vb, va), n, vb, va))

    print("instances with any field moved: %d of %d"
          % (len(instances_moved), len(before)))
    print()

    print("VERDICTS MOVED: %d" % len(verdict_moved))
    for n, k, vb, va in verdict_moved:
        print("  %-14s %-7s %s -> %s" % (n, k, vb, va))
    if not verdict_moved:
        print("  none -- every pfeas, dfeas, gcert and cert is unchanged")
    print()

    print("%-10s %8s   %-14s %s" % ("field", "moved", "worst instance",
                                    "relative move"))
    for k in sorted(field_moved):
        rows = worst.get(k, [])
        if rows:
            rows.sort(reverse=True)
            r, n, vb, va = rows[0]
            print("%-10s %8d   %-14s %.3g" % (k, len(field_moved[k]), n, r))
            print("%-10s %8s   %-14s %s -> %s" % ("", "", "", vb, va))
        else:
            print("%-10s %8d   %-14s (verdict)" % (k, len(field_moved[k]), "-"))
    if not field_moved:
        print("(nothing moved at all)")

    return 1 if verdict_moved else 0


if __name__ == "__main__":
    sys.exit(main())
