#!/usr/bin/env bash
# D270. numerics-reviewer's first finding: with the primal objective now a
# compensated pair, the dual side read the SUM HALF at two sites and both
# figures reach the gate. This puts the raw half back and watches the test
# written for it go red.
#
# Arm 2's tree is what the change looked like before the review. It is not a
# hypothetical: it was committed to the working tree and would have gone
# through the campaign.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 1
cp src/check.c /tmp/check.c.keep || exit 1

WATCH="read_with_its_compensation|two_large_halves_cancelling|Tests .* Failures"
run() { make build/dev/test_check >/dev/null 2>&1 || make test >/dev/null 2>&1
        ./build/dev/test_check 2>&1 | grep -E "$WATCH"; }

echo "=== ARM 0: the tree as it stands (all PASS) ==="
run

echo
echo "=== ARM 1: the two dual-side reads take the sum half again ==="
echo "expect: read_with_its_compensation FAILS, the cancelling test still PASSES"
perl -0pi -e 's/1\.0L \+ fabs\(pobj\) \+ fabsl\(true_dual_obj\)/1.0L + fabs(primal_obj) + fabsl(true_dual_obj)/' src/check.c
perl -0pi -e 's/\(double\)\(a\.pos \/ \(1\.0L \+ fabs\(pobj\)\)\)/(double)(a.pos \/ (1.0L + fabs(primal_obj)))/' src/check.c
echo "pobj reads left in the dual half (0 means both are back to the raw sum):"
grep -c "fabs(pobj)" src/check.c
run
cp /tmp/check.c.keep src/check.c

echo
echo "=== RESTORED (all PASS) ==="
run
echo "diff against keep:"; diff -q src/check.c /tmp/check.c.keep && echo "identical"
