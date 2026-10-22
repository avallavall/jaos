/* The reading behind 02-431: 02-224's perm.c with cones and quadratic
 * rows. A model with its rows and columns in another order is the same
 * model, so the status has to match and the objective with it.
 *
 * On top of 02-224's generated models (integer marks, an SOS1 or SOS2
 * set, a semi-continuous column or an indicator row): 0 to 2 cones, a
 * quadratic one over 2 to 4 columns or a rotated one over 3 to 4, and 0
 * to 2 rows given a diagonal quadratic part over 1 to 3 columns, convex
 * with a finite upper side or concave with a finite lower side. Every
 * model gets at least one cone or quadratic row, so the conic interior
 * point or the conic tree solves it. A column, a row, a cone member and a
 * quadratic entry all move with the permutation; a cone keeps the order
 * of its members.
 *
 * Usage: permcone RUNS SEED [dump] [continuous]
 *   dump        prints every model that disagrees
 *   continuous  takes the integer marks and the discrete shapes off
 * Both solves run under 2e7 work units, and a pair where either stops on
 * it is skipped. Objectives must agree within 2e-6 of (1 + |a|), twice
 * the trees' default gap.
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
    int64_t ncone, cone_n[2], cone_col[2][4];
    jaos_cone_type cone_type[2];
    int64_t nq, q_row[2], q_n[2], q_col[2][3];
    double q_v[2][3];
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

    g->ncone = pick(3);
    for (int64_t k = 0; k < g->ncone; k++) {
        const bool rot = pick(2) == 1 && g->nc >= 3;
        g->cone_type[k] = rot ? JAOS_CONE_ROTATED : JAOS_CONE_QUADRATIC;
        const int64_t want = rot ? 3 + pick(2) : 2 + pick(3);
        int64_t n = 0;
        for (int tries = 0; n < want && tries < 40; tries++) {
            const int64_t j = pick(g->nc);
            bool seen = false;
            for (int64_t p = 0; p < n; p++)
                seen |= g->cone_col[k][p] == j;
            for (int64_t c = 0; c < k; c++)
                for (int64_t p = 0; p < g->cone_n[c]; p++)
                    seen |= g->cone_col[c][p] == j;
            if (!seen)
                g->cone_col[k][n++] = j;
        }
        if (n < (rot ? 3 : 2)) {
            g->ncone = k;
            break;
        }
        g->cone_n[k] = n;
    }

    g->nq = g->ncone == 0 ? 1 + pick(2) : pick(3);
    if (g->nq > g->nr)
        g->nq = g->nr;
    for (int64_t q = 0; q < g->nq; q++) {
        g->q_row[q] = q;
        const bool up = pick(2) == 1;
        const double b = (double)(pick(13) - 4);
        if (up) {
            if (!isfinite(g->ru[q])) g->ru[q] = b;
            g->rl[q] = -INFINITY;
        } else {
            if (!isfinite(g->rl[q])) g->rl[q] = b;
            g->ru[q] = INFINITY;
        }
        const int64_t want = 1 + pick(3);
        int64_t n = 0;
        for (int tries = 0; n < want && tries < 40; tries++) {
            const int64_t j = pick(g->nc);
            bool seen = false;
            for (int64_t p = 0; p < n; p++)
                seen |= g->q_col[q][p] == j;
            if (!seen) {
                g->q_col[q][n] = j;
                g->q_v[q][n] = (double)(1 + pick(3)) * (up ? 1.0 : -1.0);
                n++;
            }
        }
        g->q_n[q] = n;
    }
    if (g->ncone == 0 && g->nq == 0) {
        g->ncone = 1;
        g->cone_type[0] = JAOS_CONE_QUADRATIC;
        g->cone_n[0] = 2;
        g->cone_col[0][0] = 0;
        g->cone_col[0][1] = 1;
    }
}

static void show(const model *g)
{
    printf("  nc=%" PRId64 " nr=%" PRId64 " sense=%s\n", g->nc, g->nr,
           g->sense == JAOS_MINIMIZE ? "min" : "max");
    for (int64_t j = 0; j < g->nc; j++)
        printf("  col %" PRId64 " cost=%g lo=%g up=%g int=%d semi=%d\n", j,
               g->cost[j], g->cl[j], g->cu[j], (int)g->integer[j],
               (int)g->semi[j]);
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
    for (int64_t k = 0; k < g->ncone; k++) {
        printf("  cone %s:", g->cone_type[k] == JAOS_CONE_ROTATED ? "rotated"
                                                                  : "quadratic");
        for (int64_t p = 0; p < g->cone_n[k]; p++)
            printf(" %" PRId64, g->cone_col[k][p]);
        printf("\n");
    }
    for (int64_t q = 0; q < g->nq; q++) {
        printf("  quadratic row %" PRId64 ":", g->q_row[q]);
        for (int64_t p = 0; p < g->q_n[q]; p++)
            printf(" %g*x%" PRId64 "^2", g->q_v[q][p], g->q_col[q][p]);
        printf("\n");
    }
}

static jaos_status build(const model *g, const int64_t *pc, const int64_t *pr,
                         jaos_model **out)
{
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    int64_t ic[MAXC];

    for (int64_t j = 0; j < g->nc; j++) {
        const int64_t t = pc[j];
        cost[t] = g->cost[j]; cl[t] = g->cl[j]; cu[t] = g->cu[j];
        ic[t] = j;
    }
    for (int64_t i = 0; i < g->nr; i++) {
        const int64_t t = pr[i];
        rl[t] = g->rl[i]; ru[t] = g->ru[i];
    }

    int64_t nz = 0;
    for (int64_t t = 0; t < g->nc; t++) {
        const int64_t j = ic[t];
        ap[t] = nz;
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++) {
            ai[nz] = pr[g->ai[k]]; av[nz] = g->av[k]; nz++;
        }
        for (int64_t a = ap[t] + 1; a < nz; a++) {
            const int64_t ki = ai[a]; const double kv = av[a];
            int64_t b = a - 1;
            while (b >= ap[t] && ai[b] > ki) { ai[b + 1] = ai[b]; av[b + 1] = av[b]; b--; }
            ai[b + 1] = ki; av[b + 1] = kv;
        }
    }
    ap[g->nc] = nz;

    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK)
        return rc;
    rc = jaos_load_lp(m, g->nc, g->nr, g->sense, 0.0, cost, cl, cu, rl, ru,
                      nz, ap, ai, av);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }

    for (int64_t j = 0; j < g->nc; j++) {
        if (g->integer[j]) {
            rc = jaos_set_col_integer(m, pc[j], true);
            if (rc != JAOS_OK) goto bad;
        }
        if (g->semi[j]) {
            rc = jaos_set_col_semicontinuous(m, pc[j], true);
            if (rc != JAOS_OK) goto bad;
        }
    }
    if (g->sos_n > 0) {
        int64_t cols[MAXC];
        for (int64_t k = 0; k < g->sos_n; k++)
            cols[k] = pc[g->sos_col[k]];
        rc = jaos_add_sos(m, g->sos_type, g->sos_n, cols, g->sos_w);
        if (rc != JAOS_OK) goto bad;
    }
    if (g->ind_row >= 0) {
        rc = jaos_set_row_indicator(m, pr[g->ind_row], pc[g->ind_col],
                                    g->ind_val);
        if (rc != JAOS_OK) goto bad;
    }
    for (int64_t k = 0; k < g->ncone; k++) {
        int64_t cols[4];
        for (int64_t p = 0; p < g->cone_n[k]; p++)
            cols[p] = pc[g->cone_col[k][p]];
        rc = jaos_add_cone(m, g->cone_type[k], g->cone_n[k], cols);
        if (rc != JAOS_OK) goto bad;
    }
    int64_t qorder[2] = {0, 1};
    if (g->nq == 2 && pr[g->q_row[1]] < pr[g->q_row[0]]) {
        qorder[0] = 1;
        qorder[1] = 0;
    }
    for (int64_t s = 0; s < g->nq; s++) {
        const int64_t q = qorder[s];
        int64_t cols[3];
        for (int64_t p = 0; p < g->q_n[q]; p++)
            cols[p] = pc[g->q_col[q][p]];
        rc = jaos_set_row_quadratic(m, pr[g->q_row[q]], g->q_n[q], cols, cols,
                                    g->q_v[q]);
        if (rc != JAOS_OK) goto bad;
    }
    rc = jaos_set_work_limit(m, 20000000);
    if (rc != JAOS_OK) goto bad;
    *out = m;
    return JAOS_OK;
bad:
    jaos_model_free(m);
    return rc;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? strtoll(argv[1], nullptr, 10) : 20000;
    rs = argc > 2 ? strtoull(argv[2], nullptr, 10) : 88172645463325252ULL;
    const bool dump = argc > 3 && strcmp(argv[3], "dump") == 0;
    const bool continuous = argc > 4 || (argc > 3 && !dump);

    int64_t built = 0, ran = 0, skipped = 0, bad = 0;
    int64_t verdicts[4] = {0, 0, 0, 0};
    for (int64_t t = 0; t < runs; t++) {
        model g;
        gen(&g);
        if (continuous) {
            for (int64_t j = 0; j < g.nc; j++) {
                g.integer[j] = false;
                g.semi[j] = false;
            }
            g.sos_n = 0;
            g.ind_row = -1;
            g.ind_col = -1;
        }

        int64_t pc[MAXC], pr[MAXR];
        for (int64_t j = 0; j < g.nc; j++) pc[j] = j;
        for (int64_t i = 0; i < g.nr; i++) pr[i] = i;
        for (int64_t j = g.nc - 1; j > 0; j--) {
            const int64_t k = pick(j + 1);
            const int64_t s = pc[j]; pc[j] = pc[k]; pc[k] = s;
        }
        for (int64_t i = g.nr - 1; i > 0; i--) {
            const int64_t k = pick(i + 1);
            const int64_t s = pr[i]; pr[i] = pr[k]; pr[k] = s;
        }

        jaos_model *a = nullptr, *b = nullptr;
        int64_t idr[MAXR];
        for (int64_t i = 0; i < g.nr; i++) idr[i] = i;
        int64_t idc[MAXC];
        for (int64_t j = 0; j < g.nc; j++) idc[j] = j;

        if (build(&g, idc, idr, &a) != JAOS_OK) { skipped++; continue; }
        if (build(&g, pc, pr, &b) != JAOS_OK) { jaos_model_free(a); skipped++; continue; }
        built++;

        const jaos_status ra = jaos_solve(a);
        const jaos_status rb = jaos_solve(b);
        if (ra != JAOS_OK || rb != JAOS_OK) {
            if (ra != rb) {
                printf("RC t=%" PRId64 " a=%s b=%s\n", t,
                       jaos_status_str(ra), jaos_status_str(rb));
                bad++;
                if (dump) {
                    show(&g);
                    printf("  err a=[%s] b=[%s]\n", jaos_model_error(a),
                           jaos_model_error(b));
                }
            } else {
                skipped++;
            }
            jaos_model_free(a); jaos_model_free(b);
            continue;
        }
        const jaos_solve_status sa = jaos_status_of(a);
        const jaos_solve_status sb = jaos_status_of(b);
        if (sa == JAOS_SOLVE_WORK_LIMIT || sb == JAOS_SOLVE_WORK_LIMIT) {
            skipped++;
            jaos_model_free(a); jaos_model_free(b);
            continue;
        }
        ran++;
        verdicts[sa == JAOS_SOLVE_OPTIMAL ? 0 : sa == JAOS_SOLVE_INFEASIBLE ? 1
                 : sa == JAOS_SOLVE_UNBOUNDED ? 2 : 3]++;
        if (sa != sb) {
            printf("STATUS t=%" PRId64 " a=%s b=%s\n", t,
                   jaos_solve_status_str(sa), jaos_solve_status_str(sb));
            bad++;
            if (dump) {
                show(&g);
                printf("  perm cols:");
                for (int64_t j = 0; j < g.nc; j++) printf(" %" PRId64, pc[j]);
                printf("  rows:");
                for (int64_t i = 0; i < g.nr; i++) printf(" %" PRId64, pr[i]);
                printf("\n  err a=[%s] b=[%s]\n", jaos_model_error(a),
                       jaos_model_error(b));
                char pa[64], pb[64];
                snprintf(pa, sizeof pa, "t%" PRId64 "a.mps", t);
                snprintf(pb, sizeof pb, "t%" PRId64 "b.mps", t);
                if (jaos_write_mps(a, pa) != JAOS_OK ||
                    jaos_write_mps(b, pb) != JAOS_OK)
                    return 2;
            }
        } else if (sa == JAOS_SOLVE_OPTIMAL) {
            double oa = 0.0, ob = 0.0;
            if (jaos_objective(a, &oa) == JAOS_OK &&
                jaos_objective(b, &ob) == JAOS_OK &&
                fabs(oa - ob) > 2e-6 * (1.0 + fabs(oa))) {
                printf("OBJ t=%" PRId64 " a=%.17g b=%.17g\n", t, oa, ob);
                bad++;
                if (dump) {
                    show(&g);
                    char pa[64], pb[64];
                    snprintf(pa, sizeof pa, "t%" PRId64 "a.mps", t);
                    snprintf(pb, sizeof pb, "t%" PRId64 "b.mps", t);
                    if (jaos_write_mps(a, pa) != JAOS_OK ||
                        jaos_write_mps(b, pb) != JAOS_OK)
                        return 2;
                }
            }
        }
        jaos_model_free(a); jaos_model_free(b);
    }
    printf("built=%" PRId64 " compared=%" PRId64 " skipped=%" PRId64
           " bad=%" PRId64 " optimal=%" PRId64 " infeasible=%" PRId64
           " unbounded=%" PRId64 " other=%" PRId64 "\n", built, ran, skipped,
           bad, verdicts[0], verdicts[1], verdicts[2], verdicts[3]);
    return bad != 0 ? 1 : 0;
}
