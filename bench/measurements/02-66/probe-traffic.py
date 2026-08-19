#!/usr/bin/env python3
"""Instrument row_traffic saturation. Applies hooks guarded by JAOS_DIAG.

Computes the REPAIRED traffic alongside the shipped one and changes nothing,
so one run reports both. Revert with: git checkout -- src/presolve.c
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
    'stdio for the probe')


# ---- 1. the second array, allocated and freed beside the first
sub("""    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);""",
    """    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
#ifdef JAOS_DIAG
    /* The repaired accumulation, carried alongside and read by nothing. */
    double *dg_traffic2 = jm_calloc_array(nr, sizeof *dg_traffic2);
    int64_t dg_sat_sites = 0, dg_read_empty_inf = 0, dg_read_fold_inf = 0;
    int64_t dg_frozen = 0, dg_frozen_inf = 0, dg_frozen_zero_inf = 0;
    int64_t dg_zero_margin = 0;
    double dg_worst_ratio = 0.0, dg_zero_worst_traffic = 0.0;
    double dg_max_traffic = 0.0;
#endif""", 'declare')

sub("""    free(col_pending_dual); free(row_traffic);""",
    """#ifdef JAOS_DIAG
    {
        int64_t sat_rows = 0;
        for (int64_t i = 0; i < nr; i++) {
            if (!isfinite(row_traffic[i])) sat_rows++;
            /* The repaired traffic over EVERY row, which is the headroom the
             * assert after the round loop depends on. */
            if (isfinite(dg_traffic2[i]) && dg_traffic2[i] > dg_max_traffic)
                dg_max_traffic = dg_traffic2[i];
        }
        fprintf(stderr,
            "DIAG-TRAFFIC rows=%lld sat_sites=%lld sat_rows=%lld "
            "read_empty_inf=%lld read_fold_inf=%lld frozen=%lld "
            "frozen_inf=%lld zero_margin=%lld frozen_zero_inf=%lld "
            "worst_repaired_over_bscale=%.6g zero_worst_repaired_traffic=%.6g "
            "max_repaired_traffic=%.6g\\n",
            (long long)nr, (long long)dg_sat_sites, (long long)sat_rows,
            (long long)dg_read_empty_inf, (long long)dg_read_fold_inf,
            (long long)dg_frozen, (long long)dg_frozen_inf,
            (long long)dg_zero_margin, (long long)dg_frozen_zero_inf,
            dg_worst_ratio, dg_zero_worst_traffic, dg_max_traffic);
    }
    free(dg_traffic2);
#endif
    free(col_pending_dual); free(row_traffic);""", 'report')

# ---- 2. the fixed-column site: traffic is finite there, mirror it
sub("""                    row_traffic[i] += fabs(m->a_value[k] * v);
                    row_deg[i]--;
                }
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_FIXED_COL, .index = j,""",
    """                    row_traffic[i] += fabs(m->a_value[k] * v);
#ifdef JAOS_DIAG
                    dg_traffic2[i] += fabs(m->a_value[k] * v);
#endif
                    row_deg[i]--;
                }
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_FIXED_COL, .index = j,""", 'fixed col')

sub("""                            row_traffic[ii] += fabs(m->a_value[kk] * v);
                            row_deg[ii]--;""",
    """                            row_traffic[ii] += fabs(m->a_value[kk] * v);
#ifdef JAOS_DIAG
                            dg_traffic2[ii] += fabs(m->a_value[kk] * v);
#endif
                            row_deg[ii]--;""", 'forcing row')

