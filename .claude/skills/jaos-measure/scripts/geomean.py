#!/usr/bin/env python3
"""Geometric mean of per-instance ratios. Never a total over a set.

Usage:
    # work or iteration ratios between two records
    geomean.py --metric work  old.txt new.txt
    geomean.py --metric iters old.txt new.txt --plus-one

    # a time ratio, or anything else, from a table this script is handed
    geomean.py --pairs timings.txt
    printf 'maros-r7 4.10 3.55\\npilot87 9.80 8.11\\n' | geomean.py --pairs -

`--pairs` reads `name baseline candidate` per line; '#' comments and blank
lines are skipped. Seconds never enter bench/results, so a time ratio has to
come in this way rather than out of a record.

Why this is a script and not an instruction: D46 says two instances are 74%
of the standard set's total work, so a ratio of sums is a statement about
those two and nothing else. The correct figure is exp(mean(ln r_i)) over
per-instance ratios, and a model computing that over 94 rows produces a
plausible number that cannot be checked by looking at it. The total is
printed too, labelled as what it is, precisely so the difference is visible.

Exit status: 0 on success, 2 on a usage or data error.
No third-party imports.
"""

import argparse
import math
import re
import sys
from pathlib import Path

LINE_RE = re.compile(r"^(?P<name>[A-Za-z0-9][A-Za-z0-9_.\-]*)\s+(?P<status>[a-z_]+)\s")
KV_RE = re.compile(r"\b([a-z_]+)=([^\s()]+)")
SUMMARY_RE = re.compile(r"^\d+\s+instances?:")


def read_record(path, metric):
    """name -> float(metric) from a bench results file."""
    out = {}
    text = Path(path).read_text(encoding="utf-8")
    for raw in text.split("\n"):
        line = raw.rstrip()
        if (not line or line.startswith(("#", "--", "gate:", "baseline:"))
                or SUMMARY_RE.match(line)):
            continue
        m = LINE_RE.match(line)
        if not m:
            continue
        kv = dict(KV_RE.findall(line))
        if metric in kv:
            try:
                out[m.group("name")] = float(kv[metric])
            except ValueError:
                pass
    return out


def read_pairs(path):
    """[(name, a, b)] from a whitespace table, or from stdin when path is '-'."""
    text = sys.stdin.read() if path == "-" else Path(path).read_text(encoding="utf-8")
    rows = []
    for n, raw in enumerate(text.split("\n"), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) < 3:
            raise ValueError("line %d: need 'name baseline candidate', got %r"
                             % (n, raw.strip()))
        try:
            rows.append((parts[0], float(parts[1]), float(parts[2])))
        except ValueError:
            raise ValueError("line %d: %r is not two numbers"
                             % (n, " ".join(parts[1:3])))
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("files", nargs="*",
                    help="two results files (with --metric), or nothing (with --pairs)")
    ap.add_argument("--metric", choices=("work", "iters"),
                    help="figure to take from two bench results files")
    ap.add_argument("--pairs", metavar="FILE",
                    help="read 'name baseline candidate' rows; '-' for stdin")
    ap.add_argument("--plus-one", action="store_true",
                    help="use (a+1)/(b+1), so a zero does not annihilate the mean "
                         "-- the form bench/warm.c uses for iterations")
    args = ap.parse_args(argv)

    if args.pairs:
        try:
            rows = read_pairs(args.pairs)
        except (OSError, ValueError) as e:
            print("error: %s" % e, file=sys.stderr)
            return 2
        label = "baseline -> candidate"
    else:
        if not args.metric or len(args.files) != 2:
            print("give --metric with exactly two results files, or --pairs FILE",
                  file=sys.stderr)
            return 2
        try:
            a = read_record(args.files[0], args.metric)
            b = read_record(args.files[1], args.metric)
        except OSError as e:
            print("error: %s" % e, file=sys.stderr)
            return 2
        shared = sorted(set(a) & set(b))
        if not shared:
            print("no instance appears in both files -- check the metric and the "
                  "files", file=sys.stderr)
            return 2
        only_a, only_b = sorted(set(a) - set(b)), sorted(set(b) - set(a))
        if only_a or only_b:
            print("note: %d instance(s) only in the first file, %d only in the "
                  "second; they are excluded" % (len(only_a), len(only_b)))
        rows = [(n, a[n], b[n]) for n in shared]
        label = "%s: %s -> %s" % (args.metric, args.files[0], args.files[1])

    logs, ratios, skipped = [], [], []
    for name, base, cand in rows:
        if args.plus_one:
            base, cand = base + 1.0, cand + 1.0
        if base <= 0 or cand <= 0:
            skipped.append(name)
            continue
        r = cand / base
        ratios.append((name, base, cand, r))
        logs.append(math.log(r))

    if not logs:
        print("nothing to average: every row had a non-positive value. Use "
              "--plus-one if zeros are expected.", file=sys.stderr)
        return 2

    gm = math.exp(sum(logs) / len(logs))
    ratios.sort(key=lambda t: t[3])

    print(label)
    print("-" * 60)
    for name, base, cand, r in ratios:
        print("  %-14s %14.6g -> %-14.6g %8.4fx" % (name, base, cand, r))

    print("-" * 60)
    print("  instances averaged   %d" % len(ratios))
    if skipped:
        print("  skipped (non-positive) %d: %s" % (len(skipped), ", ".join(skipped)))
    print("  GEOMETRIC MEAN       %.4fx     <-- this is the result" % gm)
    print("  best  %-14s %.4fx" % (ratios[0][0], ratios[0][3]))
    print("  worst %-14s %.4fx" % (ratios[-1][0], ratios[-1][3]))

    tb = sum(t[1] for t in ratios)
    tc = sum(t[2] for t in ratios)
    if tb > 0:
        print("  ratio of totals      %.4fx     <-- NOT the result (D46): two "
              "instances are 74%% of the standard set" % (tc / tb))
    return 0


if __name__ == "__main__":
    sys.exit(main())
