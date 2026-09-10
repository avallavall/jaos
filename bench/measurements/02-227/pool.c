/* The solution pool holds points, and a point it holds has to be a point of
 * the model. Six properties, each of which the pool would break on its own:
 *
 *   1. the count never passes the size that was asked for
 *   2. entry 0 carries the objective the solve published
 *   3. the objectives run best first, in the model's own sense
 *   4. no two entries are the same point
 *   5. the objective an entry carries is the objective its point has
 *   6. every entry passes `jaos_check_solution`: its bounds, its rows, its
 *      integer marks, its SOS sets, its semi-continuous floors and its
 *      indicator rows
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rs;
static uint64_t nx(void)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return rs;
}
static int64_t pick(int64_t n) { return (int64_t)(nx() % (uint64_t)n); }

#define MAXC 8
#define MAXR 6

typedef struct {
    int64_t nc, nr, nz;
    jaos_obj_sense sense;
    double cost[MAXC], cl[MAXC], cu[MAXC];
    double rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool integer[MAXC], semi[MAXC];
    int64_t sos_n, sos_col[MAXC];
    double sos_w[MAXC];
    int sos_type;
    int64_t ind_row, ind_col;
    int ind_val;
    int64_t pool_size;
} model;

static void gen(model *g)
{
    memset(g, 0, sizeof *g);
    g->nc = 2 + pick(MAXC - 1);
    g->nr = 1 + pick(MAXR);
    g->sense = pick(2) ? JAOS_MINIMIZE : JAOS_MAXIMIZE;
    g->sos_n = 0;
    g->ind_row = -1;
    g->ind_col = -1;
    g->pool_size = 1 + pick(8);

    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)(pick(11) - 5);
        /* Every box holds zero, so the zero point is there to be found and
         * the model is feasible far more often than not. A pool with one
         * entry says nothing about the order it keeps. */
        switch (pick(4)) {
        case 0: g->cl[j] = 0.0;  g->cu[j] = 1.0; break;
        case 1: g->cl[j] = 0.0;  g->cu[j] = (double)(1 + pick(4)); break;
        default: g->cl[j] = -(double)(1 + pick(3));
                 g->cu[j] = (double)(1 + pick(3)); break;
        }
        /* Every column integer and every box finite, so the pool has whole
         * points to hold and the tree ends. */
        g->integer[j] = true;
    }

    int64_t nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->ap[j] = nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (pick(10) < 4)
                continue;
            const double v = (double)(pick(7) - 3);
            if (v == 0.0)
                continue;
            g->ai[nz] = i; g->av[nz] = v; nz++;
        }
    }
    g->ap[g->nc] = nz;
    g->nz = nz;

    /* Every row holds the zero point, so the model is feasible and the tree
     * has somewhere to walk. */
    for (int64_t i = 0; i < g->nr; i++) {
        const double b = (double)(1 + pick(12));
        switch (pick(5)) {
        case 0: g->rl[i] = -INFINITY; g->ru[i] = b; break;
        case 1: g->rl[i] = -b; g->ru[i] = INFINITY; break;
        case 2: g->rl[i] = -b; g->ru[i] = b; break;
        case 3: g->rl[i] = 0.0; g->ru[i] = b; break;
        default: g->rl[i] = -b; g->ru[i] = 0.0;
        }
    }

    const int64_t shape = pick(4);
    if (shape == 1 && g->nc >= 2) {
        g->sos_type = 1 + (int)pick(2);
        g->sos_n = 2 + pick(g->nc - 1);
        for (int64_t k = 0; k < g->sos_n; k++) {
            g->sos_col[k] = k;
            g->sos_w[k] = (double)(k + 1);
        }
    } else if (shape == 2) {
        const int64_t j = pick(g->nc);
        g->semi[j] = true;
        g->integer[j] = false;
        if (g->cl[j] <= 0.0) g->cl[j] = 1.0;
        if (g->cu[j] < g->cl[j]) g->cu[j] = g->cl[j] + 3.0;
    } else if (shape == 3) {
        const int64_t j = pick(g->nc);
        g->cl[j] = 0.0; g->cu[j] = 1.0; g->integer[j] = true;
        g->ind_col = j;
        g->ind_row = pick(g->nr);
        g->ind_val = (int)pick(2);
    }
}

