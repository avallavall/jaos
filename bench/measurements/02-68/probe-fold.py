#!/usr/bin/env python3
"""Where does the collapsed fold's midpoint land?

When a singleton row's implied interval collapses inside the fold's rounding
window, src/presolve.c writes the midpoint of the two ends into BOTH folded
bounds. The midpoint is unclamped, so it can sit outside the column's own box
by up to half the window, and the window carries row_traffic[i]/|a|.

This counts the collapses and measures how far outside the midpoint lands,
against two boxes: the column's CURRENT box (what the fold is narrowing) and
the CALLER's original box (what jaos.h promises about). It changes nothing.

Hooks guarded by JAOS_DIAG. Revert with: git checkout -- src/presolve.c
"""
import sys, io

P = 'src/presolve.c'
s = io.open(P, encoding='utf-8', newline='').read()


def sub(old, new, why):
    global s
    if s.count(old) != 1:
        sys.exit('PATCH FAILED (%d matches): %s' % (s.count(old), why))
    s = s.replace(old, new, 1)


sub("#include <string.h>",
    "#include <string.h>\n#ifdef JAOS_DIAG\n#include <stdio.h>\n#endif",
    'stdio')

sub("""    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);""",
    """    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
#ifdef JAOS_DIAG
    int64_t dg_folds = 0, dg_collapse = 0, dg_out_cur = 0, dg_out_orig = 0;
    double dg_worst_out_cur = 0.0, dg_worst_out_orig = 0.0;
    double dg_worst_rel_orig = 0.0, dg_worst_btol = 0.0, dg_worst_gap = 0.0;
#endif""", 'declare')

sub("""    free(col_dead); free(row_dead); free(row_frozen);""",
    """#ifdef JAOS_DIAG
    fprintf(stderr,
        "DIAG-FOLD folds=%lld collapse=%lld out_cur=%lld out_orig=%lld "
        "worst_out_cur=%.6g worst_out_orig=%.6g worst_rel_orig=%.6g "
        "worst_btol=%.6g worst_gap=%.6g\\n",
        (long long)dg_folds, (long long)dg_collapse, (long long)dg_out_cur,
        (long long)dg_out_orig, dg_worst_out_cur, dg_worst_out_orig,
        dg_worst_rel_orig, dg_worst_btol, dg_worst_gap);
#endif
    free(col_dead); free(row_dead); free(row_frozen);""", 'report')

sub("""                if (new_lo > new_hi) {""",
    """#ifdef JAOS_DIAG
                dg_folds++;
                if (new_lo > new_hi) {
                    dg_collapse++;
                    const double mid_ = 0.5 * (new_lo + new_hi);
                    const double gap_ = new_lo - new_hi;
                    if (gap_ > dg_worst_gap) dg_worst_gap = gap_;
                    if (btol > dg_worst_btol) dg_worst_btol = btol;

                    /* against the column's CURRENT box, which is what this
                     * fold is narrowing */
                    double o_ = 0.0;
                    if (isfinite(cur_cl[j]) && mid_ < cur_cl[j])
                        o_ = cur_cl[j] - mid_;
                    if (isfinite(cur_cu[j]) && mid_ - cur_cu[j] > o_)
                        o_ = mid_ - cur_cu[j];
                    if (o_ > 0.0) {
                        dg_out_cur++;
                        if (o_ > dg_worst_out_cur) dg_worst_out_cur = o_;
                    }

                    /* against the CALLER's box, which is what jaos.h promises */
                    double p_ = 0.0;
                    if (isfinite(m->col_lower[j]) && mid_ < m->col_lower[j])
                        p_ = m->col_lower[j] - mid_;
                    if (isfinite(m->col_upper[j]) && mid_ - m->col_upper[j] > p_)
                        p_ = mid_ - m->col_upper[j];
                    if (p_ > 0.0) {
                        dg_out_orig++;
                        if (p_ > dg_worst_out_orig) dg_worst_out_orig = p_;
                        const double sc_ = ps_bound_scale(m->col_lower[j],
                                                          m->col_upper[j]);
                        const double r_ = p_ / sc_;
                        if (r_ > dg_worst_rel_orig) dg_worst_rel_orig = r_;
                    }
                }
#endif
                if (new_lo > new_hi) {""", 'collapse branch')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