# ---- 3. the saturating site. The repaired rule: only a magnitude that a
#         still-finite end actually absorbed.
sub("""                    row_traffic[i] += fabs(cmax) > fabs(cmin) ? fabs(cmax)
                                                              : fabs(cmin);
                    col_dead[j] = true;""",
    """#ifdef JAOS_DIAG
                    {
                        const bool lo_was_finite = isfinite(rl_before_dg);
                        const bool hi_was_finite = isfinite(ru_before_dg);
                        double moved = 0.0;
                        if (lo_was_finite && isfinite(cmax) && fabs(cmax) > moved)
                            moved = fabs(cmax);
                        if (hi_was_finite && isfinite(cmin) && fabs(cmin) > moved)
                            moved = fabs(cmin);
                        dg_traffic2[i] += moved;
                        if (!isfinite(fabs(cmax) > fabs(cmin) ? cmax : cmin))
                            dg_sat_sites++;
                    }
#endif
                    row_traffic[i] += fabs(cmax) > fabs(cmin) ? fabs(cmax)
                                                              : fabs(cmin);
                    col_dead[j] = true;""", 'saturating site')

# the two ends before the subtraction, for the repaired rule above
sub("""                    if (isfinite(cur_rl[i]))
                        cur_rl[i] -= cmax;
                    if (isfinite(cur_ru[i]))
                        cur_ru[i] -= cmin;""",
    """#ifdef JAOS_DIAG
                    const double rl_before_dg = cur_rl[i];
                    const double ru_before_dg = cur_ru[i];
#endif
                    if (isfinite(cur_rl[i]))
                        cur_rl[i] -= cmax;
                    if (isfinite(cur_ru[i]))
                        cur_ru[i] -= cmin;""", 'ends before')

# ---- 4. the two consumers: does an infinite traffic ever reach them?
sub("""                double etol = 0.0;
                if (row_traffic[i] > 0.0) {""",
    """                double etol = 0.0;
#ifdef JAOS_DIAG
                if (!isfinite(row_traffic[i])) dg_read_empty_inf++;
#endif
                if (row_traffic[i] > 0.0) {""", 'empty-row consumer')

sub("""                double bscale = ps_bound_scale(new_lo, new_hi);
                if (row_traffic[i] > 0.0 && isfinite(row_traffic[i])) {""",
    """                double bscale = ps_bound_scale(new_lo, new_hi);
#ifdef JAOS_DIAG
                if (!isfinite(row_traffic[i])) dg_read_fold_inf++;
#endif
                if (row_traffic[i] > 0.0 && isfinite(row_traffic[i])) {""",
    'fold consumer')

# ---- 5. the frozen-row test: what TODO's 117 rows actually carry
sub("""        const double rtol = ps_round_tol(ps_bound_scale(cur_rl[i], cur_ru[i]));
        const double min_act = ps_min_act(&rg);
        const double max_act = ps_max_act(&rg);""",
    """        const double rtol = ps_round_tol(ps_bound_scale(cur_rl[i], cur_ru[i]));
        const double min_act = ps_min_act(&rg);
        const double max_act = ps_max_act(&rg);
#ifdef JAOS_DIAG
        {
            dg_frozen++;
            if (!isfinite(row_traffic[i]))
                dg_frozen_inf++;

            /* "Zero margin" is the test's own violation being exactly 0.0 --
             * the row passes because the cancellation happened to be exact,
             * not because the window covered anything. greenbea row 57 is the
             * case named in the source. */
            double over  = (isfinite(cur_ru[i])) ? min_act - cur_ru[i] : -HUGE_VAL;
            double under = (isfinite(cur_rl[i])) ? cur_rl[i] - max_act : -HUGE_VAL;
            double viol  = over > under ? over : under;
            if (viol == 0.0) {
                dg_zero_margin++;
                if (!isfinite(row_traffic[i])) dg_frozen_zero_inf++;
                if (dg_traffic2[i] > dg_zero_worst_traffic)
                    dg_zero_worst_traffic = dg_traffic2[i];
            }

            /* How much WIDER the window would be if the repaired traffic
             * replaced the bound scale here. Above 1.0 the repair would
             * change this test's behaviour on an existing reduction. */
            const double bs = ps_bound_scale(cur_rl[i], cur_ru[i]);
            if (isfinite(dg_traffic2[i]) && bs > 0.0) {
                const double r = dg_traffic2[i] / bs;
                if (r > dg_worst_ratio) dg_worst_ratio = r;
            }
        }
#endif""", 'frozen-row test')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
