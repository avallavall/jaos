/* The reading behind 02-427: 02-425's edits on models with convex
 * quadratic rows instead of cones.
 *
 * The generator is 02-235's LPs, minimising, with one or two quadratic
 * rows added. A quadratic row has a linear part over about a third of the
 * columns and a diagonal quadratic part over 1 to 3 of them, with weights
 * 1 to 3: convex with a finite upper side, or concave (negated weights)
 * with a finite lower side, the side drawn near the row's value at the last
 * optimum. Each model is solved, then edited EDITS times. Each edit is one
 * of: a cost, a column's bounds, a row's sides (on a quadratic row only the
 * side that keeps it convex), a coefficient (0 removes it), a new row, a
 * new column, a deleted row, a deleted column, a new quadratic row, and a
 * quadratic row dropped (deleted, or its quadratic part cleared). After an
 * optimal solve, new bounds and sides are drawn near the point and its row
 * activities. After each edit the model is solved again, and a copy of it
 * (jaos_model_copy) is solved from nothing. Properties, after each edit:
 *   P1 both solves return JAOS_OK with the same status
 *   P2 an optimum is within 1e-6 of (1 + |copy's|) of the copy's objective,
 *      and jaos_check_conic_solution passes the point and its row duals at
 *      1e-6
 *   P3 a certificate published with an infeasible answer passes
 *      jaos_check_conic_certificate at 1e-7
 * With quadratic rows the checker's certificate test is not exact (TODO's
 * J22), and an infeasible answer whose certificate it refuses publishes
 * none; those are counted, not failed. With no quadratic row left, an
 * infeasible answer with no certificate fails P3.
 * In MODE miqcp each column is integer with probability one half, new
 * columns too; an optimum must then be integral and pass
 * jaos_check_solution's primal half, and P3 is not read. Both solves of a
 * miqcp model run under 2e8 work units, and an edit where either stops on
 * that limit is counted as capped and not read. A pair that ends with any
 * other status is counted as other, and the first eight are printed.
 *
 * Usage: warmqc RUNS SEED EDITS MODE
 *   MODE qcp or miqcp
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
#define BIG 64

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    double offset;
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->offset = (double)ri(-3, 3);
    const bool broken = ri(0, 9) < 6;
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
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

static bool g_have;
static double g_x[BIG], g_act[BIG];

static void keep_point(const jaos_model *m)
{
    g_have = jaos_status_of(m) == JAOS_SOLVE_OPTIMAL &&
             jaos_solution(m, g_x, g_act, nullptr, nullptr) == JAOS_OK;
}

static void sides(double *lo, double *up, bool near, double at)
{
    const double base = near ? round(at) + (double)ri(-2, 2)
                             : (double)ri(-20, 20);
    const int64_t kind = ri(0, 9);
    if (kind < 3) {
        *lo = *up = base;
    } else if (kind < 6) {
        *lo = -INFINITY;
        *up = base;
    } else if (kind < 9) {
        *lo = base;
        *up = INFINITY;
    } else {
        *lo = base - (double)ri(0, 3);
        *up = base + (double)ri(0, 3);
    }
}

static void box(double *lo, double *up, bool near, double at)
{
    if (near) {
        const double shift = ri(0, 4) == 0 ? (ri(0, 1) ? 1.0 : -1.0) : 0.0;
        *lo = ri(0, 9) == 0 ? -INFINITY : floor(at) - (double)ri(0, 2) + shift;
        *up = ri(0, 9) == 0 ? INFINITY : ceil(at) + (double)ri(0, 3) + shift;
        return;
    }
    *lo = ri(0, 9) == 0 ? -INFINITY : (double)ri(-2, 3);
    *up = ri(0, 9) == 0 ? INFINITY
                        : (isfinite(*lo) ? *lo : 0.0) + (double)ri(0, 8);
}

static int curve(const jaos_model *m, int64_t i)
{
    const int64_t n = jaos_row_quadratic_nz(m, i);
    if (n == 0)
        return 0;
    int64_t r[BIG], c[BIG];
    double v[BIG];
    if (n > BIG || jaos_row_quadratic(m, i, r, c, v) != JAOS_OK)
        exit(1);
    return v[0] > 0.0 ? 1 : -1;
}

static int64_t quad_rows(const jaos_model *m)
{
    int64_t n = 0;
    for (int64_t i = 0; i < jaos_num_row(m); i++)
        n += curve(m, i) != 0;
    return n;
}

static jaos_status add_qrow(jaos_model *m, char *what, size_t len)
{
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    int64_t idx[BIG], qc[3], nq = 0, n = 0;
    double val[BIG], qv[3];
    for (int64_t j = 0; j < nc; j++)
        if (ri(0, 9) < 3) {
            idx[n] = j;
            val[n] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
            n++;
        }
    const int64_t want = ri(1, 3);
    for (int tries = 0; nq < want && tries < 40; tries++) {
        const int64_t j = ri(0, nc - 1);
        bool seen = false;
        for (int64_t p = 0; p < nq; p++)
            seen |= qc[p] == j;
        if (!seen)
            qc[nq++] = j;
    }
    const bool up = ri(0, 1) == 1;
    double at = 0.0;
    for (int64_t k = 0; k < nq; k++) {
        qv[k] = (double)ri(1, 3) * (up ? 1.0 : -1.0);
        if (g_have)
            at += qv[k] * g_x[qc[k]] * g_x[qc[k]];
    }
    for (int64_t k = 0; g_have && k < n; k++)
        at += val[k] * g_x[idx[k]];
    const double base = g_have ? round(at) : (double)ri(-20, 20);
    const double side = base + (double)ri(-2, 4) * (up ? 1.0 : -1.0);
    const double lo = up ? -INFINITY : side, hi = up ? side : INFINITY;
    const int64_t start[2] = {0, n};
    jaos_status st = jaos_add_rows(m, 1, &lo, &hi, n, start, idx, val);
    if (st == JAOS_OK)
        st = jaos_set_row_quadratic(m, nr, nq, qc, qc, qv);
    snprintf(what, len, "add-qrow %s over %" PRId64 " with %" PRId64
             " linear, side %g", up ? "convex" : "concave", nq, n, side);
    return st;
}

static const char *const EDIT[10] = {"cost", "bounds", "sides", "coefficient",
                                     "add-row", "add-col", "delete-row",
                                     "delete-col", "add-qrow", "drop-qrow"};

static int edit(jaos_model *m, bool mip, char *what, size_t len)
{
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    for (;;) {
        const int e = (int)ri(0, 9);
        if ((e == 6 && nr < 2) || (e == 7 && nc < 2) ||
            ((e == 4 || e == 8) && nr >= BIG - 1) || (e == 5 && nc >= BIG - 1) ||
            ((e == 2 || e == 3) && nr == 0) ||
            (e == 8 && quad_rows(m) >= 4) ||
            (e == 9 && quad_rows(m) == 0))
            continue;
        jaos_status st = JAOS_OK;
        if (e == 0) {
            const int64_t j = ri(0, nc - 1);
            const double c = (double)ri(-9, 9);
            st = jaos_set_col_cost(m, j, c);
            snprintf(what, len, "%s %" PRId64 " %g", EDIT[e], j, c);
        } else if (e == 1) {
            const int64_t j = ri(0, nc - 1);
            double lo, up;
            box(&lo, &up, g_have, g_x[j]);
            st = jaos_set_col_bounds(m, j, lo, up);
            snprintf(what, len, "%s %" PRId64 " %g %g", EDIT[e], j, lo, up);
        } else if (e == 2) {
            const int64_t i = ri(0, nr - 1);
            double lo, up;
            sides(&lo, &up, g_have, g_act[i]);
            const int c = curve(m, i);
            if (c > 0) {
                if (!isfinite(up))
                    up = lo;
                lo = -INFINITY;
            } else if (c < 0) {
                if (!isfinite(lo))
                    lo = up;
                up = INFINITY;
            }
            st = jaos_set_row_bounds(m, i, lo, up);
            snprintf(what, len, "%s %" PRId64 " %g %g", EDIT[e], i, lo, up);
        } else if (e == 3) {
            const int64_t i = ri(0, nr - 1), j = ri(0, nc - 1);
            const double v = (double)ri(-3, 3);
            st = jaos_set_coefficient(m, i, j, v);
            snprintf(what, len, "%s %" PRId64 " %" PRId64 " %g", EDIT[e], i, j,
                     v);
        } else if (e == 4) {
            int64_t idx[BIG];
            double val[BIG];
            int64_t n = 0;
            for (int64_t j = 0; j < nc; j++)
                if (ri(0, 9) < 4) {
                    idx[n] = j;
                    val[n] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
                    n++;
                }
            const int64_t start[2] = {0, n};
            double lo, up, at = 0.0;
            for (int64_t k = 0; g_have && k < n; k++)
                at += val[k] * g_x[idx[k]];
            sides(&lo, &up, g_have, at);
            st = jaos_add_rows(m, 1, &lo, &up, n, start, idx, val);
            snprintf(what, len, "%s with %" PRId64 " entries", EDIT[e], n);
        } else if (e == 5) {
            int64_t idx[BIG];
            double val[BIG];
            int64_t n = 0;
            for (int64_t i = 0; i < nr; i++)
                if (ri(0, 9) < 4) {
                    idx[n] = i;
                    val[n] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
                    n++;
                }
            const int64_t start[2] = {0, n};
            const double c = (double)ri(-9, 9);
            double lo, up;
            box(&lo, &up, false, 0.0);
            st = jaos_add_cols(m, 1, &c, &lo, &up, n, start, idx, val);
            if (st == JAOS_OK && mip && ri(0, 1) == 1)
                st = jaos_set_col_integer(m, nc, true);
            snprintf(what, len, "%s with %" PRId64 " entries", EDIT[e], n);
        } else if (e == 6) {
            const int64_t i = ri(0, nr - 1);
            st = jaos_delete_rows(m, 1, &i);
            snprintf(what, len, "%s %" PRId64, EDIT[e], i);
        } else if (e == 7) {
            const int64_t j = ri(0, nc - 1);
            st = jaos_delete_cols(m, 1, &j);
            snprintf(what, len, "%s %" PRId64, EDIT[e], j);
        } else if (e == 8) {
            st = add_qrow(m, what, len);
        } else {
            int64_t pick = ri(0, quad_rows(m) - 1), i = 0;
            for (; i < nr; i++)
                if (curve(m, i) != 0 && pick-- == 0)
                    break;
            const bool drop = ri(0, 1) == 1;
            st = drop ? jaos_delete_rows(m, 1, &i)
                      : jaos_set_row_quadratic(m, i, 0, nullptr, nullptr,
                                               nullptr);
            snprintf(what, len, "%s %" PRId64 " %s", EDIT[e], i,
                     drop ? "deleted" : "cleared");
        }
        return st == JAOS_OK ? e : -1;
    }
}

static int64_t broke[4];

static void fail(int p, int64_t idx, int64_t step, const char *how,
                 const char *what)
{
    broke[p]++;
    if (broke[p] <= 8)
        printf("P%d m%" PRId64 " edit %" PRId64 " (%s): %s\n", p, idx, step,
               how, what);
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: warmqc RUNS SEED EDITS MODE\n");
        return 2;
    }
    const bool mip = strcmp(argv[4], "miqcp") == 0;
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const int64_t edits = atoll(argv[3]);
    rng_state = 0x9E3779B97F4A7C15ull ^ seed ^ (mip ? 0x7171u : 0x7272u);
    int64_t verdicts[3] = {0, 0, 0}, kinds[10] = {0}, capped = 0, other = 0;
    int64_t bare = 0;
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK ||
            jaos_load_lp(m, g.nc, g.nr, JAOS_MINIMIZE, g.offset, g.cost, g.cl,
                         g.cu, g.rl, g.ru, g.nz, g.ap, g.ai, g.av) != JAOS_OK)
            return 1;
        if (mip)
            for (int64_t j = 0; j < g.nc; j++)
                if (ri(0, 1) == 1 && jaos_set_col_integer(m, j, true) != JAOS_OK)
                    return 1;
        char how[96];
        g_have = false;
        const int64_t first = ri(1, 2);
        for (int64_t k = 0; k < first; k++)
            if (add_qrow(m, how, sizeof how) != JAOS_OK)
                return 1;
        if (jaos_solve(m) != JAOS_OK) {
            fail(1, r, 0, "start", jaos_model_error(m));
            jaos_model_free(m);
            continue;
        }
        keep_point(m);
        for (int64_t s = 1; s <= edits; s++) {
            const int e = edit(m, mip, how, sizeof how);
            if (e < 0) {
                printf("edit failed: %s: %s\n", how, jaos_model_error(m));
                return 1;
            }
            kinds[e]++;
            jaos_model *c = nullptr;
            if (jaos_model_copy(m, &c) != JAOS_OK)
                return 1;
            const int64_t nc = jaos_num_col(m);
            for (int64_t j = 0; mip && j < nc; j++) {
                bool in = false;
                if (jaos_col_integer(m, j, &in) != JAOS_OK ||
                    (in && jaos_set_col_integer(c, j, true) != JAOS_OK))
                    return 1;
            }
            if (mip && (jaos_set_work_limit(m, 200000000) != JAOS_OK ||
                        jaos_set_work_limit(c, 200000000) != JAOS_OK))
                return 1;
            const jaos_status ws = jaos_solve(m), cs = jaos_solve(c);
            keep_point(m);
            if (ws == JAOS_OK && cs == JAOS_OK &&
                (jaos_status_of(m) == JAOS_SOLVE_WORK_LIMIT ||
                 jaos_status_of(c) == JAOS_SOLVE_WORK_LIMIT)) {
                capped++;
                jaos_model_free(c);
                continue;
            }
            char what[300];
            if (ws != JAOS_OK || cs != JAOS_OK) {
                snprintf(what, sizeof what, "warm %d cold %d: %s / %s", ws, cs,
                         jaos_model_error(m), jaos_model_error(c));
                fail(1, r, s, how, what);
                jaos_model_free(c);
                break;
            }
            const jaos_solve_status wst = jaos_status_of(m),
                                    cst = jaos_status_of(c);
            if (cst == JAOS_SOLVE_OPTIMAL) verdicts[0]++;
            else if (cst == JAOS_SOLVE_INFEASIBLE) verdicts[1]++;
            else if (cst == JAOS_SOLVE_UNBOUNDED) verdicts[2]++;
            else if (other++ < 8)
                printf("other m%" PRId64 " edit %" PRId64 " (%s): %s: %.200s\n",
                       r, s, how, jaos_solve_status_str(cst),
                       jaos_model_error(c));
            if (wst != cst) {
                snprintf(what, sizeof what, "warm %s, cold %s: %s / %s",
                         jaos_solve_status_str(wst), jaos_solve_status_str(cst),
                         jaos_model_error(m), jaos_model_error(c));
                fail(1, r, s, how, what);
            } else if (wst == JAOS_SOLVE_OPTIMAL) {
                double wo = 0.0, co = 0.0, x[BIG], y[BIG];
                jaos_check_report ck;
                if (jaos_objective(m, &wo) != JAOS_OK ||
                    jaos_objective(c, &co) != JAOS_OK ||
                    jaos_solution(m, x, nullptr, mip ? nullptr : y, nullptr) !=
                        JAOS_OK ||
                    (mip ? jaos_check_solution(m, x, nullptr, 1e-6, &ck)
                         : jaos_check_conic_solution(m, x, y, nullptr, 1e-6,
                                                     &ck)) != JAOS_OK)
                    return 1;
                bool integral = true;
                for (int64_t j = 0; mip && j < nc; j++) {
                    bool in = false;
                    if (jaos_col_integer(m, j, &in) != JAOS_OK)
                        return 1;
                    if (in && x[j] != round(x[j]))
                        integral = false;
                }
                if (fabs(wo - co) > 1e-6 * (1.0 + fabs(co)) || !integral ||
                    !ck.primal_feasible || (!mip && !ck.dual_feasible)) {
                    snprintf(what, sizeof what, "warm %.12g cold %.12g, primal "
                             "%d, dual %d (rows %.3g, duals %.3g)",
                             wo, co, ck.primal_feasible, ck.dual_feasible,
                             ck.max_row_violation_relative,
                             ck.max_dual_violation);
                    fail(2, r, s, how, what);
                }
            } else if (!mip && wst == JAOS_SOLVE_INFEASIBLE) {
                double y[BIG];
                jaos_certificate_report cr;
                if (jaos_certificate(m, y) != JAOS_OK && quad_rows(m) == 0)
                    fail(3, r, s, how, "no certificate and no quadratic row");
                else if (jaos_certificate(m, y) != JAOS_OK)
                    bare++;
                else if (jaos_check_conic_certificate(m, y, nullptr, 1e-7,
                                                      &cr) != JAOS_OK ||
                         !cr.certified)
                    fail(3, r, s, how, "a certificate the checker refuses");
            }
            jaos_model_free(c);
        }
        jaos_model_free(m);
    }
    printf("models %" PRId64 " edits %" PRId64 ": optimal %" PRId64
           " infeasible %" PRId64 " unbounded %" PRId64 " capped %" PRId64
           " other %" PRId64 "; infeasible with no certificate %" PRId64 "\n",
           runs, runs * edits, verdicts[0], verdicts[1], verdicts[2], capped,
           other, bare);
    printf("kinds");
    for (int e = 0; e < 10; e++)
        printf(" %s=%" PRId64, EDIT[e], kinds[e]);
    printf("\nP1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " broken total %" PRId64
           "\n", broke[1], broke[2], broke[3], broke[1] + broke[2] + broke[3]);
    return 0;
}
