#!/usr/bin/env python3
"""How much error arrives inside a VALUE, and would carrying it change anything?

D162 and D163 put a shift COUNT on the four windows that judge a row's running
difference. A count bounds the roundings that happened in THIS row. It cannot
see an error that arrived from another row, and one route does exactly that:

  1. a singleton row folds, and its implied end is `cur_rl[i] / a` -- carrying
     row i's whole accumulated error, divided by the coefficient
  2. that value is written into `cur_cl[j]` / `cur_cu[j]`
  3. the column is then fixed, and every OTHER row it touches subtracts it
  4. those rows are charged ONE shift at their own traffic

`bench/measurements/02-73/` has the model where step 4's window comes out short
and presolve refuses a feasible model. This measures the route on the three
sets before anything is built for it.

What it computes, beside the shipped windows and deciding nothing:

  col_err[j]   an absolute error bound on the value column j is fixed at.
               Zero for the caller's own bounds; set only where a fold writes
               a derived end, and propagated through a second fold.
  row_inh[i]   sum over the columns removed from row i of |a| * col_err[j].

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
    "#include <string.h>\n"
    "#ifdef JAOS_DIAG\n"
    "#include <stdio.h>\n"
    "#include <unistd.h>\n"
    "static void dg_emit(const char *s, size_t n)\n"
    "{\n"
    "    ssize_t rc = write(2, s, n);\n"
    "    (void)rc;\n"
    "}\n"
    "#endif",
    'stdio')

sub("""    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);""",
    """    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
#ifdef JAOS_DIAG
    /* The two arrays the candidate would need, and the tallies. */
    double *dg_colerr = jm_calloc_array(nc, sizeof *dg_colerr);
    double *dg_rowinh = jm_calloc_array(nr, sizeof *dg_rowinh);
    int64_t dg_folds = 0;          /* folds that wrote a derived end        */
    int64_t dg_folds_err = 0;      /* ... carrying a non-zero error         */
    int64_t dg_carried = 0;        /* subtractions of such a column         */
    int64_t dg_rows_inh = 0;       /* row-tests reached with inheritance    */
    double dg_max_colerr = 0.0;    /* worst absolute error in a fixed value */
    double dg_max_inh = 0.0;       /* worst absolute inheritance on a row   */
    /* Per site: the shipped window, the window with the inheritance added,
     * and whether adding it would spare a row the shipped one refuses. */
    int64_t dgE_fires = 0, dgE_flip = 0, dgI_fires = 0, dgI_flip = 0;
    int64_t dgF_fires = 0, dgF_flip = 0, dgD_fires = 0, dgD_flip = 0;
    double dgE_w = 0.0, dgI_w = 0.0, dgF_w = 0.0, dgD_w = 0.0;
    /* The fold's SHIPPED window, so the record can say how far it moved. The
     * other three are already owned by 02-72 and read the same here. */
    double dgD_wnow = 0.0;
    /* The ratio that says whether the inheritance is the dominant term. */
    double dg_worst_inh_over_win = 0.0;
#endif""", 'declare')

# ---- the fold: what error does the value it writes carry? -------------------
sub("""                cur_cl[j] = fold_lo;
                cur_cu[j] = fold_hi;""",
    """#ifdef JAOS_DIAG
                {
                    /* An implied end is `cur_rl[i] / a`, so it carries this
                     * row's whole budget divided by |a|, plus the rounding of
                     * the division itself. A STORED end is the column's own
                     * bound and carries whatever it already carried, which is
                     * zero unless a previous fold wrote it. */
                    const double own = ps_shift_excess(
                        row_traffic[i], ps_end_scale(cur_rl[i]),
                        row_shifts[i]) + dg_rowinh[i];
                    const double own_hi = ps_shift_excess(
                        row_traffic[i], ps_end_scale(cur_ru[i]),
                        row_shifts[i]) + dg_rowinh[i];
                    const double implied_err =
                        (own > own_hi ? own : own_hi) / fabs(a);
                    const double elo = tightens_lo
                        ? implied_err + DBL_EPSILON * fabs(fold_lo)
                        : dg_colerr[j];
                    const double ehi = tightens_hi
                        ? implied_err + DBL_EPSILON * fabs(fold_hi)
                        : dg_colerr[j];
                    const double e = elo > ehi ? elo : ehi;
                    dg_folds++;
                    if (e > 0.0) dg_folds_err++;
                    if (e > dg_max_colerr) dg_max_colerr = e;
                    dg_colerr[j] = e;
                }
#endif
                cur_cl[j] = fold_lo;
                cur_cu[j] = fold_hi;""", 'fold writes the column')

