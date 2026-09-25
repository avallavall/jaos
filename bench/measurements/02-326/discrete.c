/* The reading behind 02-326: generated models with SOS sets,
 * semi-continuous columns and indicator rows, beside cones and quadratic
 * rows or without them, each answer of the tree judged against brute
 * force over every piece.
 *
 * The models follow 02-255's misocp.c: 3 to 8 columns and a planted point,
 * 0 to 2 cones whose heads are moved so the point sits inside, 0 to 4
 * linear rows and 0 to 2 convex quadratic rows strictly satisfied at the
 * point, a third with a convex diagonal objective, and every fifth with a
 * row that holds its first integer column between two integers. On top:
 * 0 to 2 integer columns boxed to their planted value plus or minus 1 or
 * 2; half the models get a semi-continuous column planted inside a box
 * whose lower bound is positive; half get an SOS set of type 1 or 2 over
 * 2 to 3 continuous columns, the members the type leaves out planted at 0;
 * and half of the models with an integer column get an indicator row over
 * 1 to 3 continuous columns, satisfied at the point when the column sits at
 * the switching value and broken there when it does not.
 *
 * Brute force takes every integer value, both sides of the semi-continuous
 * column (0, or its box) and every pattern of the SOS set (one member free
 * for type 1, two adjacent for type 2, the rest at 0), drops the indicator
 * row where the column does not take the switching value, and solves each
 * continuous model. The tree must end INFEASIBLE when no piece is
 * feasible, UNBOUNDED when a feasible piece is unbounded, and otherwise
 * OPTIMAL at the best piece within 1e-6 relative, with a point the checker
 * takes at 1e-6 and integer columns exactly integral.
 *
 * Usage: discrete RUNS SEED DIR
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
    int64_t icol[MAXC], ni;
    double ilo[MAXC], ihi[MAXC];
    int64_t semi;
    int64_t sos[3], nsos, sos_type;
    int64_t ind_row, ind_col, ind_val;
} shape;

typedef struct {
    int verdict;
    double best;
    int64_t solves, unsure;
} brute;

static void brute_force(const jaos_model *b0, const shape *s, brute *b)
{
    b->verdict = JAOS_SOLVE_INFEASIBLE;
    b->best = 0.0;
    b->solves = b->unsure = 0;
    jaos_obj_sense sense = JAOS_MINIMIZE;
    if (jaos_objective_sense(b0, &sense) != JAOS_OK)
        exit(1);
    const double sigma = sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const int64_t nsemi = s->semi >= 0 ? 2 : 1;
    const int64_t npat = s->nsos == 0 ? 1
                       : s->sos_type == 1 ? s->nsos : s->nsos - 1;
    double v[MAXC];
    for (int64_t t = 0; t < s->ni; t++)
        v[t] = s->ilo[t];
    for (;;) {
        for (int64_t side = 0; side < nsemi; side++)
            for (int64_t pat = 0; pat < npat; pat++) {
                jaos_model *c = nullptr;
                if (jaos_model_copy(b0, &c) != JAOS_OK)
                    exit(1);
                bool empty = false;
                for (int64_t t = 0; t < s->ni; t++)
                    if (jaos_set_col_integer(c, s->icol[t], false) != JAOS_OK ||
                        jaos_set_col_bounds(c, s->icol[t], v[t], v[t]) !=
                            JAOS_OK)
                        exit(1);
                if (s->semi >= 0 && side == 0 &&
                    jaos_set_col_bounds(c, s->semi, 0.0, 0.0) != JAOS_OK)
                    exit(1);
                for (int64_t t = 0; t < s->nsos; t++) {
                    const bool free_here = s->sos_type == 1
                        ? t == pat : t == pat || t == pat + 1;
                    if (free_here)
                        continue;
                    double lo = 0.0, hi = 0.0;
                    if (jaos_col_bounds(c, s->sos[t], &lo, &hi) != JAOS_OK)
                        exit(1);
                    if (lo > 0.0 || hi < 0.0)
                        empty = true;
                    else if (jaos_set_col_bounds(c, s->sos[t], 0.0, 0.0) !=
                             JAOS_OK)
                        exit(1);
                }
                if (s->ind_row >= 0) {
                    double zv = 0.0;
                    for (int64_t t = 0; t < s->ni; t++)
                        if (s->icol[t] == s->ind_col)
                            zv = v[t];
                    if (zv != (double)s->ind_val &&
                        jaos_set_row_bounds(c, s->ind_row, -INFINITY,
                                            INFINITY) != JAOS_OK)
                        exit(1);
                }
                if (empty) {
                    jaos_model_free(c);
                    continue;
                }
                b->solves++;
                if (jaos_solve(c) != JAOS_OK) {
                    b->unsure++;
                } else {
                    const jaos_solve_status st = jaos_status_of(c);
                    double obj = 0.0;
                    if (st == JAOS_SOLVE_UNBOUNDED) {
                        b->verdict = JAOS_SOLVE_UNBOUNDED;
                    } else if (st == JAOS_SOLVE_OPTIMAL &&
                               jaos_objective(c, &obj) == JAOS_OK) {
                        if (b->verdict == JAOS_SOLVE_INFEASIBLE ||
                            (b->verdict == JAOS_SOLVE_OPTIMAL &&
                             sigma * obj < sigma * b->best)) {
                            b->verdict = JAOS_SOLVE_OPTIMAL;
                            b->best = obj;
                        }
                    } else if (st != JAOS_SOLVE_INFEASIBLE) {
                        b->unsure++;
                    }
                }
                jaos_model_free(c);
            }
        int64_t t = 0;
        while (t < s->ni && v[t] >= s->ihi[t]) {
            v[t] = s->ilo[t];
            t++;
        }
        if (t >= s->ni)
            break;
        v[t] += 1.0;
    }
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: discrete RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = strtoll(argv[1], nullptr, 10);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)strtoll(argv[2], nullptr, 10);
    const char *dir = argv[3];
    const double inf = jaos_infinity();
    int64_t fails = 0, optimal = 0, infeasible = 0, unbounded = 0;
    int64_t unsure = 0, tree_errors = 0, total_nodes = 0, total_work = 0;
    int64_t brute_solves = 0, with_semi = 0, with_sos = 0, with_ind = 0;
    int64_t conic = 0;
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

        shape s = {.ni = 0, .semi = -1, .nsos = 0, .ind_row = -1,
                   .ind_col = -1};
        const int64_t want_int = ri(0, 2);
        for (int64_t j = 0; j < n && s.ni < want_int; j++)
            if (!head[j] && ri(0, 1) == 1) {
                isint[j] = true;
                s.icol[s.ni++] = j;
                x0[j] = round(x0[j]);
            }
        if (ri(0, 1) == 1)
            for (int64_t j = 0; j < n && s.semi < 0; j++)
                if (!head[j] && !isint[j] && ri(0, 1) == 1) {
                    s.semi = j;
                    x0[j] = rf(0.5, 2.0);
                }
        if (ri(0, 1) == 1) {
            const int64_t want = ri(2, 3);
            int64_t mem[3], got = 0;
            for (int64_t j = 0; j < n && got < want; j++)
                if (!head[j] && !isint[j] && j != s.semi && ri(0, 2) != 0)
                    mem[got++] = j;
            if (got >= 2) {
                s.nsos = got;
                s.sos_type = ri(1, 2);
                const int64_t keep = ri(0, s.sos_type == 1 ? got - 1 : got - 2);
                for (int64_t t = 0; t < got; t++) {
                    s.sos[t] = mem[t];
                    const bool live = s.sos_type == 1
                        ? t == keep : t == keep || t == keep + 1;
                    if (!live)
                        x0[mem[t]] = 0.0;
                }
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

        for (int64_t j = 0; j < n; j++) {
            if (isint[j]) {
                const double w = (double)ri(1, 2);
                lo[j] = x0[j] - w;
                hi[j] = x0[j] + w;
            } else if (j == s.semi) {
                lo[j] = x0[j] - rf(0.1, 0.4);
                hi[j] = ri(0, 1) ? x0[j] + rf(0.5, 3.0) : inf;
            } else {
                const int shape_ = (int)ri(0, 3);
                lo[j] = shape_ == 0 || shape_ == 2 ? x0[j] - rf(0.5, 3.0) : -inf;
                hi[j] = shape_ == 0 || shape_ == 3 ? x0[j] + rf(0.5, 3.0) : inf;
            }
            cost[j] = rf(-1.0, 1.0);
        }
        for (int64_t t = 0; t < s.ni; t++) {
            s.ilo[t] = lo[s.icol[t]];
            s.ihi[t] = hi[s.icol[t]];
        }
        const jaos_obj_sense sense = ri(0, 1) ? JAOS_MAXIMIZE : JAOS_MINIMIZE;
        const int64_t as0[MAXC + 1] = {0};
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK ||
            jaos_load_lp(m, n, 0, sense, rf(-5.0, 5.0), cost, lo, hi, nullptr,
                         nullptr, 0, as0, nullptr, nullptr) != JAOS_OK)
            return 1;
        for (int64_t t = 0; t < s.ni; t++)
            if (jaos_set_col_integer(m, s.icol[t], true) != JAOS_OK)
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
                    for (int64_t u = 0; u < t; u++)
                        again |= idx[u] == j;
                } while (again);
                idx[t] = j;
                val[t] = rf(-3.0, 3.0);
                act += val[t] * x0[j];
            }
            const int shape_ = (int)ri(0, 2);
            const double rl = shape_ == 0
                ? -inf : ldexp(floor(ldexp(act - rf(0.1, 2.0), 20)), -20);
            const double rh = shape_ == 1
                ? inf : ldexp(ceil(ldexp(act + rf(0.1, 2.0), 20)), -20);
            if (add_row(m, rl, rh, nn, idx, val) != JAOS_OK)
                return 1;
        }

        const int64_t nq = ri(0, 2);
        for (int64_t t = 0; t < nq; t++) {
            int64_t col[MAXS];
            double a[MAXS], d[MAXS], bmat[MAXS][MAXS];
            const int64_t k = ri(1, n < MAXS ? n : MAXS);
            for (int64_t u = 0; u < k; u++) {
                int64_t j;
                bool again;
                do {
                    j = ri(0, n - 1);
                    again = false;
                    for (int64_t w = 0; w < u; w++)
                        again |= col[w] == j;
                } while (again);
                col[u] = j;
                a[u] = rf(-2.0, 2.0);
                d[u] = ri(0, 2) == 0 ? 0.0 : rf(0.1, 1.5);
            }
            const int64_t rb = ri(0, k);
            for (int64_t u = 0; u < rb; u++)
                for (int64_t w = 0; w < k; w++)
                    bmat[u][w] = rf(-1.0, 1.0);
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
        if (s.ni > 0 && ri(0, 1) == 1) {
            int64_t idx[3];
            double val[3], act = 0.0;
            int64_t nn = 0;
            const int64_t want = ri(1, 3);
            for (int64_t j = 0; j < n && nn < want; j++)
                if (!isint[j] && ri(0, 1) == 1) {
                    idx[nn] = j;
                    val[nn] = rf(-3.0, 3.0);
                    act += val[nn] * x0[j];
                    nn++;
                }
            if (nn > 0) {
                s.ind_col = s.icol[ri(0, s.ni - 1)];
                s.ind_val = ri(0, 1);
                double rl, rh;
                if (x0[s.ind_col] == (double)s.ind_val) {
                    rl = ri(0, 1) ? -inf : ldexp(floor(ldexp(act - rf(0.1, 2.0), 20)), -20);
                    rh = ldexp(ceil(ldexp(act + rf(0.1, 2.0), 20)), -20);
                } else {
                    rl = ldexp(floor(ldexp(act + rf(0.5, 1.0), 20)), -20);
                    rh = ri(0, 1) ? inf : ldexp(ceil(ldexp(act + rf(1.5, 3.0), 20)), -20);
                }
                if (add_row(m, rl, rh, nn, idx, val) != JAOS_OK)
                    return 1;
                s.ind_row = jaos_num_row(m) - 1;
            }
        }
        if (trap && s.ni > 0) {
            const int64_t j = s.icol[0];
            const double one = 1.0;
            if (add_row(m, x0[j] + 0.25, x0[j] + 0.75, 1, &j, &one) !=
                JAOS_OK)
                return 1;
        }

        brute b;
        brute_force(m, &s, &b);
        brute_solves += b.solves;

        if (s.semi >= 0 &&
            jaos_set_col_semicontinuous(m, s.semi, true) != JAOS_OK)
            return 1;
        if (s.nsos > 0) {
            const double w[3] = {1.0, 2.0, 3.0};
            if (jaos_add_sos(m, (int)s.sos_type, s.nsos, s.sos, w) != JAOS_OK)
                return 1;
        }
        if (s.ind_row >= 0 &&
            jaos_set_row_indicator(m, s.ind_row, s.ind_col, (int)s.ind_val) !=
                JAOS_OK)
            return 1;
        with_semi += s.semi >= 0;
        with_sos += s.nsos > 0;
        with_ind += s.ind_row >= 0;
        conic += ncone > 0 || nq > 0;

        jaos_model *copy = nullptr;
        if (jaos_model_copy(m, &copy) != JAOS_OK)
            return 1;
        const jaos_status st = jaos_solve(m);
        const jaos_solve_status ss = jaos_status_of(m);
        mix(&ss, sizeof ss);
        total_work += jaos_work_units(m);
        jaos_mip_report rep;
        if (st == JAOS_OK && jaos_mip_result(m, &rep) == JAOS_OK)
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
                                              fmax(ck.max_cone_violation,
                                                   ck.max_integrality_violation)));
                if (viol > worst_viol)
                    worst_viol = viol;
                bool integral = true;
                for (int64_t t = 0; t < s.ni; t++)
                    integral &= x[s.icol[t]] == round(x[s.icol[t]]);
                if (gap > 1e-6 || !ck.primal_feasible || !integral ||
                    ck.max_integrality_violation > 1e-6) {
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
    printf("with a cone or a quadratic row %" PRId64 "\n", conic);
    printf("with a semi-continuous column %" PRId64 "\n", with_semi);
    printf("with an SOS set %" PRId64 "\n", with_sos);
    printf("with an indicator row %" PRId64 "\n", with_ind);
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
