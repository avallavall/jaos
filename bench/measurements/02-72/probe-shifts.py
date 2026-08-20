#!/usr/bin/env python3
"""How many removals does a row's bound carry, and which shape should cover it?

`cur_rl[i]` and `cur_ru[i]` are plain running differences. Every removed column
subtracts its own a*v from both, with no compensation, so after k removals the
error goes with k and not only with the scale of the terms. All three windows
that judge one of those numbers count a fixed 8 ulps, which covers k of about
three.

THREE candidate shapes, swept together, because "the wider one is correct" is
an argument and the alternative shape is what refutes an argument here. Each is
evaluated PER END, because the two ends of a row are two different numbers:

  A   the count times the row's TRAFFIC
        8*eps*act + 8*eps*tr + k*eps*tr

  B   A, plus the count times the magnitude of the END this comparison reads,
      which is what the partial sums it rounded actually walked through
        8*eps*act + 8*eps*tr + k*eps*(|this end| + tr)

  C   B with `ps_bound_scale` in place of the end — the larger of the two ends
        8*eps*act + 8*eps*tr + k*eps*(max(|rl|,|ru|) + tr)

B is what ships. C is here because it was built first and D161's own test
refused it: two cost-0 singleton relaxations are two shifts, and on
`-1e12 <= x0 + x1 <= 0` that is 2*eps*1e12 = 4.4e-4 of window on the UPPER
side against an infeasibility of 2e-4. All three vanish at k = 0, which is
what keeps D161 for a row nothing was removed from.

The probe changes no decision: the shipped window still runs the solve.

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


# ---- one write(2) per record; bench/run -j N shares one stderr (02-69) -------
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
    "/* The three shapes, as one function so the sites cannot drift apart.\n"
    " * `shape` picks what the count is multiplied by: 0 the traffic alone,\n"
    " * 1 this end plus the traffic, 2 the larger end plus the traffic. */\n"
    "static double dg_win(int shape, double act, double tr,\n"
    "                     double this_end, double far_end, int64_t k)\n"
    "{\n"
    "    const double a = act > 1.0 ? act : 1.0;\n"
    "    const double t = (tr > 1.0 && tr < HUGE_VAL) ? tr : 1.0;\n"
    "    const double base = 8.0 * DBL_EPSILON * a + 8.0 * DBL_EPSILON * t;\n"
    "    double e = 1.0;\n"
    "    if (k <= 0)\n"
    "        return base;\n"
    "    if (shape == 1) {\n"
    "        const double m = (this_end > -HUGE_VAL && this_end < HUGE_VAL)\n"
    "                         ? fabs(this_end) : 1.0;\n"
    "        e = m > 1.0 ? m : 1.0;\n"
    "    } else if (shape == 2) {\n"
    "        const double m1 = (this_end > -HUGE_VAL && this_end < HUGE_VAL)\n"
    "                          ? fabs(this_end) : 1.0;\n"
    "        const double m2 = (far_end > -HUGE_VAL && far_end < HUGE_VAL)\n"
    "                          ? fabs(far_end) : 1.0;\n"
    "        e = m1 > m2 ? m1 : m2;\n"
    "        if (e < 1.0) e = 1.0;\n"
    "    } else {\n"
    "        return base + (double)k * DBL_EPSILON * t;\n"
    "    }\n"
    "    return base + (double)k * DBL_EPSILON * (e + t);\n"
    "}\n"
    "#endif",
    'stdio')

# ---- the counter, and the per-site tallies ----------------------------------
sub("""    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);""",
    """    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
