/* The checker's terms at full precision. The record prints %.3g, which is
 * where "identical to the digit" comes from; this says whether they are. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "jaos.h"
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) return 1;
    if (jaos_solve(m) != JAOS_OK) return 1;
    int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
    double *x = calloc((size_t)nc, sizeof *x), *y = calloc((size_t)nr, sizeof *y);
    if (jaos_solution(m, x, NULL, y, NULL) != JAOS_OK) return 1;
    jaos_check_report r;
    if (jaos_check_solution(m, x, y, 1e-6, &r) != JAOS_OK) return 1;
    printf("%-10s dual=%.17g row=%.17g col=%.17g\n",
           argv[2], r.max_dual_violation, r.max_row_violation, r.max_col_violation);
    return 0;
}
