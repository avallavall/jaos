#!/bin/bash
# What compensating the refinement residual actually buys, on the sets.
#
# D168 compensated `compute_primal`'s right-hand side and left two sums of the
# same shape alone. `numerics-reviewer` argued the refusal from the terms:
# `subtract_basis_times` sums products of `x_B`, which is an FTRAN output
# already carrying the factorization's error, so an accumulator cannot reach
# an error that is already inside a term. This measures it instead.
#
# Three trees, all built with the SAME flags -- the control is what makes the
# comparison mean anything, and its first version did not have one:
#
#   control       the working tree, unpatched
#   primal side   subtract_basis_times compensated
#   both sides    that, plus compute_duals' refinement dot product, which is
#                 D29's primal-dual symmetry
#
# The control against the committed record differs on exactly two lines, the
# baseline footer, because this runs without `-b`. The 94 instance lines are
# byte-identical, which is also the evidence that -O2 here and the Makefile's
# -O3 -flto -march=native produce the same bits.
#
# The control is built from a GIT REF and not from the working tree, because
# once this change landed the working tree became the candidate and a control
# read from it would be comparing a tree with itself.
#
# Usage: run-refinement-residual.sh [git-ref]     default: 4747f29, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-4747f29}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/ctl"
for f in src/*.c src/*.h; do
    git show "$ref:$f" > "$D/ctl/$(basename "$f")" || exit 2
done
JAOS_SRC="$D/ctl" python3 "$here/patch-primal-side.py" "$D/primal" >/dev/null || exit 2
JAOS_SRC="$D/ctl" python3 "$here/patch-both-sides.py"  "$D/both"   >/dev/null || exit 2
gcc-14 $P "$D/ctl"/*.c bench/run.c -o "$D/r-ctl"    -lm || exit 2
gcc-14 $P "$D/primal"/*.c bench/run.c -o "$D/r-primal" -lm || exit 2
gcc-14 $P "$D/both"/*.c   bench/run.c -o "$D/r-both"   -lm || exit 2

for t in ctl primal both; do
    "$D/r-$t" -j 12 -o "$D/$t-nl.txt"  >/dev/null 2>&1
    "$D/r-$t" -j 12 -m bench/netlib-kennington.manifest \
        -d bench/instances-kennington -o "$D/$t-kn.txt" >/dev/null 2>&1
    "$D/r-$t" -j 12 -m bench/netlib-infeas.manifest \
        -d bench/instances-infeas -e infeasible -o "$D/$t-inf.txt" >/dev/null 2>&1
done

# The per-instance records, kept beside this script. The first version left
# them in the mktemp directory and the trap deleted them, so the pds-06/10/20
# table in README.md had no file behind it (`numerics-reviewer`).
mkdir -p "$here/records"
cp "$D"/ctl-*.txt "$D"/primal-*.txt "$D"/both-*.txt "$here/records/" || exit 2

{
python3 - "$D" <<'PY'
import re, sys, os, math
D = sys.argv[1]
# NARROWER THAN THE RECORD IT READS, and it cost something: this never parses
# `dual`, `drop`, `cert`, `rays` or `basis`, all of which bench/run.c prints,
# and the `moved` count below compares only what it parsed. Nine instances
# whose only change was `basis` counted as not moved, and D171's first version
# missed them (`numerics-reviewer`). record_diff.py is the honest instrument
# here; this one is for the direction counts, which it does compute correctly.
KEYS = ["col", "row", "rowrel", "gap", "Q", "N", "sub", "rsub"]
def load(p):
    d = {}
    if not os.path.exists(p):
        return d
    for line in open(p):
        line = line.strip()
        if (not line or " instances:" in line
                or line.startswith(("--", "baseline", "gate", "time"))):
            continue
        f = line.split()
        if len(f) < 3:
            continue
        v = {}
        for k in KEYS + ["work", "iters", "digest"]:
            m = re.search(r'\b' + k + r'=([-0-9.eE+a-f]+)', line)
            if m:
                v[k] = m.group(1)
        d[f[0]] = v
    return d
for setname, tag in (("netlib", "nl"), ("kennington", "kn"), ("infeas", "inf")):
    ctl = load(f"{D}/ctl-{tag}.txt")
    if not ctl:
        continue
    print(f"==== {setname}, {len(ctl)} instances")
    for label, other in (("primal side", load(f"{D}/primal-{tag}.txt")),
                         ("both sides",  load(f"{D}/both-{tag}.txt"))):
        moved = dig = 0
        wr = []
        for n in ctl:
            if n not in other:
                continue
            if ctl[n] != other[n]:
                moved += 1
            if ctl[n].get("digest") != other[n].get("digest"):
                dig += 1
            try:
                a, b = float(ctl[n]["work"]), float(other[n]["work"])
                if a > 0:
                    wr.append(b / a)
            except Exception:
                pass
        g = math.exp(sum(math.log(x) for x in wr) / len(wr)) if wr else float("nan")
        print(f"  {label}: {moved} moved, {dig} digest change(s), "
              f"work geomean {g:.4f}x")
        for k in KEYS:
            bet = wor = 0
            wctl = woth = 0.0
            for n in ctl:
                if n not in other or k not in ctl[n] or k not in other[n]:
                    continue
                try:
                    x, y = float(ctl[n][k]), float(other[n][k])
                except Exception:
                    continue
                if y < x:
                    bet += 1
                elif y > x:
                    wor += 1
                wctl = max(wctl, x)
                woth = max(woth, y)
            if bet or wor:
                print(f"     {k:7s} better={bet:3d} worse={wor:3d}"
                      f"   worst over the set {wctl:.3g} -> {woth:.3g}")
    print()
PY
} | tee "$here/refinement-residual.txt"
