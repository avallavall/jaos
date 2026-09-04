#!/bin/bash
# Remove each D268 repair, confirm its test fails, restore. A test that
# passes on the broken tree is not evidence.
#
# Every arm edits src/exact.c in place and puts it back from a copy. The
# last line of the output diffs the file against that copy, so a run that
# left the tree wrong says so rather than leaving it to be discovered.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 1
cp src/exact.c /tmp/exact.c.keep || exit 1

WATCH="subnormal|refused_walk|exponent_that_does_not_fit|wider_than_the_limbs"

run() { make build/dev/test_exact >/dev/null 2>&1 || make test >/dev/null 2>&1
        ./build/dev/test_exact 2>&1 | grep -E "^tests/test_exact.c:.*($WATCH)"; }

echo "=== ARM 0: the tree as it stands (all must PASS) ==="
run

echo
echo "=== ARM 1: the dyadic subnormal repair removed ==="
echo "expect: test_a_subnormal_result_is_rounded_once FAILS, rest PASS"
sed -i '/if (-1074 - d->e > drop)/,+1d' src/exact.c
run
cp /tmp/exact.c.keep src/exact.c

echo
echo "=== ARM 2: the NaN poison removed ==="
echo "expect: the two evaluator tests FAIL, rest PASS"
sed -i 's|out->objective = (double)NAN;|out->objective = p.objective;|;
        s|out->row_violation = (double)NAN;|out->row_violation = p.row_violation;|;
        s|out->col_violation = (double)NAN;|out->col_violation = p.col_violation;|;
        s|^    out->row_at = -1;$|    out->row_at = p.row_at;|;
        s|^    out->col_at = -1;$|    out->col_at = p.col_at;|' src/exact.c
run
cp /tmp/exact.c.keep src/exact.c

echo
echo "=== ARM 3: the rational subnormal repair removed ==="
echo "expect: test_a_rational_subnormal_is_rounded_once FAILS, rest PASS"
sed -i '/if (shift - 1074 > drop)/,+1d' src/exact.c
run
cp /tmp/exact.c.keep src/exact.c

echo
echo "=== ARM 4: the exponent overflow guards removed ==="
echo "expect: test_an_exponent_that_does_not_fit_is_refused FAILS, rest PASS"
echo "this arm's tree has signed overflow in it, which is the finding: the"
echo "sum is undefined, and whatever it produces is not a refusal."
perl -0pi -e 's/    if \(ckd_add\(&r->e, a->e, b->e\)\)\n        return false;/    r->e = a->e + b->e;/' src/exact.c
perl -0pi -e 's/    int64_t diff;\n    if \(ckd_sub\(&diff, hi->e, lo->e\)\)\n        return false;[^\n]*\n/    const int64_t diff = hi->e - lo->e;\n/' src/exact.c
echo "ckd_ calls left in src/exact.c (0 means both guards are out):"
grep -c "ckd_" src/exact.c
run
cp /tmp/exact.c.keep src/exact.c

echo
echo "=== RESTORED: rebuilding and re-running (all must PASS) ==="
run
echo "diff against keep:"; diff -q src/exact.c /tmp/exact.c.keep && echo "identical"