# ---- the three subtraction sites carry it into the receiving row ------------
sub("""                    const double t = m->a_value[k] * v;
                    cur_rl[i] -= t;
                    cur_ru[i] -= t;
                    row_traffic[i] += fabs(t);""",
    """                    const double t = m->a_value[k] * v;
                    cur_rl[i] -= t;
                    cur_ru[i] -= t;
                    row_traffic[i] += fabs(t);
#ifdef JAOS_DIAG
                    if (dg_colerr[j] > 0.0) {
                        dg_rowinh[i] += fabs(m->a_value[k]) * dg_colerr[j];
                        dg_carried++;
                        if (dg_rowinh[i] > dg_max_inh) dg_max_inh = dg_rowinh[i];
                    }
#endif""", 'fixed col carries')

sub("""                    if (lo_absorbs)
                        cur_rl[i] -= cmax;
                    if (hi_absorbs)
                        cur_ru[i] -= cmin;""",
    """#ifdef JAOS_DIAG
                    if (dg_colerr[j] > 0.0) {
                        dg_rowinh[i] += fabs(a) * dg_colerr[j];
                        dg_carried++;
                        if (dg_rowinh[i] > dg_max_inh) dg_max_inh = dg_rowinh[i];
                    }
#endif
                    if (lo_absorbs)
                        cur_rl[i] -= cmax;
                    if (hi_absorbs)
                        cur_ru[i] -= cmin;""", 'relaxation carries')

sub("""                            const double t = m->a_value[kk] * v;
                            cur_rl[ii] -= t;
                            cur_ru[ii] -= t;
                            row_traffic[ii] += fabs(t);""",
    """                            const double t = m->a_value[kk] * v;
                            cur_rl[ii] -= t;
                            cur_ru[ii] -= t;
                            row_traffic[ii] += fabs(t);
#ifdef JAOS_DIAG
                            if (dg_colerr[j] > 0.0) {
                                dg_rowinh[ii] += fabs(m->a_value[kk]) *
                                                 dg_colerr[j];
                                dg_carried++;
                                if (dg_rowinh[ii] > dg_max_inh)
                                    dg_max_inh = dg_rowinh[ii];
                            }
#endif""", 'forcing carries')

# ---- the four windows -------------------------------------------------------
sub("""                if (cur_rl[i] > etol_lo || cur_ru[i] < -etol_hi) {""",
    """#ifdef JAOS_DIAG
                {
                    const double inh = dg_rowinh[i];
                    const bool now = (cur_rl[i] > etol_lo ||
                                      cur_ru[i] < -etol_hi);
                    const bool cand = (cur_rl[i] > etol_lo + inh ||
                                       cur_ru[i] < -(etol_hi + inh));
                    if (inh > 0.0) dg_rows_inh++;
                    if (now) dgE_fires++;
                    if (now && !cand) dgE_flip++;
                    if (etol_lo + inh > dgE_w) dgE_w = etol_lo + inh;
                    if (inh > 0.0 && etol_lo > 0.0 &&
                        inh / etol_lo > dg_worst_inh_over_win)
                        dg_worst_inh_over_win = inh / etol_lo;
                }
#endif
                if (cur_rl[i] > etol_lo || cur_ru[i] < -etol_hi) {""",
    'emptied-row window')