#ifdef JAOS_DIAG
    int64_t *dg_terms = jm_calloc_array(nr, sizeof *dg_terms);
    /* Per site: rows tested, rows the shipped window refuses, rows each shape
     * would spare, the largest shift count, and the WIDEST ABSOLUTE window
     * each produces -- the last one is the only figure that says whether the
     * widening can reach PRIMAL_TOL 1e-7 or CHECK_TOL 1e-6. */
    int64_t dgE_rows = 0, dgE_fires = 0, dgE_fA = 0, dgE_fB = 0, dgE_fC = 0;
    int64_t dgE_k = 0, dgE_k8 = 0;
    int64_t dgI_rows = 0, dgI_fires = 0, dgI_fA = 0, dgI_fB = 0, dgI_fC = 0;
    int64_t dgI_k = 0, dgI_k8 = 0;
    int64_t dgF_rows = 0, dgF_fires = 0, dgF_fA = 0, dgF_fB = 0, dgF_fC = 0;
    int64_t dgF_k = 0, dgF_k8 = 0;
    double dgE_wnow = 0.0, dgE_wA = 0.0, dgE_wB = 0.0, dgE_wC = 0.0;
    double dgI_wnow = 0.0, dgI_wA = 0.0, dgI_wB = 0.0, dgI_wC = 0.0;
    double dgF_wnow = 0.0, dgF_wA = 0.0, dgF_wB = 0.0, dgF_wC = 0.0;
    double dgE_near = 0.0, dgI_near = 0.0, dgF_near = 0.0;
    /* The worst B/A and C/B on one row, which is what says whether the shapes
     * are separable at all on this population. */
    double dgE_BoA = 0.0, dgI_BoA = 0.0, dgF_BoA = 0.0;
    double dgE_CoB = 0.0, dgI_CoB = 0.0, dgF_CoB = 0.0;
#endif""", 'declare')

# ---- count a removal wherever one shifts an end -----------------------------
sub("""                    cur_rl[i] -= m->a_value[k] * v;
                    cur_ru[i] -= m->a_value[k] * v;
                    row_traffic[i] += fabs(m->a_value[k] * v);""",
    """                    cur_rl[i] -= m->a_value[k] * v;
                    cur_ru[i] -= m->a_value[k] * v;
                    row_traffic[i] += fabs(m->a_value[k] * v);
#ifdef JAOS_DIAG
                    if (m->a_value[k] * v != 0.0) dg_terms[i]++;
#endif""", 'fixed col shift')

sub("""                    if (lo_absorbs)
                        cur_rl[i] -= cmax;
                    if (hi_absorbs)
                        cur_ru[i] -= cmin;""",
    """                    if (lo_absorbs)
                        cur_rl[i] -= cmax;
                    if (hi_absorbs)
                        cur_ru[i] -= cmin;
#ifdef JAOS_DIAG
                    if ((lo_absorbs && cmax != 0.0) ||
                        (hi_absorbs && cmin != 0.0)) dg_terms[i]++;
#endif""", 'relaxation shift')

sub("""                            cur_rl[ii] -= m->a_value[kk] * v;
                            cur_ru[ii] -= m->a_value[kk] * v;
                            row_traffic[ii] += fabs(m->a_value[kk] * v);""",
    """                            cur_rl[ii] -= m->a_value[kk] * v;
                            cur_ru[ii] -= m->a_value[kk] * v;
                            row_traffic[ii] += fabs(m->a_value[kk] * v);
#ifdef JAOS_DIAG
                            if (m->a_value[kk] * v != 0.0) dg_terms[ii]++;
#endif""", 'forcing shift')

# ---- site E: the emptied-row feasibility test --------------------------------
# The comparison is `cur_rl > etol` on the lower side and `cur_ru < -etol` on
# the upper one, so the residue is per end and so is the window.
sub("""                if (cur_rl[i] > etol || cur_ru[i] < -etol) {
                    p->outcome = JM_PRESOLVE_INFEASIBLE;""",
    """#ifdef JAOS_DIAG
                {
                    const int64_t k = dg_terms[i];
                    const double rlo = cur_rl[i], rhi = -cur_ru[i];
                    double wA = 0.0, wB = 0.0, wC = 0.0;
                    bool now = false, fa = false, fb = false, fc = false;
                    for (int e = 0; e < 2; e++) {
                        const double res  = e == 0 ? rlo : rhi;
                        const double mine = e == 0 ? cur_rl[i] : cur_ru[i];
                        const double far  = e == 0 ? cur_ru[i] : cur_rl[i];
                        const double a = dg_win(0, 0.0, row_traffic[i],
                                                mine, far, k);
                        const double b = dg_win(1, 0.0, row_traffic[i],
                                                mine, far, k);
                        const double c = dg_win(2, 0.0, row_traffic[i],
                                                mine, far, k);
                        if (a > wA) wA = a;
                        if (b > wB) wB = b;
                        if (c > wC) wC = c;
                        if (res > etol) now = true;
                        if (res > a) fa = true;
                        if (res > b) fb = true;
                        if (res > c) fc = true;
                        if (!(res > etol) && res > 0.0 && etol > 0.0 &&
                            res / etol > dgE_near) dgE_near = res / etol;
                    }
                    dgE_rows++;
                    if (k > dgE_k) dgE_k = k;
                    if (k > 8) dgE_k8++;
                    if (etol > dgE_wnow) dgE_wnow = etol;
                    if (wA > dgE_wA) dgE_wA = wA;
                    if (wB > dgE_wB) dgE_wB = wB;
                    if (wC > dgE_wC) dgE_wC = wC;
                    if (wA > 0.0 && wB / wA > dgE_BoA) dgE_BoA = wB / wA;
                    if (wB > 0.0 && wC / wB > dgE_CoB) dgE_CoB = wC / wB;
                    if (now) dgE_fires++;
                    if (now && !fa) dgE_fA++;
                    if (now && !fb) dgE_fB++;
                    if (now && !fc) dgE_fC++;
                }
#endif
                if (cur_rl[i] > etol || cur_ru[i] < -etol) {
                    p->outcome = JM_PRESOLVE_INFEASIBLE;""", 'emptied-row test')

