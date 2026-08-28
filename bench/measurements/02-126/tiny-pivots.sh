#!/bin/bash
# PINNED: 078a862 -- the anchors are code, not comments.
#
# Stage 8d, step two. unguarded-pivots.sh refuted its own hypothesis -- the
# two pivot values agree at every pivot behind a rise -- and found the shape
# instead: the objective turns at iteration 341234 on a pivot element of
# 3.26e-09, and jumps by 2.1e+11 at 344067 on one of 1.76e-09 whose step was
# only 3.3e-05. A step that small cannot move the objective; a basis that has
# just admitted a 1e-9 pivot can, when xb is next recomputed from it.
#
# THE QUESTION HERE. Was 341234 the FIRST pivot that small in 341000
# iterations, or had phase 1 been pivoting on 1e-9 elements all along without
# harm? The first reading makes the tiny pivot the cause. The second makes it
# a symptom of something earlier. The control is dfl001 again.
#
# Whole-run counters only, no window: a histogram of |alpha| over every
# phase-1 pivot taken, and for each decade below 1e-4 the iteration of the
# first pivot in it and the objective at that moment.
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

CONST = 'constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */'
sub(sx, CONST, CONST + """
#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
/* decade d covers [1e(d-12), 1e(d-11)); d = 0 is below 1e-12, d = 15 is >= 1e3 */
static long long tp_hist[16];
static long long tp_first_iter[16];
static double    tp_first_total[16];
static double    tp_total_now;
static int       tp_ready;
void jaos_tp_dump(const char *name);
void jaos_tp_dump(const char *name)
{
    char b[1024];
    int n = snprintf(b, sizeof b, "TP %s hist=", name);
    for (int d = 0; d < 16 && n > 0 && n < (int)sizeof b; d++)
        n += snprintf(b + n, sizeof b - (size_t)n, "%lld%s", tp_hist[d],
                      d + 1 < 16 ? "," : "");
    for (int d = 0; d < 16 && n > 0 && n < (int)sizeof b; d++)
        if (tp_hist[d] > 0 && d <= 8)
            n += snprintf(b + n, sizeof b - (size_t)n,
                          " first[1e%d]=iter:%lld,total:%.6g",
                          d - 12, tp_first_iter[d], tp_first_total[d]);
    if (n > 0 && n < (int)sizeof b) {
        b[n++] = '\\n';
        (void)!write(2, b, (size_t)n);
    }
}
#endif""")

sub(sx, """        const double total = primal_phase1_costs(s);
        /* On a count and never on a clock (D8). */""",
"""        const double total = primal_phase1_costs(s);
#ifdef JAOS_DIAG
        tp_total_now = total;
#endif
        /* On a count and never on a clock (D8). */""")

sub(sx, """    *took = true;

    /* Primal step: row r lands exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;""",
"""    *took = true;

    /* Primal step: row r lands exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;
#ifdef JAOS_DIAG
    if (s->in_phase1) {
        if (!tp_ready) {
            for (int d = 0; d < 16; d++) tp_first_iter[d] = -1;
            tp_ready = 1;
        }
        const double a = fabs(alpha_q);
        int d = a > 0.0 ? (int)floor(log10(a)) + 12 : 0;
        if (d < 0) d = 0;
        if (d > 15) d = 15;
        tp_hist[d]++;
        if (tp_first_iter[d] < 0) {
            tp_first_iter[d] = s->iters;
            tp_first_total[d] = tp_total_now;
        }
    }
#endif""")

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
void jaos_tp_dump(const char *name);
#endif""")

sub(bp, """                measure_one(&ents[sel[launched]], dir, factor, &r);""",
"""                measure_one(&ents[sel[launched]], dir, factor, &r);
#ifdef JAOS_DIAG
                jaos_tp_dump(ents[sel[launched]].name);
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
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  echo "# hist: every phase-1 pivot taken, by decade of |alpha| -- 16 buckets,"
  echo "#   bucket k is [1e(k-12), 1e(k-11)); bucket 0 is below 1e-12, 15 is >= 1e3."
  echo "# first[1eD]: the iteration of the first pivot with |alpha| in [1eD, 1eD+1),"
  echo "#   and the phase-1 objective at the top of that iteration."
  echo
  ./build/bench/primal -j 2 -o "$D/p.txt" pilot87 dfl001 2>&1 | grep -E '^TP '
  echo "# control: $(grep -c . "$D/p.txt" 2>/dev/null) record lines written"
} 2>&1 | tee "$here/tiny-pivots.txt"
