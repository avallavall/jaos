/* The reading behind 02-235: the certificates over generated models.
 *
 * The generator is 02-234's with the rows pulled off the planted point
 * on six models in ten and some boxes opened, so the set holds many
 * infeasible and unbounded models beside the feasible ones. A third of
 * the models carry no integer mark.
 *
 * The harness judges a certificate on its own, from the model's arrays,
 * with the same reading jaos_check_certificate and jaos_check_ray use,
 * written again here in plain arithmetic: an infeasibility certificate y
 * is a row combination whose least value over the row bounds exceeds its
 * greatest value over the column boxes, and an unboundedness ray r moves
 * every row and every column the way its bounds allow while the cost
 * falls (or rises, maximised).
 *
 * Eight properties:
 *   P1 an infeasible LP publishes a row certificate, and it certifies
 *   P2 jaos_check_certificate agrees, and its two sums are the harness's
 *      to 1e-9
 *   P3 an unbounded LP publishes a column ray, and it certifies
 *   P4 jaos_check_ray agrees
 *   P5 the certificate's sign flipped, or its entries zeroed, does not
 *      certify: the harness's reading is not vacuous
 *   P6 the exact certificate derives whenever a basis is there to derive
 *      it from, and it is refused, not wrong, when presolve settled the
 *      answer
 *   P7 a second solve publishes the same certificate bit for bit
 *   P8 a MIP publishes a row certificate only when its relaxation is
 *      infeasible, and then it certifies the relaxation
 *
 * Usage: certs RUNS SEED [DUMP_INDEX DUMP_PATH]
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng_state;

static uint64_t rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static int64_t ri(int64_t lo, int64_t hi)
{
    return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1));
}

#define MAXC 24
#define MAXR 16

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool isint[MAXC];
    bool maximise, lp_only;
    double offset;
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->maximise = ri(0, 1) == 1;
    g->lp_only = ri(0, 2) == 0;
    g->offset = (double)ri(-3, 3);
    const bool broken = ri(0, 9) < 6;
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = !g->lp_only && ri(0, 9) < 6;
        g->cost[j] = (double)ri(-9, 9);
        g->cl[j] = 0.0;
        g->cu[j] = broken && ri(0, 3) == 0 ? INFINITY : (double)ri(1, 10);
        z[j] = ri(0, isfinite(g->cu[j]) ? (int64_t)g->cu[j] : 10);
        g->ap[j] = g->nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (ri(0, 9) >= 5)
                continue;
            int64_t a = ri(-3, 3);
            if (a == 0)
                a = 1;
            g->ai[g->nz] = i;
            g->av[g->nz] = (double)a;
            g->nz++;
            act[i] += (double)a * (double)z[j];
        }
    }
    g->ap[g->nc] = g->nz;
    for (int64_t i = 0; i < g->nr; i++) {
        const int64_t kind = ri(0, 9);
        const double shift = broken ? (double)ri(-40, 40) : 0.0;
        if (kind < 3) {
            g->rl[i] = g->ru[i] = act[i] + shift;
        } else if (kind < 6) {
            g->rl[i] = -INFINITY;
            g->ru[i] = act[i] + (double)ri(0, 3) + shift;
        } else if (kind < 9) {
            g->rl[i] = act[i] - (double)ri(0, 3) + shift;
            g->ru[i] = INFINITY;
        } else {
            g->rl[i] = act[i] - (double)ri(0, 2) + shift;
            g->ru[i] = act[i] + (double)ri(0, 2) + shift;
        }
    }
}

static jaos_model *load(const gen *g, bool with_marks)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    if (jaos_load_lp(m, g->nc, g->nr,
                     g->maximise ? JAOS_MAXIMIZE : JAOS_MINIMIZE, g->offset,
                     g->cost, g->cl, g->cu, g->rl, g->ru, g->nz, g->ap,
                     g->ai, g->av) != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    if (with_marks)
        for (int64_t j = 0; j < g->nc; j++)
            if (g->isint[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) {
                jaos_model_free(m);
                return nullptr;
            }
    return m;
}

/* The harness's own reading of a row certificate: the greatest value the
 * combination takes over the column boxes, the least it takes over the
 * row bounds, and whether the second exceeds the first. */
