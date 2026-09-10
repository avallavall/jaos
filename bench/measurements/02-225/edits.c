/* A model does not change with the route that built it. Load it whole, or
 * grow it with add_cols and add_rows, or hang the structure on a model with
 * a junk column and a junk row and cut those away, or copy it: the status
 * has to match and the objective with it.
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
    bool start_set;
    double start[MAXC];
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

    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)(pick(11) - 5);
        switch (pick(6)) {
        case 0: g->cl[j] = 0.0;  g->cu[j] = 1.0; break;
        case 1: g->cl[j] = 0.0;  g->cu[j] = (double)(1 + pick(6)); break;
        case 2: g->cl[j] = -2.0; g->cu[j] = 3.0; break;
        case 3: g->cl[j] = 0.0;  g->cu[j] = INFINITY; break;
        case 4: g->cl[j] = -INFINITY; g->cu[j] = INFINITY; break;
        default: g->cl[j] = (double)pick(3); g->cu[j] = g->cl[j] + (double)pick(4);
        }
        g->integer[j] = pick(2) != 0;
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

    for (int64_t i = 0; i < g->nr; i++) {
        const double b = (double)(pick(13) - 4);
        switch (pick(4)) {
        case 0: g->rl[i] = -INFINITY; g->ru[i] = b; break;
        case 1: g->rl[i] = b; g->ru[i] = INFINITY; break;
        case 2: g->rl[i] = b; g->ru[i] = b; break;
        default: g->rl[i] = b; g->ru[i] = b + (double)pick(5);
        }
    }

    const int64_t shape = pick(4);
    if (shape == 1 && g->nc >= 2) {
        g->sos_type = 1 + (int)pick(2);
        g->sos_n = 2 + pick(g->nc - 1);
        for (int64_t k = 0; k < g->sos_n; k++) {
            g->sos_col[k] = k;
            g->sos_w[k] = (double)(k + 1);
            if (g->cl[k] == -INFINITY) g->cl[k] = 0.0;
            if (g->cu[k] == INFINITY) g->cu[k] = 4.0;
        }
    } else if (shape == 2) {
        const int64_t j = pick(g->nc);
        g->semi[j] = true;
        g->integer[j] = false;
        if (g->cl[j] <= 0.0) g->cl[j] = 1.0;
        if (g->cu[j] == INFINITY || g->cu[j] < g->cl[j]) g->cu[j] = g->cl[j] + 3.0;
    } else if (shape == 3) {
        const int64_t j = pick(g->nc);
        g->cl[j] = 0.0; g->cu[j] = 1.0; g->integer[j] = true;
        g->ind_col = j;
        g->ind_row = pick(g->nr);
        g->ind_val = (int)pick(2);
    }

    g->start_set = pick(2) != 0;
    if (g->start_set) {
        for (int64_t j = 0; j < g->nc; j++) {
            double v = 0.0;
            if (g->cl[j] > 0.0) v = g->cl[j];
            else if (g->cu[j] < 0.0) v = g->cu[j];
            if (!isfinite(v)) v = 0.0;
            double w = v + (double)pick(3);
            if (w > g->cu[j]) w = v;
            g->start[j] = w;
        }
    }
}

static void show(const model *g)
{
    printf("  nc=%" PRId64 " nr=%" PRId64 " sense=%s start=%d\n", g->nc, g->nr,
           g->sense == JAOS_MINIMIZE ? "min" : "max", (int)g->start_set);
    for (int64_t j = 0; j < g->nc; j++)
        printf("  col %" PRId64 " cost=%g lo=%g up=%g int=%d semi=%d start=%g\n",
               j, g->cost[j], g->cl[j], g->cu[j], (int)g->integer[j],
               (int)g->semi[j], g->start_set ? g->start[j] : 0.0);
    for (int64_t i = 0; i < g->nr; i++)
        printf("  row %" PRId64 " lo=%g up=%g\n", i, g->rl[i], g->ru[i]);
    for (int64_t j = 0; j < g->nc; j++)
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++)
            printf("  a[%" PRId64 ",%" PRId64 "]=%g\n", g->ai[k], j, g->av[k]);
    if (g->sos_n > 0) {
        printf("  sos type=%d n=%" PRId64 " cols=", g->sos_type, g->sos_n);
        for (int64_t k = 0; k < g->sos_n; k++) printf("%" PRId64 " ", g->sos_col[k]);
        printf("\n");
    }
    if (g->ind_row >= 0)
        printf("  ind row=%" PRId64 " col=%" PRId64 " val=%d\n", g->ind_row,
               g->ind_col, g->ind_val);
}

/* Every route hangs the discrete structure on last, with `shift` added to
 * every column index and `rshift` to every row index. */
