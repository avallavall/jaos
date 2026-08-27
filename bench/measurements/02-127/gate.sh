#!/bin/bash
# The loop's steps 2 and 4 for stage 2. `tests/` changed (a comment and the
# new asserts run in the dev build), so `make configs` is the honest run
# (D154); `make test` alone would fail on record-check until D212 exists,
# which is why configs is run through its pieces here and record-check is
# run on its own afterwards.
#
# The gate is expected to MOVE this time, on the three instances whose dual
# path reaches primal_cleanup: wood1p (169 calls), pilot87 (1), etamacro (1).
# Harris chooses a different pivot than the exact minimum did, so digests may
# change there. What must not change is the verdict and the checker's
# acceptance, per instance.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
echo "===== make configs ====="
make configs 2>&1 | grep -E '^==|configurations|broken|FAIL|record-check' | tail -12
echo
echo "===== the three gate sets ====="
make netlib netlib-infeas netlib-kennington J=12 2>&1 | grep -E 'gate:|baseline:' | tail -8
echo
echo "===== what moved ====="
git diff --stat bench/results/netlib.txt bench/results/netlib-infeas.txt bench/results/netlib-kennington.txt
git diff -U0 bench/results/ | grep -E '^[-+][a-z0-9]' | sed -E 's/ presolve=[^ ]*//; s/ ref=.*//' | cut -c1-150
echo "===== done ====="