static void show(const model *g)
{
    printf("  nc=%" PRId64 " nr=%" PRId64 " sense=%s pool=%" PRId64 "\n",
           g->nc, g->nr, g->sense == JAOS_MINIMIZE ? "min" : "max",
           g->pool_size);
    for (int64_t j = 0; j < g->nc; j++)
        printf("  col %" PRId64 " cost=%g lo=%g up=%g int=%d semi=%d\n", j,
               g->cost[j], g->cl[j], g->cu[j], (int)g->integer[j],
               (int)g->semi[j]);
    for (int64_t i = 0; i < g->nr; i++)
        printf("  row %" PRId64 " lo=%g up=%g\n", i, g->rl[i], g->ru[i]);
    for (int64_t j = 0; j < g->nc; j++)
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++)
            printf("  a[%" PRId64 ",%" PRId64 "]=%g\n", g->ai[k], j, g->av[k]);
    if (g->sos_n > 0)
        printf("  sos type=%d n=%" PRId64 "\n", g->sos_type, g->sos_n);
    if (g->ind_row >= 0)
        printf("  ind row=%" PRId64 " col=%" PRId64 " val=%d\n", g->ind_row,
               g->ind_col, g->ind_val);
}

static jaos_status build(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK)
        return rc;
    rc = jaos_load_lp(m, g->nc, g->nr, g->sense, 0.0, g->cost, g->cl, g->cu,
                      g->rl, g->ru, g->nz, g->ap, g->ai, g->av);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    for (int64_t j = 0; j < g->nc && rc == JAOS_OK; j++) {
        if (g->integer[j])
            rc = jaos_set_col_integer(m, j, true);
        if (rc == JAOS_OK && g->semi[j])
            rc = jaos_set_col_semicontinuous(m, j, true);
    }
    if (rc == JAOS_OK && g->sos_n > 0)
        rc = jaos_add_sos(m, g->sos_type, g->sos_n, g->sos_col, g->sos_w);
    if (rc == JAOS_OK && g->ind_row >= 0)
        rc = jaos_set_row_indicator(m, g->ind_row, g->ind_col, g->ind_val);
    if (rc == JAOS_OK)
        rc = jaos_set_mip_pool_size(m, g->pool_size);
    if (rc == JAOS_OK)
        rc = jaos_set_work_limit(m, 20000000);
    if (rc == JAOS_OK)
        rc = jaos_set_mip_node_limit(m, 200000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

static double objective_of(const model *g, const double *x)
{
    double s = 0.0;
    for (int64_t j = 0; j < g->nc; j++)
        s += g->cost[j] * x[j];
    return s;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? strtoll(argv[1], nullptr, 10) : 20000;
    rs = argc > 2 ? strtoull(argv[2], nullptr, 10) : 88172645463325252ULL;
    const char *dump = argc > 3 && argv[3][0] != '\0' ? argv[3] : nullptr;

    int64_t built = 0, pooled = 0, entries = 0, skipped = 0, bad = 0;
    int64_t multi = 0, near_dup = 0;
    for (int64_t t = 0; t < runs; t++) {
        model g;
        gen(&g);

        jaos_model *m = nullptr;
        if (build(&g, &m) != JAOS_OK) { skipped++; continue; }
        if (jaos_solve(m) != JAOS_OK) { jaos_model_free(m); skipped++; continue; }
        const jaos_solve_status ss = jaos_status_of(m);
        if (ss != JAOS_SOLVE_OPTIMAL) { jaos_model_free(m); skipped++; continue; }
        built++;

        double opt = 0.0;
        (void)jaos_objective(m, &opt);

        int64_t n = 0;
        if (jaos_mip_pool_count(m, &n) != JAOS_OK) {
            printf("COUNT t=%" PRId64 " the pool count is not readable\n", t);
            bad++; jaos_model_free(m); continue;
        }
        if (n > g.pool_size) {
            printf("SIZE t=%" PRId64 " asked for %" PRId64 " and got %" PRId64 "\n",
                   t, g.pool_size, n);
            bad++;
            if (dump != nullptr) show(&g);
        }
        if (n == 0) { jaos_model_free(m); continue; }
        pooled++;
        entries += n;
        if (n > 1)
            multi++;

        const double sigma = g.sense == JAOS_MINIMIZE ? 1.0 : -1.0;
        double xs[9][MAXC];
        double objs[9];
        bool ok = true;
        for (int64_t k = 0; k < n && k < 9; k++) {
            double o = 0.0;
            if (jaos_mip_pool_solution(m, k, xs[k], &o) != JAOS_OK) {
                printf("READ t=%" PRId64 " entry %" PRId64 " is not readable\n", t, k);
                bad++; ok = false; break;
            }
            objs[k] = o;

            const double mine = objective_of(&g, xs[k]);
            const double s = fabs(o) > 1.0 ? fabs(o) : 1.0;
            if (fabs(mine - o) > 1e-6 * s) {
                printf("OBJ t=%" PRId64 " entry %" PRId64 " carries %.17g and "
                       "its point is worth %.17g\n", t, k, o, mine);
                bad++;
                if (dump != nullptr) show(&g);
            }

            jaos_check_report rep;
            memset(&rep, 0, sizeof rep);
            if (jaos_check_solution(m, xs[k], nullptr, 1e-6, &rep) != JAOS_OK) {
                printf("CHECK t=%" PRId64 " entry %" PRId64 " the checker refused "
                       "to judge it\n", t, k);
                bad++;
                continue;
            }
            if (!rep.primal_feasible) {
                printf("FEAS t=%" PRId64 " entry %" PRId64 " breaks the model: "
                       "col %.3g row %.3g int %.3g\n", t, k,
                       rep.max_col_violation, rep.max_row_violation,
                       rep.max_integrality_violation);
                bad++;
                if (dump != nullptr) show(&g);
            }
        }
        if (!ok) { jaos_model_free(m); continue; }

        const int64_t lim = n < 9 ? n : 9;
        const double s0 = fabs(opt) > 1.0 ? fabs(opt) : 1.0;
        if (fabs(objs[0] - opt) > 1e-6 * s0) {
            printf("BEST t=%" PRId64 " entry 0 carries %.17g and the solve "
                   "published %.17g\n", t, objs[0], opt);
            bad++;
            if (dump != nullptr) show(&g);
        }
        for (int64_t k = 1; k < lim; k++) {
            if (sigma * objs[k] < sigma * objs[k - 1] - 1e-9) {
                printf("ORDER t=%" PRId64 " entry %" PRId64 " is %.17g after "
                       "%.17g, and the sense is %s\n", t, k, objs[k],
                       objs[k - 1], sigma > 0 ? "min" : "max");
                bad++;
                if (dump != nullptr) show(&g);
            }
        }
        for (int64_t k = 0; k < lim; k++)
            for (int64_t p = k + 1; p < lim; p++) {
                /* Exactly the same point is a defect: the caller asked for
                 * two answers and got one twice. The same point to within
                 * the last bits of a continuous column is a separate
                 * question, counted and not judged here, because telling
                 * one vertex reached twice from two vertices of one optimal
                 * face needs a tolerance this repository has not set. */
                bool same = true, near = true;
                for (int64_t j = 0; j < g.nc; j++) {
                    if (xs[k][j] != xs[p][j])
                        same = false;
                    if (fabs(xs[k][j] - xs[p][j]) > 1e-9)
                        near = false;
                }
                if (near && !same)
                    near_dup++;
                if (same) {
                    printf("SAME t=%" PRId64 " entries %" PRId64 " and %" PRId64
                           " are the same point\n", t, k, p);
                    for (int64_t j = 0; j < g.nc; j++)
                        printf("    x[%" PRId64 "] %.17g %s / %.17g %s\n", j,
                               xs[k][j], signbit(xs[k][j]) ? "(neg)" : "",
                               xs[p][j], signbit(xs[p][j]) ? "(neg)" : "");
                    printf("    objectives %.17g and %.17g\n", objs[k], objs[p]);
                    bad++;
                    if (dump != nullptr) show(&g);
                }
            }
        jaos_model_free(m);
    }
    printf("built=%" PRId64 " with_a_pool=%" PRId64 " entries=%" PRId64
           " of_them_more_than_one=%" PRId64 " skipped=%" PRId64
           " bad=%" PRId64 " near_duplicates=%" PRId64 "\n",
           built, pooled, entries, multi, skipped, bad, near_dup);
    return bad != 0 ? 1 : 0;
}
