#!/bin/bash
# PINNED: cee723a -- the anchors are code, not comments.
#
# THE CONTROL for what `pilot87-stall.txt` found.
#
# Under the floor, `pilot87`'s phase 1 runs 387235 iterations and its
# infeasibility -- a SUM OF BOUND VIOLATIONS, which the method must never
# increase -- goes 7.72e+13 down to 1.28e+12 by iteration 339000, then UP to
# 4.18e+18 by 359000 and 4.04e+20 by 379000, and ends alternating between
# 3.24653e+20 and 3.23341e+20. Bland's rule armed at 343682, inside that
# window. Two values repeating is what cycling looks like.
#
# THE QUESTION THIS ANSWERS. Is that the floor, or is it what this phase 1
# does whenever it runs long? `pilot87` cannot be compared against itself at
# C=0, because at C=0 it refuses at iteration 17165 and never gets near
# 343682. So the control is the OTHER instances that run phase 1 to the work
# limit, which they already did before the floor existed: `d6cube`, `scsd8`
# and `scrs8` all overrun at C=0 and at C=1 alike.
#
# HOW TO READ IT, fixed before the numbers are seen:
#   their infeasibility also rises by orders of magnitude   -> the divergence
#       is a pre-existing property of the primal phase 1 and D207 only made
#       `pilot87` reach it; the finding belongs to phase 1, not to the floor;
#   their infeasibility falls or holds at both settings     -> `pilot87`'s
#       divergence is something the floor introduced, and stage 8b is a real
#       defect that has to be answered before the floor is trusted.
#
# Both settings on each, so the floor's own effect is visible per instance.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
ln -s "$root/bench/instances" "$D/wt/bench/instances"
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/primal.c" <<'PY'
import sys
sx, bp = sys.argv[1], sys.argv[2]

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times: %r" % (n, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

sub(sx, "constexpr double PIVOT_MARGIN  = 1.0;",
"""#include <stdlib.h>
static double sweep_margin(void)
{
    static int done = 0;
    static double v = 1.0;
    if (!done) {
        const char *e = getenv("JAOS_PIVOT_MARGIN");
        v = e != nullptr ? atof(e) : 1.0;
        done = 1;
    }
    return v;
}
#define PIVOT_MARGIN sweep_margin()""")

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
static const char *g_diag_name = "?";
static void diag_log(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    char buf[1024];
    int n = snprintf(buf, sizeof buf, "DIV %s | %s\\n", g_diag_name, line);
    if (n > 0 && n < (int)sizeof buf)
        (void)!write(2, buf, (size_t)n);
}
#endif""")

sub(bp, """    jaos_clear_basis(m);""",
"""    jaos_clear_basis(m);
#ifdef JAOS_DIAG
    g_diag_name = e->name;
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

# The three cheapest instances that overrun INSIDE phase 1 at both settings.
# `degen3`, `dfl001` and `maros-r7` do too and are far more expensive.
#
# THE FIRST RUN OF THESE THREE WAS TOO SHORT TO ANSWER THE QUESTION. Their
# budgets end phase 1 at 1000-3000 iterations, and `pilot87` does not turn
# until ~341000. A control that stops 300000 iterations before the effect
# cannot see it. Pass a set on the command line to run the long ones:
# `dfl001` reaches 136695, which is the furthest any other instance gets.
SET="${*:-d6cube scsd8 scrs8}"
OUT="$here/divergence.txt"
[ $# -gt 0 ] && OUT="$here/divergence-$1.txt"

{
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  echo "# phase-1 infeasibility is a SUM OF BOUND VIOLATIONS: it must not rise."
  echo "# instances: $SET -- all three overrun inside phase 1 at both settings."
  for C in 0 1; do
    echo
    echo "## PIVOT_MARGIN=$C"
    # No `| head`: it closes the pipe and the solver dies part-way, which is
    # how the first pilot87 trace lost 85% of its run.
    JAOS_PIVOT_MARGIN="$C" ./build/bench/primal -j 3 -o "$D/p-$C.txt" $SET 2>&1 |
        grep -E '^DIV .*(phase 1, iter|Bland|switching|abandoned|work limit)'
    echo "# control: $(grep -c . "$D/p-$C.txt" 2>/dev/null) record lines written"
  done
} 2>&1 | tee "$OUT"
