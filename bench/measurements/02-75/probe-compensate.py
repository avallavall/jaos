#!/usr/bin/env python3
"""What would compensating `cur_rl`/`cur_ru` change?

They are the only running sums in src/presolve.c with no compensation.
`ps_row_range` has used a Neumaier accumulator for activities since 02-04, for
the reason its own comment gives. The row BOUNDS never got one, and D162, D163
and D164 are all consequences: three windows widened to cover the error, a
fourth found later, and one wrong answer that no window can repair because the
error is inside a value a fold already published.

Compensating removes the error instead of covering it, which would subsume all
of that. It also changes the reduced model on real instances, so unlike those
three this cannot be a no-op — and how far from a no-op it is is exactly what
this measures, BEFORE anything is built.

Four questions:

  1. How many rows end a window read with a non-zero correction, and how large
     is it — absolutely, and against the row's own traffic?
  2. Would any of the four windows flip a verdict, in EITHER direction? The
     compensated value can refuse a row the shipped one accepts as well as the
     other way round, and only the second direction is safe.
  3. How many FOLDED VALUES would move? That is what decides whether the gate
     moves, because a folded value is written into a column box and every later
     reduction reads it.
  4. How large is the worst change in a folded value, against the column's own
     box width?

Hooks guarded by JAOS_DIAG. It changes no decision: the shipped uncompensated
value still runs the solve. Revert with: git checkout -- src/presolve.c
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
    "/* One Neumaier step, tracking the correction the shipped subtraction\n"
    " * throws away. `before` is the accumulator's value before the real code\n"
    " * updated it, `after` its value now, `t` the term that was subtracted. */\n"
    "static double dg_neumaier(double before, double after, double t)\n"
    "{\n"
    "    const double term = -t;\n"
    "    return (fabs(before) >= fabs(term)) ? ((before - after) + term)\n"
    "                                        : ((term - after) + before);\n"
    "}\n"
    "#endif",
    'stdio')

sub("""    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);""",
    """    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
#ifdef JAOS_DIAG
    double *dg_rlc = jm_calloc_array(nr, sizeof *dg_rlc);
    double *dg_ruc = jm_calloc_array(nr, sizeof *dg_ruc);
    int64_t dg_reads = 0, dg_reads_corr = 0;
    double dg_max_abs = 0.0, dg_max_rel = 0.0;
    /* Verdict flips in BOTH directions. `spared` is a row the shipped window
     * refuses and the compensated value does not -- safe. `newly_refused` is
     * the opposite and is the direction that would cost a feasible model. */
    int64_t dg_spared = 0, dg_newly_refused = 0;
    /* Folded values: how many move, and by how much against the box they land
     * in. This is what decides whether the gate moves. */
    int64_t dg_folds = 0, dg_folds_moved = 0;
    double dg_max_fold_delta = 0.0, dg_max_fold_rel = 0.0;
#endif""", 'declare')

# ---- the three shift sites --------------------------------------------------
sub("""                    const double t = m->a_value[k] * v;
                    cur_rl[i] -= t;
                    cur_ru[i] -= t;""",
    """                    const double t = m->a_value[k] * v;
#ifdef JAOS_DIAG
                    const double dg_l0 = cur_rl[i], dg_u0 = cur_ru[i];
#endif
                    cur_rl[i] -= t;
                    cur_ru[i] -= t;
#ifdef JAOS_DIAG
                    dg_rlc[i] += dg_neumaier(dg_l0, cur_rl[i], t);
                    dg_ruc[i] += dg_neumaier(dg_u0, cur_ru[i], t);
#endif""", 'fixed col shift')

sub("""                            const double t = m->a_value[kk] * v;
                            cur_rl[ii] -= t;
                            cur_ru[ii] -= t;""",
    """                            const double t = m->a_value[kk] * v;
#ifdef JAOS_DIAG
                            const double dg_l0 = cur_rl[ii];
                            const double dg_u0 = cur_ru[ii];
#endif
                            cur_rl[ii] -= t;
                            cur_ru[ii] -= t;
#ifdef JAOS_DIAG
                            dg_rlc[ii] += dg_neumaier(dg_l0, cur_rl[ii], t);
                            dg_ruc[ii] += dg_neumaier(dg_u0, cur_ru[ii], t);
#endif""", 'forcing shift')

