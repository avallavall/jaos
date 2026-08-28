#!/bin/bash
# PINNED: cee723a -- the anchors are code, not comments.
#
# Stage 8b, step two. `bland.txt` says the floor moves thirteen instances'
# phase-1 counts and arms Bland's rule on exactly ONE of them: `pilot87`, once,
# in 387235 iterations. Twelve movers stall zero times, and so do the three
# controls.
#
# One arming is not by itself a defect -- it is the stall detector doing its
# job. What decides 8b is what happened AFTER it armed:
#
#   armed late, progress resumed, solve ran on   -> the machinery works and
#                                                   the blow-up is a longer
#                                                   trajectory, not a broken
#                                                   finiteness argument;
#   armed early and never disarmed               -> no progress under Bland's
#                                                   rule, which is what a
#                                                   candidate set that shifts
#                                                   under the rule would do.
#
# `s->bland` is set false again the moment the phase-1 objective improves
# (`src/simplex.c`, the `last_gain` update), so a second arming means progress
# happened in between. n_bland = 1 therefore says: armed once, and either
# never disarmed, or disarmed and never stalled again. The DETAIL log
# separates those -- it prints the iteration at which the rule armed, and
# `run_primal_phase1` logs its infeasibility total as it goes.
#
# `pilot87` alone, at C=1, -j 1, DETAIL level. src/ is read, never written.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
root="$JAOS_ROOT"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
ln -s "$root/bench/instances" "$D/wt/bench/instances"
cd "$D/wt" || exit 2

python3 - "$D/wt/bench/primal.c" <<'PY'
import sys
bp = sys.argv[1]

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times: %r" % (n, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
static void diag_log(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    char buf[1024];
    int n = snprintf(buf, sizeof buf, "P87 %s\\n", line);
    if (n > 0 && n < (int)sizeof buf)
        (void)!write(2, buf, (size_t)n);
}
#endif""")

sub(bp, """    jaos_clear_basis(m);""",
"""    jaos_clear_basis(m);
#ifdef JAOS_DIAG
    (void)jaos_set_log_callback(m, diag_log, nullptr);
    (void)jaos_set_log_level(m, JAOS_LOG_DETAIL);
#endif""")
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"
         make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | grep -E 'error' | head
         exit 2; }

{
  echo "# tree: $(git rev-parse --short "$ref"), PIVOT_MARGIN = 1.0 as it ships"
  echo "# pilot87 only, -j 1, JAOS_LOG_DETAIL on the forced-primal solve"
  echo
  # NO `| head` here. It closes the pipe, the solver takes SIGPIPE, and the
  # run dies part-way -- which the first version of this script did at
  # iteration 59000, leaving `0 record lines written` as the only sign.
  # The progress line is every 1000 iterations (LOG_EVERY), so the whole
  # trace is a few hundred lines.
  ./build/bench/primal -j 1 -o "$D/p.txt" pilot87 2>&1 |
      grep -E "^P87 .*(Bland|switching|phase 1|infeasib|abandoned|optimal)"
  echo "# control: $(grep -c . "$D/p.txt" 2>/dev/null) record lines written"
  echo "# control: the run is complete only if a record line was written above"
} 2>&1 | tee "$here/pilot87-stall.txt"
