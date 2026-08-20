/* Two distinct points that a naive sum cannot tell apart.
 *
 * This is the arithmetic behind the item, on its own, with no solve in the
 * way. `settled_objective` ranks two rounds by `sum cost0[v] * x[v]`, and
 * `better_point` publishes the lower one. The shape below makes that sum
 * return exactly 0.0 for BOTH points however carefully the terms are
 * ordered, because one ulp at 1e16 is 2: adding 1 to it never moves it, so
 * the 256 unit terms are gone before the -1e16 arrives.
 *
 * It says nothing about whether a solve can reach two such points. That is
 * what the instrumented three-set run in this directory measures, and the
 * answer there is no.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <math.h>

#define N 258

static double naive(const double *c, const double *x)
{
    double s = 0.0;
    for (int i = 0; i < N; i++)
        s += c[i] * x[i];
    return s;
}

/* Neumaier, plus Dekker's split for what each product lost — the pair
 * `jm_model_publish_objective` uses for the published number (D169, D172). */
static double compensated(const double *c, const double *x)
{
    const double SPLIT = 134217729.0, BIG = 0x1p996;
    double sum = 0.0, comp = 0.0;
    for (int i = 0; i < N; i++) {
        const double t = c[i] * x[i];
        double a = sum, u = a + t;
        comp += (fabs(a) >= fabs(t)) ? ((a - u) + t) : ((t - u) + a);
        sum = u;
        if (isfinite(t) && fabs(c[i]) <= BIG && fabs(x[i]) <= BIG) {
            const double ca = SPLIT * c[i], ah = ca - (ca - c[i]);
            const double al = c[i] - ah;
            const double cb = SPLIT * x[i], bh = cb - (cb - x[i]);
            const double bl = x[i] - bh;
            const double e = ((ah * bh - t) + ah * bl + al * bh) + al * bl;
            if (e != 0.0 && isfinite(e)) {
                a = sum; u = a + e;
                comp += (fabs(a) >= fabs(e)) ? ((a - u) + e) : ((e - u) + a);
                sum = u;
            }
        }
    }
    return sum + comp;
}

int main(void)
{
    double c[N], xa[N], xb[N];

    /* A column of cost +1e16 held at 1, then 256 columns of cost 1, then a
     * column of cost -1e16 held at 1. **The order is the mechanism**, and
     * `settled_objective` walks the variables in index order: the large term
     * has to arrive BEFORE the small ones, so that each of them is added to
     * 1e16 and lost. Put the -1e16 second instead and the cancellation
     * happens first, the small terms land on zero, and the naive sum is
     * right — which is what the first version of this file measured. */
    c[0] = 1e16;      xa[0] = 1.0;      xb[0] = 1.0;
    for (int i = 1; i < N - 1; i++) {
        c[i]  = 1.0;
        xa[i] = 1.0;    /* round A: every unit column at its upper bound */
        xb[i] = 0.0;    /* round B: every unit column at zero           */
    }
    c[N - 1] = -1e16; xa[N - 1] = 1.0;  xb[N - 1] = 1.0;

    const double na = naive(c, xa),  nb = naive(c, xb);
    const double ca = compensated(c, xa), cb = compensated(c, xb);

    printf("point A: naive %.17g   compensated %.17g\n", na, ca);
    printf("point B: naive %.17g   compensated %.17g\n", nb, cb);
    printf("\nseparation under the naive sum        : %.17g\n", na - nb);
    printf("separation under the compensated sum  : %.17g\n", ca - cb);
    /* The roles: B is the saved best and A is where the loop stopped.
     * take_best_if_better asks better_point(tol, bst_dviol, bst_obj,
     * cur_dviol, cur_obj) and restores the best only when it says yes. */
    printf("\nB is the better point: its true objective is %.17g against "
           "A's %.17g.\n", cb, ca);
    printf("take_best_if_better restores B when better_point says so:\n"
           "  under the naive sum        : %s   (0 < 0 is false)\n"
           "  under the compensated sum  : %s\n",
           nb < na ? "yes" : "NO", cb < ca ? "yes" : "no");
    printf("\nSo the loop publishes A and leaves %.17g on the table, and no\n"
           "number anywhere says it happened.\n", ca - cb);

    /* One ulp at 1e16 is 2, which is the whole mechanism. */
    printf("\nulp(1e16) = %.17g, so 1e16 + 1 == 1e16 is %s\n",
           nextafter(1e16, INFINITY) - 1e16, (1e16 + 1.0 == 1e16) ? "true" : "false");
    return (na == nb && ca != cb) ? 0 : 1;
}
