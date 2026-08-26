#!/bin/bash
# Does test_the_summary_separates_phase_1_from_phase_2 fail when the counter
# is broken? A test that only passes is half a test.
#
# The injected fault is the defect class itself: n_phase1_iters left at zero on
# a solve where phase 1 ran, which is exactly what reading a success-only log
# line produced and what made D194 wrong.
#
# It prints every FAIL line, because "make test came back non-zero" is not the
# same claim as "this test caught it" — a build break would also come back
# non-zero and would prove nothing.
#
# Worktree under mktemp -d, OUTSIDE the repository (make clean is rm -rf build).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
cd "$D/wt" || exit 2

# The fault must leave phase1_entered used, or -Werror=unused-variable
# breaks the build and the control proves nothing instead of failing the
# test. That happened on the first attempt and is why this is two
# statements rather than one.
sed -i 's|^        s->n_phase1_iters = s->iters - phase1_entered;|        s->n_phase1_iters = s->iters - phase1_entered; s->n_phase1_iters = 0;  /* INJECTED FAULT */|' src/simplex.c
grep -q 'INJECTED FAULT' src/simplex.c || { echo "the substitution did not apply"; exit 2; }

out="$D/out.txt"
{
  echo "# negative control for test_the_summary_separates_phase_1_from_phase_2"
  echo "# tree: $ref with n_phase1_iters forced to 0 in run_primal"
  echo "# the test MUST appear as a FAIL below, or it is not evidence"
  echo
  make test > "$out" 2>&1
  echo "make test rc=$?"
  echo
  echo "## every FAIL line:"
  grep -E ":FAIL" "$out" || echo "  (none - the build broke instead, which proves nothing)"
  echo
  echo "## suite totals:"
  grep -E "Tests .* Failures" "$out"
} 2>&1 | tee "$here/validate-test.txt"