static bool farkas_certifies(const gen *g, const double *y, double *sup,
                             double *inf)
{
    double s = 0.0, t = 0.0;
    for (int64_t j = 0; j < g->nc; j++) {
        double a = 0.0, traffic = 0.0;
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++) {
            a += g->av[p] * y[g->ai[p]];
            traffic += fabs(g->av[p] * y[g->ai[p]]);
        }
        if (fabs(a) <= 1e-9 * traffic)
            continue;
        if (a > 0.0) {
            if (!isfinite(g->cu[j])) return false;
            s += a * g->cu[j];
        } else {
            if (!isfinite(g->cl[j])) return false;
            s += a * g->cl[j];
        }
    }
    for (int64_t i = 0; i < g->nr; i++) {
        if (y[i] > 0.0) {
            if (!isfinite(g->rl[i])) return false;
            t += y[i] * g->rl[i];
        } else if (y[i] < 0.0) {
            if (!isfinite(g->ru[i])) return false;
            t += y[i] * g->ru[i];
        }
    }
    *sup = s;
    *inf = t;
    return t - s > 1e-9 * (1.0 + fabs(s) + fabs(t));
}

/* The harness's own reading of a column ray. */
static bool ray_certifies(const gen *g, const double *r)
{
    double scale = 0.0;
    for (int64_t j = 0; j < g->nc; j++) {
        if (!isfinite(r[j])) return false;
        if (fabs(r[j]) > scale) scale = fabs(r[j]);
    }
    if (scale == 0.0) return false;
    const double tol = 1e-9 * scale;
    for (int64_t j = 0; j < g->nc; j++) {
        if (isfinite(g->cl[j]) && r[j] < -tol) return false;
        if (isfinite(g->cu[j]) && r[j] > tol) return false;
    }
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    for (int64_t j = 0; j < g->nc; j++)
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++)
            act[g->ai[p]] += g->av[p] * r[j];
    for (int64_t i = 0; i < g->nr; i++) {
        if (isfinite(g->rl[i]) && act[i] < -3.0 * tol * MAXC) return false;
        if (isfinite(g->ru[i]) && act[i] > 3.0 * tol * MAXC) return false;
    }
    double rate = 0.0;
    for (int64_t j = 0; j < g->nc; j++) rate += g->cost[j] * r[j];
    return g->maximise ? rate > 9.0 * tol : rate < -9.0 * tol;
}

