#!/usr/bin/env python3
# The root LP's speed in each pair of pair.sh legs: where neither leg's root
# LP ended, the after leg's iterations over the before leg's in the same
# 300 s; where both ended, the before leg's root LP seconds over the after
# leg's. The rows of the model come from the manifest.
#
#   python3 speed.py OUT[,OUT...] MANIFEST
#
# SPDX-License-Identifier: Apache-2.0
import math
import os
import re
import sys


def lp(path):
    it, st, sec = None, None, None
    for ln in open(path, errors="replace"):
        m = re.match(r"root split: total .* lp iters (\d+) .* status (-?\d+) ", ln)
        if m:
            it, st = int(m.group(1)), int(m.group(2))
        m = re.match(r"root piece: root_lp -?\d+ ([\d.]+)", ln)
        if m:
            sec = float(m.group(1))
    return it, st, sec


rows = {}
for ln in open(sys.argv[2]):
    t = ln.split()
    if len(t) >= 4 and not ln.startswith("#"):
        rows[t[0]] = int(t[2])
out = []
for o in sys.argv[1].split(","):
    logs = os.path.join(o, "logs")
    for f in sorted(os.listdir(logs)):
        if not f.endswith(".before.out"):
            continue
        n = f[:-len(".before.out")]
        a = lp(os.path.join(logs, f))
        pb = os.path.join(logs, n + ".after.out")
        if not os.path.exists(pb):
            continue
        b = lp(pb)
        if a[0] is None or b[0] is None:
            continue
        if a[1] == 1 and b[1] == 1 and a[2] and b[2]:
            out.append((a[2] / b[2], n, "seconds", a[2], b[2]))
        elif a[1] != 1 and b[1] != 1:
            out.append((b[0] / a[0], n, "iterations", a[0], b[0]))
        else:
            out.append((float("nan"), n, "one ended", a[0], b[0]))
out.sort(key=lambda r: -r[0] if r[0] == r[0] else 0)
print("# instance rows measure before after speed")
for r in out:
    print("%-28s %8d %-10s %10s %10s %6.2f" % (r[1], rows.get(r[1], 0), r[2],
                                               r[3], r[4], r[0]))
good = [r[0] for r in out if r[0] == r[0]]
big = [r[0] for r in out if r[0] == r[0] and rows.get(r[1], 0) >= 90000]
small = [r[0] for r in out if r[0] == r[0] and rows.get(r[1], 0) < 90000]
for name, v in (("all", good), ("90,000 rows or more", big), ("fewer rows", small)):
    if v:
        print("# %s: %d pairs, geometric mean %.3f" % (
            name, len(v), math.exp(sum(math.log(x) for x in v) / len(v))))
