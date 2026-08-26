#!/bin/bash
# Is the s->col assert reached by the suite, and does it catch a violation?
#
# Two questions, one experiment: corrupt s->col immediately before the assert.
# If a test aborts, the assert both runs and works. If everything passes, the
# assert is a comment with extra steps and its green result means nothing.
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

sed -i 's|^        double \*chk = jm_alloc_array(s->nrow, sizeof \*chk);|        if (s->nrow > 0) s->col[0] += 1.0;   /* INJECTED FAULT */\n        double *chk = jm_alloc_array(s->nrow, sizeof *chk);|' src/simplex.c
grep -q 'INJECTED FAULT' src/simplex.c || { echo "the substitution did not apply"; exit 2; }

out="$D/out.txt"
{
  echo "# negative control for the s->col contract assert"
  echo "# tree: $ref with s->col[0] perturbed just before the check"
  echo "# the suite MUST fail below, or the assert is never reached"
  echo
  make test > "$out" 2>&1
  echo "make test rc=$?"
  echo
  echo "## assertion failures and aborts:"
  grep -E "Assertion|assert|Aborted|:FAIL" "$out" | head || echo "  (none)"
  echo
  echo "## suite totals:"
  grep -E "Tests .* Failures" "$out"
} 2>&1 | tee "$here/validate-assert.txt"
