#!/usr/bin/env bash
# D284. What jaos_write_lp can express once it writes the model's own names
# rather than C1..Cn, and whether every name comes back.
#
# 02-138's instrument with the names compared too (lpcover-names.c beside
# this script). 02-181's reading was D276's and is not overwritten: one file
# cannot carry two trees. This writes lpcover.txt beside THIS script.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-188/run-lpcover.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
. bench/measurements/objs.sh
jaos_objs || exit 1

$CC $JAOS_OBJS_FLAGS -Iinclude -Isrc "$here/lpcover-names.c" $JAOS_OBJS_LIST \
    -o "build/lpcover-names-$JAOS_OBJS_KIND" -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
done

{
    echo "# jaos_write_lp coverage over every gate instance, names included"
    echo "# instrument: bench/measurements/02-188/lpcover-names.c"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# objects: $JAOS_OBJS_KIND"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# write LP; where it succeeds, read back and compare every field and every name with =="
    echo
    "./build/lpcover-names-$JAOS_OBJS_KIND" bench/instances/*.mps \
        bench/instances-infeas/*.mps bench/instances-kennington/*.mps
} > "$here/lpcover.txt"
rc=$?

tail -4 "$here/lpcover.txt"
echo "lpcover exit=$rc"
exit $rc
