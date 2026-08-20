#!/bin/bash
# How often does the configuration §8d actually names occur?
#
# 02-87 counted rows that impose bounds on two or more of their columns, which
# is what §8d's refusal declines. The rank argument breaks only when two of
# those columns also REST at those bounds in the final solution, and that is
# strictly rarer. This measures it, so the refusal can be judged against the
# hazard rather than against its own proxy.
#
# Tree is 7c7375c, where the activity tightening exists, with 02-87's caveat:
# it is the design D114 refused, so the ratio is the finding and not the
# absolute count.
#
# The control comes first and must report zero imposed bounds, so a zero
# anywhere below means the family did not fire rather than the hook did not
# compile.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
TREE=${1:-7c7375c}
d=$(mktemp -d) || exit 2
trap 'rm -rf "$d"' EXIT

git -C "$root" archive "$TREE" src include | tar -x -C "$d" || exit 2
python3 "$here/patch-rested.py" "$d" || exit 2

P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG"
gcc-14 $P -I"$d/include" -I"$d/src" "$d"/src/*.c "$here/rested-driver.c" \
    -o "$d/probe" -lm || exit 2

{
    echo "tree: $TREE"
    echo ""
    echo "== netlib"
    "$d/probe" bench/instances/*.mps
    echo ""
    echo "== netlib-kennington"
    "$d/probe" bench/instances-kennington/*.mps
} | tee "$here/rested.txt"
