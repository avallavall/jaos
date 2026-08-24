#!/bin/bash
# D177 — what RSUB_FLOOR costs, and where the knee is.
#
# Reads only committed records. Runs no solve, builds nothing, and takes no
# reading that depends on the tree, so it is valid from any checkout that has
# 02-81 and 02-83 in it.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
for f in bench/results/netlib.txt bench/results/netlib-kennington.txt \
         bench/measurements/02-81/gate-diff.txt \
         bench/measurements/02-83/exact-objective-netlib.txt \
         bench/measurements/02-83/exact-objective-netlib-kennington.txt; do
    [ -r "$f" ] || { echo "missing $f" >&2; exit 2; }
done
python3 "$here/rsub-coverage.py" | tee "$here/rsub-coverage.txt"