static int64_t broke[10];

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 1000;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    const int64_t dump = argc > 4 ? atoll(argv[3]) : -1;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, infeasible = 0, unbounded = 0, other = 0;
    int64_t exact_derived = 0, exact_refused = 0;
    int64_t mip_with_cert = 0, mip_without = 0;
    static double y[MAXR], y2[MAXR], r[MAXC], r2[MAXC], flipped[MAXR];
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        if (idx == dump) {
            jaos_model *d = load(&g, true);
            if (d == nullptr || jaos_write_mps(d, argv[4]) != JAOS_OK)
                abort();
            jaos_model_free(d);
        }

        jaos_model *m = load(&g, false);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        const jaos_solve_status st = jaos_status_of(m);
        if (st == JAOS_SOLVE_OPTIMAL) optimal++;
        else if (st == JAOS_SOLVE_INFEASIBLE) infeasible++;
        else if (st == JAOS_SOLVE_UNBOUNDED) unbounded++;
        else other++;

        if (st == JAOS_SOLVE_INFEASIBLE) {
            if (jaos_certificate(m, y) != JAOS_OK) {
                fail(1, idx, "no-certificate", 0.0, 0.0);
            } else {
                double sup, inf;
                if (!farkas_certifies(&g, y, &sup, &inf))
                    fail(1, idx, "does-not-certify", sup, inf);
                jaos_certificate_report cr;
                if (jaos_check_certificate(m, y, 1e-9, &cr) != JAOS_OK)
                    abort();
                if (!cr.certified)
                    fail(2, idx, "checker-refuses", cr.sup_columns, cr.inf_rows);
                else if (fabs(cr.sup_columns - sup) >
                             1e-9 * (1.0 + fabs(sup)) ||
                         fabs(cr.inf_rows - inf) > 1e-9 * (1.0 + fabs(inf)))
                    fail(2, idx, "checker-sums-differ", cr.sup_columns, sup);
                for (int64_t i = 0; i < g.nr; i++) flipped[i] = -y[i];
                double s2, i2;
                if (farkas_certifies(&g, flipped, &s2, &i2))
                    fail(5, idx, "flipped-certifies", s2, i2);
                memset(flipped, 0, sizeof flipped);
                if (farkas_certifies(&g, flipped, &s2, &i2))
                    fail(5, idx, "zero-certifies", s2, i2);
                jaos_exact_ray_report er;
                const jaos_status xs = jaos_exact_certificate(m, &er);
                if (xs == JAOS_OK) {
                    if (!er.derived)
                        fail(6, idx, "exact-ok-not-derived", 0.0, 0.0);
                    else
                        exact_derived++;
                } else {
                    exact_refused++;
                }
            }
        }
        if (st == JAOS_SOLVE_UNBOUNDED) {
            if (jaos_unbounded_ray(m, r) != JAOS_OK) {
                fail(3, idx, "no-ray", 0.0, 0.0);
            } else {
                if (!ray_certifies(&g, r))
                    fail(3, idx, "ray-does-not-certify", r[0], 0.0);
                jaos_ray_report rr;
                if (jaos_check_ray(m, r, 1e-9, &rr) != JAOS_OK)
                    abort();
                if (!rr.certified)
                    fail(4, idx, "ray-checker-refuses", rr.max_col_escape,
                         rr.max_row_escape);
                for (int64_t j = 0; j < g.nc; j++) r2[j] = -r[j];
                if (ray_certifies(&g, r2))
                    fail(5, idx, "flipped-ray-certifies", 0.0, 0.0);
            }
        }

        if (st == JAOS_SOLVE_INFEASIBLE || st == JAOS_SOLVE_UNBOUNDED) {
            jaos_model *again = load(&g, false);
            if (again == nullptr) abort();
            if (jaos_solve(again) != JAOS_OK) abort();
            if (jaos_status_of(again) != st) {
                fail(7, idx, "second-status", (double)jaos_status_of(again),
                     (double)st);
            } else if (st == JAOS_SOLVE_INFEASIBLE) {
                if (jaos_certificate(again, y2) != JAOS_OK ||
                    memcmp(y, y2, (size_t)g.nr * sizeof *y) != 0)
                    fail(7, idx, "second-certificate", y[0], y2[0]);
            } else {
                if (jaos_unbounded_ray(again, r2) != JAOS_OK ||
                    memcmp(r, r2, (size_t)g.nc * sizeof *r) != 0)
                    fail(7, idx, "second-ray", r[0], r2[0]);
            }
            jaos_model_free(again);
        }
        jaos_model_free(m);

        if (!g.lp_only) {
            jaos_model *mip = load(&g, true);
            if (mip == nullptr) abort();
            if (jaos_set_work_limit(mip, 20000000) != JAOS_OK) abort();
            if (jaos_solve(mip) != JAOS_OK) abort();
            if (jaos_status_of(mip) == JAOS_SOLVE_INFEASIBLE) {
                const bool have = jaos_certificate(mip, y) == JAOS_OK;
                if (have) {
                    mip_with_cert++;
                    double sup, inf;
                    if (st != JAOS_SOLVE_INFEASIBLE)
                        fail(8, idx, "mip-certificate-on-feasible-relaxation",
                             (double)st, 0.0);
                    else if (!farkas_certifies(&g, y, &sup, &inf))
                        fail(8, idx, "mip-certificate-does-not-certify", sup,
                             inf);
                } else {
                    mip_without++;
                    if (st == JAOS_SOLVE_INFEASIBLE)
                        fail(8, idx, "mip-no-certificate-on-infeasible-relaxation",
                             0.0, 0.0);
                }
            }
            jaos_model_free(mip);
        }
    }

    int64_t total = 0;
    for (int p = 1; p <= 8; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " infeasible %" PRId64
           " unbounded %" PRId64 " other %" PRId64 " exact-derived %" PRId64
           " exact-refused %" PRId64 " mip-with-certificate %" PRId64
           " mip-without %" PRId64 "\n", runs, optimal, infeasible,
           unbounded, other, exact_derived, exact_refused, mip_with_cert,
           mip_without);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " P7=%" PRId64 " P8=%" PRId64
           " total=%" PRId64 "\n", broke[1], broke[2], broke[3], broke[4],
           broke[5], broke[6], broke[7], broke[8], total);
    return total == 0 ? 0 : 1;
}
