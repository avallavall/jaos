#!/bin/bash
# What tightening DUAL_TOL costs over an instance set. One binary, several
# settings, no rebuild — the constant is caller-owned (D64), so the sweep
# goes through jaos_set_dual_tolerance and cannot accidentally measure one
# binary N times (D154, and the sweeping-a-constant note).
#
# Sequential on purpose: work units are deterministic, so parallelism would
# buy wall clock only, and a shared stderr is how three readings of one
# counter came out different (the probe-output note).
#
# Usage: run-dual-tol-sweep.sh [manifest] [settings]
#   manifest  default bench/netlib.manifest
#   settings  comma-separated; default is the full seven-point sweep.
#             A set whose instances cost minutes each wants "0,1e-9".
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/dual-tol-sweep.c" -o "$D/s" -lm || exit 2

manifest=${1:-bench/netlib.manifest}
settings=${2:-}
tag=$(basename "$manifest" .manifest)
case "$manifest" in
    *kennington*) dir=bench/instances-kennington ;;
    *infeas*)     dir=bench/instances-infeas ;;
    *)            dir=bench/instances ;;
esac

if [ -n "$settings" ]; then
    "$D/s" --settings "$settings" "$manifest" "$dir"/*.mps \
        | tee "$here/dual-tol-sweep-$tag.txt"
else
    "$D/s" "$manifest" "$dir"/*.mps | tee "$here/dual-tol-sweep-$tag.txt"
fi
