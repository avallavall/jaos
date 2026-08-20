#include <stdio.h>
#include <math.h>
#include "jaos.h"
static void g(const char *label, double rlo)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {1e-4, 1e-4};
    const double cu[] = {1.0, 1.0};
    const double rl[] = {rlo};
    const double ru[] = {0.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s ALLOC\n", label); return; }
    if (jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v) != JAOS_OK) { printf("%s LOAD\n", label); return; }
    if (jaos_solve(m) != JAOS_OK) { printf("%s SOLVE\n", label); return; }
    double x[2] = {0,0};
    (void)jaos_solution(m, x, NULL, NULL, NULL);
    printf("%-14s status=%d x={%g, %g}\n", label, (int)jaos_status_of(m), x[0], x[1]);
    jaos_model_free(m);
}
int main(void){ g("G-rl=-1e12", -1e12); g("G-control-inf", -INFINITY); return 0; }
