#!/usr/bin/env python3
"""The activity pass's INFEASIBLE clause, and the window it does not have.

`rtol = ps_row_tol(&rg)` is 8*eps*rg.traffic, the ACTIVITY half only. Clause 1
compares `min_act` against `cur_ru[i]`, and `cur_ru[i]` is a running difference
every removed column shifted by its own a*v — the BOUND half, which nothing
here covers. D159 fixed exactly this at the frozen-row test; this is the same
defect at the site that is not frozen.

The three clauses share `rtol` and the direction is not shared:

  clause 1 INFEASIBLE  min_act > ru + rtol   wider fires LESS  (safe)
  clause 2 FORCING     min_act >= ru - rtol  wider fires MORE  (02-04's cost)
  clause 3 REDUNDANT   ...                   wider fires MORE

So only clause 1 can take a wider window, and it has to be its own.

This counts what a clause-1-only window would change, and decides nothing.
ONE write(2) per record: -j N forks children onto one stderr, and a torn line
lowers a SUM while leaving the line count intact (02-69).
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
    "#ifdef JAOS_DIAG\n#include <stdio.h>\n#include <unistd.h>\n"
    "static void dg_emit(const char *b, size_t n)\n"
    "{ ssize_t r = write(2, b, n); (void)r; }\n#endif",
    'stdio')

sub("""    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);""",
    """    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
#ifdef JAOS_DIAG
    int64_t dg_rows = 0, dg_c1_now = 0, dg_c1_cand = 0, dg_flip = 0;
    int64_t dg_wider = 0;
    double dg_worst_ratio = 0.0, dg_max_now = 0.0, dg_max_cand = 0.0;
    double dg_max_rowtraf = 0.0;
#endif""", 'declare')

sub("""    free(col_dead); free(row_dead); free(row_frozen);""",
    """#ifdef JAOS_DIAG
    {
        char b[512];
        const int n = snprintf(b, sizeof b,
            "DIAG-ACT rows=%lld c1_now=%lld c1_cand=%lld flip=%lld "
            "wider=%lld worst_ratio=%.6g max_now=%.6g max_cand=%.6g "
            "max_rowtraf=%.6g\\n",
            (long long)dg_rows, (long long)dg_c1_now, (long long)dg_c1_cand,
            (long long)dg_flip, (long long)dg_wider, dg_worst_ratio,
            dg_max_now, dg_max_cand, dg_max_rowtraf);
        if (n > 0 && (size_t)n < sizeof b) dg_emit(b, (size_t)n);
        else dg_emit("DIAG-ACT TRUNCATED\\n", 19);
    }
#endif
    free(col_dead); free(row_dead); free(row_frozen);""", 'report')

sub("""            const double rtol = ps_row_tol(&rg);
            const double min_act = ps_min_act(&rg);
            const double max_act = ps_max_act(&rg);
            const double rl = cur_rl[i], ru = cur_ru[i];""",
    """            const double rtol = ps_row_tol(&rg);
            const double min_act = ps_min_act(&rg);
            const double max_act = ps_max_act(&rg);
            const double rl = cur_rl[i], ru = cur_ru[i];
#ifdef JAOS_DIAG
            {
                dg_rows++;
                /* The clause-1-only candidate: the activity half it already
                 * has, plus the bound half it does not. Same expression D159
                 * landed at the frozen-row test. */
                double sc = rg.traffic > 1.0 ? rg.traffic : 1.0;
                if (isfinite(row_traffic[i]) && row_traffic[i] > sc)
                    sc = row_traffic[i];
                const double bs = ps_bound_scale(rl, ru);
                if (bs > sc) sc = bs;
                const double itol = ps_round_tol(sc);

                const bool now  = (isfinite(ru) && min_act > ru + rtol) ||
                                  (isfinite(rl) && max_act < rl - rtol);
                const bool cand = (isfinite(ru) && min_act > ru + itol) ||
                                  (isfinite(rl) && max_act < rl - itol);
                if (now)  dg_c1_now++;
                if (cand) dg_c1_cand++;
                if (now && !cand) dg_flip++;
                if (itol > rtol) {
                    dg_wider++;
                    const double r = itol / rtol;
                    if (r > dg_worst_ratio) dg_worst_ratio = r;
                }
                if (rtol > dg_max_now)  dg_max_now  = rtol;
                if (itol > dg_max_cand) dg_max_cand = itol;
                if (isfinite(row_traffic[i]) && row_traffic[i] > dg_max_rowtraf)
                    dg_max_rowtraf = row_traffic[i];
            }
#endif""", 'activity pass')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
