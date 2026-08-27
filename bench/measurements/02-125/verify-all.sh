#!/bin/bash
# Independent re-verification of everything landed today, from the committed
# tree, in one run. Nothing here trusts an earlier result in this session.
#
# `make configs` rather than `make test` + `make sanitize` by hand: make does
# not track a change in EXTRA_CFLAGS, so running them separately re-runs the
# plain binaries and exits 0 (D154). This is the honest check.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2

echo "===== tree state ====="
git log --oneline -1
git status --short
echo

echo "===== record-check ====="
python3 tools/record-check.py 2>&1 | grep -E 'FAIL|PASS|failure'
echo

echo "===== make configs: all five build configurations ====="
make configs 2>&1 | grep -E '^==|configurations|broken' | tail -10
echo

echo "===== the three gate sets ====="
make netlib netlib-infeas netlib-kennington J=12 2>&1 |
    grep -E 'gate:|baseline:' | tail -8
echo

echo "===== did any committed record move? (must be empty) ====="
git status --short bench/results/
echo "(nothing above = every record reproduces from the committed tree)"
echo

echo "===== the primal campaign record ====="
make primal J=12 >/dev/null 2>&1
echo "primal.txt after regeneration:"
git status --short bench/results/primal.txt
awk '$2=="ok"||$2=="DISAGREE"||$2=="ERROR"||$2=="overrun"{n[$2]++} END {for (k in n) printf "%s=%d ", k, n[k]; print ""}' bench/results/primal.txt
echo

echo "===== make refusals ====="
make refusals 2>&1 | tail -12
echo
echo "===== git status after everything ====="
git status --short
echo "===== VERIFY DONE ====="
