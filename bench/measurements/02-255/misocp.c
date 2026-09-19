/* The reading behind 02-255: generated mixed-integer conic models, each
 * answer of the conic branch and bound judged against brute force.
 *
 * The models: 3 to 8 columns and a planted point whose 1 to 3 integer
 * columns are integral, each integer column boxed to the planted value
 * plus or minus 1 or 2; the continuous columns take boxes of four shapes
 * (box, free, open above, open below). 0 to 2 cones, quadratic or
 * rotated, whose head columns are continuous and moved so the point sits
 * inside; 0 to 4 linear rows and 0 to 2 convex quadratic rows, all
 * strictly satisfied at the point; a third of the models carry a convex
 * diagonal objective Q. Every fifth model gets a row that holds one
 * integer column strictly between two integers, so no integer point is
 * feasible though the relaxation is.
 *
 * Brute force fixes the integer columns at every value of their boxes
 * and solves each continuous model. The tree must end INFEASIBLE when no
 * value is feasible, UNBOUNDED when a feasible value is unbounded, and
 * otherwise OPTIMAL at the best value within 1e-6 relative, with a point
 * the checker takes and integer columns exactly integral.
 *
 * Usage: misocp RUNS SEED DIR
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

#define MAXC 8
#define MAXS 4

static uint64_t digest = 14695981039346656037u;

static void mix(const void *p, size_t len)
{
    const unsigned char *b = p;
    for (size_t t = 0; t < len; t++) {
        digest ^= b[t];
        digest *= 1099511628211u;
    }
}

static jaos_status add_row(jaos_model *m, double lo, double hi, int64_t nn,
                           const int64_t *idx, const double *val)
{
    const int64_t start[2] = {0, nn};
    return jaos_add_rows(m, 1, &lo, &hi, nn, start, idx, val);
}

static void dump_as(jaos_model *m, const char *dir, int64_t r)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/f%05" PRId64 ".mps", dir, r);
    if (jaos_write_mps(m, path) != JAOS_OK)
        fprintf(stderr, "m%05" PRId64 ": cannot write %s: %s\n", r, path,
                jaos_model_error(m));
}

typedef struct {
    int verdict;
    double best;
    int64_t solves, unsure;
} brute;

static void brute_force(const jaos_model *m, const int64_t *icol, int64_t ni,
                        const double *ilo, const double *ihi, brute *b)
{
    b->verdict = JAOS_SOLVE_INFEASIBLE;
    b->best = 0.0;
    b->solves = b->unsure = 0;
    jaos_obj_sense sense = JAOS_MINIMIZE;
    if (jaos_objective_sense(m, &sense) != JAOS_OK)
        exit(1);
    const double sigma = sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    double v[MAXC];
    for (int64_t t = 0; t < ni; t++)
        v[t] = ilo[t];
    for (;;) {
        jaos_model *c = nullptr;
        if (jaos_model_copy(m, &c) != JAOS_OK)
            exit(1);
        for (int64_t t = 0; t < ni; t++)
            if (jaos_set_col_integer(c, icol[t], false) != JAOS_OK ||
                jaos_set_col_bounds(c, icol[t], v[t], v[t]) != JAOS_OK)
                exit(1);
        b->solves++;
        if (jaos_solve(c) != JAOS_OK) {
            b->unsure++;
        } else {
            const jaos_solve_status s = jaos_status_of(c);
            double obj = 0.0;
            if (s == JAOS_SOLVE_UNBOUNDED) {
                b->verdict = JAOS_SOLVE_UNBOUNDED;
            } else if (s == JAOS_SOLVE_OPTIMAL &&
                       jaos_objective(c, &obj) == JAOS_OK) {
                if (b->verdict == JAOS_SOLVE_INFEASIBLE ||
                    (b->verdict == JAOS_SOLVE_OPTIMAL &&
                     sigma * obj < sigma * b->best)) {
                    b->verdict = JAOS_SOLVE_OPTIMAL;
                    b->best = obj;
                }
            } else if (s != JAOS_SOLVE_INFEASIBLE) {
                b->unsure++;
            }
        }
        jaos_model_free(c);
        int64_t t = 0;
        while (t < ni && v[t] >= ihi[t]) {
            v[t] = ilo[t];
            t++;
        }
        if (t == ni)
            break;
        v[t] += 1.0;
    }
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: misocp RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = strtoll(argv[1], nullptr, 10);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)strtoll(argv[2], nullptr, 10);
    const char *dir = argv[3];
    const double inf = jaos_infinity();
    int64_t fails = 0, optimal = 0, infeasible = 0, unbounded = 0;
    int64_t unsure = 0, tree_errors = 0, total_nodes = 0, total_work = 0;
    int64_t brute_solves = 0;
    double worst_gap = 0.0, worst_viol = 0.0;

    for (int64_t r = 0; r < runs; r++) {
        const int64_t n = ri(3, MAXC);
        const bool trap = r % 5 == 4;
        double x0[MAXC], lo[MAXC], hi[MAXC], cost[MAXC];
        bool isint[MAXC] = {false};
        for (int64_t j = 0; j < n; j++)
            x0[j] = rf(-2.0, 2.0);

        int64_t ncone = ri(0, 2), nc_real = 0;
        int64_t cone_n[2] = {0, 0}, cone_col[2][MAXC];
        jaos_cone_type cone_ty[2] = {JAOS_CONE_QUADRATIC, JAOS_CONE_QUADRATIC};
        bool used[MAXC] = {false}, head[MAXC] = {false};
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
            head[cone_col[nc_real][0]] = true;
            if (rot)
                head[cone_col[nc_real][1]] = true;
            nc_real++;
        }
        ncone = nc_real;

        int64_t icol[MAXC], ni = 0;
        const int64_t want_int = ri(1, 3);
        for (int64_t j = 0; j < n && ni < want_int; j++)
            if (!head[j] && ri(0, 1) == 1) {
                isint[j] = true;
                icol[ni++] = j;
                x0[j] = round(x0[j]);
            }
        if (ni == 0) {
            for (int64_t j = 0; j < n && ni == 0; j++)
                if (!head[j]) {
                    isint[j] = true;
                    icol[ni++] = j;
                    x0[j] = round(x0[j]);
                }
        }
        for (int64_t k = 0; k < ncone; k++) {
            const int64_t *cc = cone_col[k];
            double s2 = 0.0;
            if (cone_ty[k] == JAOS_CONE_ROTATED) {
                for (int64_t t = 2; t < cone_n[k]; t++)
                    s2 += x0[cc[t]] * x0[cc[t]];
                x0[cc[0]] = rf(0.5, 2.0);
                x0[cc[1]] = s2 / (2.0 * x0[cc[0]]) + rf(0.1, 1.0);
            } else {
                for (int64_t t = 1; t < cone_n[k]; t++)
                    s2 += x0[cc[t]] * x0[cc[t]];
                x0[cc[0]] = sqrt(s2) + rf(0.1, 2.0);
            }
        }

        double ilo[MAXC], ihi[MAXC];
        for (int64_t j = 0; j < n; j++) {
            if (isint[j]) {
                const double w = (double)ri(1, 2);
                lo[j] = x0[j] - w;
                hi[j] = x0[j] + w;
            } else {
                const int shape = (int)ri(0, 3);
                lo[j] = shape == 0 || shape == 2 ? x0[j] - rf(0.5, 3.0) : -inf;
                hi[j] = shape == 0 || shape == 3 ? x0[j] + rf(0.5, 3.0) : inf;
            }
            cost[j] = rf(-1.0, 1.0);
        }
        for (int64_t t = 0; t < ni; t++) {
            ilo[t] = lo[icol[t]];
            ihi[t] = hi[icol[t]];
        }
        const jaos_obj_sense sense = ri(0, 1) ? JAOS_MAXIMIZE : JAOS_MINIMIZE;
        const int64_t as0[MAXC + 1] = {0};
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK ||
            jaos_load_lp(m, n, 0, sense, rf(-5.0, 5.0), cost, lo, hi, nullptr,
                         nullptr, 0, as0, nullptr, nullptr) != JAOS_OK)
            return 1;
        for (int64_t t = 0; t < ni; t++)
            if (jaos_set_col_integer(m, icol[t], true) != JAOS_OK)
                return 1;
        if (ri(0, 2) == 0)
            for (int64_t j = 0; j < n; j++)
                if (jaos_set_col_quadratic(m, j, (sense == JAOS_MAXIMIZE
                                                      ? -1.0 : 1.0) *
                                                     rf(0.0, 2.0)) != JAOS_OK)
                    return 1;
        for (int64_t k = 0; k < ncone; k++)
            if (jaos_add_cone(m, cone_ty[k], cone_n[k], cone_col[k]) != JAOS_OK)
                return 1;

        const int64_t nlin = ri(0, 4);
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
            const int shape = (int)ri(0, 2);
            const double rl = shape == 0
                ? -inf : ldexp(floor(ldexp(act - rf(0.1, 2.0), 20)), -20);
            const double rh = shape == 1
                ? inf : ldexp(ceil(ldexp(act + rf(0.1, 2.0), 20)), -20);
            if (add_row(m, rl, rh, nn, idx, val) != JAOS_OK)
                return 1;
        }

        const int64_t nq = ri(0, 2);
        for (int64_t t = 0; t < nq; t++) {
            int64_t col[MAXS];
            double a[MAXS], d[MAXS], bmat[MAXS][MAXS];
            const int64_t k = ri(1, n < MAXS ? n : MAXS);
            for (int64_t s = 0; s < k; s++) {
                int64_t j;
                bool again;
                do {
                    j = ri(0, n - 1);
                    again = false;
                    for (int64_t u = 0; u < s; u++)
                        again |= col[u] == j;
                } while (again);
                col[s] = j;
                a[s] = rf(-2.0, 2.0);
                d[s] = ri(0, 2) == 0 ? 0.0 : rf(0.1, 1.5);
            }
            const int64_t rb = ri(0, k);
            for (int64_t u = 0; u < rb; u++)
                for (int64_t s = 0; s < k; s++)
                    bmat[u][s] = rf(-1.0, 1.0);
            double qm[MAXS][MAXS], quad = 0.0, lin = 0.0;
            for (int64_t p = 0; p < k; p++)
                for (int64_t q = 0; q < k; q++) {
                    double v = p == q ? d[p] : 0.0;
                    for (int64_t u = 0; u < rb; u++)
                        v += bmat[u][p] * bmat[u][q];
                    qm[p][q] = v;
                }
            for (int64_t p = 0; p < k; p++) {
                lin += a[p] * x0[col[p]];
                for (int64_t q = 0; q < k; q++)
                    quad += 0.5 * qm[p][q] * x0[col[p]] * x0[col[q]];
            }
            int64_t qi[MAXS * MAXS], qj[MAXS * MAXS];
            double qv[MAXS * MAXS];
            int64_t nz = 0;
            for (int64_t p = 0; p < k; p++)
                for (int64_t q = 0; q <= p; q++)
                    if (qm[p][q] != 0.0) {
                        qi[nz] = col[p];
                        qj[nz] = col[q];
                        qv[nz++] = qm[p][q];
                    }
            if (add_row(m, -inf, quad + lin + rf(0.1, 2.0), k, col, a) !=
                    JAOS_OK ||
                jaos_set_row_quadratic(m, jaos_num_row(m) - 1, nz, qi, qj,
                                       qv) != JAOS_OK)
                return 1;
        }
        if (trap) {
            const int64_t j = icol[0];
            const double one = 1.0;
            if (add_row(m, x0[j] + 0.25, x0[j] + 0.75, 1, &j, &one) !=
                JAOS_OK)
                return 1;
        }

        brute b;
        brute_force(m, icol, ni, ilo, ihi, &b);
        brute_solves += b.solves;

        jaos_model *copy = nullptr;
        if (jaos_model_copy(m, &copy) != JAOS_OK)
            return 1;
        const jaos_status st = jaos_solve(m);
        const jaos_solve_status ss = jaos_status_of(m);
        mix(&ss, sizeof ss);
        total_work += jaos_work_units(m);
        jaos_mip_report rep;
        if (jaos_mip_result(m, &rep) == JAOS_OK)
            total_nodes += rep.nodes;
        bool bad = false;
        if (b.unsure > 0) {
            unsure++;
        } else if (st != JAOS_OK) {
            printf("m%05" PRId64 ": solve returned %d: %s\n", r, (int)st,
                   jaos_model_error(m));
            bad = true;
        } else if (ss == JAOS_SOLVE_NUMERICAL_ERROR) {
            printf("m%05" PRId64 ": numerical error: %s\n", r,
                   jaos_model_error(m));
            tree_errors++;
            bad = true;
        } else if ((int)ss != b.verdict) {
            printf("m%05" PRId64 ": the tree answered %s, brute force %s\n", r,
                   jaos_solve_status_str(ss),
                   jaos_solve_status_str((jaos_solve_status)b.verdict));
            bad = true;
        } else if (ss == JAOS_SOLVE_INFEASIBLE) {
            infeasible++;
        } else if (ss == JAOS_SOLVE_UNBOUNDED) {
            unbounded++;
        } else {
            optimal++;
            double obj = 0.0, x[MAXC], y[16], z[MAXC];
            int64_t at = 0;
            bool zok = true;
            for (int64_t k = 0; k < jaos_num_cones(m) && zok; k++) {
                int64_t cn = 0;
                zok = jaos_cone(m, k, nullptr, &cn, nullptr) == JAOS_OK &&
                      jaos_cone_dual(m, k, z + at) == JAOS_OK;
                at += cn;
            }
            jaos_check_report ck;
            if (!zok || jaos_objective(m, &obj) != JAOS_OK ||
                jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK ||
                jaos_check_conic_solution(m, x, y, at > 0 ? z : nullptr, 1e-6,
                                          &ck) != JAOS_OK) {
                printf("m%05" PRId64 ": cannot read the answer: %s\n", r,
                       jaos_model_error(m));
                bad = true;
            } else {
                mix(x, (size_t)n * sizeof *x);
                const double gap = fabs(obj - b.best) / (1.0 + fabs(b.best));
                if (gap > worst_gap)
                    worst_gap = gap;
                const double viol = fmax(ck.max_row_violation_relative,
                                         fmax(ck.max_col_violation,
                                              ck.max_cone_violation));
                if (viol > worst_viol)
                    worst_viol = viol;
                bool integral = true;
                for (int64_t t = 0; t < ni; t++)
                    integral &= x[icol[t]] == round(x[icol[t]]);
                if (gap > 1e-6 || !ck.primal_feasible || !integral) {
                    printf("m%05" PRId64 ": objective %.12g against brute "
                           "force %.12g, primal %d, integral %d, violation "
                           "%.2e\n", r, obj, b.best, ck.primal_feasible,
                           integral, viol);
                    bad = true;
                }
            }
        }
        if (bad) {
            fails++;
            dump_as(copy, dir, r);
        }
        jaos_model_free(copy);
        jaos_model_free(m);
    }
    printf("models %" PRId64 "\n", runs);
    printf("optimal %" PRId64 "\n", optimal);
    printf("infeasible %" PRId64 "\n", infeasible);
    printf("unbounded %" PRId64 "\n", unbounded);
    printf("brute force unsure %" PRId64 "\n", unsure);
    printf("tree numerical errors %" PRId64 "\n", tree_errors);
    printf("worst objective gap %.3e\n", worst_gap);
    printf("worst violation %.3e\n", worst_viol);
    printf("nodes %" PRId64 "\n", total_nodes);
    printf("work units %" PRId64 "\n", total_work);
    printf("brute force solves %" PRId64 "\n", brute_solves);
    printf("failed models %" PRId64 "\n", fails);
    printf("digest %016" PRIx64 "\n", digest);
    return fails == 0 ? 0 : 1;
}
