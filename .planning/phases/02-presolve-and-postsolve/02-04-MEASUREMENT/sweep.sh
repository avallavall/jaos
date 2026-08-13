#!/usr/bin/env bash
# 02-04's sweep: the two constants src/presolve.c declares, measured rather
# than chosen (D-02, D91, and CLAUDE.md's "every number needs a measurement
# on both sides").
#
# Run from inside WSL, from the repository root:
#     bash .planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/sweep.sh eps
#     bash .planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/sweep.sh rounds EPSVALUE
#
# Two rules this script exists to enforce, because both have been got wrong
# in this repository before and both fail silently:
#
#   1. `make clean` between EVERY setting. Without it `make` cannot see an
#      EXTRA_CFLAGS change, one binary is measured N times, and the sweep
#      reports a flat line that looks like a result (D82).
#   2. A canary that MUST move between the ends of the grid, checked before
#      any row is believed. `canary.c` carries one model per constant.
#
# Nothing here writes to bench/results/ or to any baseline: the runner is
# invoked directly with -o pointing into this directory and with no -b, so
# the committed record and the three baselines are untouched. 02-07 owns the
# baseline rewrite, not this plan.
set -u

REPO=$(git rev-parse --show-toplevel) || exit 1
cd "$REPO" || exit 1
OUT=$REPO/.planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT
mkdir -p "$OUT"

MODE=${1:-eps}
FIXED_EPS=${2:-1e-9}
FIXED_ROUNDS=${3:-8}

# Rows and columns removed, summed over the whole set, out of the runner's
# own presolve=R/C/NZ->R/C/NZ field.
removed() {
    awk '{ for (i=1;i<=NF;i++) if ($i ~ /^presolve=/) {
             split(substr($i,10), a, "->");
             split(a[1], b, "/"); split(a[2], c, "/");
             r += b[1]-c[1]; k += b[2]-c[2]; n += b[3]-c[3] } }
         END { printf "%d %d %d", r, k, n }' "$1"
}

summary() { grep -E "^94 instances|^29 instances" "$1" | head -1; }

run_setting() {
    local tag="$1" eps="$2" rounds="$3"
    make -j12 clean > /dev/null 2>&1
    make -j12 EXTRA_CFLAGS="-DJAOS_PRESOLVE_TIGHTEN_EPS_VALUE=$eps -DJAOS_PRESOLVE_ROUNDS_VALUE=$rounds" \
        build/bench/run > "$OUT/build-$tag.log" 2>&1 || { echo "$tag BUILD FAILED"; return 1; }
    gcc-14 -std=c23 -O2 -Iinclude -Isrc -ffp-contract=off \
        -DJAOS_PRESOLVE_TIGHTEN_EPS_VALUE=$eps -DJAOS_PRESOLVE_ROUNDS_VALUE=$rounds \
        "$OUT/canary.c" build/release/libjaos.a -o build/canary -lm \
        >> "$OUT/build-$tag.log" 2>&1 || { echo "$tag CANARY BUILD FAILED"; return 1; }

    ./build/canary > "$OUT/canary-$tag.txt"
    local can
    can=$(tr '\n' ' ' < "$OUT/canary-$tag.txt")

    local t0 t1
    t0=$(date +%s)
    ./build/bench/run -j 12 -o "$OUT/netlib-$tag.txt" > "$OUT/netlib-$tag.gate" 2>&1
    t1=$(( $(date +%s) - t0 ))

    ./build/bench/run -j 12 -m bench/netlib-infeas.manifest -e infeasible \
        -d bench/instances-infeas -o "$OUT/infeas-$tag.txt" \
        > "$OUT/infeas-$tag.gate" 2>&1

    echo "SETTING $tag eps=$eps rounds=$rounds"
    echo "  canary   $can"
    echo "  netlib   $(summary "$OUT/netlib-$tag.gate")"
    echo "  removed  rows/cols/nz = $(removed "$OUT/netlib-$tag.txt")"
    echo "  wall     ${t1}s (J=12, inflated by design -- this is a sweep step, not a time ratio)"
    echo "  infeas   $(summary "$OUT/infeas-$tag.gate")"
    echo "  notopt   $(awk 'NF>6 && $2 != "optimal" {printf "%s=%s ", $1, $2}' "$OUT/netlib-$tag.txt")"
    echo
}

case "$MODE" in
eps)
    for e in 1e-12 1e-11 1e-10 1e-9 1e-8 1e-7 1e-6 1e-5 1e-4; do
        run_setting "eps-$e" "$e" "$FIXED_ROUNDS"
    done
    ;;
rounds)
    for r in 1 2 4 8 16 32 64 128; do
        run_setting "rounds-$r" "$FIXED_EPS" "$r"
    done
    ;;
*)
    echo "usage: sweep.sh [eps|rounds] [eps-value] [rounds-value]" >&2
    exit 2
    ;;
esac

make -j12 clean > /dev/null 2>&1
