#!/bin/bash
# Does anything read the reduced model's objective offset?
#
# TODO.md carries presolve's `obj_offset` as the last naive accumulation of
# D168's shape and says nothing reads it for the answer. Reading the code says
# the same: it lands only in `reduced.objective`, and both postsolve paths
# recompute the caller's objective from the caller's own model. This measures
# it instead — the offset is replaced with a value that is obviously wrong,
# and the three sets are run.
#
# TWO COMPARISONS, and the first is the one that matters:
#
#   poison against CONTROL — the same harness with no poison. This isolates
#   the poison from everything the harness itself does differently.
#   control against the COMMITTED RECORD — this says whether the harness
#   reproduces the gate at all. The first version of this script omitted
#   `-e infeasible`, so the control's own infeasible records differed from
#   the committed ones on all 29 instances; without the control that would
#   have read as a poison effect on one set and a clean result on two.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
out="$here/poison-offset.txt"
W=$(mktemp -d) || exit 2
trap 'rm -rf "$W"' EXIT
: > "$out"

SETS="netlib netlib-infeas netlib-kennington"

run_one() {
    local tag=$1 poison=$2
    local d="$W/$tag"
    mkdir -p "$d" || return 2
    git -C "$root" archive HEAD src include | tar -x -C "$d" || return 2
    if [ "$poison" != none ]; then
        python3 "$here/patch-poison-offset.py" "$d" "$poison" >> "$out" || return 2
    else
        echo "control: HEAD unmodified" >> "$out"
    fi
    gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
        -I"$d/include" -I"$d/src" "$d"/src/*.c bench/run.c -o "$d/run" -lm \
        >> "$out" 2>&1 || return 2

    local set dir man extra
    for set in $SETS; do
        extra=""
        case $set in
            netlib)            dir=bench/instances ;;
            netlib-infeas)     dir=bench/instances-infeas; extra="-e infeasible" ;;
            netlib-kennington) dir=bench/instances-kennington ;;
        esac
        man=bench/$set.manifest
        # shellcheck disable=SC2086
        "$d/run" -d "$dir" -m "$man" $extra -j 12 -o "$d/$set.txt" \
            -b "bench/$set.baseline" > "$d/$set.console" 2>&1
        grep -E "^gate:" "$d/$set.console" | sed "s/^/$tag $set /" >> "$out"
    done
}

run_one control none || { echo "control failed" >> "$out"; cat "$out"; exit 2; }

echo "" >> "$out"
echo "== does the harness reproduce the committed gate records?" >> "$out"
for set in $SETS; do
    if diff -q "$W/control/$set.txt" "bench/results/$set.txt" > /dev/null 2>&1; then
        echo "  control $set: BIT-IDENTICAL to bench/results/$set.txt" >> "$out"
    else
        echo "  control $set: DIFFERS from the committed record" >> "$out"
        diff "$W/control/$set.txt" "bench/results/$set.txt" | head -4 >> "$out"
    fi
done

for poison in big nan; do
    echo "" >> "$out"
    run_one "poison-$poison" "$poison" || { echo "poison-$poison failed" >> "$out"; continue; }
    echo "== poison-$poison against the control, which is the isolating test" >> "$out"
    for set in $SETS; do
        if diff -q "$W/poison-$poison/$set.txt" "$W/control/$set.txt" > /dev/null 2>&1; then
            echo "  $set: BIT-IDENTICAL to the control" >> "$out"
        else
            echo "  $set: DIFFERS from the control -- the value is READ" >> "$out"
            diff "$W/poison-$poison/$set.txt" "$W/control/$set.txt" | head -6 >> "$out"
        fi
    done
done
cat "$out"
