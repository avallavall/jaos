#!/usr/bin/env bash
# What can jaos_write_lp express at HEAD, and what stops the rest?
#
# The same instrument as 02-138's, on a later tree: D239 taught jaos_read_lp
# a ranged constraint, so the writer stopped refusing one. 02-138's own
# lpcover.txt is D226's reading and is deliberately not overwritten -- one
# file cannot carry two trees. This writes lpcover.txt beside THIS script.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-172/run-lpcover.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../02-138"
cd "$(dirname "$0")/../../.." || exit 1

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_write >/dev/null 2>&1 || { echo "library build failed"; exit 1; }
objs=$(ls build/dev/*.o | grep -v unity)

$CC -std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -O1 -Iinclude -Isrc "$src/lpcover.c" $objs -o build/dev/lpcover -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
done

{
    echo "# jaos_write_lp coverage over every gate instance"
    echo "# instrument: bench/measurements/02-138/lpcover.c, unchanged"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# write LP; where it succeeds, read back and compare every field with =="
    echo
    ./build/dev/lpcover bench/instances/*.mps bench/instances-infeas/*.mps bench/instances-kennington/*.mps
} > "$here/lpcover.txt"
rc=$?

tail -4 "$here/lpcover.txt"
echo "lpcover exit=$rc"
exit $rc
