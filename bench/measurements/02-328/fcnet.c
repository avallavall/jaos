/* The reading behind 02-328's cut check: generated fixed-charge networks,
 * each answer of the tree judged against brute force over every value of
 * its integer columns.
 *
 * A model has 3 to 6 nodes and 3 to 11 arcs. Arc a carries a flow x_a in
 * [0, U_a] (U_a its capacity, or infinite in a third of the arcs) and opens
 * with a binary y_a: the row k_a x_a - k_a u_a y_a <= 0 with a scale k_a in
 * {1, 2, 0.5}, so the flow columns sit under binaries as the network
 * cuts need. A node balances inflow against outflow at a demand, as an
 * equality, a >= row or a <= row. Three nodes in four can buy or dump flow
 * at a high price through a column of their own, so most models are
 * feasible. A
 * fifth of the models add a general integer column z in [0, 2] that caps
 * one arc as x_a <= u_a z. Costs are on the flows, the binaries and z.
 *
 * Built against a library compiled with -DJAOS_MIP_NET_MIN_COLS_VALUE=1,
 * so these small models take the network cut rounds. Brute force fixes
 * every integer column at each of its values and solves the continuous
 * model. The tree must end INFEASIBLE when no assignment is feasible and
 * otherwise OPTIMAL at the best one within 1e-6 relative, with integer
 * columns integral.
 *
 * Usage: fcnet RUNS SEED DIR
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

#define MAXA 11
#define MAXV 6
#define MAXC (2 * MAXA + MAXV + 1)

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

static jaos_status add_col(jaos_model *m, double cost, double lo, double hi)
{
    const int64_t start[2] = {0, 0};
    return jaos_add_cols(m, 1, &cost, &lo, &hi, 0, start, nullptr, nullptr);
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
    int64_t icol[MAXA + 1], ni;
    double ihi[MAXA + 1];
} shape;

static double brute_force(const jaos_model *b0, const shape *s,
                          int64_t *solves)
{
    double best = INFINITY;
    int64_t val[MAXA + 1] = {0};
    for (;;) {
        jaos_model *b = nullptr;
        if (jaos_model_copy(b0, &b) != JAOS_OK)
            return NAN;
        for (int64_t k = 0; k < s->ni; k++) {
            const int64_t j = s->icol[k];
            if (jaos_set_col_integer(b, j, false) != JAOS_OK ||
                jaos_set_col_bounds(b, j, (double)val[k], (double)val[k]) !=
                    JAOS_OK) {
                jaos_model_free(b);
                return NAN;
            }
        }
        if (jaos_solve(b) != JAOS_OK) {
            jaos_model_free(b);
            return NAN;
        }
        (*solves)++;
        const jaos_solve_status st = jaos_status_of(b);
        if (st == JAOS_SOLVE_OPTIMAL) {
            double obj = 0.0;
            if (jaos_objective(b, &obj) == JAOS_OK && obj < best)
                best = obj;
        } else if (st != JAOS_SOLVE_INFEASIBLE) {
            jaos_model_free(b);
            return NAN;
        }
        jaos_model_free(b);
        int64_t k = 0;
        while (k < s->ni && val[k] == (int64_t)s->ihi[k]) {
            val[k] = 0;
            k++;
        }
        if (k == s->ni)
            break;
        val[k]++;
    }
    return best;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? strtoll(argv[1], nullptr, 10) : 1000;
    rng_state = argc > 2 ? strtoull(argv[2], nullptr, 10) : 1;
    const char *dir = argc > 3 ? argv[3] : ".";
    if (rng_state == 0)
        rng_state = 1;
    int64_t optimal = 0, infeasible = 0, failed = 0, unsure = 0;
    int64_t brute_solves = 0, with_z = 0, cut_models = 0;
    for (int64_t r = 0; r < runs; r++) {
        const int64_t nv = ri(3, MAXV), na = ri(3, MAXA);
        int64_t from[MAXA], to[MAXA];
        double cap[MAXA], scale[MAXA];
        bool open_cap[MAXA];
        for (int64_t a = 0; a < na; a++) {
            from[a] = ri(0, nv - 1);
            to[a] = (from[a] + ri(1, nv - 1)) % nv;
            cap[a] = (double)ri(2, 20);
            const int64_t sk = ri(0, 2);
            scale[a] = sk == 0 ? 1.0 : sk == 1 ? 2.0 : 0.5;
            open_cap[a] = ri(0, 2) == 0;
        }
        double dem[MAXV], tot = 0.0;
        for (int64_t v = 0; v + 1 < nv; v++) {
            dem[v] = (double)ri(-8, 8);
            tot += dem[v];
        }
        dem[nv - 1] = -tot;
        const bool zcol = ri(0, 4) == 0;
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK)
            return 2;
        shape s = {0};
        int64_t ncol = 0;
        bool ok = true;
        for (int64_t a = 0; a < na && ok; a++) {
            const double x_hi = open_cap[a] ? INFINITY : cap[a];
            ok = add_col(m, rf(0.0, 5.0), 0.0, x_hi) == JAOS_OK;
        }
        for (int64_t a = 0; a < na && ok; a++) {
            ok = add_col(m, rf(1.0, 20.0), 0.0, 1.0) == JAOS_OK &&
                 jaos_set_col_integer(m, na + a, true) == JAOS_OK;
            s.icol[s.ni] = na + a;
            s.ihi[s.ni] = 1.0;
            s.ni++;
        }
        ncol = 2 * na;
        int64_t buy[MAXV];
        double side[MAXV];
        for (int64_t v = 0; v < nv && ok; v++) {
            buy[v] = -1;
            side[v] = ri(0, 1) == 0 ? 1.0 : -1.0;
            if (ri(0, 3) == 0)
                continue;
            ok = add_col(m, rf(30.0, 60.0), 0.0, INFINITY) == JAOS_OK;
            buy[v] = ncol++;
        }
        int64_t zc = -1, za = -1;
        if (ok && zcol) {
            ok = add_col(m, rf(1.0, 10.0), 0.0, 2.0) == JAOS_OK &&
                 jaos_set_col_integer(m, ncol, true) == JAOS_OK;
            zc = ncol++;
            za = ri(0, na - 1);
            s.icol[s.ni] = zc;
            s.ihi[s.ni] = 2.0;
            s.ni++;
            with_z++;
        }
        for (int64_t a = 0; a < na && ok; a++) {
            const int64_t idx[2] = {a, na + a};
            const double val[2] = {scale[a], -scale[a] * cap[a]};
            ok = add_row(m, -INFINITY, 0.0, 2, idx, val) == JAOS_OK;
        }
        if (ok && zc >= 0) {
            const int64_t idx[2] = {za, zc};
            const double val[2] = {1.0, -0.5 * cap[za]};
            ok = add_row(m, -INFINITY, 0.0, 2, idx, val) == JAOS_OK;
        }
        for (int64_t v = 0; v < nv && ok; v++) {
            int64_t idx[MAXA + 1], nn = 0;
            double val[MAXA + 1];
            for (int64_t a = 0; a < na; a++) {
                if (to[a] == v) {
                    idx[nn] = a;
                    val[nn++] = 1.0;
                } else if (from[a] == v) {
                    idx[nn] = a;
                    val[nn++] = -1.0;
                }
            }
            if (buy[v] >= 0) {
                idx[nn] = buy[v];
                val[nn++] = side[v];
            }
            if (nn == 0)
                continue;
            const int64_t kind = ri(0, 2);
            const double lo = kind == 2 ? -INFINITY : dem[v];
            const double hi = kind == 1 ? INFINITY : dem[v];
            ok = add_row(m, lo, hi, nn, idx, val) == JAOS_OK;
        }
        if (!ok) {
            fprintf(stderr, "m%05" PRId64 ": build failed: %s\n", r,
                    jaos_model_error(m));
            jaos_model_free(m);
            failed++;
            continue;
        }
        jaos_model *b0 = nullptr;
        if (jaos_model_copy(m, &b0) != JAOS_OK)
            return 2;
        int64_t solves = 0;
        const double best = brute_force(b0, &s, &solves);
        brute_solves += solves;
        jaos_model_free(b0);
        if (isnan(best)) {
            unsure++;
            jaos_model_free(m);
            continue;
        }
        if (jaos_set_mip_node_limit(m, 200000) != JAOS_OK ||
            jaos_solve(m) != JAOS_OK) {
            fprintf(stderr, "m%05" PRId64 ": solve failed: %s\n", r,
                    jaos_model_error(m));
            dump_as(m, dir, r);
            failed++;
            jaos_model_free(m);
            continue;
        }
        jaos_mip_report rep = {0};
        if (jaos_mip_result(m, &rep) == JAOS_OK && rep.cuts > 0)
            cut_models++;
        const jaos_solve_status st = jaos_status_of(m);
        bool good;
        double obj = NAN;
        if (!isfinite(best)) {
            good = st == JAOS_SOLVE_INFEASIBLE;
            infeasible += good;
        } else {
            double x[MAXC] = {0};
            good = st == JAOS_SOLVE_OPTIMAL &&
                   jaos_objective(m, &obj) == JAOS_OK &&
                   fabs(obj - best) <= 1e-6 * fmax(1.0, fabs(best)) &&
                   jaos_solution(m, x, nullptr, nullptr, nullptr) == JAOS_OK;
            for (int64_t k = 0; good && k < s.ni; k++)
                good = x[s.icol[k]] == round(x[s.icol[k]]);
            optimal += good;
        }
        const double rec[2] = {(double)st, isfinite(obj) ? obj : 0.0};
        mix(rec, sizeof rec);
        if (!good) {
            fprintf(stderr,
                    "m%05" PRId64 ": tree status %d objective %.12g, brute "
                    "force %.12g\n",
                    r, (int)st, obj, best);
            dump_as(m, dir, r);
            failed++;
        }
        jaos_model_free(m);
    }
    printf("models %" PRId64 " with z %" PRId64 " with cuts %" PRId64
           " optimal %" PRId64 " infeasible %" PRId64 " unsure %" PRId64
           " failed %" PRId64 " brute solves %" PRId64 " digest %016" PRIx64
           "\n",
           runs, with_z, cut_models, optimal, infeasible, unsure, failed,
           brute_solves, digest);
    return failed > 0 ? 1 : 0;
}
