/* The reading behind 02-252: every model JAOS writes, read by a reader
 * JAOS did not write.
 *
 * Each model is generated, solved by JAOS, and written as MPS and as LP.
 * writers.sh then hands both files to HiGHS and compares its verdict and
 * objective with JAOS's. A writer that drops a section, a sign, a bound
 * or a pair of Q shows up as a different optimum.
 *
 * The models: 5 to 20 columns, 3 to 12 rows, a planted point inside the
 * boxes with each row's bounds set around its activity there. Sense
 * minimise or maximise, an objective offset, boxes of six shapes (on
 * [0, U], [L, U] with L below zero, open below, open above, free,
 * fixed), rows of four shapes (<=, >=, =, ranged). A third of the models
 * carry integer marks, some of them binaries and some semi-continuous
 * columns; a fifth of the continuous ones carry a convex Q, diagonal or
 * with pairs built as B'B plus a diagonal. Half the models carry names.
 * Every tenth model gets one row of a single column and one empty row.
 *
 * Usage: writers RUNS SEED DIR
 * Writes DIR/mNNNNN.mps, DIR/mNNNNN.lp and DIR/expect.txt, one line per
 * model: name status objective kind.
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

#define MAXC 20
#define MAXR 14

static const char *word[] = {"flow", "cap", "x", "y", "stock", "buy", "sell",
                             "make", "ship", "hold", "z", "w"};

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: writers RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = strtoll(argv[1], nullptr, 10);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)strtoll(argv[2], nullptr, 10);
    const char *dir = argv[3];
    const double inf = jaos_infinity();

    char path[1024];
    snprintf(path, sizeof path, "%s/expect.txt", dir);
    FILE *ex = fopen(path, "w");
    if (ex == nullptr)
        return 1;

    int64_t written = 0, refused = 0;
    for (int64_t k = 0; k < runs; k++) {
        const int64_t nc = ri(5, MAXC), nr0 = ri(3, 12);
        const bool odd_rows = k % 10 == 9;
        const int64_t nr = nr0 + (odd_rows ? 2 : 0);
        const bool mip = ri(0, 2) == 0;
        const bool quad = !mip && ri(0, 4) == 0;
        const bool pairs = quad && ri(0, 1) == 1;
        const bool named = ri(0, 1) == 1;
        const jaos_obj_sense sense = ri(0, 1) ? JAOS_MAXIMIZE : JAOS_MINIMIZE;
        const double offset = (double)ri(-5, 5) / 2.0;

        double cost[MAXC], cl[MAXC], cu[MAXC], z[MAXC];
        bool integer[MAXC] = {false}, semi[MAXC] = {false};
        for (int64_t j = 0; j < nc; j++) {
            cost[j] = (double)ri(-9, 9) / (ri(0, 3) == 0 ? 4.0 : 1.0);
            const int64_t shape = ri(0, 9);
            double lo = 0.0, hi = (double)ri(1, 12);
            if (shape == 1)
                lo = -(double)ri(1, 6);
            else if (shape == 2)
                lo = -inf;
            else if (shape == 3)
                hi = inf;
            else if (shape == 4)
                lo = -inf, hi = inf;
            else if (shape == 5)
                lo = hi = (double)ri(-3, 3);
            cl[j] = lo;
            cu[j] = hi;
            if (mip && ri(0, 1) == 0) {
                integer[j] = true;
                if (ri(0, 3) == 0) {
                    cl[j] = 0.0;
                    cu[j] = 1.0;
                }
            } else if (mip && ri(0, 7) == 0 && isfinite(hi) && hi > 1.0) {
                semi[j] = true;
                cl[j] = (double)ri(1, (int64_t)hi - 1);
            }
            double a = isfinite(cl[j]) ? cl[j] : (isfinite(cu[j]) ? cu[j] - 6.0 : -3.0);
            double b = isfinite(cu[j]) ? cu[j] : a + 6.0;
            z[j] = a + (double)ri(0, 4) * (b - a) / 4.0;
            if (integer[j])
                z[j] = floor(z[j]);
            if (semi[j] && ri(0, 2) == 0)
                z[j] = 0.0;
        }

        int64_t ap[MAXC + 1], ai[MAXC * MAXR];
        double av[MAXC * MAXR], act[MAXR] = {0.0};
        int64_t nz = 0;
        for (int64_t j = 0; j < nc; j++) {
            ap[j] = nz;
            for (int64_t i = 0; i < nr0; i++) {
                if (ri(0, 2) != 0)
                    continue;
                double v = (double)ri(-6, 6);
                if (v == 0.0)
                    v = 1.0;
                ai[nz] = i;
                av[nz++] = v;
                act[i] += v * z[j];
            }
            if (odd_rows && j == 0) {
                ai[nz] = nr0;
                av[nz++] = 2.0;
                act[nr0] += 2.0 * z[j];
            }
        }
        ap[nc] = nz;

        double rl[MAXR], ru[MAXR];
        for (int64_t i = 0; i < nr; i++) {
            const int64_t shape = ri(0, 3);
            const double slack = (double)ri(0, 3);
            if (odd_rows && i == nr0 + 1) {
                rl[i] = -(double)ri(0, 2);
                ru[i] = (double)ri(0, 2);
                continue;
            }
            if (shape == 0) {
                rl[i] = -inf;
                ru[i] = act[i] + slack;
            } else if (shape == 1) {
                rl[i] = act[i] - slack;
                ru[i] = inf;
            } else if (shape == 2) {
                rl[i] = ru[i] = act[i];
            } else {
                rl[i] = act[i] - slack;
                ru[i] = act[i] + slack + 1.0;
            }
        }

        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK)
            return 1;
        if (jaos_load_lp(m, nc, nr, sense, offset, cost, cl, cu, rl, ru, nz,
                         ap, ai, av) != JAOS_OK) {
            fprintf(stderr, "load %" PRId64 ": %s\n", k, jaos_model_error(m));
            return 1;
        }
        for (int64_t j = 0; j < nc; j++) {
            if (integer[j] && jaos_set_col_integer(m, j, true) != JAOS_OK)
                return 1;
            if (semi[j] && jaos_set_col_semicontinuous(m, j, true) != JAOS_OK)
                return 1;
        }
        if (quad) {
            const double sg = sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
            int64_t qr[MAXC * MAXC], qc[MAXC * MAXC];
            double qv[MAXC * MAXC];
            int64_t qn = 0;
            if (!pairs) {
                for (int64_t j = 0; j < nc; j++)
                    if (ri(0, 1) == 0) {
                        qr[qn] = qc[qn] = j;
                        qv[qn++] = sg * (double)ri(1, 6);
                    }
            } else {
                double b[3][MAXC] = {{0.0}};
                for (int64_t r = 0; r < 3; r++)
                    for (int64_t j = 0; j < nc; j++)
                        b[r][j] = ri(0, 2) == 0 ? (double)ri(-2, 2) : 0.0;
                for (int64_t j = 0; j < nc; j++)
                    for (int64_t i = j; i < nc; i++) {
                        double s = 0.0;
                        for (int64_t r = 0; r < 3; r++)
                            s += b[r][i] * b[r][j];
                        if (i == j)
                            s += (double)ri(0, 1);
                        if (s != 0.0) {
                            qr[qn] = i;
                            qc[qn] = j;
                            qv[qn++] = sg * s;
                        }
                    }
            }
            if (qn > 0 && jaos_set_quadratic(m, qn, qr, qc, qv) != JAOS_OK) {
                fprintf(stderr, "q %" PRId64 ": %s\n", k, jaos_model_error(m));
                return 1;
            }
        }
        if (named) {
            char nm[64];
            for (int64_t j = 0; j < nc; j++) {
                snprintf(nm, sizeof nm, "%s_%" PRId64, word[ri(0, 11)], j);
                if (jaos_set_col_name(m, j, nm) != JAOS_OK)
                    return 1;
            }
            for (int64_t i = 0; i < nr; i++) {
                snprintf(nm, sizeof nm, "r%s%" PRId64, word[ri(0, 11)], i);
                if (jaos_set_row_name(m, i, nm) != JAOS_OK)
                    return 1;
            }
        }

        const jaos_status sst = jaos_solve(m);
        const char *status = "error";
        double obj = 0.0;
        if (sst == JAOS_OK) {
            switch (jaos_status_of(m)) {
            case JAOS_SOLVE_OPTIMAL:
                status = "optimal";
                if (jaos_objective(m, &obj) != JAOS_OK)
                    return 1;
                break;
            case JAOS_SOLVE_INFEASIBLE: status = "infeasible"; break;
            case JAOS_SOLVE_UNBOUNDED: status = "unbounded"; break;
            default: status = "other"; break;
            }
        }
        const char *point = "-";
        if (mip && strcmp(status, "unbounded") == 0) {
            jaos_model *z = nullptr;
            double x[MAXC];
            jaos_check_report rep;
            point = "none";
            if (jaos_model_copy(m, &z) != JAOS_OK)
                return 1;
            for (int64_t j = 0; j < nc; j++)
                if (jaos_set_col_cost(z, j, 0.0) != JAOS_OK)
                    return 1;
            if (jaos_solve(z) == JAOS_OK &&
                jaos_status_of(z) == JAOS_SOLVE_OPTIMAL &&
                jaos_solution(z, x, nullptr, nullptr, nullptr) == JAOS_OK &&
                jaos_check_solution(m, x, nullptr, 1e-7, &rep) == JAOS_OK &&
                rep.primal_feasible)
                point = "checked";
            jaos_model_free(z);
        }
        const char *kind = quad ? (pairs ? "qp-pairs" : "qp-diag")
                                : (mip ? "mip" : "lp");
        snprintf(path, sizeof path, "%s/m%05" PRId64 ".mps", dir, k);
        const jaos_status w1 = jaos_write_mps(m, path);
        snprintf(path, sizeof path, "%s/m%05" PRId64 ".lp", dir, k);
        const jaos_status w2 = jaos_write_lp(m, path);
        if (w1 != JAOS_OK || w2 != JAOS_OK) {
            refused++;
            fprintf(ex, "m%05" PRId64 " refused 0 %s %s\n", k, kind,
                    jaos_model_error(m));
        } else {
            written++;
            fprintf(ex, "m%05" PRId64 " %s %.17g %s %s\n", k, status, obj, kind,
                    point);
        }
        jaos_model_free(m);
    }
    fclose(ex);
    fprintf(stderr, "models %" PRId64 ": %" PRId64 " written, %" PRId64
            " refused by a writer\n", runs, written, refused);
    return 0;
}
