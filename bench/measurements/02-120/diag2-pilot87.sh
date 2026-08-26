#!/bin/bash
# PINNED: 17d1fe5 -- evidence for that tree; the anchors include a comment the purge may rewrite
# pilot87, step two. At the refusal, alpha[q] is EXACTLY zero while the FTRAN
# says -1.59e-7, on a fresh factorization. An exact zero from a dot product is
# a term that was never added, not rounding. Three places could drop it:
#   (a) jm_lu_btran_sparse's pattern omits a row where rho is nonzero
#   (b) price_all's row walk skips a row that is in the pattern
#   (c) neither: rho really is zero on every row of column q, and the FTRAN
#       value is the wrong one (a conditioning failure the refusal is right about)
# This recomputes rho densely (jm_lu_btran, own buffer), recomputes alpha[q]
# densely from that rho and column q, and reports which of the three it is.
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
                       (long long)r);"""
assert s.count(OLD) == 1, "anchor did not match exactly once"
NEW = """        build_pricing_row(s, r);
        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
#ifdef JAOS_DIAG
            {
                /* Own buffers; nothing here touches solver state. */
                double *rho_d = calloc((size_t)s->nrow, sizeof *rho_d);
                double *colq  = calloc((size_t)s->nrow, sizeof *colq);
                char   *inpat = calloc((size_t)s->nrow, 1);
                if (rho_d && colq && inpat) {
                    const jm_work saved = s->work;
                    rho_d[r] = 1.0;
                    jm_lu_btran(&s->lu, rho_d, &s->work);      /* dense */
                    var_column(s, q, colq);                     /* A_q, scaled */
                    s->work = saved;
                    const int64_t np = s->nrpat >= 0 ? s->nrpat : -1;
                    for (int64_t k = 0; k < (np > 0 ? np : 0); k++) inpat[s->rpat[k]] = 1;
                    double a_dense = 0.0, a_pat = 0.0, a_sparse_rho = 0.0;
                    int64_t rho_nz = 0, rho_miss = 0, rho_diff = 0, col_nz = 0, dropped = 0;
                    for (int64_t i = 0; i < s->nrow; i++) {
                        if (rho_d[i] != 0.0) rho_nz++;
                        if (colq[i] != 0.0) col_nz++;
                        a_dense += rho_d[i] * colq[i];
                        a_sparse_rho += s->rho[i] * colq[i];
                        if (rho_d[i] != s->rho[i]) rho_diff++;
                        if (rho_d[i] != 0.0 && np >= 0 && !inpat[i]) {
                            rho_miss++;
                            if (colq[i] != 0.0) {
                                dropped++;
                                fprintf(stderr, "DIAG2 dropped-term row=%lld rho_dense=%.17g rho_sparse=%.17g a_iq=%.17g\\n",
                                        (long long)i, rho_d[i], s->rho[i], colq[i]);
                            }
                        }
                        if (np >= 0 && inpat[i]) a_pat += rho_d[i] * colq[i];
                    }
                    fprintf(stderr,
                        "DIAG2 iter=%lld q=%lld r=%lld nrpat=%lld col[r]=%.17g alpha[q]=%.17g\\n"
                        "DIAG2 alpha_dense=%.17g alpha_over_pattern=%.17g alpha_from_sparse_rho=%.17g\\n"
                        "DIAG2 rho_nonzero=%lld rho_not_in_pattern=%lld rho_dense_vs_sparse_differ=%lld col_nonzero=%lld dropped_terms=%lld\\n",
                        (long long)s->iters, (long long)q, (long long)r, (long long)np,
                        s->col[r], s->alpha[q], a_dense, a_pat, a_sparse_rho,
                        (long long)rho_nz, (long long)rho_miss, (long long)rho_diff,
                        (long long)col_nz, (long long)dropped);
                    fflush(stderr);
                }
                free(rho_d); free(colq); free(inpat);
            }
#endif
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization; this is a "
                       "JAOS defect", (long long)q, s->alpha[q],
                       (long long)r);"""
s = s.replace(OLD, NEW)
ANCHOR = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(ANCHOR) == 1
s = s.replace(ANCHOR, ANCHOR + """
#ifdef JAOS_DIAG
#include <stdio.h>
#endif""")
open(p, 'w', encoding='utf-8').write(s)
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }
make clean >/dev/null 2>&1
make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | grep -E 'error' | head; exit 2; }
{
  echo "# pilot87, step two: where does the zero come from?"
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  ./build/bench/primal -j 1 -o "$D/out.txt" pilot87 2>&1 | grep -E '^DIAG2|^pilot87'
} 2>&1 | tee "$here/diag2-pilot87.txt"
