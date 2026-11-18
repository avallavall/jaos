"""Reads a record file of read.sh and prints what the README of 02-485
quotes: the verdicts of each tool, the models whose verdict changed, and for
the models both tools prove, whether the exact objectives and the work
units agree and how the seconds compare.

    python3 summary.py read.txt [BEFORE AFTER]

SPDX-License-Identifier: Apache-2.0
"""
import sys
from collections import Counter

path = sys.argv[1]
before = sys.argv[2] if len(sys.argv) > 2 else "head"
after = sys.argv[3] if len(sys.argv) > 3 else "final"
runs = {}
for line in open(path):
    f = line.split()
    if len(f) < 8:
        continue
    name, tool, status, why, obj, wu, secs, rss = f[:8]
    runs.setdefault(name, {})[tool] = dict(
        status=status, why=why, obj=obj, wu=wu, secs=float(secs),
        rss=int(rss) if rss.isdigit() else 0)


def verdict(r):
    if r["status"] == "optimal" and "proved" in r["why"]:
        return "proved"
    if r["status"] == "timeout":
        return "timeout"
    return r["why"].replace("exact_solving:_", "").replace("_", " ")


for tool in (before, after):
    c = Counter(verdict(runs[n][tool]) for n in runs if tool in runs[n])
    print(tool, dict(c))

print("\nchanged verdicts:")
for n in sorted(runs):
    a, b = runs[n].get(before), runs[n].get(after)
    if a and b and verdict(a) != verdict(b):
        print(f"  {n}: {verdict(a)} {a['secs']:.1f}s -> {verdict(b)} "
              f"{b['secs']:.1f}s {b['rss'] // 1024} MiB")

both = [n for n in sorted(runs)
        if verdict(runs[n].get(before, {"status": "", "why": ""})) == "proved"
        and verdict(runs[n].get(after, {"status": "", "why": ""})) == "proved"]
same_obj = sum(runs[n][before]["obj"] == runs[n][after]["obj"] for n in both)
paired = [n for n in sorted(runs) if before in runs[n] and after in runs[n]]
same_wu = sum(runs[n][before]["wu"] == runs[n][after]["wu"] for n in paired)
print(f"\nproved by both: {len(both)}; same exact objective: {same_obj}")
print(f"same work units: {same_wu} of {len(paired)} models run by both")
for n in paired:
    if runs[n][before]["wu"] != runs[n][after]["wu"]:
        print(f"  work units differ on {n}: {runs[n][before]['wu']} "
              f"{runs[n][after]['wu']}")
tb = sum(runs[n][before]["secs"] for n in both)
ta = sum(runs[n][after]["secs"] for n in both)
print(f"seconds on those: {tb:.1f} -> {ta:.1f} ({ta / tb:.3f}x)")
ratios = sorted((runs[n][after]["secs"] / max(runs[n][before]["secs"], 1e-9), n)
                for n in both)
slower = [(r, n) for r, n in ratios if r > 1.0]
print("slowest ratio:", [f"{n} {r:.2f}" for r, n in ratios[-5:]])
print("slower than before:", [f"{n} {r:.2f} ({runs[n][before]['secs']:.2f}s "
                              f"-> {runs[n][after]['secs']:.2f}s)"
                              for r, n in slower])
big = sorted(both, key=lambda n: -runs[n][before]["secs"])[:10]
print("\nlongest before:")
for n in big:
    print(f"  {n}: {runs[n][before]['secs']:.2f}s -> {runs[n][after]['secs']:.2f}s")
