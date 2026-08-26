#!/bin/bash
# The loop's steps 2 and 4 for D207's candidate.
#
# `make configs` first, and not `make test` + `make sanitize` by hand: `make`
# does not track a change in EXTRA_CFLAGS, so running them separately re-runs
# the plain binaries and exits 0. Three of five configurations were broken for
# a whole session that way (D154). `tests/test_simplex.c` changed here, which
# is exactly the case CLAUDE.md names for `make configs`.
#
# Then all three gate sets, every time. The census says the floor cannot move
# any of them -- `primal_ratio_test` is reached by 3 of the 94 standard
# instances, lowest ratio 5.4855, and by none at all on infeas -- so every
# digest and every work figure must come back byte-identical. That is the
# prediction this run tests.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2

echo "===== make configs ====="
make configs 2>&1 | tail -12
echo

echo "===== the three gate sets ====="
make netlib netlib-infeas netlib-kennington J=12 2>&1 | grep -E \
    'gate:|baseline:|regressed|FAIL|error|^== ' | tail -40
echo
echo "===== done ====="
