/* The reading behind 02-248: the push that puts a QP's answer on its bounds.
 *
 * Every model is a convex QP whose optimum is known by construction: x*,
 * the multipliers y and the reduced costs d are drawn first, with the
 * signs the KKT conditions ask for, Q = L L' + eps I is drawn, and the
 * cost is what makes them fit, c = d + A'y - Q x*. A third of the columns
 * sit on a bound with a reduced cost that is not zero, which is where the
 * barrier's point stops short and the checker refuses the dual side.
 * A third of the models are maximised with -Q and -c.
 *
 * Properties, for every model:
 *   P1 the solve ends OPTIMAL
 *   P2 the objective is f* = c'x* + 1/2 x*'Q x* to 1e-6 relative
 *   P3 the checker takes the point and the duals, primal and dual side,
 *      at 1e-6
 *   P4 a column whose reduced cost is not zero is on its bound to 1e-6
 *   P5 with eps > 0 the optimum is one point and the answer is it to 1e-6
 *
 * Every 50th model is written as LP and MPS, and the tool solves each
 * with --check; the runner reads its check_ok lines.
 *
 * Usage: push RUNS SEED DIR
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

#define MAXC 16
#define MAXR 8

typedef struct {
    int64_t nr, nc, nz, nq;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    int64_t qr[MAXC * MAXC], qc[MAXC * MAXC];
    double qv[MAXC * MAXC];
    double xs[MAXC], d[MAXC], y[MAXR], fstar;
    double eps;
    bool maximise;
} gen;

static void generate(gen *g)
{
    g->nc = ri(4, MAXC);
    g->nr = ri(2, MAXR);
    g->maximise = ri(0, 2) == 0;
    const int64_t e = ri(0, 5);
    g->eps = e == 0 ? 0.0 : (double)e * 0.25;

    for (int64_t j = 0; j < g->nc; j++) {
        g->cl[j] = (double)ri(-5, 2);
        g->cu[j] = g->cl[j] + (double)ri(1, 8);
        const int64_t where = ri(0, 5);
        if (where <= 1) {
            g->xs[j] = g->cl[j];
            g->d[j] = ri(0, 3) == 0 ? 0.0 : (double)ri(1, 6) * 0.5;
        } else if (where == 2) {
            g->xs[j] = g->cu[j];
            g->d[j] = ri(0, 3) == 0 ? 0.0 : -(double)ri(1, 6) * 0.5;
        } else {
            g->xs[j] = g->cl[j] + (double)ri(1, 3) * (g->cu[j] - g->cl[j]) / 4.0;
            g->d[j] = 0.0;
        }
    }

    double L[MAXC][MAXC];
    for (int64_t i = 0; i < g->nc; i++)
        for (int64_t j = 0; j < g->nc; j++)
            L[i][j] = 0.0;
    for (int64_t i = 0; i < g->nc; i++)
        for (int64_t j = 0; j <= i; j++)
            if (ri(0, 2) == 0)
                L[i][j] = (double)ri(-2, 2);
    double Q[MAXC][MAXC];
    for (int64_t i = 0; i < g->nc; i++)
        for (int64_t j = 0; j < g->nc; j++) {
            double t = 0.0;
            for (int64_t k = 0; k < g->nc; k++)
                t += L[i][k] * L[j][k];
            Q[i][j] = t;
        }
    for (int64_t i = 0; i < g->nc; i++)
        Q[i][i] += g->eps;
    g->nq = 0;
    for (int64_t j = 0; j < g->nc; j++)
        for (int64_t i = j; i < g->nc; i++)
            if (Q[i][j] != 0.0) {
                g->qr[g->nq] = i;
                g->qc[g->nq] = j;
                g->qv[g->nq] = Q[i][j];
                g->nq++;
            }

    g->nz = 0;
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++)
        act[i] = 0.0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->ap[j] = g->nz;
        for (int64_t i = 0; i < g->nr; i++)
            if (ri(0, 2) == 0) {
                const double a = (double)ri(-4, 4);
                if (a == 0.0)
                    continue;
                g->ai[g->nz] = i;
                g->av[g->nz] = a;
                g->nz++;
                act[i] += a * g->xs[j];
            }
    }
    g->ap[g->nc] = g->nz;
    for (int64_t i = 0; i < g->nr; i++) {
        const int64_t kind = ri(0, 4);
        g->y[i] = 0.0;
        if (kind == 0) {
            g->rl[i] = act[i];
            g->ru[i] = act[i];
            g->y[i] = (double)ri(-4, 4) * 0.5;
        } else if (kind == 1) {
            g->rl[i] = act[i];
            g->ru[i] = ri(0, 1) ? jaos_infinity() : act[i] + (double)ri(1, 5);
            g->y[i] = (double)ri(1, 4) * 0.5;
        } else if (kind == 2) {
            g->ru[i] = act[i];
            g->rl[i] = ri(0, 1) ? -jaos_infinity() : act[i] - (double)ri(1, 5);
            g->y[i] = -(double)ri(1, 4) * 0.5;
        } else {
            g->rl[i] = ri(0, 1) ? -jaos_infinity() : act[i] - (double)ri(1, 5);
            g->ru[i] = ri(0, 1) ? jaos_infinity() : act[i] + (double)ri(1, 5);
            if (!isfinite(g->rl[i]) && !isfinite(g->ru[i]))
                g->ru[i] = act[i] + (double)ri(1, 5);
        }
    }

    double qx[MAXC];
    for (int64_t j = 0; j < g->nc; j++) {
        double t = 0.0;
        for (int64_t k = 0; k < g->nc; k++)
            t += Q[j][k] * g->xs[k];
        qx[j] = t;
    }
    double f = 0.0;
    for (int64_t j = 0; j < g->nc; j++) {
        double aty = 0.0;
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++)
            aty += g->av[p] * g->y[g->ai[p]];
        g->cost[j] = g->d[j] + aty - qx[j];
        f += g->cost[j] * g->xs[j] + 0.5 * qx[j] * g->xs[j];
    }
    g->fstar = f;
    if (g->maximise) {
        for (int64_t j = 0; j < g->nc; j++)
            g->cost[j] = -g->cost[j];
        for (int64_t p = 0; p < g->nq; p++)
            g->qv[p] = -g->qv[p];
        g->fstar = -f;
    }
}

#define MAXROUNDS 16
static int64_t settled[MAXROUNDS + 1], unsettled, factor_failed;
static int64_t pinned_total, with_freeing;
static char last_push[256];

static void read_log(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    if (strncmp(line, "push:", 5) != 0)
        return;
    snprintf(last_push, sizeof last_push, "%s", line);
    long long a = 0, b = 0, r = 0;
    if (sscanf(line, "push: %lld of %lld variables pinned on a bound, settled "
                     "in %lld round", &a, &b, &r) == 3) {
        pinned_total += a;
        settled[r < MAXROUNDS ? r : MAXROUNDS]++;
        long long fr = 0;
        const char *c = strrchr(line, ',');
        if (c != nullptr && sscanf(c, ", %lld freed", &fr) == 1 && fr > 0)
            with_freeing++;
    } else if (strstr(line, "did not settle") != nullptr ||
               strstr(line, "wrong sign") != nullptr ||
               strstr(line, "unsatisfied") != nullptr) {
        unsettled++;
    } else if (strstr(line, "factorisation failed") != nullptr) {
        factor_failed++;
    }
}

static jaos_model *build(const gen *g)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    (void)jaos_set_log_callback(m, read_log, nullptr);
    (void)jaos_set_log_level(m, JAOS_LOG_SUMMARY);
    if (jaos_load_lp(m, g->nc, g->nr,
                     g->maximise ? JAOS_MAXIMIZE : JAOS_MINIMIZE, 0.0,
                     g->cost, g->cl, g->cu, g->rl, g->ru, g->nz, g->ap,
                     g->ai, g->av) != JAOS_OK ||
        jaos_set_quadratic(m, g->nq, g->qr, g->qc, g->qv) != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    return m;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: push RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = strtoll(argv[1], nullptr, 10);
    const int64_t seed = strtoll(argv[2], nullptr, 10);
    const char *dir = argv[3];
    rng_state = 0x9E3779B97F4A7C15ull ^ ((uint64_t)seed * 0x2545F4914F6CDD1Dull);
    for (int k = 0; k < 8; k++)
        (void)rnd();

    int64_t not_optimal = 0, wrong_value = 0, primal_refused = 0,
            dual_refused = 0, off_bound = 0, off_point = 0, strict_cols = 0,
            unique = 0, checked = 0;
    double worst_gap = 0.0, worst_bound = 0.0, worst_point = 0.0,
           worst_dual = 0.0;
    int64_t work = 0;
    const double tol = 1e-6;

    for (int64_t run = 0; run < runs; run++) {
        gen g;
        generate(&g);
        jaos_model *m = build(&g);
        if (m == nullptr) {
            fprintf(stderr, "run %" PRId64 ": out of memory\n", run);
            return 2;
        }
        if (run % 50 == 0) {
            char path[512];
            snprintf(path, sizeof path, "%s/s%" PRId64 "-m%" PRId64 ".lp", dir,
                     seed, run);
            if (jaos_write_lp(m, path) != JAOS_OK)
                fprintf(stderr, "run %" PRId64 ": could not write %s\n", run,
                        path);
            snprintf(path, sizeof path, "%s/s%" PRId64 "-m%" PRId64 ".mps", dir,
                     seed, run);
            if (jaos_write_mps(m, path) != JAOS_OK)
                fprintf(stderr, "run %" PRId64 ": could not write %s\n", run,
                        path);
        }
        const jaos_status st = jaos_solve(m);
        if (st != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            not_optimal++;
            printf("run %" PRId64 ": status %s / %s\n", run,
                   jaos_status_str(st),
                   jaos_solve_status_str(jaos_status_of(m)));
            char path[512];
            snprintf(path, sizeof path, "%s/fail-s%" PRId64 "-m%" PRId64 ".lp",
                     dir, seed, run);
            (void)jaos_write_lp(m, path);
            jaos_model_free(m);
            continue;
        }
        work += jaos_work_units(m);
        double obj = 0.0;
        (void)jaos_objective(m, &obj);
        const double gap = fabs(obj - g.fstar) / (1.0 + fabs(g.fstar));
        if (gap > worst_gap)
            worst_gap = gap;
        if (gap > tol) {
            wrong_value++;
            printf("run %" PRId64 ": objective %.12g, expected %.12g\n", run,
                   obj, g.fstar);
        }
        double x[MAXC], y[MAXR];
        (void)jaos_solution(m, x, nullptr, y, nullptr);
        jaos_check_report rep = {0};
        bool checked_here = false;
        if (jaos_check_solution(m, x, y, tol, &rep) == JAOS_OK) {
            checked++;
            checked_here = true;
            if (!rep.primal_feasible) {
                primal_refused++;
                printf("run %" PRId64 ": primal side refused, col %.3e row %.3e\n",
                       run, rep.max_col_violation, rep.max_row_violation);
            }
            if (!rep.dual_feasible) {
                dual_refused++;
                printf("run %" PRId64 ": dual side refused, dual %.3e gap %.3e\n",
                       run, rep.max_dual_violation, rep.objective_gap);
            }
            if (rep.max_dual_violation > worst_dual)
                worst_dual = rep.max_dual_violation;
        }
        bool this_off_bound = false, this_off_point = false;
        for (int64_t j = 0; j < g.nc; j++) {
            if (g.d[j] != 0.0) {
                strict_cols++;
                const double dist = fabs(x[j] - g.xs[j]);
                if (dist > worst_bound)
                    worst_bound = dist;
                if (dist > tol)
                    this_off_bound = true;
            }
            if (g.eps > 0.0) {
                const double dist = fabs(x[j] - g.xs[j]);
                if (dist > worst_point)
                    worst_point = dist;
                if (dist > tol)
                    this_off_point = true;
            }
        }
        if (g.eps > 0.0)
            unique++;
        if (this_off_bound) {
            off_bound++;
            printf("run %" PRId64 ": a strict column is off its bound\n", run);
        }
        if (this_off_point)
            off_point++;
        const bool failed = gap > tol || this_off_bound || this_off_point ||
                            (checked_here && (!rep.primal_feasible ||
                                              !rep.dual_feasible));
        if (failed) {
            char path[512];
            snprintf(path, sizeof path, "%s/fail-s%" PRId64 "-m%" PRId64 ".lp",
                     dir, seed, run);
            (void)jaos_write_lp(m, path);
            printf("run %" PRId64 ": %s\n", run, last_push);
        }
        jaos_model_free(m);
    }

    printf("seed %" PRId64 ": %" PRId64 " models, %" PRId64 " checked, %" PRId64
           " with one optimum, %" PRId64 " strict columns\n",
           seed, runs, checked, unique, strict_cols);
    printf("  P1 not optimal %" PRId64 "\n", not_optimal);
    printf("  P2 wrong value %" PRId64 " (worst %.3e)\n", wrong_value,
           worst_gap);
    printf("  P3 primal refused %" PRId64 ", dual refused %" PRId64
           " (worst dual %.3e)\n",
           primal_refused, dual_refused, worst_dual);
    printf("  P4 models with a strict column off its bound %" PRId64
           " (worst %.3e)\n",
           off_bound, worst_bound);
    printf("  P5 models off the one optimum %" PRId64 " (worst %.3e)\n",
           off_point, worst_point);
    printf("  work %" PRId64 "\n", work);
    printf("  push settled in rounds:");
    for (int r = 1; r <= MAXROUNDS; r++)
        if (settled[r] > 0)
            printf(" %d:%" PRId64, r, settled[r]);
    printf("; did not settle %" PRId64 ", factorisation failed %" PRId64
           ", pinned %" PRId64 " variables, %" PRId64
           " settled after a freeing\n",
           unsettled, factor_failed, pinned_total, with_freeing);
    return (not_optimal || wrong_value || primal_refused || dual_refused ||
            off_bound || off_point) ? 1 : 0;
}
