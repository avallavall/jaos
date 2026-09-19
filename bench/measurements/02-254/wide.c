/* The reading behind 02-254's cone widths: minimise -a'x over ||x|| <= t
 * with t fixed at 1 and x of W members, whose optimum is -||a||, and print
 * the status, the relative error of the objective and the work units.
 *
 * Usage: wide W
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const int64_t w = argc > 1 ? atoll(argv[1]) : 500;
    const int64_t n = w + 1;
    jaos_model *m = nullptr;
    if (w < 1 || jaos_model_new(&m) != JAOS_OK)
        return 1;
    const double inf = jaos_infinity();
    double *cost = calloc((size_t)n, sizeof *cost);
    double *cl = calloc((size_t)n, sizeof *cl);
    double *cu = calloc((size_t)n, sizeof *cu);
    int64_t *as = calloc((size_t)n + 1, sizeof *as);
    int64_t *cols = calloc((size_t)n, sizeof *cols);
    if (cost == nullptr || cl == nullptr || cu == nullptr || as == nullptr ||
        cols == nullptr)
        return 1;
    double aa = 0.0;
    cl[0] = cu[0] = 1.0;
    for (int64_t j = 1; j < n; j++) {
        const double a = (j % 2 == 0 ? 1.0 : -1.0) * (double)(1 + j % 5);
        cost[j] = -a;
        cl[j] = -inf;
        cu[j] = inf;
        aa += a * a;
    }
    for (int64_t j = 0; j < n; j++)
        cols[j] = j;
    double obj = 0.0;
    if (jaos_load_lp(m, n, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr) != JAOS_OK ||
        jaos_add_cone(m, JAOS_CONE_QUADRATIC, n, cols) != JAOS_OK ||
        jaos_solve(m) != JAOS_OK || jaos_objective(m, &obj) != JAOS_OK) {
        fprintf(stderr, "%s\n", jaos_model_error(m));
        return 1;
    }
    printf("members %lld status %s err %.3e iterations %lld work %lld\n",
           (long long)w, jaos_solve_status_str(jaos_status_of(m)),
           fabs(obj + sqrt(aa)) / sqrt(aa), (long long)jaos_iterations(m),
           (long long)jaos_work_units(m));
    jaos_model_free(m);
    free(cost);
    free(cl);
    free(cu);
    free(as);
    free(cols);
    return 0;
}
