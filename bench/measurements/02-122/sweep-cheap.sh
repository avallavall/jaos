#!/bin/bash
# PINNED: 1fe8bc6 -- the anchors are code, not comments.
#
# Round one billed the floor an extra O(nrow) scan of the column, and that was
# not a neutral accounting change: `bench/primal` gives the primal solve a
# budget of 10x the DUAL's WORK, so charging more work shortens every primal
# solve. The C=0 control came out 18 instances away from the committed record
# with four verdicts changed, none of it from the floor.
#
# This version takes the column's largest entry from the loop that already
# runs, checks only the row that loop chose, and pays for a second pass just
# when that row is below the floor. With PIVOT_MARGIN = 0 the second pass
# never runs and the charge is exactly the shipping one -- so the control has
# to reproduce bench/results/primal.txt line for line, which is the canary.
#
# Same trajectories as round one for a given C: the winner of the unfloored
# pass, when it clears the floor, is also the winner over the floored subset,
# because that subset is smaller and contains it.
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

sub("""/* How far column q can travel before a basic variable reaches a bound.""",
"""#include <stdlib.h>
/* PIVOT_MARGIN, read once from the environment so one binary can be swept.
 * 0.0 is the shipping behaviour: the second pass never runs. */
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

/* How far column q can travel before a basic variable reaches a bound.""")

# ---- phase-2 / cleanup ratio test -------------------------------------
sub("""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < PIVOT_MIN)
            continue;                        /* cannot be told from zero */

        int64_t b = s->basis[i];
        double limit = move < 0.0 ? real_lower(s, b) : real_upper(s, b);
        if (!isfinite(limit))
            continue;

        double step = (limit - s->xb[i]) / move;
        if (step < 0.0)
            step = 0.0;                      /* already there: degenerate */
        if (jm_primal_row_wins(step, b, best_step,
                               best >= 0 ? s->basis[best] : -1, bland)) {
            best_step = step;
            best = i;
            *below = move < 0.0;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);""",
"""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;
    double cmax = 0.0;
    double thr = PIVOT_MIN;

    for (int pass = 0; pass < 2; pass++) {
        best = -1;
        best_step = HUGE_VAL;
        for (int64_t i = 0; i < s->nrow; i++) {
            double move = -dir * s->col[i];      /* per unit q travels */
            const double amove = fabs(move);
            if (pass == 0 && amove > cmax)
                cmax = amove;
            if (amove < thr)
                continue;                        /* cannot be told from zero */

            int64_t b = s->basis[i];
            double limit = move < 0.0 ? real_lower(s, b) : real_upper(s, b);
            if (!isfinite(limit))
                continue;

            double step = (limit - s->xb[i]) / move;
            if (step < 0.0)
                step = 0.0;                      /* already there: degenerate */
            if (jm_primal_row_wins(step, b, best_step,
                                   best >= 0 ? s->basis[best] : -1, bland)) {
                best_step = step;
                best = i;
                *below = move < 0.0;
            }
        }
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
        const double rel = sweep_margin() * DBL_EPSILON * cmax;
        if (rel <= thr || best < 0 || fabs(s->col[best]) >= rel)
            break;
        thr = rel;
    }""")

# ---- phase-1 ratio test -----------------------------------------------
sub("""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < PIVOT_MIN)
            continue;                              /* cannot be told from zero */

        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        const bool under = isfinite(lo) && s->xb[i] < lo - s->primal_tol;
        const bool over  = isfinite(up) && s->xb[i] > up + s->primal_tol;

        double limit;
        bool lands_low;
        if (under) {
            if (move < 0.0)
                continue;                          /* going further under */
            limit = lo;
            lands_low = true;
        } else if (over) {
            if (move > 0.0)
                continue;                          /* going further over */
            limit = up;
            lands_low = false;
        } else {
            limit = move < 0.0 ? lo : up;
            lands_low = move < 0.0;
            if (!isfinite(limit))
                continue;
        }

        double t = (limit - s->xb[i]) / move;
        if (t < 0.0)
            t = 0.0;                               /* already there: degenerate */
        if (jm_primal_row_wins(t, v, best_step,
                               best >= 0 ? s->basis[best] : -1, bland)) {
            best_step = t;
            best = i;
            *below = lands_low;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);""",
"""    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;
    double cmax = 0.0;
    double thr = PIVOT_MIN;

    for (int pass = 0; pass < 2; pass++) {
        best = -1;
        best_step = HUGE_VAL;
        for (int64_t i = 0; i < s->nrow; i++) {
            const double move = -dir * s->col[i];      /* per unit q travels */
            const double amove = fabs(move);
            if (pass == 0 && amove > cmax)
                cmax = amove;
            if (amove < thr)
                continue;                              /* cannot be told from zero */

            const int64_t v = s->basis[i];
            const double lo = real_lower(s, v), up = real_upper(s, v);
            const bool under = isfinite(lo) && s->xb[i] < lo - s->primal_tol;
            const bool over  = isfinite(up) && s->xb[i] > up + s->primal_tol;

            double limit;
            bool lands_low;
            if (under) {
                if (move < 0.0)
                    continue;                          /* going further under */
                limit = lo;
                lands_low = true;
            } else if (over) {
                if (move > 0.0)
                    continue;                          /* going further over */
                limit = up;
                lands_low = false;
            } else {
                limit = move < 0.0 ? lo : up;
                lands_low = move < 0.0;
                if (!isfinite(limit))
                    continue;
            }

            double t = (limit - s->xb[i]) / move;
            if (t < 0.0)
                t = 0.0;                               /* already there: degenerate */
            if (jm_primal_row_wins(t, v, best_step,
                                   best >= 0 ? s->basis[best] : -1, bland)) {
                best_step = t;
                best = i;
                *below = lands_low;
            }
        }
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
        const double rel = sweep_margin() * DBL_EPSILON * cmax;
        if (rel <= thr || best < 0 || fabs(s->col[best]) >= rel)
            break;
        thr = rel;
    }""")

open(p, 'w', encoding='utf-8').write(s)
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/primal 2>&1 | grep -E 'error' | head; exit 2; }

for C in ${*:-0 1}; do
    echo "### PIVOT_MARGIN=$C"
    JAOS_PIVOT_MARGIN="$C" ./build/bench/primal -j 12 \
        -o "$here/cheap-$C.txt" >"$here/cheap-$C.log" 2>&1
    echo "exit=$?  $(grep -c '^' "$here/cheap-$C.txt" 2>/dev/null) record lines"
    tail -12 "$here/cheap-$C.log"
    echo
done
