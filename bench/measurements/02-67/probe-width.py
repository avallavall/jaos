#!/usr/bin/env python3
"""Does a row bound shift ever destroy the row's own width?

The two sites that remove a value-determined column subtract the SAME term
from both ends, so the width `ru - rl` is invariant in exact arithmetic. Any
change it shows is floating-point loss, and it is measured here per event:
w_before against w_after, at the moment of the subtraction.

The third shift site (the cost-0 singleton column's relaxation) subtracts
different terms from the two ends, so its width change is legitimate and is
not counted.

Hooks are guarded by JAOS_DIAG. Revert with: git checkout -- src/presolve.c
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
    "/* ONE write(2) per record. `bench/run -j N` forks children that share one\n"
    " * stderr, and fprintf issues several writes for a format with many\n"
    " * conversions, so another child's output lands between them and the line\n"
    " * is torn. The line count stays right and the SUMS come out low, which is\n"
    " * the wrong failure mode for a probe whose finding is a zero (02-69). */\n"
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
    int64_t dg_shifts = 0, dg_lost = 0, dg_destroyed = 0, dg_rows_hit = 0;
    double dg_worst_rel = 0.0, dg_worst_abs = 0.0;
    bool *dg_row_hit = jm_calloc_array(nr, sizeof *dg_row_hit);
    /* Every finite-width row's width when it entered the run, so the
     * end-of-run comparison is against the caller's own number and not
     * against a running difference. */
    double *dg_w0 = jm_calloc_array(nr, sizeof *dg_w0);
#define DG_SHIFT(ROW, TERM) do {                                            \\
    const double w0_ = cur_ru[ROW] - cur_rl[ROW];                           \\
    const double after_lo_ = cur_rl[ROW] - (TERM);                          \\
    const double after_hi_ = cur_ru[ROW] - (TERM);                          \\
    const double w1_ = after_hi_ - after_lo_;                               \\
    if (isfinite(w0_) && w0_ > 0.0) {                                       \\
        dg_shifts++;                                                        \\
        if (w1_ != w0_) {                                                   \\
            dg_lost++;                                                      \\
            if (!dg_row_hit[ROW]) { dg_row_hit[ROW] = true; dg_rows_hit++; }\\
            if (w1_ == 0.0) dg_destroyed++;                                 \\
            const double abs_ = w0_ - w1_;                                  \\
            const double rel_ = abs_ / w0_;                                 \\
            if (rel_ > dg_worst_rel) dg_worst_rel = rel_;                   \\
            if (abs_ > dg_worst_abs) dg_worst_abs = abs_;                   \\
        }                                                                   \\
    }                                                                       \\
} while (0)
#endif""", 'declare')

sub("""    free(col_dead); free(row_dead); free(row_frozen);""",
    """#ifdef JAOS_DIAG
    {
        /* The end state: how many SURVIVING rows reach the simplex with a
         * width that is not the one the caller wrote. */
        int64_t surv = 0, surv_narrowed = 0, surv_equal = 0;
        double worst_end_rel = 0.0;
        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i]) continue;
            const double w0 = dg_w0[i];
            if (!isfinite(w0) || w0 <= 0.0) continue;
            surv++;
            const double w1 = cur_ru[i] - cur_rl[i];
            if (w1 != w0) {
                surv_narrowed++;
                if (w1 == 0.0) surv_equal++;
                const double r = (w0 - w1) / w0;
                if (r > worst_end_rel) worst_end_rel = r;
            }
        }
        char dgbuf[512];
        const int dgn = snprintf(dgbuf, sizeof dgbuf,
            "DIAG-WIDTH shifts=%lld lost=%lld destroyed=%lld rows_hit=%lld "
            "worst_rel=%.6g worst_abs=%.6g surv=%lld surv_narrowed=%lld "
            "surv_equal=%lld worst_end_rel=%.6g\\n",
            (long long)dg_shifts, (long long)dg_lost, (long long)dg_destroyed,
            (long long)dg_rows_hit, dg_worst_rel, dg_worst_abs,
            (long long)surv, (long long)surv_narrowed, (long long)surv_equal,
            worst_end_rel);
        if (dgn > 0 && (size_t)dgn < sizeof dgbuf)
            dg_emit(dgbuf, (size_t)dgn);
        else
            dg_emit("DIAG-WIDTH TRUNCATED\\n", 21);
    }
    free(dg_row_hit); free(dg_w0);
#undef DG_SHIFT
#endif
    free(col_dead); free(row_dead); free(row_frozen);""", 'report')

# the caller's own widths, captured where cur_rl/cur_ru are seeded
sub("""        cur_rl[i] = m->row_lower[i];
        cur_ru[i] = m->row_upper[i];""",
    """        cur_rl[i] = m->row_lower[i];
        cur_ru[i] = m->row_upper[i];
#ifdef JAOS_DIAG
        dg_w0[i] = m->row_upper[i] - m->row_lower[i];
#endif""", 'seed widths')

# site 1: the fixed column
sub("""                    cur_rl[i] -= m->a_value[k] * v;
                    cur_ru[i] -= m->a_value[k] * v;""",
    """#ifdef JAOS_DIAG
                    DG_SHIFT(i, m->a_value[k] * v);
#endif
                    cur_rl[i] -= m->a_value[k] * v;
                    cur_ru[i] -= m->a_value[k] * v;""", 'fixed col shift')

# site 2: the forcing row's own fixing
sub("""                            cur_rl[ii] -= m->a_value[kk] * v;
                            cur_ru[ii] -= m->a_value[kk] * v;""",
    """#ifdef JAOS_DIAG
                            DG_SHIFT(ii, m->a_value[kk] * v);
#endif
                            cur_rl[ii] -= m->a_value[kk] * v;
                            cur_ru[ii] -= m->a_value[kk] * v;""",
    'forcing row shift')

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('probe applied to', P)