# ---- site I: clause 1 of the activity pass -----------------------------------
sub("""            const double itol = 8.0 * DBL_EPSILON * iscale;
            if ((isfinite(ru) && min_act > ru + itol) ||""",
    """            const double itol = 8.0 * DBL_EPSILON * iscale;
#ifdef JAOS_DIAG
            {
                const int64_t k = dg_terms[i];
                const double over  = (isfinite(ru) && isfinite(min_act))
                                     ? min_act - ru : -HUGE_VAL;
                const double under = (isfinite(rl) && isfinite(max_act))
                                     ? rl - max_act : -HUGE_VAL;
                double wA = 0.0, wB = 0.0, wC = 0.0;
                bool now = false, fa = false, fb = false, fc = false;
                for (int e = 0; e < 2; e++) {
                    const double res  = e == 0 ? over : under;
                    const double mine = e == 0 ? ru : rl;
                    const double far  = e == 0 ? rl : ru;
                    const double a = dg_win(0, rg.traffic, row_traffic[i],
                                            mine, far, k);
                    const double b = dg_win(1, rg.traffic, row_traffic[i],
                                            mine, far, k);
                    const double c = dg_win(2, rg.traffic, row_traffic[i],
                                            mine, far, k);
                    if (a > wA) wA = a;
                    if (b > wB) wB = b;
                    if (c > wC) wC = c;
                    if (res > itol) now = true;
                    if (res > a) fa = true;
                    if (res > b) fb = true;
                    if (res > c) fc = true;
                    if (!(res > itol) && res > 0.0 && itol > 0.0 &&
                        res / itol > dgI_near) dgI_near = res / itol;
                }
                dgI_rows++;
                if (k > dgI_k) dgI_k = k;
                if (k > 8) dgI_k8++;
                if (itol > dgI_wnow) dgI_wnow = itol;
                if (wA > dgI_wA) dgI_wA = wA;
                if (wB > dgI_wB) dgI_wB = wB;
                if (wC > dgI_wC) dgI_wC = wC;
                if (wA > 0.0 && wB / wA > dgI_BoA) dgI_BoA = wB / wA;
                if (wB > 0.0 && wC / wB > dgI_CoB) dgI_CoB = wC / wB;
                if (now) dgI_fires++;
                if (now && !fa) dgI_fA++;
                if (now && !fb) dgI_fB++;
                if (now && !fc) dgI_fC++;
            }
#endif
            if ((isfinite(ru) && min_act > ru + itol) ||""", 'activity clause 1')

