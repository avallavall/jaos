"""Three-way comparison across D198 and D199.

`pre-d198` is the under-billed tree, `d198` bills the `memset` honestly, and
`after` replaces the `memset` with an `O(nrow)` clear. The interesting numbers
are how much of D198's cost D199 gives back, and what honest billing still
charges that the under-billed tree did not.

**Objectives are compared as well as work**, because the claim D199 makes is
that no digit of any answer moves — only the units. Work alone cannot say that.
"""
import re, sys, math

def read(path):
    out = {}
    for line in open(path, encoding="utf-8"):
        f = line.split()
        if len(f) < 2 or f[0].startswith(("#", "-")) or line.startswith(" "):
            continue
        name, verdict = f[0], f[1]
        if name in ("measured", "iterations", "work", "took", "bit-identical",
                    "elapsed"):
            continue
        m = re.search(r"primal=(\d+)/(\d+)", line)
        o = re.search(r"obj=(\S+)/(\S+)", line)
        out[name] = (verdict,
                     int(m.group(2)) if m else None,
                     int(m.group(1)) if m else None,
                     o.group(2) if o else None)
    return out

pre, d198, now = (read(p) for p in sys.argv[1:4])

def cmp(a, b, la, lb):
    print()
    print("== %s -> %s" % (la, lb))
    moved = [(n, a[n][0], b[n][0]) for n in sorted(a)
             if n in b and a[n][0] != b[n][0]]
    print("  category changes: %d" % len(moved))
    for n, va, vb in moved:
        print("    %-12s %s -> %s" % (n, va, vb))
    both = [n for n in sorted(a)
            if n in b and a[n][0] == "ok" and b[n][0] == "ok"
            and a[n][1] and b[n][1]]
    r = [b[n][1] / a[n][1] for n in both]
    gm = math.exp(sum(math.log(x) for x in r) / len(r))
    print("  ok on both: %d,  work geometric mean %s/%s: %.4f"
          % (len(both), lb, la, gm))
    ident = [n for n in both if a[n][1] == b[n][1]]
    print("  bit-identical primal work: %d of %d" % (len(ident), len(both)))
    iters = [n for n in both if a[n][2] != b[n][2]]
    print("  primal ITERATION count moved on: %d" % len(iters))
    for n in iters:
        print("    %-12s %d -> %d" % (n, a[n][2], b[n][2]))
    objs = [n for n in both if a[n][3] != b[n][3]]
    print("  primal OBJECTIVE moved on: %d%s"
          % (len(objs), "" if objs else "  <- no digit of any answer moved"))
    for n in objs:
        print("    %-12s %s -> %s" % (n, a[n][3], b[n][3]))
    ext = sorted(((b[n][1] / a[n][1]), n) for n in both)
    print("  cheapest %s at %.4f, dearest %s at %.4f"
          % (ext[0][1], ext[0][0], ext[-1][1], ext[-1][0]))

cmp(d198, now, "D198", "D199")
cmp(pre, now, "pre-D198", "D199")
