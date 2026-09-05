#!/usr/bin/env bash
# D278. The LP reader folds a constant inside a constraint instead of
# refusing it. The question this answers is the one the unit tests cannot:
# did the change move what `jaos_write_lp` can express and read back?
#
# 02-138's instrument again, unchanged, on a later tree. D276's reading lives
# in 02-181 and is NOT overwritten -- one file cannot carry two trees, and
# re-running that script in place is how D167 lost four lines of D162's
# evidence. This writes lpcover.txt beside THIS script.
#
# Expected: identical to 02-181's, 138 round-tripped, 1 refused, 0 differ.
# The writer never emits a constant inside a constraint, so a reader that
# accepts one more shape cannot change what comes back.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-183/run-lpcover.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../02-138"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
. bench/measurements/objs.sh
jaos_objs || exit 1

$CC $JAOS_OBJS_FLAGS -Iinclude -Isrc "$src/lpcover.c" $JAOS_OBJS_LIST \
    -o "build/lpcover-$JAOS_OBJS_KIND" -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
done

{
    echo "# jaos_write_lp coverage over every gate instance, after D278"
    echo "# instrument: bench/measurements/02-138/lpcover.c, unchanged"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# objects: $JAOS_OBJS_KIND"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# write LP; where it succeeds, read back and compare every field with =="
    echo
    "./build/lpcover-$JAOS_OBJS_KIND" bench/instances/*.mps \
        bench/instances-infeas/*.mps bench/instances-kennington/*.mps
} > "$here/lpcover.txt"
rc=$?

echo "--- this run ---"
tail -4 "$here/lpcover.txt"
echo "--- D276's, 02-181, for comparison ---"
tail -4 bench/measurements/02-181/lpcover.txt
echo "lpcover exit=$rc"
exit $rc