static jaos_status decorate(jaos_model *m, const model *g, int64_t shift,
                            int64_t rshift)
{
    jaos_status rc;
    for (int64_t j = 0; j < g->nc; j++) {
        if (g->integer[j]) {
            rc = jaos_set_col_integer(m, j + shift, true);
            if (rc != JAOS_OK) return rc;
        }
        if (g->semi[j]) {
            rc = jaos_set_col_semicontinuous(m, j + shift, true);
            if (rc != JAOS_OK) return rc;
        }
    }
    if (g->sos_n > 0) {
        int64_t cols[MAXC];
        for (int64_t k = 0; k < g->sos_n; k++)
            cols[k] = g->sos_col[k] + shift;
        rc = jaos_add_sos(m, g->sos_type, g->sos_n, cols, g->sos_w);
        if (rc != JAOS_OK) return rc;
    }
    if (g->ind_row >= 0) {
        rc = jaos_set_row_indicator(m, g->ind_row + rshift,
                                    g->ind_col + shift, g->ind_val);
        if (rc != JAOS_OK) return rc;
    }
    return JAOS_OK;
}

static jaos_status set_start(jaos_model *m, const model *g, int64_t shift,
                             int64_t total)
{
    if (!g->start_set)
        return JAOS_OK;
    double v[MAXC + 2];
    for (int64_t j = 0; j < total; j++)
        v[j] = 0.0;
    for (int64_t j = 0; j < g->nc; j++)
        v[j + shift] = g->start[j];
    return jaos_set_mip_start(m, v);
}

