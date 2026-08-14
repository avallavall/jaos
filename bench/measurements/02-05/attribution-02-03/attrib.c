#include "jaos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int main(int argc, char **argv)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) { fprintf(stderr,"read fail\n"); return 1; }
    if (jaos_solve(m) != JAOS_OK) { fprintf(stderr,"solve fail\n"); return 1; }
    jaos_check_report rep;
    if (jaos_check_solution(m, m->sol_col, m->sol_dual, 1e-6, &rep) != JAOS_OK) return 1;
    const char *tag = (argc>2)?argv[2]:"";
    printf("%-12s %-22s obj=%.15g col=%.3g row=%.3g rowrel=%.4g dual=%.3g gap=%.3g rsub=%.3g rays=%lld cert=%s\n",
        argv[1], tag, rep.primal_objective, rep.max_col_violation, rep.max_row_violation,
        rep.max_row_violation_relative, rep.max_dual_violation, rep.objective_gap,
        rep.relative_suboptimality, (long long)rep.unquantified_rays,
        rep.gap_certified?"yes":"no");
    return 0;
}
