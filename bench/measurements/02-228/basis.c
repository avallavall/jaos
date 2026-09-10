/* A basis written to the MPS basis format and read back is the same basis,
 * and a solve that starts from it reaches the same answer. Five properties:
 *
 *   1. what `jaos_write_mps_basis` wrote, `jaos_read_mps_basis` gives back,
 *      status for status
 *   2. the basis the solve published holds exactly `num_row` basic
 *      variables, which is what `jaos_set_basis` takes
 *   3. `jaos_set_basis` of the basis read back is accepted
 *   4. a solve from it reaches the objective the first solve reached
 *   5. the same, through the solution file: what `jaos_write_solution`
 *      wrote, `jaos_read_solution` gives back, value for value
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
} model;

static void gen(model *g)
{
    memset(g, 0, sizeof *g);
    g->nc = 2 + pick(MAXC - 1);
    g->nr = 1 + pick(MAXR);
    g->sense = pick(2) ? JAOS_MINIMIZE : JAOS_MAXIMIZE;

    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)(pick(11) - 5);
        /* Every kind of box the basis format has a letter for: boxed, one
         * sided either way, free, and fixed. */
        switch (pick(6)) {
        case 0: g->cl[j] = 0.0;  g->cu[j] = 1.0; break;
        case 1: g->cl[j] = 0.0;  g->cu[j] = INFINITY; break;
        case 2: g->cl[j] = -INFINITY; g->cu[j] = 0.0; break;
        case 3: g->cl[j] = -INFINITY; g->cu[j] = INFINITY; break;
        case 4: g->cl[j] = 2.0; g->cu[j] = 2.0; break;
        default: g->cl[j] = -(double)(1 + pick(3));
                 g->cu[j] = (double)(1 + pick(3)); break;
        }
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

    /* Every row holds the zero point, so the model is feasible. */
    for (int64_t i = 0; i < g->nr; i++) {
        const double b = (double)(1 + pick(12));
        switch (pick(5)) {
        case 0: g->rl[i] = -INFINITY; g->ru[i] = b; break;
        case 1: g->rl[i] = -b; g->ru[i] = INFINITY; break;
        case 2: g->rl[i] = -b; g->ru[i] = b; break;
        case 3: g->rl[i] = 0.0; g->ru[i] = 0.0; break;
        default: g->rl[i] = -b; g->ru[i] = 0.0;
        }
    }
}

static void show(const model *g)
{
    printf("  nc=%" PRId64 " nr=%" PRId64 " sense=%s\n", g->nc, g->nr,
           g->sense == JAOS_MINIMIZE ? "min" : "max");
    for (int64_t j = 0; j < g->nc; j++)
        printf("  col %" PRId64 " cost=%g lo=%g up=%g\n", j, g->cost[j],
               g->cl[j], g->cu[j]);
    for (int64_t i = 0; i < g->nr; i++)
        printf("  row %" PRId64 " lo=%g up=%g\n", i, g->rl[i], g->ru[i]);
    for (int64_t j = 0; j < g->nc; j++)
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++)
            printf("  a[%" PRId64 ",%" PRId64 "]=%g\n", g->ai[k], j, g->av[k]);
}

