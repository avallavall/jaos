#!/bin/bash
# PINNED: cee723a -- the anchors are code, not comments.
#
# TODO.md section 0, stage 8b. `numerics-reviewer` raised it on D207's diff and
# `jaos-measurer` did not close it.
#
# THE QUESTION. Bland's rule needs the lowest-index basic among those attaining
# the minimum ratio, over a FIXED candidate set. `PIVOT_MIN` narrowed that set
# the same way for every column. `PIVOT_MARGIN * eps * cmax` narrows it PER
# COLUMN, because `cmax` is that column's own norm. So at a degenerate vertex a
# row can be eligible for `q1` and rejected for `q2`, the set the tie-break
# ranges over changes between iterations, and Bland's finiteness argument no
# longer covers it. Determinism is not at risk -- the choice is a function of
# the data and identical on every machine. TERMINATION is.
#
# The evidence that raised it is circumstantial and pointed: `pilot87`'s phase 1
# goes 17165 -> 387235 iterations at C=1, a 22x blow-up ending in `overrun`.
# That fits the floor persistently excluding the row Bland wants.
#
# WHAT THIS MEASURES. `n_bland` is the count of times a solve gave up on
# Dantzig and armed Bland's rule after a stall. The solver already prints it,
# as "stalls", in its own JAOS_LOG_SUMMARY line -- `bench/primal` just installs
# no callback, so it is silent. This patch installs one and reads the number
# off both solves, at C=0 and C=1, on the fifteen instances the floor moves.
#
# HOW TO READ IT. If `n_bland` rises on the instances whose phase-1 iterations
# rise, the floor is fighting the anti-cycling rule and 8b is a real defect. If
# `n_bland` is unchanged while the iterations move, the blow-up is the floor
# choosing different pivots, which is a trajectory and not a termination
# problem. If `n_bland` is zero everywhere, the rule never armed at all and the
# question cannot be answered on this population -- which is itself the answer
# to record.
#
# Nothing here is billed and no solver state is touched. src/ is read, never
# written: the patch lands in a worktree.
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

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/primal.c" <<'PY'
import sys
sx, bp = sys.argv[1], sys.argv[2]

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times in %s: %r" % (n, path, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

# PIVOT_MARGIN from the environment, so one binary serves both settings and
# the sweep cannot measure one build twice (D154).
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

# A log callback on the forced-primal solve only. The dual's line would be
# the same on both settings -- the floor is not reached from the dual path --
# and printing it doubles the output for nothing.
# After <unistd.h>, not before it: the block below calls write(2), and
# bench/primal.c includes sys/wait.h two lines earlier than unistd.h.
sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
static const char *g_diag_name = "?";
static void diag_log(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    if (level != JAOS_LOG_SUMMARY)
        return;
    /* One write per record: `-j N` shares one stderr (D57's twin). */
    char buf[1024];
    int n = snprintf(buf, sizeof buf, "BLAND %s | %s\\n", g_diag_name, line);
    if (n > 0 && n < (int)sizeof buf)
        (void)!write(2, buf, (size_t)n);
}
#endif""")

sub(bp, """    jaos_clear_basis(m);""",
"""    jaos_clear_basis(m);
#ifdef JAOS_DIAG
    g_diag_name = e->name;
    (void)jaos_set_log_callback(m, diag_log, nullptr);
    (void)jaos_set_log_level(m, JAOS_LOG_SUMMARY);
#endif""")
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"
         make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | grep -E 'error' | head
         exit 2; }

# The fifteen the floor moves at C=1, plus three controls it provably cannot
# reach (min r far above 1 in census.txt). A control that moves means the
# harness moved something, not the floor.
MOVED="d2q06c d6cube dfl001 greenbea perold pilot pilot-ja pilot4 pilot87 scsd1 scsd6 scsd8 stair tuff woodw"
CONTROLS="25fv47 degen2 maros"

{
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  echo "# 'stalls' in each line is n_bland: how many times the solve gave up on"
  echo "# Dantzig and armed Bland's rule. Forced-primal solve only."
  echo "# instances: $(echo $MOVED | wc -w) moved + $(echo $CONTROLS | wc -w) controls"
  for C in 0 1; do
    echo
    echo "## PIVOT_MARGIN=$C"
    JAOS_PIVOT_MARGIN="$C" ./build/bench/primal -j 12 -o "$D/p-$C.txt" \
        $MOVED $CONTROLS 2>&1 | grep '^BLAND ' | sort
    echo "# control: $(grep -c . "$D/p-$C.txt" 2>/dev/null) record lines written"
  done
} 2>&1 | tee "$here/bland.txt"