# ---- site F: the frozen-row test ---------------------------------------------
sub("""        if ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol) ||
            (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol)) {""",
    """#ifdef JAOS_DIAG
        {
            const int64_t k = dg_terms[i];
            const double over  = (isfinite(cur_ru[i]) && isfinite(min_act))
                                 ? min_act - cur_ru[i] : -HUGE_VAL;
            const double under = (isfinite(cur_rl[i]) && isfinite(max_act))
                                 ? cur_rl[i] - max_act : -HUGE_VAL;
            double wA = 0.0, wB = 0.0, wC = 0.0;
            bool now = false, fa = false, fb = false, fc = false;
            for (int e = 0; e < 2; e++) {
                const double res  = e == 0 ? over : under;
                const double mine = e == 0 ? cur_ru[i] : cur_rl[i];
                const double far  = e == 0 ? cur_rl[i] : cur_ru[i];
                const double a = dg_win(0, rg.traffic, row_traffic[i],
                                        mine, far, k);
                const double b = dg_win(1, rg.traffic, row_traffic[i],
                                        mine, far, k);
                const double c = dg_win(2, rg.traffic, row_traffic[i],
                                        mine, far, k);
                if (a > wA) wA = a;
                if (b > wB) wB = b;
                if (c > wC) wC = c;
                if (res > rtol) now = true;
                if (res > a) fa = true;
                if (res > b) fb = true;
                if (res > c) fc = true;
                if (!(res > rtol) && res > 0.0 && rtol > 0.0 &&
                    res / rtol > dgF_near) dgF_near = res / rtol;
            }
            dgF_rows++;
            if (k > dgF_k) dgF_k = k;
            if (k > 8) dgF_k8++;
            if (rtol > dgF_wnow) dgF_wnow = rtol;
            if (wA > dgF_wA) dgF_wA = wA;
            if (wB > dgF_wB) dgF_wB = wB;
            if (wC > dgF_wC) dgF_wC = wC;
            if (wA > 0.0 && wB / wA > dgF_BoA) dgF_BoA = wB / wA;
            if (wB > 0.0 && wC / wB > dgF_CoB) dgF_CoB = wC / wB;
            if (now) dgF_fires++;
            if (now && !fa) dgF_fA++;
            if (now && !fb) dgF_fB++;
            if (now && !fc) dgF_fC++;
        }
#endif
        if ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol) ||
            (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol)) {""",
    'frozen-row test')

# ---- report and free ---------------------------------------------------------
sub("""cleanup_scratch:
    free(col_dead); free(row_dead); free(row_frozen);""",
    """cleanup_scratch:
#ifdef JAOS_DIAG
    {
        char dgbuf[1400];
        const int dgn = snprintf(dgbuf, sizeof dgbuf,
            "DIAG-SHIFTS "
            "E_rows=%lld E_fires=%lld E_fA=%lld E_fB=%lld E_fC=%lld "
            "E_k=%lld E_k8=%lld E_wnow=%.6g E_wA=%.6g E_wB=%.6g E_wC=%.6g "
            "E_near=%.6g E_BoA=%.6g E_CoB=%.6g "
            "I_rows=%lld I_fires=%lld I_fA=%lld I_fB=%lld I_fC=%lld "
            "I_k=%lld I_k8=%lld I_wnow=%.6g I_wA=%.6g I_wB=%.6g I_wC=%.6g "
            "I_near=%.6g I_BoA=%.6g I_CoB=%.6g "
            "F_rows=%lld F_fires=%lld F_fA=%lld F_fB=%lld F_fC=%lld "
            "F_k=%lld F_k8=%lld F_wnow=%.6g F_wA=%.6g F_wB=%.6g F_wC=%.6g "
            "F_near=%.6g F_BoA=%.6g F_CoB=%.6g\\n",
            (long long)dgE_rows, (long long)dgE_fires, (long long)dgE_fA,
            (long long)dgE_fB, (long long)dgE_fC,
            (long long)dgE_k, (long long)dgE_k8,
            dgE_wnow, dgE_wA, dgE_wB, dgE_wC, dgE_near, dgE_BoA, dgE_CoB,
            (long long)dgI_rows, (long long)dgI_fires, (long long)dgI_fA,
            (long long)dgI_fB, (long long)dgI_fC,
            (long long)dgI_k, (long long)dgI_k8,
            dgI_wnow, dgI_wA, dgI_wB, dgI_wC, dgI_near, dgI_BoA, dgI_CoB,
            (long long)dgF_rows, (long long)dgF_fires, (long long)dgF_fA,
            (long long)dgF_fB, (long long)dgF_fC,
            (long long)dgF_k, (long long)dgF_k8,
            dgF_wnow, dgF_wA, dgF_wB, dgF_wC, dgF_near, dgF_BoA, dgF_CoB);
        if (dgn > 0 && (size_t)dgn < sizeof dgbuf)
            dg_emit(dgbuf, (size_t)dgn);
        else
            dg_emit("DIAG-SHIFTS TRUNCATED\\n", 22);
    }
    free(dg_terms);
#endif
    free(col_dead); free(row_dead); free(row_frozen);""", 'report')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
