#!/usr/bin/env bash
# Exact evaluation of every gate instance's published point, against what
# jaos_check_solution reports for the same point. Writes exact-cover.txt
# beside this script.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-175/run-exact-cover.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$(dirname "$0")/../../.." || exit 1

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_exact >/dev/null 2>&1 || { echo "library build failed"; exit 1; }
objs=$(ls build/dev/*.o | grep -v unity)

$CC -std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -O1 -Iinclude -Isrc "$here/exact-cover.c" $objs -o build/dev/exact-cover -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
done

{
    echo "# jm_exact_evaluate over every gate instance's published point"
    echo "# instrument: bench/measurements/02-175/exact-cover.c"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# obj_ulps: distance between the checker's objective and the exact one"
    echo "# secs is a development number and belongs in no baseline"
    echo
    ./build/dev/exact-cover bench/instances/*.mps bench/instances-infeas/*.mps bench/instances-kennington/*.mps
} > "$here/exact-cover.txt"
rc=$?

tail -4 "$here/exact-cover.txt"
echo "exact-cover exit=$rc"
exit $rc
