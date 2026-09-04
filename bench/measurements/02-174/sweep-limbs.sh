#!/bin/bash
# D268 finding 9, tested rather than argued.
#
# jm_rational_cmp answers 0 both for "equal" and for "the cross-multiply
# did not fit". JM_EXACT_LIMBS is documented as sweepable, so the question
# is whether test_a_dyadic_agrees_with_the_general_rational would keep
# passing at a capacity where its comparator can no longer compare.
#
# 70 limbs is the interesting setting: the dyadic multiply wants 4 limbs
# and the rational multiply 68, so both still fit, and only the comparator
# overflows. The test now asserts the cross-multiply's width first, so it
# must go RED at 70 and stay green at 128.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 1

CC=gcc-14
FLAGS="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -Og
       -Iinclude -Isrc -Itests/vendor/unity -DUNITY_INCLUDE_DOUBLE"
SRC="tests/test_exact.c tests/vendor/unity/unity.c src/alloc.c src/check.c
     src/exact.c src/iis.c src/inflate.c src/lpfmt.c src/lu.c src/model.c
     src/mps.c src/presolve.c src/ranging.c src/scale.c src/simplex.c
     src/status.c src/util.c src/version.c src/write.c"

for n in 128 70; do
    echo "=== JM_EXACT_LIMBS=$n ==="
    out=/tmp/test_exact_$n
    if ! $CC $FLAGS -DJM_EXACT_LIMBS=$n $SRC -o "$out" -lm 2>/tmp/build_$n.err; then
        echo "BUILD FAILED"; tail -5 /tmp/build_$n.err; continue
    fi
    "$out" 2>&1 | grep -E "agrees_with_the_general_rational|Tests .* Failures"
done

echo
echo "Expected: 128 PASS, 70 FAIL on the width assertion. A PASS at 70"
echo "would mean the comparison is answering 0 without comparing."
