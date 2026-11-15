"""Counts 02-455's mirror lines by verdict and by what differs.

For each mode of SET in DIR, the lines that read same, path and DIFF, and
of the path lines those whose nodes or iterations differ, with the files
named; then the shape of each such model, read from its MPS file.

Usage: python3 summary.py DIR SET

SPDX-License-Identifier: Apache-2.0
"""
import os
import re
import sys

d, which = sys.argv[1], sys.argv[2]


def shape(name):
    text = open(os.path.join(d, which + "-models", name)).read()
    kinds = []
    if "\nCSECTION" in text or "\nQCMATRIX" in text:
        kinds.append("cone")
    if "\nSOS\n" in text:
        kinds.append("sos")
    if " SC BND " in text:
        kinds.append("semi")
    if "\nINDICATORS\n" in text:
        kinds.append("indicator")
    return "+".join(kinds) or "plain"


for mode in ("both", "rows", "cols"):
    counts = {}
    moved = []
    for line in open(os.path.join(d, f"{which}-{mode}.txt")):
        f = line.split()
        counts[f[1]] = counts.get(f[1], 0) + 1
        if f[1] == "path":
            it = re.search(r"iters (\d+)/(\d+)", line)
            nd = re.search(r"nodes (\d+)/(\d+)", line)
            if it.group(1) != it.group(2) or nd.group(1) != nd.group(2):
                moved.append((f[0], shape(f[0]), it.group(0), nd.group(0)))
        elif f[1] != "same":
            moved.append((f[0], shape(f[0]), f[1], line.strip()[25:185]))
    print(f"== {which} {mode}: " + " ".join(f"{k}={v}" for k, v in sorted(counts.items()))
          + f", nodes or iterations differ in {len(moved)}")
    by = {}
    for _, k, _, _ in moved:
        by[k] = by.get(k, 0) + 1
    print("   by shape: " + " ".join(f"{k}={v}" for k, v in sorted(by.items())))
    for m in moved[:40]:
        print("   " + " ".join(m))
