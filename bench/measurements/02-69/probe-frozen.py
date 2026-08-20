#!/usr/bin/env python3
"""What would the frozen-row test do with a window that covers both sides?

It compares min_act/max_act against cur_ru[i]/cur_rl[i]. Those are two sums
with two different error budgets:

  min_act, max_act   a sum over the surviving columns   -> eps * rg.traffic
  cur_rl, cur_ru     a running difference               -> eps * row_traffic[i]

The shipped window is `ps_round_tol(ps_bound_scale(cur_rl, cur_ru))`, which
scales with the MAGNITUDE OF THE BOUND and not with either of those. It was
chosen because row_traffic saturated to +inf (D155 fixed that) and because a
frozen row's LIVE traffic is routinely zero.

This computes the candidate window beside the shipped one and reports where
they disagree. It changes nothing: the shipped window still decides.

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
    "/* ONE write(2) per record, not fprintf.\n"
    " * bench/run -j N forks children that share one stderr. fprintf issues\n"
    " * several writes for a format with many conversions, so another child's\n"
    " * output lands between them and the line is torn. grep still counts one\n"
    " * line, because the fragment carries the tag; awk then parses the\n"
    " * fragment's fields only, and the sum comes out LOW.\n"
    " *\n"
    " * Measured before fixing it: the same source rebuilt and re-run gave\n"
    " * 4858, 4844 and 4798 for one counter, with the line count intact at 188\n"
    " * every time -- monotonically down, which is the signature. A count that\n"
    " * can only be undercounted is exactly the wrong shape for a probe whose\n"
    " * finding is `0 verdicts flip`. */\n"
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
    int64_t dg_rows = 0, dg_res_pos = 0, dg_fires_now = 0, dg_fires_cand = 0;
    int64_t dg_flip = 0, dg_between = 0, dg_bound_only = 0;
    double dg_worst_ratio = 0.0, dg_worst_res_ulps = 0.0;
    double dg_worst_res_over_now = 0.0;
    /* Shape A: the bound half only. Shape B: both halves. Swept together so
     * the choice is measured rather than argued (jaos-measure). */
    int64_t dg_flipA = 0, dg_flipB = 0, dg_A_is_max = 0, dg_B_is_max = 0;
    double dg_worst_ratioA = 0.0, dg_worst_ratioB = 0.0;
    /* Every other figure here is a RATIO. The absolute window is what says
     * whether the widening can ever reach PRIMAL_TOL or CHECK_TOL, both 1e-6
     * or 1e-7, and no ratio can answer that (`numerics-reviewer`). */
    double dg_max_rtol_now = 0.0, dg_max_rtolA = 0.0, dg_max_rtolB = 0.0;
    double dg_max_rg_traffic = 0.0;
#endif""", 'declare')

sub("""    free(col_dead); free(row_dead); free(row_frozen);""",
    """#ifdef JAOS_DIAG
    {
        char dgbuf[512];
        const int dgn = snprintf(dgbuf, sizeof dgbuf,
            "DIAG-FROZEN rows=%lld res_pos=%lld fires_now=%lld fires_cand=%lld "
            "flip=%lld between=%lld bound_only=%lld worst_ratio=%.6g "
            "worst_res_over_now=%.6g A_is_max=%lld B_is_max=%lld "
            "flipA=%lld flipB=%lld worst_ratioA=%.6g worst_ratioB=%.6g "
            "max_rtol_now=%.6g max_rtolA=%.6g max_rtolB=%.6g "
            "max_rg_traffic=%.6g\\n",
            (long long)dg_rows, (long long)dg_res_pos, (long long)dg_fires_now,
            (long long)dg_fires_cand, (long long)dg_flip, (long long)dg_between,
            (long long)dg_bound_only, dg_worst_ratio, dg_worst_res_over_now,
            (long long)dg_A_is_max, (long long)dg_B_is_max,
            (long long)dg_flipA, (long long)dg_flipB,
            dg_worst_ratioA, dg_worst_ratioB,
            dg_max_rtol_now, dg_max_rtolA, dg_max_rtolB, dg_max_rg_traffic);
        if (dgn > 0 && (size_t)dgn < sizeof dgbuf)
            dg_emit(dgbuf, (size_t)dgn);
        else
            dg_emit("DIAG-FROZEN TRUNCATED\\n", 22);
    }
#endif
    free(col_dead); free(row_dead); free(row_frozen);""", 'report')

sub("""        const double rtol = ps_round_tol(ps_bound_scale(cur_rl[i], cur_ru[i]));
        const double min_act = ps_min_act(&rg);
        const double max_act = ps_max_act(&rg);""",
    """        const double rtol = ps_round_tol(ps_bound_scale(cur_rl[i], cur_ru[i]));
        const double min_act = ps_min_act(&rg);
        const double max_act = ps_max_act(&rg);
#ifdef JAOS_DIAG
        {
            dg_rows++;
            /* The candidate: the larger of the three error budgets that meet
             * in this comparison. row_traffic is finite since D155. */
            const double bs = ps_bound_scale(cur_rl[i], cur_ru[i]);
            double scA = bs;   /* bound half only */
            if (isfinite(row_traffic[i]) && row_traffic[i] > scA)
                scA = row_traffic[i];
            double scB = scA;  /* both halves */
            if (isfinite(rg.traffic) && rg.traffic > scB)
                scB = rg.traffic;
            const double rtolA = ps_round_tol(scA);
            const double rtolB = ps_round_tol(scB);
            const double sc = scB;
            const double rtol2 = rtolB;
            if (scA > bs) dg_A_is_max++;
            if (scB > scA) dg_B_is_max++;
            if (rtol  > dg_max_rtol_now) dg_max_rtol_now = rtol;
            if (rtolA > dg_max_rtolA)    dg_max_rtolA    = rtolA;
            if (rtolB > dg_max_rtolB)    dg_max_rtolB    = rtolB;
            if (isfinite(rg.traffic) && rg.traffic > dg_max_rg_traffic)
                dg_max_rg_traffic = rg.traffic;
            if (rtolA > rtol) {
                const double r = rtolA / rtol;
                if (r > dg_worst_ratioA) dg_worst_ratioA = r;
            }
            if (rtolB > rtol) {
                const double r = rtolB / rtol;
                if (r > dg_worst_ratioB) dg_worst_ratioB = r;
            }

            const double over  = (isfinite(cur_ru[i]) && isfinite(min_act))
                                 ? min_act - cur_ru[i] : -HUGE_VAL;
            const double under = (isfinite(cur_rl[i]) && isfinite(max_act))
                                 ? cur_rl[i] - max_act : -HUGE_VAL;
            const double res = over > under ? over : under;

            if (res > 0.0) dg_res_pos++;
            const bool now  = res > rtol;
            const bool cand = res > rtol2;
            if (now)  dg_fires_now++;
            if (cand) dg_fires_cand++;
            if (now && !cand) dg_flip++;           /* the candidate spares it */
            if (now && !(res > rtolA)) dg_flipA++;
            if (now && !(res > rtolB)) dg_flipB++;
            if (res > 0.0 && res <= rtol) dg_between++;  /* passes on the window */
            if (rtol2 > rtol) {
                dg_bound_only++;                   /* the bound scale was NOT the max */
                const double r = rtol2 / rtol;
                if (r > dg_worst_ratio) dg_worst_ratio = r;
            }
            if (res > 0.0 && rtol > 0.0) {
                const double q = res / rtol;
                if (q > dg_worst_res_over_now) dg_worst_res_over_now = q;
            }
        }
#endif""", 'frozen-row test')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
