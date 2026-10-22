/* The reading behind 02-435: 02-434's flip.c with the second model in the
 * same order, some of its columns mirrored and some of its rows negated,
 * as mirrorcone.c does. A mirrored column x is -x: its bounds swap sides
 * and change sign, its cost and coefficients change sign, and its square
 * in the objective stays. A negated row a'x in [l, u] is -a'x in [-u, -l].
 * Each column is mirrored and each row negated with chance 1/2, drawn from
 * a second stream so the models are flip.c's on the same seed; SOS
 * members, semi-continuous columns and the indicator column are not
 * mirrored. The status has to match and the objective with it.
 *
 * Usage: mirror RUNS SEED MODE [dump]
 *   MODE        lp: no discrete shapes; mixed: 02-224's integer marks, SOS
 *               set, semi-continuous column or indicator row; qp and miqp:
 *               the same two with a diagonal quadratic objective over 1 to
 *               3 columns, weights 1 to 3, convex in the model's sense
 *   dump        prints every model that disagrees and writes both to MPS
 * Both solves run under 2e7 work units, and a pair where either stops on
 * it is skipped. The objectives must agree within 1e-6 of max(1, |a|).
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

static uint64_t ms;
static bool coin(void)
{
    ms ^= ms << 13; ms ^= ms >> 7; ms ^= ms << 17;
    return (ms >> 32) & 1;
}

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
    double quad[MAXC];
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
}

static void gen_quad(model *g)
{
    const double s = g->sense == JAOS_MINIMIZE ? 1.0 : -1.0;
    const int64_t n = 1 + pick(3);
    for (int64_t k = 0; k < n; k++)
        g->quad[pick(g->nc)] = s * (double)(1 + pick(3));
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
    for (int64_t j = 0; j < g->nc; j++)
        if (g->quad[j] != 0.0)
            printf("  quad %" PRId64 " %g\n", j, g->quad[j]);
}

static jaos_status build(const model *g, const int64_t *pc, const int64_t *pr,
                         const double *sc, const double *sr, jaos_model **out)
{
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    int64_t ic[MAXC];

    for (int64_t j = 0; j < g->nc; j++) {
        const int64_t t = pc[j];
        cost[t] = sc[j] * g->cost[j];
        cl[t] = sc[j] < 0.0 ? -g->cu[j] : g->cl[j];
        cu[t] = sc[j] < 0.0 ? -g->cl[j] : g->cu[j];
        ic[t] = j;
    }
    for (int64_t i = 0; i < g->nr; i++) {
        const int64_t t = pr[i];
        rl[t] = sr[i] < 0.0 ? -g->ru[i] : g->rl[i];
        ru[t] = sr[i] < 0.0 ? -g->rl[i] : g->ru[i];
    }

    int64_t nz = 0;
    for (int64_t t = 0; t < g->nc; t++) {
        const int64_t j = ic[t];
        ap[t] = nz;
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++) {
            ai[nz] = pr[g->ai[k]]; av[nz] = sc[j] * sr[g->ai[k]] * g->av[k]; nz++;
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
    int64_t qn = 0, qi[MAXC];
    double qv[MAXC];
    for (int64_t j = 0; j < g->nc; j++)
        if (g->quad[j] != 0.0) {
            qi[qn] = pc[j];
            qv[qn++] = g->quad[j];
        }
    if (qn > 0) {
        rc = jaos_set_quadratic(m, qn, qi, qi, qv);
        if (rc != JAOS_OK) goto bad;
    }

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
    const char *mode = argc > 3 ? argv[3] : "mixed";
    const char *dump = argc > 4 ? argv[4] : nullptr;
    const bool lponly = strcmp(mode, "lp") == 0 || strcmp(mode, "qp") == 0;
    const bool quad = strcmp(mode, "qp") == 0 || strcmp(mode, "miqp") == 0;

    int64_t built = 0, ran = 0, skipped = 0, bad = 0;
    for (int64_t t = 0; t < runs; t++) {
        model g;
        gen(&g);
        if (quad)
            gen_quad(&g);
        if (lponly) {
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

        ms = 0x9e3779b97f4a7c15ULL ^ ((uint64_t)t * 0xbf58476d1ce4e5b9ULL) ^
             (strtoull(argv[2], nullptr, 10) << 1);
        double one[MAXC] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, sc[MAXC], sr[MAXR];
        bool keep[MAXC] = {false};
        for (int64_t k = 0; k < g.sos_n; k++)
            keep[g.sos_col[k]] = true;
        if (g.ind_col >= 0)
            keep[g.ind_col] = true;
        for (int64_t j = 0; j < g.nc; j++) {
            one[j] = 1.0;
            sc[j] = !keep[j] && !g.semi[j] && coin() ? -1.0 : 1.0;
        }
        for (int64_t i = 0; i < g.nr; i++)
            sr[i] = coin() ? -1.0 : 1.0;

        if (build(&g, idc, idr, one, one, &a) != JAOS_OK) { skipped++; continue; }
        if (build(&g, idc, idr, sc, sr, &b) != JAOS_OK) { jaos_model_free(a); skipped++; continue; }
        built++;

        const jaos_status ra = jaos_solve(a);
        const jaos_status rb = jaos_solve(b);
        if (ra != JAOS_OK || rb != JAOS_OK) {
            if (ra != rb) {
                printf("RC t=%" PRId64 " a=%s b=%s\n", t,
                       jaos_status_str(ra), jaos_status_str(rb));
                bad++;
                if (dump != nullptr) {
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
        if (sa != sb) {
            printf("STATUS t=%" PRId64 " a=%s b=%s\n", t,
                   jaos_solve_status_str(sa), jaos_solve_status_str(sb));
            bad++;
            if (dump != nullptr) {
                show(&g);
                printf("  err a=[%s] b=[%s]\n", jaos_model_error(a),
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
                jaos_objective(b, &ob) == JAOS_OK) {
                const double s = fabs(oa) > 1.0 ? fabs(oa) : 1.0;
                if (fabs(oa - ob) > 1e-6 * s) {
                    printf("OBJ t=%" PRId64 " a=%.17g b=%.17g\n", t, oa, ob);
                    bad++;
                    if (dump != nullptr) {
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
        }
        jaos_model_free(a); jaos_model_free(b);
    }
    printf("built=%" PRId64 " compared=%" PRId64 " skipped=%" PRId64
           " bad=%" PRId64 "\n", built, ran, skipped, bad);
    return bad != 0 ? 1 : 0;
}