sub("""                    if (lo_absorbs)
                        cur_rl[i] -= cmax;
                    if (hi_absorbs)
                        cur_ru[i] -= cmin;""",
    """#ifdef JAOS_DIAG
                    const double dg_l0 = cur_rl[i], dg_u0 = cur_ru[i];
#endif
                    if (lo_absorbs)
                        cur_rl[i] -= cmax;
                    if (hi_absorbs)
                        cur_ru[i] -= cmin;
#ifdef JAOS_DIAG
                    if (lo_absorbs)
                        dg_rlc[i] += dg_neumaier(dg_l0, cur_rl[i], cmax);
                    if (hi_absorbs)
                        dg_ruc[i] += dg_neumaier(dg_u0, cur_ru[i], cmin);
#endif""", 'relaxation shift')

# ---- the fold: would the value it writes move? ------------------------------
sub("""                cur_cl[j] = fold_lo;
                cur_cu[j] = fold_hi;""",
    """#ifdef JAOS_DIAG
                {
                    /* The same fold recomputed from the compensated bounds. */
                    const double rl_c = cur_rl[i] + dg_rlc[i];
                    const double ru_c = cur_ru[i] + dg_ruc[i];
                    double ilo_c, ihi_c;
                    if (a > 0.0) {
                        ilo_c = isfinite(rl_c) ? rl_c / a : -HUGE_VAL;
                        ihi_c = isfinite(ru_c) ? ru_c / a : HUGE_VAL;
                    } else {
                        ilo_c = isfinite(ru_c) ? ru_c / a : -HUGE_VAL;
                        ihi_c = isfinite(rl_c) ? rl_c / a : HUGE_VAL;
                    }
                    const double nlo_c = ilo_c > cur_cl[j] ? ilo_c : cur_cl[j];
                    const double nhi_c = ihi_c < cur_cu[j] ? ihi_c : cur_cu[j];
                    const double dlo = fabs(nlo_c - new_lo);
                    const double dhi = fabs(nhi_c - new_hi);
                    const double d = dlo > dhi ? dlo : dhi;
                    dg_folds++;
                    if (d > 0.0) {
                        dg_folds_moved++;
                        if (d > dg_max_fold_delta) dg_max_fold_delta = d;
                        const double box = fabs(cur_cu[j] - cur_cl[j]);
                        const double scale = box > 1.0 ? box : 1.0;
                        if (d / scale > dg_max_fold_rel)
                            dg_max_fold_rel = d / scale;
                    }
                }
#endif
                cur_cl[j] = fold_lo;
                cur_cu[j] = fold_hi;""", 'fold value')

# ---- the four windows, both directions --------------------------------------
def window(anchor, body, why):
    sub(anchor, "#ifdef JAOS_DIAG\n" + body + "#endif\n" + anchor, why)


window("""                if (cur_rl[i] > etol_lo || cur_ru[i] < -etol_hi) {""",
       """                {
                    const double lc = cur_rl[i] + dg_rlc[i];
                    const double uc = cur_ru[i] + dg_ruc[i];
                    const double corr = fabs(dg_rlc[i]) > fabs(dg_ruc[i])
                                        ? fabs(dg_rlc[i]) : fabs(dg_ruc[i]);
                    const double sc = (isfinite(row_traffic[i]) &&
                                       row_traffic[i] > 1.0)
                                      ? row_traffic[i] : 1.0;
                    dg_reads++;
                    if (corr > 0.0) dg_reads_corr++;
                    if (corr > dg_max_abs) dg_max_abs = corr;
                    if (corr / sc > dg_max_rel) dg_max_rel = corr / sc;
                    const bool now  = (cur_rl[i] > etol_lo ||
                                       cur_ru[i] < -etol_hi);
                    const bool comp = (lc > etol_lo || uc < -etol_hi);
                    if (now && !comp) dg_spared++;
                    if (!now && comp) dg_newly_refused++;
                }
""", 'emptied-row window')

