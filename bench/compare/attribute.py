#!/usr/bin/env python3
"""What each competitor's own presolve is worth, read against itself.

bench/compare/README.md: "Read a rung difference against the competitor
itself, not through JAOS." At T0 the competitors run presolve off; at P0 they
run it on. Both binaries are pinned by checksum, so the same executable is
being timed twice and the difference is its presolve.

The caveat this cannot dodge: T0 was taken 2026-08-11 and P0 on 2026-08-14, so
the two readings are in different sessions and the usual within-session JAOS
control is not available -- JAOS is the one thing that DID change between
them. The harness repeats to 1.4% across sessions (D81), so a ratio near 1.0
here says nothing and only the large ones are readable.
"""
import sys
from pathlib import Path

FLOOR = 0.05


def read(path):
    out = {}
    for line in Path(path).read_text().split("\n"):
        f = line.split("\t")
        if len(f) < 7 or line.startswith("#"):
            continue
        inst, solver, status = f[0], f[1], f[2]
        try:
            secs = float(f[5].rstrip(","))
        except ValueError:
            continue
        out[(inst, solver)] = (secs, status, int(f[4]))
    return out


t0, p0 = read(sys.argv[1]), read(sys.argv[2])

for solver in ("jaos", "highs", "soplex", "clp"):
    rows = []
    for (inst, s), (secs, status, iters) in t0.items():
        if s != solver or secs < FLOOR:
            continue
        if (inst, s) not in p0:
            continue
        n_secs, n_status, n_iters = p0[(inst, s)]
        if "ok" not in status or "ok" not in n_status:
            continue
        rows.append((inst, secs, n_secs, iters, n_iters))
    rows.sort(key=lambda r: r[2] / r[1])
    print(f"\n=== {solver}: its own presolve, T0 (off) -> P0 (on) ===")
    print(f"{'instance':12} {'off_s':>8} {'on_s':>8} {'ratio':>8} "
          f"{'off_it':>8} {'on_it':>8} {'it_ratio':>9}")
    for inst, o, n, oi, ni in rows:
        print(f"{inst:12} {o:8.2f} {n:8.2f} {n/o:7.3f}x {oi:8d} {ni:8d} "
              f"{ni/oi:8.3f}x")
    if rows:
        import math
        g = math.exp(sum(math.log(n / o) for _, o, n, _, _ in rows) / len(rows))
        gi = math.exp(sum(math.log(ni / oi) for _, _, _, oi, ni in rows) / len(rows))
        print(f"{'GEOMEAN':12} {'':8} {'':8} {g:7.3f}x {'':8} {'':8} {gi:8.3f}x"
              f"   over {len(rows)} instances above the {FLOOR}s floor")
