#!/bin/bash
# PINNED: 17d1fe5 -- evidence for that tree; the anchors include a comment the purge may rewrite
# What are the two disagreeing numbers when pilot87's primal phase 1 refuses?
#
# The site: run_primal_phase1 picks column q, the ratio test picks row r
# because |s->col[r]| >= PIVOT_MIN, and build_pricing_row then reports
# |s->alpha[q]| < PIVOT_MIN. Both are (B^-1 A_q)_r, computed two ways -- FTRAN
# for the first, BTRAN for the second. On a FRESHLY built factorization the
# code calls that a JAOS defect and gives up.
#
# The question this answers is which kind of disagreement it is:
#   an EDGE   -- col[r] just above 1e-9 and alpha[q] just below it, in which
#                case the two agree and the threshold is what splits them
#   a CLIFF   -- col[r] large and alpha[q] essentially zero, in which case the
#                factorization does not describe the basis and the numbers
#                mean nothing
# The repair is different for each, so nothing is proposed before this prints.
#
# Throwaway build. Worktree under mktemp -d, OUTSIDE the repository, patched
# there, built to its own tree. The main repo is never touched.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
root="$JAOS_ROOT"
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
ln -s "$root/bench/instances" "$D/wt/bench/instances"
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()

# 1. A band census over every phase-1 iteration that reaches the pricing row,
#    so the failure can be read against the population it came from.
OLD = """        build_pricing_row(s, r);
        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization; this is a "
                       "JAOS defect", (long long)q, s->alpha[q],
                       (long long)r);
            return JAOS_ERR_NUMERICAL;
        }"""
assert s.count(OLD) == 1, "phase-1 pivot check did not match exactly once"

NEW = """        build_pricing_row(s, r);
#ifdef JAOS_DIAG
        {
            extern long long jaos_diag_band[8];
            extern long long jaos_diag_seen;
            const double aq = fabs(s->alpha[q]);
            jaos_diag_seen++;
            int b = 0;
            if      (aq == 0.0)   b = 0;
            else if (aq < 1e-12)  b = 1;
            else if (aq < 1e-9)   b = 2;
            else if (aq < 1e-6)   b = 3;
            else if (aq < 1e-3)   b = 4;
            else if (aq < 1.0)    b = 5;
            else if (aq < 1e3)    b = 6;
            else                  b = 7;
            jaos_diag_band[b]++;
        }
#endif
        if (fabs(s->alpha[q]) < PIVOT_MIN) {
#ifdef JAOS_DIAG
            {
                extern long long jaos_diag_band[8];
                extern long long jaos_diag_seen;
                fprintf(stderr,
                    "DIAG hit iter=%lld q=%lld r=%lld basis_r=%lld "
                    "col[r]=%.17g alpha[q]=%.17g ratio=%.6g "
                    "n_updates=%lld n_stability=%lld n_refactor=%lld "
                    "seen=%lld bands=%lld/%lld/%lld/%lld/%lld/%lld/%lld/%lld\\n",
                    (long long)s->iters, (long long)q, (long long)r,
                    (long long)s->basis[r], s->col[r], s->alpha[q],
                    s->alpha[q] != 0.0 ? s->col[r] / s->alpha[q] : 0.0/0.0,
                    (long long)s->lu.n_updates, (long long)s->n_stability,
                    (long long)s->n_refactor, jaos_diag_seen,
                    jaos_diag_band[0], jaos_diag_band[1], jaos_diag_band[2],
                    jaos_diag_band[3], jaos_diag_band[4], jaos_diag_band[5],
                    jaos_diag_band[6], jaos_diag_band[7]);
                fflush(stderr);
            }
#endif
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization; this is a "
                       "JAOS defect", (long long)q, s->alpha[q],
                       (long long)r);
            return JAOS_ERR_NUMERICAL;
        }"""
s = s.replace(OLD, NEW)

# 2. the counters themselves
ANCHOR = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(ANCHOR) == 1, "PIVOT_MIN anchor did not match exactly once"
s = s.replace(ANCHOR, ANCHOR + """
#ifdef JAOS_DIAG
#include <stdio.h>
long long jaos_diag_band[8];
long long jaos_diag_seen;
#endif""")

open(p, 'w', encoding='utf-8').write(s)
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }
grep -q 'JAOS_DIAG' src/simplex.c || { echo "the patch did not apply"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | tail -20; exit 2; }

{
  echo "# pilot87: the two numbers at the primal phase 1's refusal"
  echo "# tree: $ref plus a JAOS_DIAG patch, built and run outside the repo"
  echo "# bands are counts of |alpha[q]| at the pricing row, over every phase-1"
  echo "# iteration that got that far: 0 / <1e-12 / <1e-9 / <1e-6 / <1e-3 / <1 / <1e3 / >=1e3"
  echo
  ./build/bench/primal -j 1 -o "$D/out.txt" pilot87 2>&1 | grep -E '^DIAG|^pilot87'
  echo
  echo "# a control: an instance that does NOT refuse, same instrument"
  ./build/bench/primal -j 1 -o "$D/out2.txt" ganges 2>&1 | grep -E '^DIAG|^ganges'
} 2>&1 | tee "$here/diag-pilot87.txt"
