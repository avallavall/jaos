/* Which starting bases actually give the primal method work to do?
 * Brute force over every basis this 2x2 model admits. */
#include "jaos.h"
#include "jaos_internal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static char g_last[512];
static void logline(void *user, jaos_log_level lvl, const char *line)
{
    (void)user; (void)lvl;
    snprintf(g_last, sizeof g_last, "%s", line);
}

/* min x + 3y  s.t.  x + y >= 2,  x + 2y <= 10,  x,y in [0,5].
 * Optimum x=2, y=0, obj 2. */
static jaos_model *build(void)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) return nullptr;
    const double c[] = {1.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, -INFINITY};
    const double ru[] = {INFINITY, 10.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};
    if (jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av) != JAOS_OK) return nullptr;
    return m;
}

static const char *nm(jaos_basis_status s)
{
    switch (s) {
    case JAOS_BASIS_BASIC:    return "B";
    case JAOS_BASIS_AT_LOWER: return "L";
    case JAOS_BASIS_AT_UPPER: return "U";
    default:                  return "F";
    }
}

static long long primal_iters(void)
{
    const char *p = strstr(g_last, " primal iterations");
    if (p == nullptr) return -1;
    /* walk back over the digits */
    const char *e = p;
    while (e > g_last && e[-1] >= '0' && e[-1] <= '9') e--;
    return atoll(e);
}

int main(void)
{
    const jaos_basis_status opts[3] = {JAOS_BASIS_BASIC, JAOS_BASIS_AT_LOWER,
                                       JAOS_BASIS_AT_UPPER};
    printf("%-10s %-8s %-8s %-9s %s\n", "cols", "rows", "status", "obj",
           "primal-iters");
    for (int a = 0; a < 3; a++)
    for (int b = 0; b < 3; b++)
    for (int c = 0; c < 3; c++)
    for (int d = 0; d < 3; d++) {
        jaos_basis_status cs[2] = {opts[a], opts[b]};
        jaos_basis_status rs[2] = {opts[c], opts[d]};
        int nb = (opts[a] == JAOS_BASIS_BASIC) + (opts[b] == JAOS_BASIS_BASIC)
               + (opts[c] == JAOS_BASIS_BASIC) + (opts[d] == JAOS_BASIS_BASIC);
        if (nb != 2) continue;

        jaos_model *m = build();
        if (jaos_set_basis(m, cs, rs) != JAOS_OK) { jaos_model_free(m); continue; }
        (void)jaos_set_log_callback(m, logline, nullptr);
        (void)jaos_set_log_level(m, JAOS_LOG_SUMMARY);
        m->cfg.force_primal = true;
        g_last[0] = '\0';
        jaos_status st = jaos_solve(m);
        double obj = 0.0;
        (void)jaos_objective(m, &obj);
        long long pit = primal_iters();
        printf("%s%-9s %s%-7s %-8s %-9.6g %lld%s\n",
               nm(opts[a]), nm(opts[b]), nm(opts[c]), nm(opts[d]),
               st == JAOS_OK ? jaos_solve_status_str(jaos_status_of(m))
                             : "REFUSED",
               st == JAOS_OK ? obj : 0.0, pit,
               (st == JAOS_OK && pit > 0) ? "   <== USABLE" : "");
        jaos_model_free(m);
    }
    return 0;
}
