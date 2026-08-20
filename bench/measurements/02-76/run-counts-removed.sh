#!/bin/bash
# Do D162's, D163's and D165's own models still hold with the shift counts
# gone? Their tests are the evidence, so this runs all five configurations on
# the candidate and names the five tests that matter.
#
# There is no probe here and that is deliberate. The claim is that four windows
# got NARROWER, and each was `base + ps_shift_excess(...)` with that term
# non-negative — so it holds by construction and a measurement of it would only
# be re-deriving arithmetic. What needs measuring is whether anything was
# relying on the extra width, and the suite plus the three gate sets answer
# that.
#
# **The worktree is in $(mktemp -d) and not under build/.** `make clean` is
# `rm -rf build` and `make configs` runs it five times; a worktree under build/
# is deleted by anyone else's build, mid-run, with no error on this side. That
# happened to a `jaos-measurer` campaign while this very change was being
# built. 44 older scripts in this directory still use `build/diag/wt-*`.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-HEAD}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT

git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || {
    echo "worktree add failed for $ref"; exit 2; }
cp "$root/src/presolve.c"        "$D/wt/src/presolve.c"
cp "$root/tests/test_presolve.c" "$D/wt/tests/test_presolve.c"

{
  echo "candidate = $ref plus the working tree's src/ and tests/"
  echo
  cd "$D/wt" || exit 2
  make configs >"$D/configs.log" 2>&1
  echo "make configs exit: $?   (five configurations, make clean between them)"
  echo
  echo "the five tests the counts were built for:"
  grep -E ":(PASS|FAIL|IGNORE)" "$D/configs.log" | grep -E \
    "counts_the_shifts_and_not|scales_by_the_end|singleton_fold_counts|folds_value_carries|frozen_rows_window_ignores" \
    | sed 's/^/   /' | sort -u
} | tee "$here/counts-removed.txt"

cd "$root" || exit 2
git worktree remove --force "$D/wt" >/dev/null 2>&1
git worktree prune
echo "readings in $here/counts-removed.txt"
