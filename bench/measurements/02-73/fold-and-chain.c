/* Four models, all with exactly representable feasible points, built to ask
 * where D162's shift count still does not reach.
 *
 * The shared trick in all four: 2^-25 is a quarter of an ulp of 1e9, so a
 * column fixed at 2^-25 subtracted from an accumulator of magnitude 1e9 rounds
 * back to where it started. 256 of them lose 2^-17 = 7.6294e-6 in total, and
 * every value involved is a dyadic rational a double holds exactly.
 *
 *   FOLD   the singleton-row fold judges `cur_rl[i] / a` -- the same running
 *          difference -- on a fixed eight ulps. D162 did not touch it.
 *   CHAIN  the fold then FIXES a column at that value, and the row receiving it
 *          is charged one shift at its own traffic. The count bounds the local
 *          rounding, not the error already inside the value.
 *   END    clause 1 with the error in a bound of magnitude 1e9 and a traffic of
 *          7.6e-6. Only the `ps_end_scale` half of the window covers it, so
 *          this is the case that separates D162's second revision from its
 *          first.
 *   EDGE   END moved 2e-4 away from feasible, which is 3.5x the widened
 *          window. The control that says the window still refuses.
 *
 * Found by `numerics-reviewer` reviewing D162.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

#define KS 256
#define SMALL ldexp(1.0, -25)      /* a quarter ulp of 1e9 */
#define LOST  ldexp(1.0, -17)      /* KS * SMALL, exactly */
#define WCAP  ldexp(1.0, -23)      /* two ulps of 1e9 */

static void report(const char *label, jaos_model *m)
{
    /* `jm_presolve_run` is the same code in both builds -- `-DJAOS_NO_PRESOLVE`
     * only stops `jaos_solve` from CONSULTING it. Printing its outcome under
     * the reference build would put the shipping verdict in the oracle's row,
     * which is a probe that lies while looking fine. Say so instead. */
#ifdef JAOS_NO_PRESOLVE
    printf("%-24s presolve=%-13s", label, "not consulted");
#else
    jm_presolve p;
    jm_presolve_init(&p);
    if (jm_presolve_run(m, &p, NULL) != JAOS_OK) { printf("%s RUN\n", label); return; }
    printf("%-24s presolve=%-13s", label,
           p.outcome == JM_PRESOLVE_INFEASIBLE ? "INFEASIBLE" : "not refused");
    jm_presolve_free(&p);
#endif
    if (jaos_solve(m) != JAOS_OK) { printf(" solve=ERR\n"); return; }
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    printf(" solve=%-11s obj=%.17g\n",
           jaos_solve_status_str(jaos_status_of(m)), obj);
    jaos_model_free(m);
}

/* FOLD: x_big + 256 smalls == 1e9, x_big in [0, 1e9 - 2^-17].
 * Round 1 removes the smalls and loses all of them; round 2 folds the row onto
 * x_big and asks whether [1e9, 1e9] meets [0, 1e9 - 2^-17]. */
static void g_fold(const char *label, double extra)
{
    enum { NC = KS + 1 };
    static double c[NC], cl[NC], cu[NC], av[NC];
    static int64_t as[NC + 1], ai[NC];
    const double rl[] = { 1e9 }, ru[] = { 1e9 };
    for (int64_t j = 0; j < NC; j++) {
        as[j] = j; ai[j] = 0; av[j] = 1.0; c[j] = 0.0;
        if (j == 0) { c[j] = 1.0; cl[j] = 0.0; cu[j] = 1e9 - LOST - extra; }
        else        { cl[j] = cu[j] = SMALL; }
    }
    as[NC] = NC;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return;
    if (jaos_load_lp(m, NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     NC, as, ai, av) != JAOS_OK) { printf("%s LOAD\n", label); return; }
    report(label, m);
}

/* END / EDGE: x_big + 256 smalls + w1 + w2 == 1e9, with two live cost-1
 * columns keeping the row at degree 3 so clause 1 judges it. */
static void g_end(const char *label, double extra)
{
    enum { NC = KS + 3 };
    static double c[NC], cl[NC], cu[NC], av[NC];
    static int64_t as[NC + 1], ai[NC];
    const double rl[] = { 1e9 }, ru[] = { 1e9 };
    for (int64_t j = 0; j < NC; j++) {
        as[j] = j; ai[j] = 0; av[j] = 1.0; c[j] = 0.0;
        if (j == 0)            { c[j] = 1.0; cl[j] = 0.0; cu[j] = 1e9 - LOST - extra; }
        else if (j < KS + 1)   { cl[j] = cu[j] = SMALL; }
        else                   { c[j] = 1.0; cl[j] = 0.0; cu[j] = WCAP; }
    }
    as[NC] = NC;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return;
    if (jaos_load_lp(m, NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     NC, as, ai, av) != JAOS_OK) { printf("%s LOAD\n", label); return; }
    report(label, m);
}

/* CHAIN: the error crosses from one row to another inside a fixed column.
 *   row S:  x1 + 256 y_s == 1e9              y_s fixed at 2^-25
 *   row R:  x1 + w1 + w2 == 1e9 - 63*2^-23   w1, w2 in [0, 2^-23]
 * Feasible exactly at x1 = 1e9 - 2^-17, w1 = 2^-23, w2 = 0. */
static void g_chain(const char *label)
{
    enum { NC = KS + 3, NNZ = KS + 4 };
    static double c[NC], cl[NC], cu[NC], av[NNZ];
    static int64_t as[NC + 1], ai[NNZ];
    const double rl[] = { 1e9, 1e9 - 63.0 * WCAP };
    const double ru[] = { 1e9, 1e9 - 63.0 * WCAP };
    int64_t nz = 0;
    for (int64_t j = 0; j < NC; j++) {
        as[j] = nz; c[j] = 0.0;
        if (j == 0) {                       /* x1, in both rows */
            cl[j] = 1e9 - 1.0; cu[j] = 1e9 + 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
            ai[nz] = 1; av[nz++] = 1.0;
        } else if (j < KS + 1) {            /* the y_s, on row S */
            cl[j] = cu[j] = SMALL;
            ai[nz] = 0; av[nz++] = 1.0;
        } else {                            /* w1, w2 on row R */
            c[j] = 1.0; cl[j] = 0.0; cu[j] = WCAP;
            ai[nz] = 1; av[nz++] = 1.0;
        }
    }
    as[NC] = nz;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return;
    if (jaos_load_lp(m, NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av) != JAOS_OK) { printf("%s LOAD\n", label); return; }
    report(label, m);
}

int main(void)
{
    g_fold ("FOLD-feasible",   0.0);
    g_fold ("FOLD-control",    1e-3);
    g_end  ("END-feasible",    0.0);
    g_end  ("EDGE-control",    2e-4);
    g_chain("CHAIN-feasible");
    return 0;
}
