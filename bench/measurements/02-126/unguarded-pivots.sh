#!/bin/bash
# PINNED: 078a862 -- the anchors are code, not comments.
#
# TODO.md section 0, stage 8d. `pilot87`'s phase-1 objective is a sum of
# bound violations and must never rise. It falls to 1.24365e+12 at iteration
# 341000, turns at 342000, and reaches 1.88282e+24 by 351000 (D208,
# bench/measurements/02-123/). `refresh` recomputes `xb` from the
# factorization every 64 updates, so the rise is not drift in the carried
# values: the bases being chosen are themselves getting worse.
#
# THE HYPOTHESIS. `pivot` takes its primal step as
#
#     theta_primal = (xb[r] - bound) / alpha_q          -- the BTRAN value
#
# and then moves every basic by theta_primal * col[i]   -- the FTRAN value.
#
# The two are one number in exact arithmetic and are checked against each
# other to LU_AGREE_TOL, but ONLY when n_updates > 0. On a freshly rebuilt
# factorization the pivot is taken unchecked, so that a refusal cannot loop
# forever (D86). The diverging regime has 6246 refactorizations and 3139
# stability rebuilds in 46000 iterations: the factorization contradicts
# itself, each contradiction forces a rebuild, and the next pivot after every
# rebuild is the unguarded one. If alpha_q and col[r] disagree THERE,
# theta_primal is wrong by their ratio and every basic moves by that wrong
# factor. That is a mechanism for 1e+12 -> 1e+24 in a few thousand pivots.
#
# WHAT DECIDES IT. Over the whole run, the count of pivots TAKEN with
# n_updates == 0 whose |alpha_q - col[r]| exceeded LU_AGREE_TOL * max, the
# iteration of the first one, and the largest |theta_primal| among them. If
# the first one sits at ~341000-342000 and the objective jumps there, the
# mechanism is found. The control is `dfl001`, 136695 phase-1 iterations and
# no divergence: the same count there should be zero or near it.
#
# Then the window itself, every pivot in [340000, 352000]: the objective at
# the top of the loop, and at the pivot both values, their disagreement,
# n_updates, theta_primal and d[q], so predicted and actual change can be
# read side by side.
#
# Own counters, nothing billed, no solver state touched. src/ is read and
# never written: the patch lands in a worktree.
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
#define DG_LO 340000
#define DG_HI 352000
static long long dg_taken, dg_fresh, dg_fresh_disagree, dg_stale_disagree_refused;
static long long dg_first_fresh_disagree = -1;
static double dg_worst_fresh_rel, dg_worst_fresh_theta;
static long long dg_worst_fresh_iter = -1;
static double dg_total_now;
static void dg_line(const char *buf, int n)
{
    if (n > 0 && n < 512)
        (void)!write(2, buf, (size_t)n);   /* one record, one write */
}
void jaos_p1_dump(const char *name);
void jaos_p1_dump(const char *name)
{
    char b[512];
    int n = snprintf(b, sizeof b,
        "P1SUM %s taken=%lld fresh=%lld fresh_disagree=%lld "
        "stale_disagree_refused=%lld first_fresh_disagree_iter=%lld "
        "worst_fresh_rel=%.6g worst_fresh_theta=%.6g worst_fresh_iter=%lld\\n",
        name, dg_taken, dg_fresh, dg_fresh_disagree,
        dg_stale_disagree_refused, dg_first_fresh_disagree,
        dg_worst_fresh_rel, dg_worst_fresh_theta, dg_worst_fresh_iter);
    dg_line(b, n);
}
#endif""")

# The objective at the top of every phase-1 iteration, kept for the pivot
# line, and printed in the window.
sub(sx, """        const double total = primal_phase1_costs(s);
        /* On a count and never on a clock (D8). */""",
"""        const double total = primal_phase1_costs(s);
#ifdef JAOS_DIAG
        dg_total_now = total;
        if (s->iters >= DG_LO && s->iters <= DG_HI) {
            char b[512];
            int n = snprintf(b, sizeof b,
                "P1TOP iter=%lld total=%.17g best=%.17g n_refactor=%lld "
                "n_stability=%lld n_updates=%lld\\n",
                (long long)s->iters, total, best_total,
                (long long)s->n_refactor, (long long)s->n_stability,
                (long long)s->lu.n_updates);
            dg_line(b, n);
        }
#endif
        /* On a count and never on a clock (D8). */""")

