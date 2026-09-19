/* The reading behind 02-253: generated convex QCQPs and SOCPs, each answer
 * judged by the checker and by a second model that says the same thing
 * with explicit cones.
 *
 * The models: 3 to 12 columns, a planted point, 0 to 2 explicit cones
 * (quadratic or rotated) whose head columns are moved so the point sits
 * inside them, column boxes of four shapes (box, free, open above, open
 * below) around the point, 0 to 5 linear rows of four shapes around the
 * point's activity, and 1 to 3 quadratic rows ½x'Qx + a'x <= u with
 * Q = B'B + D, or the concave form -½x'Qx + a'x >= l, both strictly
 * satisfied at the point. A third of the models carry a convex diagonal
 * objective Q. Every fifth model gets one of two planted infeasibilities:
 * a ball and a half-space past it, or a cone head capped below the norm it
 * must cover.
 *
 * For each feasible model: the solve must end OPTIMAL or UNBOUNDED; an
 * optimum must pass jaos_check_conic_solution at 1e-7 on both sides; a
 * published ray must pass jaos_check_ray; a copy must solve to the same
 * bits; the MPS the model writes must read back and solve to the same
 * bits; and the model with each quadratic row rewritten as a rotated cone
 * over new columns w = F x, t = u - a'x and s = 1 must reach the same
 * objective within 1e-7 relative. For each infeasible model: INFEASIBLE,
 * and a published certificate must pass jaos_check_conic_certificate.
 *
 * Usage: conic RUNS SEED DIR
 * Writes DIR/fNNNNN.mps for every model that fails a check.
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

static double rf(double lo, double hi)
{
    return lo + (hi - lo) * (double)(rnd() >> 11) / 9007199254740992.0;
}

#define MAXC 12
#define MAXQ 3
#define MAXS 5

typedef struct {
    int64_t k;
    int64_t col[MAXS];
    int64_t rb;
    double b[MAXS][MAXS];
    double d[MAXS];
    double a[MAXS];
    int side;
    double bound;
} qrow;

static int64_t fails, optimal, unbounded, infeasible, certified, uncert;
static int64_t cert_kind[3], uncert_kind[3];
static int64_t rays, rayfail, checkfail, detfail, mpsfail, reffail, statfail;
static double worst_dual, worst_primal, worst_ref;
static int64_t total_work, total_iters, longest;
static uint64_t digest = 14695981039346656037u;

static void mix(const void *p, size_t len)
{
    const unsigned char *b = p;
    for (size_t t = 0; t < len; t++) {
        digest ^= b[t];
        digest *= 1099511628211u;
    }
}

static jaos_status add_col(jaos_model *m, double lo, double hi)
{
    const double cost = 0.0;
    const int64_t start[2] = {0, 0};
    return jaos_add_cols(m, 1, &cost, &lo, &hi, 0, start, nullptr, nullptr);
}

static jaos_status add_row(jaos_model *m, double lo, double hi, int64_t nn,
                           const int64_t *idx, const double *val)
{
    const int64_t start[2] = {0, nn};
    return jaos_add_rows(m, 1, &lo, &hi, nn, start, idx, val);
}

static void dump_as(jaos_model *m, const char *dir, int64_t r, char tag)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%c%05" PRId64 ".mps", dir, tag, r);
    if (jaos_write_mps(m, path) != JAOS_OK)
        fprintf(stderr, "m%05" PRId64 ": cannot write %s: %s\n", r, path,
                jaos_model_error(m));
}

static int64_t members(const jaos_model *m)
{
    int64_t t = 0;
    for (int64_t k = 0; k < jaos_num_cones(m); k++) {
        int64_t n = 0;
        if (jaos_cone(m, k, nullptr, &n, nullptr) == JAOS_OK)
            t += n;
    }
    return t;
}

static bool cone_duals(const jaos_model *m, double *z)
{
    int64_t at = 0;
    for (int64_t k = 0; k < jaos_num_cones(m); k++) {
        int64_t n = 0;
        if (jaos_cone(m, k, nullptr, &n, nullptr) != JAOS_OK ||
            jaos_cone_dual(m, k, z + at) != JAOS_OK)
            return false;
        at += n;
    }
    return true;
}

static bool same_bits(const double *a, const double *b, int64_t n)
{
    return n == 0 || memcmp(a, b, (size_t)n * sizeof *a) == 0;
}

static jaos_model *rotated_form(const jaos_model *m, const qrow *qr,
                                int64_t nq, const int64_t *qrow_at)
{
    const double inf = jaos_infinity();
    jaos_model *c = nullptr;
    if (jaos_model_copy(m, &c) != JAOS_OK)
        return nullptr;
    for (int64_t t = 0; t < nq; t++) {
        const qrow *q = &qr[t];
        const int64_t i = qrow_at[t];
        if (jaos_set_row_quadratic(c, i, 0, nullptr, nullptr, nullptr) !=
            JAOS_OK)
            goto fail;
        const double sg = q->side > 0 ? 1.0 : -1.0;
        const double u = sg * q->bound;
        const int64_t fr = q->rb + q->k;
        const int64_t base = jaos_num_col(c);
        for (int64_t k = 0; k < 2 + fr; k++) {
            const double lo = k == 1 ? 1.0 : (k == 0 ? 0.0 : -inf);
            const double hi = k == 1 ? 1.0 : inf;
            if (add_col(c, lo, hi) != JAOS_OK)
                goto fail;
        }
        int64_t idx[MAXS + 1];
        double val[MAXS + 1];
        for (int64_t k = 0; k < q->k; k++) {
            idx[k] = q->col[k];
            val[k] = sg * q->a[k];
        }
        idx[q->k] = base;
        val[q->k] = 1.0;
        if (add_row(c, u, u, q->k + 1, idx, val) != JAOS_OK)
            goto fail;
        for (int64_t r = 0; r < fr; r++) {
            int64_t ix[MAXS + 1];
            double vx[MAXS + 1];
            int64_t nn = 0;
            for (int64_t k = 0; k < q->k; k++) {
                const double f = r < q->rb ? q->b[r][k]
                                           : (r - q->rb == k ? sqrt(q->d[k])
                                                             : 0.0);
                if (f != 0.0) {
                    ix[nn] = q->col[k];
                    vx[nn++] = -f;
                }
            }
            ix[nn] = base + 2 + r;
            vx[nn++] = 1.0;
            if (add_row(c, 0.0, 0.0, nn, ix, vx) != JAOS_OK)
                goto fail;
        }
        const double free_lo = -inf, free_hi = inf;
        if (jaos_set_row_bounds(c, i, free_lo, free_hi) != JAOS_OK)
            goto fail;
        int64_t cols[2 + 2 * MAXS];
        for (int64_t k = 0; k < 2 + fr; k++)
            cols[k] = base + k;
        if (jaos_add_cone(c, JAOS_CONE_ROTATED, 2 + fr, cols) != JAOS_OK)
            goto fail;
    }
    return c;
fail:
    jaos_model_free(c);
    return nullptr;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: conic RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = strtoll(argv[1], nullptr, 10);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)strtoll(argv[2], nullptr, 10);
    const char *dir = argv[3];
    const double inf = jaos_infinity();

    for (int64_t r = 0; r < runs; r++) {
        const int64_t n = ri(3, MAXC);
        const int kind = r % 5 == 4 ? (int)ri(1, 2) : 0;
        double x0[MAXC], lo[MAXC], hi[MAXC], cost[MAXC];
        for (int64_t j = 0; j < n; j++)
            x0[j] = rf(-2.0, 2.0);

        int64_t ncone = ri(0, 2);
        int64_t cone_n[2] = {0, 0}, cone_col[2][MAXC];
        jaos_cone_type cone_ty[2] = {JAOS_CONE_QUADRATIC, JAOS_CONE_QUADRATIC};
        bool used[MAXC] = {false};
        int64_t nc_real = 0;
        for (int64_t k = 0; k < ncone; k++) {
            const bool rot = ri(0, 2) == 0;
            const int64_t want = ri(rot ? 3 : 2, 4);
            int64_t got = 0;
            for (int64_t j = 0; j < n && got < want; j++)
                if (!used[j] && ri(0, 1) == 1) {
                    used[j] = true;
                    cone_col[nc_real][got++] = j;
                }
            if (got < (rot ? 3 : 2)) {
                for (int64_t t = 0; t < got; t++)
                    used[cone_col[nc_real][t]] = false;
                continue;
            }
            cone_n[nc_real] = got;
            cone_ty[nc_real] = rot ? JAOS_CONE_ROTATED : JAOS_CONE_QUADRATIC;
            const int64_t *cc = cone_col[nc_real];
            if (rot) {
                double s2 = 0.0;
                for (int64_t t = 2; t < got; t++)
                    s2 += x0[cc[t]] * x0[cc[t]];
                x0[cc[0]] = rf(0.5, 2.0);
                x0[cc[1]] = s2 / (2.0 * x0[cc[0]]) + rf(0.1, 1.0);
            } else {
                double s2 = 0.0;
                for (int64_t t = 1; t < got; t++)
                    s2 += x0[cc[t]] * x0[cc[t]];
                x0[cc[0]] = sqrt(s2) + rf(0.1, 2.0);
            }
            nc_real++;
        }
        ncone = nc_real;

        for (int64_t j = 0; j < n; j++) {
            const int shape = (int)ri(0, 3);
            lo[j] = shape == 0 || shape == 2 ? x0[j] - rf(0.5, 3.0) : -inf;
            hi[j] = shape == 0 || shape == 3 ? x0[j] + rf(0.5, 3.0) : inf;
            cost[j] = rf(-1.0, 1.0);
        }
        const jaos_obj_sense sense = ri(0, 1) ? JAOS_MAXIMIZE : JAOS_MINIMIZE;
        const int64_t as0[MAXC + 1] = {0};
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK)
            return 1;
        if (jaos_load_lp(m, n, 0, sense, rf(-5.0, 5.0), cost, lo, hi, nullptr,
                         nullptr, 0, as0, nullptr, nullptr) != JAOS_OK) {
            fprintf(stderr, "m%05" PRId64 ": load: %s\n", r,
                    jaos_model_error(m));
            return 1;
        }
        if (ri(0, 2) == 0)
            for (int64_t j = 0; j < n; j++)
                if (jaos_set_col_quadratic(m, j, (sense == JAOS_MAXIMIZE
                                                      ? -1.0 : 1.0) *
                                                     rf(0.0, 2.0)) != JAOS_OK)
                    return 1;
        for (int64_t k = 0; k < ncone; k++)
            if (jaos_add_cone(m, cone_ty[k], cone_n[k], cone_col[k]) !=
                JAOS_OK) {
                fprintf(stderr, "m%05" PRId64 ": cone: %s\n", r,
                        jaos_model_error(m));
                return 1;
            }

        const int64_t nlin = ri(0, 5);
        for (int64_t i = 0; i < nlin; i++) {
            int64_t idx[4];
            double val[4], act = 0.0;
            const int64_t nn = ri(1, n < 4 ? n : 4);
            for (int64_t t = 0; t < nn; t++) {
                int64_t j;
                bool again;
                do {
                    j = ri(0, n - 1);
                    again = false;
                    for (int64_t s = 0; s < t; s++)
                        again |= idx[s] == j;
                } while (again);
                idx[t] = j;
                val[t] = rf(-3.0, 3.0);
                act += val[t] * x0[j];
            }
            const int shape = (int)ri(0, 3);
            double rl = shape == 0 ? -inf
                      : shape == 2 ? act
                                   : act - rf(0.1, 2.0);
            double rh = shape == 1 ? inf
                      : shape == 2 ? act
                                   : act + rf(0.1, 2.0);
            if (shape == 3) {
                rl = ldexp(floor(ldexp(rl, 20)), -20);
                rh = ldexp(ceil(ldexp(rh, 20)), -20);
            }
            if (add_row(m, rl, rh, nn, idx, val) != JAOS_OK)
                return 1;
        }

        qrow qr[MAXQ + 1];
        int64_t qrow_at[MAXQ + 1];
        const int64_t nq = kind == 0 ? ri(1, MAXQ) : 0;
        for (int64_t t = 0; t < nq; t++) {
            qrow *q = &qr[t];
            memset(q, 0, sizeof *q);
            q->k = ri(1, n < MAXS ? n : MAXS);
            for (int64_t k = 0; k < q->k; k++) {
                int64_t j;
                bool again;
                do {
                    j = ri(0, n - 1);
                    again = false;
                    for (int64_t s = 0; s < k; s++)
                        again |= q->col[s] == j;
                } while (again);
                q->col[k] = j;
                q->a[k] = rf(-2.0, 2.0);
                q->d[k] = ri(0, 2) == 0 ? 0.0 : rf(0.1, 1.5);
            }
            q->rb = ri(0, q->k);
            for (int64_t rr = 0; rr < q->rb; rr++)
                for (int64_t k = 0; k < q->k; k++)
                    q->b[rr][k] = rf(-1.0, 1.0);
            q->side = ri(0, 1) ? 1 : -1;
            double quad = 0.0, lin = 0.0;
            double qm[MAXS][MAXS] = {{0.0}};
            for (int64_t a = 0; a < q->k; a++)
                for (int64_t b = 0; b < q->k; b++) {
                    double v = a == b ? q->d[a] : 0.0;
                    for (int64_t rr = 0; rr < q->rb; rr++)
                        v += q->b[rr][a] * q->b[rr][b];
                    qm[a][b] = v;
                }
            for (int64_t a = 0; a < q->k; a++) {
                lin += q->a[a] * x0[q->col[a]];
                for (int64_t b = 0; b < q->k; b++)
                    quad += 0.5 * qm[a][b] * x0[q->col[a]] * x0[q->col[b]];
            }
            int64_t ri_[MAXS * MAXS], rj_[MAXS * MAXS];
            double rv_[MAXS * MAXS];
            int64_t nz = 0;
            for (int64_t a = 0; a < q->k; a++)
                for (int64_t b = 0; b <= a; b++)
                    if (qm[a][b] != 0.0) {
                        ri_[nz] = q->col[a];
                        rj_[nz] = q->col[b];
                        rv_[nz++] = (q->side > 0 ? 1.0 : -1.0) * qm[a][b];
                    }
            double idxa_v[MAXS];
            int64_t idxa[MAXS];
            for (int64_t a = 0; a < q->k; a++) {
                idxa[a] = q->col[a];
                idxa_v[a] = q->a[a];
            }
            if (q->side > 0) {
                q->bound = quad + lin + rf(0.1, 2.0);
                if (add_row(m, -inf, q->bound, q->k, idxa, idxa_v) !=
                    JAOS_OK)
                    return 1;
            } else {
                q->bound = -quad + lin - rf(0.1, 2.0);
                if (add_row(m, q->bound, inf, q->k, idxa, idxa_v) !=
                    JAOS_OK)
                    return 1;
            }
            qrow_at[t] = jaos_num_row(m) - 1;
            if (jaos_set_row_quadratic(m, qrow_at[t], nz, ri_, rj_, rv_) !=
                JAOS_OK) {
                fprintf(stderr, "m%05" PRId64 ": row quadratic: %s\n", r,
                        jaos_model_error(m));
                return 1;
            }
        }
        if (kind == 1) {
            const int64_t k = n < 4 ? n : 4;
            int64_t ix[4], iy[4];
            double one[4], two[4];
            for (int64_t t = 0; t < k; t++) {
                ix[t] = iy[t] = t;
                one[t] = 1.0;
                two[t] = 2.0;
            }
            if (add_row(m, -inf, 1.0, 0, nullptr, nullptr) != JAOS_OK ||
                jaos_set_row_quadratic(m, jaos_num_row(m) - 1, k, ix, iy,
                                       two) != JAOS_OK ||
                add_row(m, 1.5 * sqrt((double)k), inf, k, ix, one) !=
                    JAOS_OK)
                return 1;
        } else if (kind == 2) {
            int64_t cols[2] = {0, 1};
            if (jaos_delete_cones(m, jaos_num_cones(m),
                                  (int64_t[]){0, 1}) != JAOS_OK &&
                jaos_num_cones(m) > 0)
                return 1;
            if (jaos_set_col_bounds(m, 0, -inf, 1.0) != JAOS_OK ||
                jaos_set_col_bounds(m, 1, 2.0, 2.0) != JAOS_OK ||
                jaos_add_cone(m, JAOS_CONE_QUADRATIC, 2, cols) != JAOS_OK)
                return 1;
        }

        jaos_model *copy = nullptr;
        if (jaos_model_copy(m, &copy) != JAOS_OK)
            return 1;
        const jaos_status st = jaos_solve(m);
        total_work += jaos_work_units(m);
        total_iters += jaos_iterations(m);
        if (jaos_iterations(m) > longest)
            longest = jaos_iterations(m);
        const jaos_solve_status ss = jaos_status_of(m);
        mix(&ss, sizeof ss);
        bool bad = false;
        if (st != JAOS_OK) {
            printf("m%05" PRId64 ": solve returned %d: %s\n", r, (int)st,
                   jaos_model_error(m));
            statfail++;
            bad = true;
        } else if (kind != 0) {
            if (ss != JAOS_SOLVE_INFEASIBLE) {
                printf("m%05" PRId64 ": planted infeasible, answered %s: %s\n",
                       r, jaos_solve_status_str(ss), jaos_model_error(m));
                statfail++;
                bad = true;
            } else {
                infeasible++;
                double y[64], z[64];
                if (jaos_certificate(m, y) == JAOS_OK) {
                    jaos_certificate_report cr;
                    bool zok = jaos_num_cones(m) == 0 || cone_duals(m, z);
                    mix(y, (size_t)jaos_num_row(m) * sizeof *y);
                    if (zok && jaos_check_conic_certificate(
                                   m, y, jaos_num_cones(m) > 0 ? z : nullptr,
                                   1e-7, &cr) == JAOS_OK && cr.certified)
                        certified++, cert_kind[kind]++;
                    else {
                        printf("m%05" PRId64 ": a published certificate "
                               "fails\n", r);
                        bad = true;
                    }
                } else {
                    uncert++, uncert_kind[kind]++;
                    if (kind == 2)
                        dump_as(copy, dir, r, 'c');
                }
            }
        } else if (ss == JAOS_SOLVE_OPTIMAL) {
            optimal++;
            const int64_t nr = jaos_num_row(m), nm = members(m);
            double *x = calloc((size_t)n + 1, sizeof *x);
            double *y = calloc((size_t)nr + 1, sizeof *y);
            double *z = calloc((size_t)nm + 1, sizeof *z);
            jaos_check_report rep;
            if (x == nullptr || y == nullptr || z == nullptr ||
                jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK ||
                !cone_duals(m, z) ||
                jaos_check_conic_solution(m, x, y, z, 1e-7, &rep) != JAOS_OK) {
                printf("m%05" PRId64 ": cannot check: %s\n", r,
                       jaos_model_error(m));
                checkfail++;
                bad = true;
            } else {
                mix(x, (size_t)n * sizeof *x);
                mix(y, (size_t)nr * sizeof *y);
                mix(z, (size_t)nm * sizeof *z);
                if (rep.max_dual_violation > worst_dual)
                    worst_dual = rep.max_dual_violation;
                if (rep.max_row_violation_relative > worst_primal)
                    worst_primal = rep.max_row_violation_relative;
                if (!rep.primal_feasible || !rep.dual_feasible) {
                    printf("m%05" PRId64 ": check primal %d dual %d: row "
                           "%.2e col %.2e cone %.2e dual %.2e\n", r,
                           rep.primal_feasible, rep.dual_feasible,
                           rep.max_row_violation_relative,
                           rep.max_col_violation, rep.max_cone_violation,
                           rep.max_dual_violation);
                    checkfail++;
                    bad = true;
                }
            }
            double obj = 0.0;
            if (jaos_objective(m, &obj) != JAOS_OK)
                return 1;

            if (jaos_solve(copy) != JAOS_OK ||
                jaos_status_of(copy) != JAOS_SOLVE_OPTIMAL) {
                printf("m%05" PRId64 ": the copy answered %s\n", r,
                       jaos_solve_status_str(jaos_status_of(copy)));
                detfail++;
                bad = true;
            } else {
                double *x2 = calloc((size_t)n + 1, sizeof *x2);
                if (x2 == nullptr ||
                    jaos_solution(copy, x2, nullptr, nullptr, nullptr) !=
                        JAOS_OK || !same_bits(x, x2, n)) {
                    printf("m%05" PRId64 ": the copy's point differs\n", r);
                    detfail++;
                    bad = true;
                }
                free(x2);
            }

            char path[1024];
            snprintf(path, sizeof path, "%s/rt.mps", dir);
            jaos_model *back = nullptr;
            double obj2 = NAN;
            if (jaos_write_mps(m, path) != JAOS_OK ||
                jaos_model_new(&back) != JAOS_OK ||
                jaos_read_mps(back, path) != JAOS_OK ||
                jaos_solve(back) != JAOS_OK ||
                jaos_status_of(back) != JAOS_SOLVE_OPTIMAL ||
                jaos_objective(back, &obj2) != JAOS_OK || obj2 != obj) {
                printf("m%05" PRId64 ": the MPS round trip gives %.17g, not "
                       "%.17g: %s\n", r, obj2, obj,
                       back != nullptr && jaos_model_error(back)[0] != 0
                           ? jaos_model_error(back) : jaos_model_error(m));
                mpsfail++;
                bad = true;
            }
            jaos_model_free(back);

            jaos_model *rot = rotated_form(copy, qr, nq, qrow_at);
            double obj3 = NAN;
            if (rot == nullptr || jaos_solve(rot) != JAOS_OK ||
                jaos_status_of(rot) != JAOS_SOLVE_OPTIMAL ||
                jaos_objective(rot, &obj3) != JAOS_OK) {
                printf("m%05" PRId64 ": the rotated form answered %s: %s\n",
                       r, rot != nullptr ? jaos_solve_status_str(
                                               jaos_status_of(rot)) : "none",
                       rot != nullptr ? jaos_model_error(rot) : "");
                reffail++;
                bad = true;
            } else {
                const double rel = fabs(obj3 - obj) / (1.0 + fabs(obj));
                if (rel > worst_ref)
                    worst_ref = rel;
                if (rel > 1e-7) {
                    printf("m%05" PRId64 ": the rotated form gives %.17g, "
                           "the quadratic rows %.17g\n", r, obj3, obj);
                    reffail++;
                    bad = true;
                }
            }
            jaos_model_free(rot);
            free(x);
            free(y);
            free(z);
        } else if (ss == JAOS_SOLVE_UNBOUNDED) {
            unbounded++;
            double d[MAXC];
            if (jaos_unbounded_ray(m, d) == JAOS_OK) {
                rays++;
                mix(d, (size_t)n * sizeof *d);
                jaos_ray_report rr;
                if (jaos_check_ray(m, d, 1e-7, &rr) != JAOS_OK ||
                    !rr.certified) {
                    printf("m%05" PRId64 ": a published ray fails\n", r);
                    rayfail++;
                    bad = true;
                }
            } else {
                printf("m%05" PRId64 ": unbounded without a ray\n", r);
                rayfail++;
                bad = true;
            }
        } else {
            printf("m%05" PRId64 ": answered %s: %s\n", r,
                   jaos_solve_status_str(ss), jaos_model_error(m));
            statfail++;
            bad = true;
        }
        if (bad) {
            fails++;
            dump_as(copy, dir, r, 'f');
        }
        jaos_model_free(copy);
        jaos_model_free(m);
    }
    printf("models %" PRId64 "\n", runs);
    printf("optimal %" PRId64 "\n", optimal);
    printf("unbounded %" PRId64 " rays %" PRId64 " failed %" PRId64 "\n",
           unbounded, rays, rayfail);
    printf("infeasible %" PRId64 " certified %" PRId64 " without %" PRId64 "\n",
           infeasible, certified, uncert);
    printf("ball: certified %" PRId64 " without %" PRId64 "; capped cone: "
           "certified %" PRId64 " without %" PRId64 "\n", cert_kind[1],
           uncert_kind[1], cert_kind[2], uncert_kind[2]);
    printf("status failures %" PRId64 "\n", statfail);
    printf("checker failures %" PRId64 "\n", checkfail);
    printf("copy failures %" PRId64 "\n", detfail);
    printf("mps failures %" PRId64 "\n", mpsfail);
    printf("rotated-form failures %" PRId64 "\n", reffail);
    printf("worst dual violation %.3e\n", worst_dual);
    printf("worst row violation %.3e\n", worst_primal);
    printf("worst rotated-form gap %.3e\n", worst_ref);
    printf("work units %" PRId64 "\n", total_work);
    printf("iterations %" PRId64 "\n", total_iters);
    printf("longest %" PRId64 "\n", longest);
    printf("failed models %" PRId64 "\n", fails);
    printf("digest %016" PRIx64 "\n", digest);
    return fails == 0 ? 0 : 1;
}
