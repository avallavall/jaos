#!/usr/bin/env bash
# What can jaos_write_lp express, and what stops the rest? Writes
# lpcover.txt beside this script.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-138/run-lpcover.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$(dirname "$0")/../../.." || exit 1

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_write >/dev/null 2>&1 || { echo "library build failed"; exit 1; }
objs=$(ls build/dev/*.o | grep -v unity)

$CC -std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -O1 \
    -Iinclude -Isrc "$here/lpcover.c" $objs -o build/dev/lpcover -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d — run bench/fetch.sh"; exit 1; }
done

{
    echo "# jaos_write_lp coverage over every gate instance"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# write LP; where it succeeds, read back and compare every field with =="
    echo
    ./build/dev/lpcover bench/instances/*.mps bench/instances-infeas/*.mps \
        bench/instances-kennington/*.mps
} > "$here/lpcover.txt"
rc=$?

tail -4 "$here/lpcover.txt"
echo "lpcover exit=$rc"
exit $rc
