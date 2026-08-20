/* The minimum model for the two-product.
 *
 *   c0 = 2^27 + 1,  x0 fixed at 2^27 + 1   -> exact product 2^54 + 2^28 + 1
 *   c1 = -1,        x1 fixed at 2^54 + 2^28 -> exact product -(2^54 + 2^28)
 *
 * One ulp at 2^54 is 4, so the first product rounds to 2^54 + 2^28 and leaves a
 * residue of exactly 1. Every accumulator over the ROUNDED products therefore
 * sums to 0, however carefully it adds; the true objective is 1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "jaos.h"

int main(void)
{
    const double big = ldexp(1.0, 27) + 1.0;
    const double prod = ldexp(1.0, 54) + ldexp(1.0, 28);
    printf("  fl(big*big)      = %.17g\n", big * big);
    printf("  prod             = %.17g\n", prod);
    printf("  residue          = %.17Lg\n",
           (long double)big * (long double)big - (long double)prod);

    const double c[]  = {big, -1.0};
    const double cl[] = {big, prod}, cu[] = {big, prod};
    const double rl[] = {-1e30}, ru[] = {1e30};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    if (jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av) != JAOS_OK) { printf("LOAD\n"); return 1; }
    if (jaos_solve(m) != JAOS_OK) return 1;
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    double x[2] = {0, 0}, y[1] = {0};
    (void)jaos_solution(m, x, NULL, y, NULL);
    jaos_check_report r;
    (void)jaos_check_solution(m, x, y, 1e-9, &r);
    printf("  status=%s  published obj=%.17g  checker=%.17g  true=1\n",
           jaos_solve_status_str(jaos_status_of(m)), obj, r.primal_objective);
    jaos_model_free(m);
    return 0;
}