window("""                if (new_lo > new_hi + btol) {""",
       """                {
                    const double lc = cur_rl[i] + dg_rlc[i];
                    const double uc = cur_ru[i] + dg_ruc[i];
                    double ilo_c, ihi_c;
                    if (a > 0.0) {
                        ilo_c = isfinite(lc) ? lc / a : -HUGE_VAL;
                        ihi_c = isfinite(uc) ? uc / a : HUGE_VAL;
                    } else {
                        ilo_c = isfinite(uc) ? uc / a : -HUGE_VAL;
                        ihi_c = isfinite(lc) ? lc / a : HUGE_VAL;
                    }
                    const double nlo_c = ilo_c > cur_cl[j] ? ilo_c : cur_cl[j];
                    const double nhi_c = ihi_c < cur_cu[j] ? ihi_c : cur_cu[j];
                    const bool now  = new_lo > new_hi + btol;
                    const bool comp = nlo_c > nhi_c + btol;
                    dg_reads++;
                    if (now && !comp) dg_spared++;
                    if (!now && comp) dg_newly_refused++;
                }
""", 'fold window')

window("""            if ((isfinite(ru) && min_act > ru + itol_hi) ||
                (isfinite(rl) && max_act < rl - itol_lo)) {""",
       """            {
                const double lc = rl + dg_rlc[i];
                const double uc = ru + dg_ruc[i];
                const double corr = fabs(dg_rlc[i]) > fabs(dg_ruc[i])
                                    ? fabs(dg_rlc[i]) : fabs(dg_ruc[i]);
                const double sc = (isfinite(row_traffic[i]) &&
                                   row_traffic[i] > 1.0)
                                  ? row_traffic[i] : 1.0;
                dg_reads++;
                if (corr > 0.0) dg_reads_corr++;
                if (corr > dg_max_abs) dg_max_abs = corr;
                if (corr / sc > dg_max_rel) dg_max_rel = corr / sc;
                const bool now =
                    ((isfinite(ru) && min_act > ru + itol_hi) ||
                     (isfinite(rl) && max_act < rl - itol_lo));
                const bool comp =
                    ((isfinite(uc) && min_act > uc + itol_hi) ||
                     (isfinite(lc) && max_act < lc - itol_lo));
                if (now && !comp) dg_spared++;
                if (!now && comp) dg_newly_refused++;
            }
""", 'activity clause 1 window')

window("""        if ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol_hi) ||
            (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol_lo)) {""",
       """        {
            const double lc = cur_rl[i] + dg_rlc[i];
            const double uc = cur_ru[i] + dg_ruc[i];
            const double corr = fabs(dg_rlc[i]) > fabs(dg_ruc[i])
                                ? fabs(dg_rlc[i]) : fabs(dg_ruc[i]);
            const double sc = (isfinite(row_traffic[i]) &&
                               row_traffic[i] > 1.0) ? row_traffic[i] : 1.0;
            dg_reads++;
            if (corr > 0.0) dg_reads_corr++;
            if (corr > dg_max_abs) dg_max_abs = corr;
            if (corr / sc > dg_max_rel) dg_max_rel = corr / sc;
            const bool now =
                ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol_hi) ||
                 (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol_lo));
            const bool comp =
                ((isfinite(uc) && min_act > uc + rtol_hi) ||
                 (isfinite(lc) && max_act < lc - rtol_lo));
            if (now && !comp) dg_spared++;
            if (!now && comp) dg_newly_refused++;
        }
""", 'frozen-row window')

sub("""cleanup_scratch:
    free(col_dead); free(row_dead); free(row_frozen);""",
    """cleanup_scratch:
#ifdef JAOS_DIAG
    {
        char dgbuf[600];
        const int dgn = snprintf(dgbuf, sizeof dgbuf,
            "DIAG-COMP reads=%lld reads_corr=%lld max_abs_corr=%.6g "
            "max_rel_corr=%.6g spared=%lld newly_refused=%lld "
            "folds=%lld folds_moved=%lld max_fold_delta=%.6g "
            "max_fold_rel=%.6g\\n",
            (long long)dg_reads, (long long)dg_reads_corr, dg_max_abs,
            dg_max_rel, (long long)dg_spared, (long long)dg_newly_refused,
            (long long)dg_folds, (long long)dg_folds_moved,
            dg_max_fold_delta, dg_max_fold_rel);
        if (dgn > 0 && (size_t)dgn < sizeof dgbuf)
            dg_emit(dgbuf, (size_t)dgn);
        else
            dg_emit("DIAG-COMP TRUNCATED\\n", 20);
    }
    free(dg_rlc); free(dg_ruc);
#endif
    free(col_dead); free(row_dead); free(row_frozen);""", 'report')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
