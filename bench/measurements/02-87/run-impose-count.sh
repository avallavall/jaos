#!/bin/bash
# What would D97's §8d refusal cost?
#
# The design proposes that a first version decline to impose a bound from a
# row that already imposed one on another of its columns, because §8c's rank
# argument holds for one and breaks for two. Nobody has counted how much of
# the family that throws away, and the answer decides whether the refusal is
# cheap or guts the reduction.
#
# THE TREE IS 7c7375c, where the activity tightening exists. That is the
# minimal failing design D114 later refused, so these numbers describe THAT
# tightening and not a corrected one. What they are good for is the ORDER of
# the collision rate. 02-21 used the same commit for the same reason.
#
# A control runs first: the same tree, unpatched, must produce no IMPOSE
# records and the same verdicts. Without it a zero could mean the hook never
# compiled in rather than the family never firing.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
TREE=${1:-7c7375c}
d=$(mktemp -d) || exit 2
trap 'rm -rf "$d"' EXIT

git -C "$root" archive "$TREE" src include | tar -x -C "$d" || exit 2
cp -r "$d/src" "$d/src-control" || exit 2
python3 "$here/patch-impose-count.py" "$d" || exit 2

P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -I$d/include"
gcc-14 $P -I"$d/src-control" -DJAOS_DIAG "$d"/src-control/*.c \
    "$here/impose-driver.c" -o "$d/control" -lm || exit 2
gcc-14 $P -I"$d/src" -DJAOS_DIAG "$d"/src/*.c \
    "$here/impose-driver.c" -o "$d/probe" -lm || exit 2

{
    echo "tree: $TREE"
    echo "== control, unpatched: must emit no IMPOSE record"
    "$d/control" bench/instances/*.mps 2>&1 >/dev/null | grep -c IMPOSE \
        | sed 's/^/  IMPOSE records from the control: /'
} > "$here/impose-count.txt"

for set in netlib netlib-kennington; do
    case $set in
        netlib)            dir=bench/instances ;;
        netlib-kennington) dir=bench/instances-kennington ;;
    esac
    "$d/probe" "$dir"/*.mps 2> "$d/$set.raw" > /dev/null
    {
        echo ""
        echo "== $set"
        python3 "$here/read-impose.py" "$d/$set.raw"
    } >> "$here/impose-count.txt"
done
cat "$here/impose-count.txt"
