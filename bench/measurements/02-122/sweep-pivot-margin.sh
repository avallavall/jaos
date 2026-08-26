#!/bin/bash
# PINNED: 1fe8bc6 -- the anchors are code, not comments.
#
# Stage 8's constant, swept on both sides. The candidate:
#
#   min_pivot = max(PIVOT_MIN, PIVOT_MARGIN * DBL_EPSILON * max_i |col[i]|)
#
# applied in BOTH primal ratio tests, in place of the absolute PIVOT_MIN.
# census.txt says which instances each setting can possibly move: a solve's
# trajectory changes iff its min_r is below PIVOT_MARGIN. Seven settings, one
# build, PIVOT_MARGIN read from the environment so the sweep cannot measure
# one binary N times -- and 0.0 reproduces the shipping behaviour exactly,
# which is the control.
#
# The canaries, both directions:
#   C=0      must equal the committed bench/results/primal.txt line for line
#   C=3e-6   must equal the C=0 run   (below pilot87's min_r of 3.35e-6)
#   C=1e-5   must differ on pilot87 and NOWHERE else
# A sweep where every setting gives the same answer measured one binary.
#
# src/ is read and never written: the patch lands in a worktree.
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

python3 - "$D/wt/src/simplex.c" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()

def sub(old, new):
    global s
    n = s.count(old)
    assert n == 1, "anchor matched %d times: %r" % (n, old[:70])
    s = s.replace(old, new)

# The floor, and the sweep's hook for its constant.
sub("""/* How far column q can travel before a basic variable reaches a bound.""",
"""#include <stdlib.h>
/* PIVOT_MARGIN, read once from the environment so one binary can be swept.
 * 0.0 is the shipping behaviour: the floor collapses to PIVOT_MIN. */
static double sweep_margin(void)
{
    static int done = 0;
    static double v = 0.0;
    if (!done) {
        const char *e = getenv("JAOS_PIVOT_MARGIN");
        v = e != nullptr ? atof(e) : 0.0;
        done = 1;
    }
    return v;
}

/* The floor the two primal ratio tests apply to an entry of `B^-1 M_q`.
 * The entries are sums and a sum is known no more finely than its terms;
 * the column's own largest entry stands in for the traffic through each
 * one. One scan, billed like the row scan that follows it. */
static double pivot_floor(sx *s)
{
    double cmax = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const double a = fabs(s->col[i]);
        if (a > cmax)
            cmax = a;
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    const double rel = sweep_margin() * DBL_EPSILON * cmax;
    return rel > PIVOT_MIN ? rel : PIVOT_MIN;
}

/* How far column q can travel before a basic variable reaches a bound.""")

# Phase-2 / cleanup ratio test.
sub("""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < PIVOT_MIN)
            continue;                        /* cannot be told from zero */""",
"""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    const double min_pivot = pivot_floor(s);
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < min_pivot)
            continue;                        /* cannot be told from zero */""")

# Phase-1 ratio test.
sub("""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < PIVOT_MIN)
            continue;                              /* cannot be told from zero */""",
"""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    const double min_pivot = pivot_floor(s);
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < min_pivot)
            continue;                              /* cannot be told from zero */""")

open(p, 'w', encoding='utf-8').write(s)
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/primal 2>&1 | grep -E 'error' | head; exit 2; }

for C in ${*:-0 3e-6 1e-5 1e-3 1e-1 1 5}; do
    echo "### PIVOT_MARGIN=$C"
    JAOS_PIVOT_MARGIN="$C" ./build/bench/primal -j 12 \
        -o "$here/sweep-$C.txt" >"$here/sweep-$C.log" 2>&1
    echo "exit=$?  $(grep -c '^' "$here/sweep-$C.txt" 2>/dev/null) record lines"
    tail -14 "$here/sweep-$C.log"
    echo
done
