#!/bin/bash
# Regenerate bench/results/primal.txt on the shipping tree, and check it
# against the sweep's C=1 record.
#
# Two reasons this is not optional. The record is what SPECS.md quotes, and
# D207 already had it go a commit stale once. And the sweep read the constant
# from the environment while the shipping code is a constexpr, so the two are
# the same number only if they fold identically -- diffing the records is the
# canary for that.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2

make primal J=12 2>&1 | tail -4
echo
echo "== regenerated record against the sweep's C=1 =="
a=$(grep -E '^[a-z0-9]' bench/measurements/02-124/alpha-1.txt)
b=$(grep -E '^[a-z0-9]' bench/results/primal.txt)
if [ "$a" = "$b" ]; then
    echo "IDENTICAL -- the constexpr build and the environment build agree"
else
    echo "DIFFERS:"
    diff <(printf '%s\n' "$a") <(printf '%s\n' "$b") | head -20
fi
echo
echo "== verdict tally =="
awk '$2=="ok"||$2=="DISAGREE"||$2=="ERROR"||$2=="overrun"{n[$2]++} END {for (k in n) printf "%s=%d ", k, n[k]; print ""}' bench/results/primal.txt