static jaos_status build(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK)
        return rc;
    rc = jaos_load_lp(m, g->nc, g->nr, g->sense, 0.0, g->cost, g->cl, g->cu,
                      g->rl, g->ru, g->nz, g->ap, g->ai, g->av);
    if (rc == JAOS_OK)
        rc = jaos_set_work_limit(m, 20000000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

static const char *st_str(jaos_basis_status s)
{
    switch (s) {
    case JAOS_BASIS_BASIC:    return "basic";
    case JAOS_BASIS_AT_LOWER: return "lower";
    case JAOS_BASIS_AT_UPPER: return "upper";
    case JAOS_BASIS_FREE:     return "free";
    }
    return "?";
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? strtoll(argv[1], nullptr, 10) : 20000;
    rs = argc > 2 ? strtoull(argv[2], nullptr, 10) : 88172645463325252ULL;
    const char *dump = argc > 3 && argv[3][0] != '\0' ? argv[3] : nullptr;

    int64_t built = 0, checked = 0, skipped = 0, bad = 0, warm = 0;
    int64_t seen[4] = {0, 0, 0, 0}, rseen[4] = {0, 0, 0, 0};
    for (int64_t t = 0; t < runs; t++) {
        model g;
        gen(&g);

        jaos_model *a = nullptr;
        if (build(&g, &a) != JAOS_OK) { skipped++; continue; }
        if (jaos_solve(a) != JAOS_OK) { jaos_model_free(a); skipped++; continue; }
        if (jaos_status_of(a) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(a); skipped++; continue;
        }
        built++;
        double oa = 0.0;
        (void)jaos_objective(a, &oa);

        jaos_basis_status cs[MAXC], rsst[MAXR];
        if (jaos_basis(a, cs, rsst) != JAOS_OK) {
            jaos_model_free(a); skipped++; continue;
        }

        for (int64_t j = 0; j < g.nc; j++) seen[(int)cs[j]]++;
        for (int64_t i = 0; i < g.nr; i++) rseen[(int)rsst[i]]++;

        /* 2. the published basis holds exactly one basic variable per row */
        int64_t nbasic = 0;
        for (int64_t j = 0; j < g.nc; j++) nbasic += cs[j] == JAOS_BASIS_BASIC;
        for (int64_t i = 0; i < g.nr; i++) nbasic += rsst[i] == JAOS_BASIS_BASIC;
        if (nbasic != g.nr) {
            printf("COUNT t=%" PRId64 " the basis holds %" PRId64 " basic of "
                   "%" PRId64 " rows\n", t, nbasic, g.nr);
            bad++;
            if (dump != nullptr) show(&g);
        }

        if (jaos_write_mps_basis(a, "build/bas_tmp.bas") != JAOS_OK) {
            jaos_model_free(a); skipped++; continue;
        }

        jaos_model *b = nullptr;
        if (build(&g, &b) != JAOS_OK) { jaos_model_free(a); skipped++; continue; }
        jaos_basis_status cb[MAXC], rb[MAXR];
        const jaos_status rr = jaos_read_mps_basis(b, "build/bas_tmp.bas",
                                                   cb, rb);
        if (rr != JAOS_OK) {
            printf("READ t=%" PRId64 " rc=%s err=[%s]\n", t,
                   jaos_status_str(rr), jaos_model_error(b));
            bad++;
            if (dump != nullptr) show(&g);
            jaos_model_free(a); jaos_model_free(b);
            continue;
        }
        checked++;

        /* 1. status for status */
        for (int64_t j = 0; j < g.nc; j++)
            if (cs[j] != cb[j]) {
                printf("COL t=%" PRId64 " column %" PRId64 " wrote %s and read "
                       "%s\n", t, j, st_str(cs[j]), st_str(cb[j]));
                bad++;
                if (dump != nullptr) show(&g);
            }
        for (int64_t i = 0; i < g.nr; i++)
            if (rsst[i] != rb[i]) {
                printf("ROW t=%" PRId64 " row %" PRId64 " wrote %s and read "
                       "%s\n", t, i, st_str(rsst[i]), st_str(rb[i]));
                bad++;
                if (dump != nullptr) show(&g);
            }

        /* 3 and 4. it is accepted, and it reaches the same answer */
        const jaos_status sb = jaos_set_basis(b, cb, rb);
        if (sb != JAOS_OK) {
            printf("SET t=%" PRId64 " the basis it read back was refused: %s "
                   "[%s]\n", t, jaos_status_str(sb), jaos_model_error(b));
            bad++;
            if (dump != nullptr) show(&g);
        } else if (jaos_solve(b) == JAOS_OK &&
                   jaos_status_of(b) == JAOS_SOLVE_OPTIMAL) {
            warm++;
            double ob = 0.0;
            (void)jaos_objective(b, &ob);
            const double s = fabs(oa) > 1.0 ? fabs(oa) : 1.0;
            if (fabs(oa - ob) > 1e-6 * s) {
                printf("OBJ t=%" PRId64 " cold %.17g and from its own basis "
                       "%.17g\n", t, oa, ob);
                bad++;
                if (dump != nullptr) show(&g);
            }
        } else if (sb == JAOS_OK) {
            printf("WARM t=%" PRId64 " the solve from its own basis ended %s\n",
                   t, jaos_solve_status_str(jaos_status_of(b)));
            bad++;
            if (dump != nullptr) show(&g);
        }

        /* 5. the solution file round trip */
        if (jaos_write_solution(a, "build/sol_tmp.sol") == JAOS_OK) {
            double xa[MAXC], xr[MAXC], obj = 0.0;
            if (jaos_solution(a, xa, nullptr, nullptr, nullptr) == JAOS_OK) {
                jaos_model *c = nullptr;
                if (build(&g, &c) == JAOS_OK) {
                    const jaos_status cr = jaos_read_solution(
                        c, "build/sol_tmp.sol", &obj, xr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
                    if (cr != JAOS_OK) {
                        printf("SOLREAD t=%" PRId64 " rc=%s err=[%s]\n", t,
                               jaos_status_str(cr), jaos_model_error(c));
                        bad++;
                        if (dump != nullptr) show(&g);
                    } else {
                        for (int64_t j = 0; j < g.nc; j++)
                            if (xa[j] != xr[j]) {
                                printf("SOLCOL t=%" PRId64 " column %" PRId64
                                       " wrote %.17g and read %.17g\n", t, j,
                                       xa[j], xr[j]);
                                bad++;
                                if (dump != nullptr) show(&g);
                            }
                        const double s = fabs(oa) > 1.0 ? fabs(oa) : 1.0;
                        if (fabs(obj - oa) > 1e-9 * s) {
                            printf("SOLOBJ t=%" PRId64 " wrote %.17g and read "
                                   "%.17g\n", t, oa, obj);
                            bad++;
                        }
                    }
                    jaos_model_free(c);
                }
            }
        }
        jaos_model_free(a);
        jaos_model_free(b);
    }
    printf("built=%" PRId64 " round_trips=%" PRId64 " solved_from_it=%" PRId64
           " skipped=%" PRId64 " bad=%" PRId64 "\n",
           built, checked, warm, skipped, bad);
    printf("  columns basic=%" PRId64 " lower=%" PRId64 " upper=%" PRId64
           " free=%" PRId64 "\n", seen[0], seen[1], seen[2], seen[3]);
    printf("  rows    basic=%" PRId64 " lower=%" PRId64 " upper=%" PRId64
           " free=%" PRId64 "\n", rseen[0], rseen[1], rseen[2], rseen[3]);
    return bad != 0 ? 1 : 0;
}