# The refused path: a stale factorization contradicting itself.
sub(sx, """        if (big > 0.0 && fabs(alpha_q - s->col[r]) > LU_AGREE_TOL * big &&
            s->lu.n_updates > 0) {
            s->needs_refactor = true;
            s->n_stability++;
            *took = false;
            return JAOS_OK;
        }
    }
    *took = true;

    /* Primal step: row r lands exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;""",
"""        if (big > 0.0 && fabs(alpha_q - s->col[r]) > LU_AGREE_TOL * big &&
            s->lu.n_updates > 0) {
#ifdef JAOS_DIAG
            if (s->in_phase1)
                dg_stale_disagree_refused++;
#endif
            s->needs_refactor = true;
            s->n_stability++;
            *took = false;
            return JAOS_OK;
        }
    }
    *took = true;

    /* Primal step: row r lands exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;
#ifdef JAOS_DIAG
    if (s->in_phase1) {
        const double a = fabs(alpha_q), c = fabs(s->col[r]);
        const double big = a > c ? a : c;
        const double rel = big > 0.0 ? fabs(alpha_q - s->col[r]) / big : 0.0;
        const bool fresh = s->lu.n_updates == 0;
        const bool disagree = rel > LU_AGREE_TOL;
        dg_taken++;
        if (fresh) {
            dg_fresh++;
            if (disagree) {
                dg_fresh_disagree++;
                if (dg_first_fresh_disagree < 0)
                    dg_first_fresh_disagree = s->iters;
                if (rel > dg_worst_fresh_rel) {
                    dg_worst_fresh_rel = rel;
                    dg_worst_fresh_theta = theta_primal;
                    dg_worst_fresh_iter = s->iters;
                }
            }
        }
        if (s->iters >= DG_LO && s->iters <= DG_HI) {
            char b[512];
            int n = snprintf(b, sizeof b,
                "P1PIV iter=%lld total=%.17g r=%lld q=%lld alpha=%.17g "
                "col_r=%.17g rel=%.6g fresh=%d theta=%.17g d_q=%.17g "
                "xb_r=%.17g bound=%.17g\\n",
                (long long)s->iters, dg_total_now, (long long)r, (long long)q,
                alpha_q, s->col[r], rel, fresh ? 1 : 0, theta_primal,
                s->d[q], s->xb[r], bound);
            dg_line(b, n);
        }
    }
#endif""")

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
void jaos_p1_dump(const char *name);
#endif""")

sub(bp, """                measure_one(&ents[sel[launched]], dir, factor, &r);""",
"""                measure_one(&ents[sel[launched]], dir, factor, &r);
#ifdef JAOS_DIAG
                jaos_p1_dump(ents[sel[launched]].name);
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
  echo "# P1SUM: over the whole forced-primal phase 1 of each instance."
  echo "#   taken           = pivots taken in phase 1"
  echo "#   fresh           = of those, on a factorization with n_updates == 0"
  echo "#   fresh_disagree  = of those, with |alpha_q - col[r]| > LU_AGREE_TOL * max"
  echo "#                     -- the unguarded case the hypothesis is about"
  echo "#   stale_disagree_refused = the guarded case, refused and rebuilt"
  echo "# P1TOP / P1PIV: every iteration in [$( echo 340000 ), 352000] on pilot87."
  echo
  # No '| head' anywhere: it closes the pipe and the solver dies part-way
  # (02-123). The window is ~24000 lines on pilot87 and none on dfl001.
  ./build/bench/primal -j 2 -o "$D/p.txt" pilot87 dfl001 2>&1 |
      grep -E '^P1(SUM|TOP|PIV) '
  echo "# control: $(grep -c . "$D/p.txt" 2>/dev/null) record lines written"
} > "$here/unguarded.txt" 2>&1

grep -E '^#|^P1SUM' "$here/unguarded.txt"
echo "window lines: $(grep -cE '^P1(TOP|PIV)' "$here/unguarded.txt")"
