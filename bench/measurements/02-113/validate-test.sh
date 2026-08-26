#!/bin/bash
# Does test_a_watcher_can_stop_the_primal_phase_1 fail when phase 1 stops
# offering the callback? A test that only passes is half a test.
#
# The injection restores the pre-change behaviour exactly: the block is guarded
# on !s->in_phase1, so it compiles cleanly and simply never runs inside phase 1.
# Guarding on `false` would be dead code and -Werror is in play.
#
# It prints every FAIL line, because "make test came back non-zero" is not the
# same claim as "this test caught it".
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

python3 - "$D/wt/src/simplex.c" <<'PY'
import sys
OLD = """        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations in the primal phase 1 """
NEW = OLD.replace("        if (s->m->cfg.progress_cb != nullptr &&",
                  "        if (!s->in_phase1 &&      /* INJECTED FAULT */\n"
                  "            s->m->cfg.progress_cb != nullptr &&", 1)
p = sys.argv[1]
src = open(p, encoding="utf-8").read()
if src.count(OLD) != 1:
    sys.exit("the phase-1 callback block did not match exactly once")
open(p, "w", encoding="utf-8").write(src.replace(OLD, NEW))
print("injected")
PY
grep -q 'INJECTED FAULT' src/simplex.c || { echo "the substitution did not apply"; exit 2; }

out="$D/out.txt"
{
  echo "# negative control for test_a_watcher_can_stop_the_primal_phase_1"
  echo "# tree: $ref with phase 1's progress callback guarded off"
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
