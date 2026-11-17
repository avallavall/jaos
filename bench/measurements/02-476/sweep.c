/* SPDX-License-Identifier: Apache-2.0 */

#include "jaos.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_log[1 << 16];
static void cap(void *u, jaos_log_level l, const char *line)
{
    (void)u; (void)l;
    size_t n = strlen(g_log);
    if (n + strlen(line) + 2 < sizeof g_log) {
        strcat(g_log, line);
        strcat(g_log, "\n");
    }
}

static double rnd(uint64_t *s)
{
    *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(*s >> 11) / 9007199254740992.0;
}

static int g_kind;
static double g_scale = 1.0;

static jaos_model *build(uint64_t seed, int n, int nr)
{
    static double c[64], cl[64], cu[64], rl[64], ru[64], av[4096];
    static int64_t as[65], ai[4096];
    uint64_t s = seed;
    static double a[64][64];
    for (int i = 0; i < nr; i++)
        for (int j = 0; j < n; j++) {
            double v = floor(100.0 * (1.0 + 9.0 * rnd(&s))) / 100.0;
            if (g_kind >= 1 && rnd(&s) < 0.2)
                v = -v;
            a[i][j] = v;
        }
    for (int j = 0; j < n; j++) {
        c[j] = floor(100.0 * (10.0 + 40.0 * rnd(&s))) / 100.0;
        cl[j] = 0.0;
        cu[j] = g_kind == 2 ? 3.0 + floor(10.0 * rnd(&s)) : INFINITY;
    }
    for (int i = 0; i < nr; i++) {
        if (g_kind == 0) {
            rl[i] = floor(100.0 * g_scale * (30.0 + 30.0 * rnd(&s))) / 100.0;
            ru[i] = INFINITY;
        } else {
            ru[i] = floor(100.0 * g_scale * (30.0 + 30.0 * rnd(&s))) / 100.0;
            rl[i] = g_kind == 2 ? ru[i] - 25.0 : -INFINITY;
        }
    }
    int64_t nz = 0;
    for (int j = 0; j < n; j++) {
        as[j] = nz;
        for (int i = 0; i < nr; i++) {
            ai[nz] = i;
            av[nz++] = a[i][j];
        }
    }
    as[n] = nz;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) exit(1);
    if (jaos_load_lp(m, n, nr, g_kind == 0 ? JAOS_MINIMIZE : JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru, nz, as, ai, av) != JAOS_OK) exit(1);
    for (int j = 0; j < n; j++)
        if (jaos_set_col_integer(m, j, true) != JAOS_OK) exit(1);
    return m;
}

static int integral(jaos_model *m, int n)
{
    double x[64];
    if (jaos_solution(m, x, NULL, NULL, NULL) != JAOS_OK) return 0;
    for (int j = 0; j < n; j++)
        if (fabs(x[j] - round(x[j])) > 1e-9) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 8, nr = argc > 2 ? atoi(argv[2]) : 6;
    g_kind = argc > 3 ? atoi(argv[3]) : 0;
    int seeds = argc > 4 ? atoi(argv[4]) : 20;
    g_scale = argc > 5 ? atof(argv[5]) : 1.0;
    for (uint64_t seed = 1; seed <= (uint64_t)seeds; seed++) {
        double obj[2] = {0, 0};
        long long nodes[2] = {0, 0}, work[2] = {0, 0};
        int lat = 0, st[2] = {0, 0}, fe[2] = {0, 0};
        for (int on = 0; on < 2; on++) {
            jaos_model *m = build(seed, n, nr);
            if (jaos_set_mip_lattice(m, on) != JAOS_OK) exit(1);
            if (jaos_set_work_limit(m, 2000000000) != JAOS_OK) exit(1);
            g_log[0] = 0;
            if (jaos_set_log_callback(m, cap, NULL) != JAOS_OK) exit(1);
            if (jaos_set_log_level(m, JAOS_LOG_SUMMARY) != JAOS_OK) exit(1);
            if (jaos_solve(m) != JAOS_OK) exit(1);
            st[on] = (int)jaos_status_of(m);
            if (jaos_objective(m, &obj[on]) != JAOS_OK) obj[on] = NAN;
            fe[on] = st[on] == JAOS_SOLVE_OPTIMAL ? integral(m, n) : -1;
            jaos_mip_report r;
            if (jaos_mip_result(m, &r) != JAOS_OK) exit(1);
            nodes[on] = r.nodes;
            work[on] = jaos_work_units(m);
            if (on) lat = strstr(g_log, "lattice: after") != NULL;
            jaos_model_free(m);
        }
        printf("kind %d seed %2llu off %d %.6f (%lld nodes %lld) on %d %.6f (%lld nodes %lld) lattice %d feas %d %d %s\n",
               g_kind, (unsigned long long)seed, st[0], obj[0], nodes[0], work[0], st[1], obj[1], nodes[1], work[1], lat,
               fe[0], fe[1], (st[0] == st[1] && st[0] == JAOS_SOLVE_OPTIMAL && fabs(obj[0] - obj[1]) > 1e-6 * (1 + fabs(obj[0]))) ? "MISMATCH" : "");
    }
    return 0;
}
