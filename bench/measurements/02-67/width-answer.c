/* Does a destroyed row width change the ANSWER?
 *
 * The width probe says the shift destroys the row's own width. That is a loss
 * of information. Whether it is a loss of ACCURACY is a different question,
 * and it is the one that decides whether anything needs repairing.
 *
 * Built twice — once normally, once with -DJAOS_NO_PRESOLVE, which is the only
 * oracle for output no predicate of the three sets reads. Each case prints its
 * status and objective; the two builds are diffed by the driver script.
 *
 * Every case destroys the width the same way (a fixed column whose a*v swamps
 * the row's own bounds), and they differ in what is left behind for the
 * surviving columns to do.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

/* min c1*x1 + c2*x2
 * s.t.  R:  big*x0 + a1*x1 + a2*x2  in [rlo, rhi]
 *       x0 fixed at v0; x1, x2 in their own boxes.                        */
static void one(const char *label, double big, double v0,
                double a1, double a2, double rlo, double rhi,
                double x1lo, double x1hi, double x2lo, double x2hi)
{
    const double c[]  = {0.0, 1.0, 0.0};
    const double cl[] = {v0, x1lo, x2lo};
    const double cu[] = {v0, x1hi, x2hi};
    const double rl[] = {rlo};
    const double ru[] = {rhi};
    const int64_t as[] = {0, 1, 2, 3};
    const int64_t ai[] = {0, 0, 0};
    const double av[]  = {big, a1, a2};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s  ALLOC FAIL\n", label); return; }
    if (jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av) != JAOS_OK) {
        printf("%s  LOAD FAIL\n", label); jaos_model_free(m); return;
    }
    if (jaos_solve(m) != JAOS_OK) {
        printf("%s  SOLVE ERROR\n", label); jaos_model_free(m); return;
    }
    const int st = (int)jaos_status_of(m);
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    double x[3] = {0, 0, 0};
    (void)jaos_solution(m, x, NULL, NULL, NULL);
    printf("%-14s status=%d obj=%.17g x1=%.17g x2=%.17g\n",
           label, st, obj, x[1], x[2]);
    jaos_model_free(m);
}

int main(void)
{
    /* A: the width dies, and what is left for x1 is at the shift's own scale. */
    one("A-dies",    1e17, 1.0, 1.0, 0.0, 1.0, 2.0, -1e18, 1e18, 0.0, 0.0);
    /* B: the control. Same shape, a magnitude that cannot destroy the width. */
    one("B-control", 1e3,  1.0, 1.0, 0.0, 1.0, 2.0, -1e18, 1e18, 0.0, 0.0);
    /* C: the width dies and the surviving column has the big coefficient too,
     *    so its own value stays at scale 1 while the activity is at 1e17. */
    one("C-bigcoef", 1e17, 1.0, 1e17, 0.0, 1.0, 2.0, -2.0, 2.0, 0.0, 0.0);
    /* D: the same as C with a second surviving column, so the row keeps
     *    degree 2 and no singleton rule can consume it. */
    one("D-deg2",    1e17, 1.0, 1e17, 1.0, 1.0, 2.0, -2.0, 2.0, -5.0, 5.0);
    /* E: an inequality rather than a range, which has no width to lose. */
    one("E-noWidth", 1e17, 1.0, 1.0, 0.0, 1.0, INFINITY, -1e18, 1e18, 0.0, 0.0);
    /* F: the amplifying shape, which is the one that would break the bound.
     *    The surviving singleton has a TINY coefficient, so the row-space
     *    width divides by 1e-6 on its way into the column: a width of 1
     *    becomes a width of 1e6 in x1. This is the shape that makes the
     *    collapsed fold's error unbounded (TODO §1) and the question is
     *    whether it does the same here. */
    one("F-amplify", 1e17, 1.0, 1e-6, 0.0, 1.0, 2.0, -1e30, 1e30, 0.0, 0.0);
    /* G: F with the amplification pushed further. */
    one("G-amplify2", 1e17, 1.0, 1e-12, 0.0, 1.0, 2.0, -1e30, 1e30, 0.0, 0.0);
    return 0;
}
