#!/bin/bash
# D186 — how many mapped bases arrive LONG?
#
# `build_warm_basis` refuses a long count with a stated premise:
#
#   "A LONG count is still refused: no long map has been measured, and a
#    demotion rule for an unmeasured case would be a constant fitted to
#    nothing."
#
# Nobody has counted. This project has had a refusal's premise expire unnoticed
# three times (D24, D94, D101), so a premise that says "no X has been measured"
# is worth measuring rather than believing.
#
# It matters beyond the bookkeeping. The rank argument §2 has waited for is
# needed at POSTSOLVE, which has no factorization. `build_warm_basis` runs
# inside the solver, and its own comment says rank stays where it already
# lives — `repair_singular_basis`, which runs downstream of it. So a demotion
# HERE would need no new rank machinery, and D179 already measured the supply
# it would draw on: 19 of 24 instances covered.
#
# Both sets, -j 1, because the DIAG lines go to stderr and belong to the
# instance named just before them.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
probe="$root/bench/measurements/02-90/patch-warm-probe.py"
cd "$root" || exit 2

python3 "$probe" apply || exit 2
trap 'python3 "$probe" revert' EXIT
mkdir -p build/diag
gcc-14 -std=c23 -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-census -lm 2>/dev/null || exit 2

./build/diag/warm-census -j 1 >"$here/census-netlib-stdout.txt" \
                              2>"$here/census-netlib.txt"
echo "netlib exit=$?"
./build/diag/warm-census -j 1 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington >"$here/census-kennington-stdout.txt" \
                                  2>"$here/census-kennington.txt"
echo "kennington exit=$?"

for f in "$here/census-netlib.txt" "$here/census-kennington.txt"; do
    n=$(grep -c DIAG-MAPPED "$f" || true)
    echo "$(basename "$f"): $n DIAG-MAPPED lines"
    if [ "$n" -eq 0 ]; then
        echo "CANARY FAILED: build_warm_basis was never reached, so this measured nothing" >&2
        exit 3
    fi
done
python3 "$here/census.py" "$here/census-netlib.txt" "$here/census-kennington.txt" \
    | tee "$here/census.txt"