/* Route A: the whole model in one jaos_load_lp. */
static jaos_status build_whole(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK) return rc;
    rc = jaos_load_lp(m, g->nc, g->nr, g->sense, 0.0, g->cost, g->cl, g->cu,
                      g->rl, g->ru, g->nz, g->ap, g->ai, g->av);
    if (rc == JAOS_OK) rc = decorate(m, g, 0, 0);
    if (rc == JAOS_OK) rc = set_start(m, g, 0, g->nc);
    if (rc == JAOS_OK) rc = jaos_set_work_limit(m, 20000000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

/* Route B: the first column loaded, the rest arriving by jaos_add_cols. */
static jaos_status build_by_adding_cols(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK) return rc;
    rc = jaos_load_lp(m, 1, g->nr, g->sense, 0.0, g->cost, g->cl, g->cu,
                      g->rl, g->ru, g->ap[1], g->ap, g->ai, g->av);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }

    /* The starting point goes on while the model is one column wide. The
     * array it holds has to grow with the model, and a start is a hint, so
     * the answer may not move either way. */
    if (g->start_set) {
        const double head[1] = {g->start[0]};
        rc = jaos_set_mip_start(m, head);
        if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    }

    for (int64_t j = 1; j < g->nc && rc == JAOS_OK; j++) {
        const int64_t b = g->ap[j], e = g->ap[j + 1];
        const int64_t start[2] = {0, e - b};
        rc = jaos_add_cols(m, 1, g->cost + j, g->cl + j, g->cu + j,
                           e - b, start, g->ai + b, g->av + b);
    }
    if (rc == JAOS_OK) rc = decorate(m, g, 0, 0);
    if (rc == JAOS_OK) rc = jaos_set_work_limit(m, 20000000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

/* Route C: the first row loaded, the rest arriving by jaos_add_rows. */
static jaos_status build_by_adding_rows(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK) return rc;

    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    int64_t nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        ap[j] = nz;
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++)
            if (g->ai[k] == 0) { ai[nz] = 0; av[nz] = g->av[k]; nz++; }
    }
    ap[g->nc] = nz;

    rc = jaos_load_lp(m, g->nc, 1, g->sense, 0.0, g->cost, g->cl, g->cu,
                      g->rl, g->ru, nz, ap, ai, av);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }

    for (int64_t i = 1; i < g->nr && rc == JAOS_OK; i++) {
        int64_t ri[MAXC];
        double rv[MAXC];
        int64_t n = 0;
        for (int64_t j = 0; j < g->nc; j++)
            for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++)
                if (g->ai[k] == i) { ri[n] = j; rv[n] = g->av[k]; n++; }
        const int64_t start[2] = {0, n};
        rc = jaos_add_rows(m, 1, g->rl + i, g->ru + i, n, start, ri, rv);
    }
    if (rc == JAOS_OK) rc = decorate(m, g, 0, 0);
    if (rc == JAOS_OK) rc = set_start(m, g, 0, g->nc);
    if (rc == JAOS_OK) rc = jaos_set_work_limit(m, 20000000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

/* Route D: a junk column and a junk row in front, both cut away after the
 * structure went on, so every index the model holds has to be renumbered. */
static jaos_status build_by_deleting(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK) return rc;

    const int64_t nc = g->nc + 1, nr = g->nr + 1;
    double cost[MAXC + 1], cl[MAXC + 1], cu[MAXC + 1];
    double rl[MAXR + 1], ru[MAXR + 1];
    int64_t ap[MAXC + 2], ai[MAXC * MAXR];
    double av[MAXC * MAXR];

    cost[0] = 0.0; cl[0] = 0.0; cu[0] = 0.0;
    for (int64_t j = 0; j < g->nc; j++) {
        cost[j + 1] = g->cost[j]; cl[j + 1] = g->cl[j]; cu[j + 1] = g->cu[j];
    }
    rl[0] = -INFINITY; ru[0] = INFINITY;
    for (int64_t i = 0; i < g->nr; i++) {
        rl[i + 1] = g->rl[i]; ru[i + 1] = g->ru[i];
    }

    int64_t nz = 0;
    ap[0] = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        ap[j + 1] = nz;
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++) {
            ai[nz] = g->ai[k] + 1; av[nz] = g->av[k]; nz++;
        }
    }
    ap[nc] = nz;

    rc = jaos_load_lp(m, nc, nr, g->sense, 0.0, cost, cl, cu, rl, ru,
                      nz, ap, ai, av);
    if (rc == JAOS_OK) rc = decorate(m, g, 1, 1);
    if (rc == JAOS_OK) rc = set_start(m, g, 1, nc);
    if (rc == JAOS_OK) {
        const int64_t dc[1] = {0};
        rc = jaos_delete_cols(m, 1, dc);
    }
    if (rc == JAOS_OK) {
        const int64_t dr[1] = {0};
        rc = jaos_delete_rows(m, 1, dr);
    }
    if (rc == JAOS_OK) rc = jaos_set_work_limit(m, 20000000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

typedef struct {
    const char *name;
    jaos_status (*fn)(const model *, jaos_model **);
} route;

/* The answer agreeing is not enough: the model the route built has to be the
 * model itself, counted the same way. */
static const char *shape_differs(const jaos_model_stats *a,
                                 const jaos_model_stats *b)
{
    if (a->num_col != b->num_col) return "num_col";
    if (a->num_row != b->num_row) return "num_row";
    if (a->num_nz != b->num_nz) return "num_nz";
    if (a->integer_col != b->integer_col) return "integer_col";
    if (a->binary_col != b->binary_col) return "binary_col";
    if (a->semicontinuous_col != b->semicontinuous_col) return "semicontinuous_col";
    if (a->sos_set != b->sos_set) return "sos_set";
    if (a->indicator_row != b->indicator_row) return "indicator_row";
    if (a->quadratic_col != b->quadratic_col) return "quadratic_col";
    if (a->equality_row != b->equality_row) return "equality_row";
    if (a->ranged_row != b->ranged_row) return "ranged_row";
    if (a->one_sided_row != b->one_sided_row) return "one_sided_row";
    if (a->free_row != b->free_row) return "free_row";
    if (a->fixed_col != b->fixed_col) return "fixed_col";
    if (a->ranged_col != b->ranged_col) return "ranged_col";
    if (a->one_sided_col != b->one_sided_col) return "one_sided_col";
    if (a->free_col != b->free_col) return "free_col";
    if (a->empty_row != b->empty_row) return "empty_row";
    if (a->empty_col != b->empty_col) return "empty_col";
    if (a->obj_nz != b->obj_nz) return "obj_nz";
    if (a->min_abs != b->min_abs) return "min_abs";
    if (a->max_abs != b->max_abs) return "max_abs";
    return nullptr;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? strtoll(argv[1], nullptr, 10) : 20000;
    rs = argc > 2 ? strtoull(argv[2], nullptr, 10) : 88172645463325252ULL;
    const char *dump = argc > 3 && argv[3][0] != '\0' ? argv[3] : nullptr;
    const bool lponly = argc > 4;

    const route routes[] = {
        {"add_cols", build_by_adding_cols},
        {"add_rows", build_by_adding_rows},
        {"delete",   build_by_deleting},
        {"copy",     nullptr},
    };
    const int64_t nroutes = (int64_t)(sizeof routes / sizeof routes[0]);

    int64_t built = 0, compared = 0, skipped = 0, bad = 0;
    for (int64_t t = 0; t < runs; t++) {
        model g;
        gen(&g);
        if (lponly) {
            for (int64_t j = 0; j < g.nc; j++) {
                g.integer[j] = false;
                g.semi[j] = false;
            }
            g.sos_n = 0; g.ind_row = -1; g.ind_col = -1;
        }

        jaos_model *a = nullptr;
        if (build_whole(&g, &a) != JAOS_OK) { skipped++; continue; }
        jaos_model_stats st_a;
        if (jaos_model_statistics(a, &st_a) != JAOS_OK) {
            jaos_model_free(a); skipped++; continue;
        }
        if (jaos_solve(a) != JAOS_OK) { jaos_model_free(a); skipped++; continue; }
        const jaos_solve_status sa = jaos_status_of(a);
        if (sa == JAOS_SOLVE_WORK_LIMIT) { jaos_model_free(a); skipped++; continue; }
        double oa = 0.0;
        (void)jaos_objective(a, &oa);
        built++;

        for (int64_t r = 0; r < nroutes; r++) {
            jaos_model *b = nullptr;
            const jaos_status rc = routes[r].fn != nullptr
                                 ? routes[r].fn(&g, &b)
                                 : jaos_model_copy(a, &b);
            if (rc != JAOS_OK) {
                printf("BUILD t=%" PRId64 " route=%s rc=%s\n", t,
                       routes[r].name, jaos_status_str(rc));
                bad++;
                if (dump != nullptr) show(&g);
                continue;
            }
            jaos_model_stats st_b;
            if (jaos_model_statistics(b, &st_b) == JAOS_OK) {
                const char *field = shape_differs(&st_a, &st_b);
                if (field != nullptr) {
                    printf("SHAPE t=%" PRId64 " route=%s field=%s\n", t,
                           routes[r].name, field);
                    bad++;
                    if (dump != nullptr) show(&g);
                }
            }
            if (jaos_solve(b) != JAOS_OK) {
                jaos_model_free(b); skipped++; continue;
            }
            const jaos_solve_status sb = jaos_status_of(b);
            if (sb == JAOS_SOLVE_WORK_LIMIT) {
                jaos_model_free(b); skipped++; continue;
            }
            compared++;
            if (sa != sb) {
                printf("STATUS t=%" PRId64 " route=%s whole=%s built=%s\n", t,
                       routes[r].name, jaos_solve_status_str(sa),
                       jaos_solve_status_str(sb));
                bad++;
                if (dump != nullptr) show(&g);
            } else if (sa == JAOS_SOLVE_OPTIMAL) {
                double ob = 0.0;
                if (jaos_objective(b, &ob) == JAOS_OK) {
                    const double s = fabs(oa) > 1.0 ? fabs(oa) : 1.0;
                    if (fabs(oa - ob) > 1e-6 * s) {
                        printf("OBJ t=%" PRId64 " route=%s whole=%.17g "
                               "built=%.17g\n", t, routes[r].name, oa, ob);
                        bad++;
                        if (dump != nullptr) show(&g);
                    }
                }
            }
            jaos_model_free(b);
        }
        jaos_model_free(a);
    }
    printf("built=%" PRId64 " compared=%" PRId64 " skipped=%" PRId64
           " bad=%" PRId64 "\n", built, compared, skipped, bad);
    return bad != 0 ? 1 : 0;
}
