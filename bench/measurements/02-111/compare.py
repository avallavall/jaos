"""Per-instance comparison of two `bench/primal` records.

The campaign's own geometric mean is taken over its measured set, and the
guard moved one instance into that set. A mean over 55 is not comparable to a
mean over 54, so this compares only the instances measured on BOTH sides and
lists the ones that changed category separately.
"""
import re, sys, math

def read(path):
    out = {}
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or line.startswith(" ") or line.startswith("-"):
            continue
        f = line.split()
        if len(f) < 2:
            continue
        name, verdict = f[0], f[1]
        if name in ("measured", "iterations", "work", "took", "bit-identical",
                    "elapsed"):
            continue
        work = None
        m = re.search(r"primal=(\d+)/(\d+)", line)
        if m:
            work = int(m.group(2))
        out[name] = (verdict, work)
    return out

a = read(sys.argv[1])   # parent
b = read(sys.argv[2])   # head

moved = [(n, a[n][0], b[n][0]) for n in sorted(a)
         if n in b and a[n][0] != b[n][0]]
print("category changes: %d" % len(moved))
for n, va, vb in moved:
    print("  %-12s %s -> %s" % (n, va, vb))

both = [n for n in sorted(a)
        if n in b and a[n][0] == "ok" and b[n][0] == "ok"
        and a[n][1] and b[n][1]]
ratios = [b[n][1] / a[n][1] for n in both]
changed = [(n, b[n][1] / a[n][1]) for n in both if b[n][1] != a[n][1]]
gm = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
print()
print("instances 'ok' on both sides: %d" % len(both))
print("identical primal work units:  %d" % (len(both) - len(changed)))
print("work geometric mean head/parent over those: %.4f" % gm)
both_bad = [n for n in sorted(a)
        if n in b and a[n][0] == "DISAGREE" and b[n][0] == "DISAGREE"
        and a[n][1] and b[n][1]]
bad_moved = [(n, b[n][1] / a[n][1]) for n in both_bad if b[n][1] != a[n][1]]
print()
print("instances DISAGREE on both sides: %d, of which work moved: %d"
      % (len(both_bad), len(bad_moved)))
for n, r in sorted(bad_moved, key=lambda t: t[1]):
    print("  %-12s %.4f  (%d -> %d)" % (n, r, a[n][1], b[n][1]))

if changed:
    changed.sort(key=lambda t: t[1])
    print("moved, cheapest first:")
    for n, r in changed:
        print("  %-12s %.4f  (%d -> %d)" % (n, r, a[n][1], b[n][1]))