sub("""                if (new_lo > new_hi + btol) {""",
    """#ifdef JAOS_DIAG
                {
                    const double inh = dg_rowinh[i] / fabs(a);
                    const bool now  = new_lo > new_hi + btol;
                    const bool cand = new_lo > new_hi + btol + inh;
                    if (inh > 0.0) dg_rows_inh++;
                    if (now) dgD_fires++;
                    if (now && !cand) dgD_flip++;
                    if (btol > dgD_wnow) dgD_wnow = btol;
                    if (btol + inh > dgD_w) dgD_w = btol + inh;
                    if (inh > 0.0 && btol > 0.0 &&
                        inh / btol > dg_worst_inh_over_win)
                        dg_worst_inh_over_win = inh / btol;
                }
#endif
                if (new_lo > new_hi + btol) {""", 'fold window')

sub("""            if ((isfinite(ru) && min_act > ru + itol_hi) ||
                (isfinite(rl) && max_act < rl - itol_lo)) {""",
    """#ifdef JAOS_DIAG
            {
                const double inh = dg_rowinh[i];
                const bool now = ((isfinite(ru) && min_act > ru + itol_hi) ||
                                  (isfinite(rl) && max_act < rl - itol_lo));
                const bool cand =
                    ((isfinite(ru) && min_act > ru + itol_hi + inh) ||
                     (isfinite(rl) && max_act < rl - itol_lo - inh));
                if (inh > 0.0) dg_rows_inh++;
                if (now) dgI_fires++;
                if (now && !cand) dgI_flip++;
                if (itol_hi + inh > dgI_w) dgI_w = itol_hi + inh;
                if (inh > 0.0 && itol_hi > 0.0 &&
                    inh / itol_hi > dg_worst_inh_over_win)
                    dg_worst_inh_over_win = inh / itol_hi;
            }
#endif
            if ((isfinite(ru) && min_act > ru + itol_hi) ||
                (isfinite(rl) && max_act < rl - itol_lo)) {""",
    'activity clause 1 window')

sub("""        if ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol_hi) ||
            (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol_lo)) {""",
    """#ifdef JAOS_DIAG
        {
            const double inh = dg_rowinh[i];
            const bool now =
                ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol_hi) ||
                 (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol_lo));
            const bool cand =
                ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol_hi + inh) ||
                 (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol_lo - inh));
            if (inh > 0.0) dg_rows_inh++;
            if (now) dgF_fires++;
            if (now && !cand) dgF_flip++;
            if (rtol_hi + inh > dgF_w) dgF_w = rtol_hi + inh;
            if (inh > 0.0 && rtol_hi > 0.0 &&
                inh / rtol_hi > dg_worst_inh_over_win)
                dg_worst_inh_over_win = inh / rtol_hi;
        }
#endif
        if ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol_hi) ||
            (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol_lo)) {""",
    'frozen-row window')

sub("""cleanup_scratch:
    free(col_dead); free(row_dead); free(row_frozen);""",
    """cleanup_scratch:
#ifdef JAOS_DIAG
    {
        char dgbuf[900];
        const int dgn = snprintf(dgbuf, sizeof dgbuf,
            "DIAG-INH folds=%lld folds_err=%lld carried=%lld rows_inh=%lld "
            "max_colerr=%.6g max_inh=%.6g worst_inh_over_win=%.6g "
            "E_fires=%lld E_flip=%lld E_w=%.6g "
            "D_fires=%lld D_flip=%lld D_wnow=%.6g D_w=%.6g "
            "I_fires=%lld I_flip=%lld I_w=%.6g "
            "F_fires=%lld F_flip=%lld F_w=%.6g\\n",
            (long long)dg_folds, (long long)dg_folds_err,
            (long long)dg_carried, (long long)dg_rows_inh,
            dg_max_colerr, dg_max_inh, dg_worst_inh_over_win,
            (long long)dgE_fires, (long long)dgE_flip, dgE_w,
            (long long)dgD_fires, (long long)dgD_flip, dgD_wnow, dgD_w,
            (long long)dgI_fires, (long long)dgI_flip, dgI_w,
            (long long)dgF_fires, (long long)dgF_flip, dgF_w);
        if (dgn > 0 && (size_t)dgn < sizeof dgbuf)
            dg_emit(dgbuf, (size_t)dgn);
        else
            dg_emit("DIAG-INH TRUNCATED\\n", 19);
    }
    free(dg_colerr); free(dg_rowinh);
#endif
    free(col_dead); free(row_dead); free(row_frozen);""", 'report')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
