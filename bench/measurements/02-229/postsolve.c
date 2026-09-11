/* What the caller gets back. Ten properties, every one read off the arrays
 * that built the model.
 *
 * Properties 1 to 8 are SPECS row 45: what presolve took out, postsolve
 * puts back in the caller's numbering, with its statuses and its duals.
 * Property 9 is row 116, direct load from arrays. Property 10 is row 51,
 * copy a model.
 *
 *   1. `row_activity[i]` is the row of the model as loaded, evaluated at
 *      the published point
 *   2. every published column value sits inside its own box
 *   3. every published activity sits inside its own row bounds
 *   4. the published objective is the cost row at the published point
 *   5. `col_dual[j]` is `cost[j]` less the column's own coefficients times
 *      the published row duals
 *   6. a status says where the value is: `AT_LOWER` at a finite lower
 *      bound, `AT_UPPER` at a finite upper one, `FREE` only where both
 *      bounds are infinite
 *   7. the published basis holds exactly one basic variable per row
 *   8. a basic variable carries a zero dual
 *   9. what `jaos_load_lp` took in, the getters give back, term for term
 *  10. `jaos_model_copy` gives back the same model, it answers the same,
 *      and a change to the copy leaves the model it came from alone
 *
 * `jaos_check` shares none of this, but it also reads neither `col_dual`
 * nor the two status arrays: it rebuilds the reduced costs from `row_dual`
 * and judges those. Properties 5, 6, 7 and 8 are about what postsolve
 * publishes, so nothing else in the tree reads them.
 *
 * The generator plants what presolve removes: an empty row, an empty
 * column, a fixed column, a singleton row, a copy of another row, and a
 * free column. Every row holds the zero point, so the model is feasible.
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

#define MAXC 10
#define MAXR 8

typedef struct {
    int64_t nc, nr, nz;
    jaos_obj_sense sense;
    double offset;
    double cost[MAXC], cl[MAXC], cu[MAXC];
    double rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    double dense[MAXR][MAXC];
} model;

static void gen(model *g)
{
    memset(g, 0, sizeof *g);
    g->nc = 4 + pick(MAXC - 3);
    g->nr = 3 + pick(MAXR - 2);
    g->sense = pick(2) ? JAOS_MINIMIZE : JAOS_MAXIMIZE;
    g->offset = (double)(pick(9) - 4);

    const int64_t empty_row = pick(g->nr);
    const int64_t single_row = (empty_row + 1) % g->nr;
    const int64_t copy_row = (empty_row + 2) % g->nr;
    const int64_t empty_col = pick(g->nc);
    const int64_t fixed_col = (empty_col + 1) % g->nc;
    const int64_t free_col = (empty_col + 2) % g->nc;
    const int64_t single_col = (empty_col + 3) % g->nc;

    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)(pick(11) - 5);
        if (j == fixed_col) {
            const double v = (double)(pick(5) - 2);
            g->cl[j] = v; g->cu[j] = v;
        } else if (j == free_col) {
            /* A cost on a free column sends most of these models to
             * unbounded, and an unbounded model publishes no point to
             * read. The column stays free, which is what presolve looks
             * at, and the model reaches an optimum far more often. */
            g->cost[j] = 0.0;
            g->cl[j] = -INFINITY; g->cu[j] = INFINITY;
        } else {
            switch (pick(5)) {
            case 0: g->cl[j] = 0.0; g->cu[j] = (double)(1 + pick(4)); break;
            case 1: g->cl[j] = 0.0; g->cu[j] = INFINITY; break;
            case 2: g->cl[j] = -INFINITY; g->cu[j] = 0.0; break;
            default: g->cl[j] = -(double)(1 + pick(3));
                     g->cu[j] = (double)(1 + pick(3)); break;
            }
        }
    }

    for (int64_t i = 0; i < g->nr; i++)
        for (int64_t j = 0; j < g->nc; j++)
            g->dense[i][j] = 0.0;

    for (int64_t i = 0; i < g->nr; i++) {
        if (i == empty_row)
            continue;
        if (i == single_row) {
            g->dense[i][single_col] = (double)(1 + pick(3));
            continue;
        }
        if (i == copy_row)
            continue;
        for (int64_t j = 0; j < g->nc; j++) {
            if (j == empty_col)
                continue;
            if (pick(10) < 4)
                continue;
            const double v = (double)(pick(7) - 3);
            if (v == 0.0)
                continue;
            g->dense[i][j] = v;
        }
    }
    /* A copy of the row above it, so presolve has a duplicate to drop. */
    for (int64_t j = 0; j < g->nc; j++)
        g->dense[copy_row][j] = g->dense[(copy_row + 1) % g->nr][j];

    int64_t nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->ap[j] = nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (g->dense[i][j] == 0.0)
                continue;
            g->ai[nz] = i; g->av[nz] = g->dense[i][j]; nz++;
        }
    }
    g->ap[g->nc] = nz;
    g->nz = nz;

    /* Every row holds the zero point, so the model is feasible, except
     * where a fixed column forces the row off it. Both bounds are kept
     * wide enough for the fixed column's own contribution. */
    for (int64_t i = 0; i < g->nr; i++) {
        const double slack = fabs(g->dense[i][fixed_col] * g->cl[fixed_col]);
        const double b = (double)(1 + pick(12)) + slack;
        switch (pick(5)) {
        case 0: g->rl[i] = -INFINITY; g->ru[i] = b; break;
        case 1: g->rl[i] = -b; g->ru[i] = INFINITY; break;
        case 2: g->rl[i] = -b; g->ru[i] = b; break;
        case 3: g->rl[i] = -slack; g->ru[i] = slack; break;
        default: g->rl[i] = -b; g->ru[i] = slack;
        }
    }
    /* The duplicate is only a duplicate while its bounds match. */
    g->rl[copy_row] = g->rl[(copy_row + 1) % g->nr];
    g->ru[copy_row] = g->ru[(copy_row + 1) % g->nr];
}

static void show(const model *g)
{
    printf("  nc=%" PRId64 " nr=%" PRId64 " sense=%s offset=%g\n", g->nc,
           g->nr, g->sense == JAOS_MINIMIZE ? "min" : "max", g->offset);
    for (int64_t j = 0; j < g->nc; j++)
        printf("  col %" PRId64 " cost=%g lo=%g up=%g\n", j, g->cost[j],
               g->cl[j], g->cu[j]);
    for (int64_t i = 0; i < g->nr; i++)
        printf("  row %" PRId64 " lo=%g up=%g\n", i, g->rl[i], g->ru[i]);
    for (int64_t j = 0; j < g->nc; j++)
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++)
            printf("  a[%" PRId64 ",%" PRId64 "]=%g\n", g->ai[k], j, g->av[k]);
}

static jaos_status build(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK)
        return rc;
    rc = jaos_load_lp(m, g->nc, g->nr, g->sense, g->offset, g->cost, g->cl,
                      g->cu, g->rl, g->ru, g->nz, g->ap, g->ai, g->av);
    if (rc == JAOS_OK)
        rc = jaos_set_work_limit(m, 20000000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

static const char *st_str(jaos_basis_status s)
{
    switch (s) {
    case JAOS_BASIS_BASIC:    return "basic";
    case JAOS_BASIS_AT_LOWER: return "lower";
    case JAOS_BASIS_AT_UPPER: return "upper";
    case JAOS_BASIS_FREE:     return "free";
    }
    return "?";
}

constexpr double TOL = 1e-6;

static bool near(double a, double b, double scale)
{
    return fabs(a - b) <= TOL * (1.0 + fabs(scale));
}

/* 9. what `jaos_load_lp` took in, the model gives back, term for term.
 * Reads the whole model through the public getters and compares it to the
 * arrays that built it. Returns the number of disagreements. */
static int64_t readback(const model *g, const jaos_model *m, const char *tag,
                        int64_t t)
{
    int64_t n = 0;
    jaos_obj_sense sense;
    double offset = 0.0;
    if (jaos_objective_sense(m, &sense) != JAOS_OK || sense != g->sense) {
        printf("%s t=%" PRId64 " the sense came back wrong\n", tag, t);
        n++;
    }
    if (jaos_objective_offset(m, &offset) != JAOS_OK || offset != g->offset) {
        printf("%s t=%" PRId64 " the offset came back %g against %g\n", tag, t,
               offset, g->offset);
        n++;
    }
    for (int64_t j = 0; j < g->nc; j++) {
        double c = 0.0, lo = 0.0, up = 0.0;
        if (jaos_col_cost(m, j, &c) != JAOS_OK || c != g->cost[j]) {
            printf("%s t=%" PRId64 " column %" PRId64 " cost %g against %g\n",
                   tag, t, j, c, g->cost[j]);
            n++;
        }
        if (jaos_col_bounds(m, j, &lo, &up) != JAOS_OK ||
            lo != g->cl[j] || up != g->cu[j]) {
            printf("%s t=%" PRId64 " column %" PRId64 " box [%g, %g] against "
                   "[%g, %g]\n", tag, t, j, lo, up, g->cl[j], g->cu[j]);
            n++;
        }
        int64_t cnt = 0;
        int64_t idx[MAXR];
        double val[MAXR];
        if (jaos_col_entries(m, j, &cnt, idx, val) != JAOS_OK ||
            cnt != g->ap[j + 1] - g->ap[j]) {
            printf("%s t=%" PRId64 " column %" PRId64 " holds %" PRId64
                   " entries against %" PRId64 "\n", tag, t, j, cnt,
                   g->ap[j + 1] - g->ap[j]);
            n++;
            continue;
        }
        for (int64_t k = 0; k < cnt; k++)
            if (idx[k] != g->ai[g->ap[j] + k] ||
                val[k] != g->av[g->ap[j] + k]) {
                printf("%s t=%" PRId64 " column %" PRId64 " entry %" PRId64
                       " is a[%" PRId64 "]=%g against a[%" PRId64 "]=%g\n",
                       tag, t, j, k, idx[k], val[k], g->ai[g->ap[j] + k],
                       g->av[g->ap[j] + k]);
                n++;
            }
    }
    for (int64_t i = 0; i < g->nr; i++) {
        double lo = 0.0, up = 0.0;
        if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK ||
            lo != g->rl[i] || up != g->ru[i]) {
            printf("%s t=%" PRId64 " row %" PRId64 " bounds [%g, %g] against "
                   "[%g, %g]\n", tag, t, i, lo, up, g->rl[i], g->ru[i]);
            n++;
        }
    }
    return n;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? strtoll(argv[1], nullptr, 10) : 20000;
    rs = argc > 2 ? strtoull(argv[2], nullptr, 10) : 88172645463325252ULL;
    const char *dump = argc > 3 && argv[3][0] != '\0' ? argv[3] : nullptr;

    int64_t checked = 0, skipped = 0, reduced = 0;
    int64_t bad[11] = {0};
    int64_t seen[4] = {0}, rseen[4] = {0};
    int64_t cut_row = 0, cut_col = 0;
    int64_t infeas = 0, unbnd = 0, other = 0, copies = 0;

    for (int64_t t = 0; t < runs; t++) {
        model g;
        gen(&g);

        jaos_model *m = nullptr;
        if (build(&g, &m) != JAOS_OK) { skipped++; continue; }

        /* 9. direct load from arrays: what went in comes back */
        bad[9] += readback(&g, m, "LOAD", t);
        if (jaos_solve(m) != JAOS_OK) { jaos_model_free(m); skipped++; continue; }
        const jaos_solve_status ss = jaos_status_of(m);
        if (ss != JAOS_SOLVE_OPTIMAL) {
            infeas += ss == JAOS_SOLVE_INFEASIBLE;
            unbnd += ss == JAOS_SOLVE_UNBOUNDED;
            other += ss != JAOS_SOLVE_INFEASIBLE && ss != JAOS_SOLVE_UNBOUNDED;
            jaos_model_free(m); skipped++; continue;
        }

        double x[MAXC], act[MAXR], y[MAXR], d[MAXC];
        if (jaos_solution(m, x, act, y, d) != JAOS_OK) {
            jaos_model_free(m); skipped++; continue;
        }
        jaos_basis_status cs[MAXC], rst[MAXR];
        if (jaos_basis(m, cs, rst) != JAOS_OK) {
            jaos_model_free(m); skipped++; continue;
        }
        double obj = 0.0;
        if (jaos_objective(m, &obj) != JAOS_OK) {
            jaos_model_free(m); skipped++; continue;
        }

        jaos_presolve_report pr;
        if (jaos_presolve_result(m, &pr) == JAOS_OK &&
            (pr.num_row < g.nr || pr.num_col < g.nc)) {
            reduced++;
            cut_row += g.nr - pr.num_row;
            cut_col += g.nc - pr.num_col;
        }
        checked++;

        bool wrong = false;

        /* 1. the activity is the row of the model as loaded */
        for (int64_t i = 0; i < g.nr; i++) {
            double s = 0.0;
            for (int64_t j = 0; j < g.nc; j++)
                s += g.dense[i][j] * x[j];
            if (!near(s, act[i], s)) {
                printf("ACT t=%" PRId64 " row %" PRId64 " published %.17g and "
                       "the model gives %.17g\n", t, i, act[i], s);
                bad[1]++; wrong = true;
            }
        }

        /* 2. every column value sits inside its own box */
        for (int64_t j = 0; j < g.nc; j++) {
            if (x[j] < g.cl[j] - TOL * (1.0 + fabs(g.cl[j])) ||
                x[j] > g.cu[j] + TOL * (1.0 + fabs(g.cu[j]))) {
                printf("BOX t=%" PRId64 " column %" PRId64 " is %.17g outside "
                       "[%g, %g]\n", t, j, x[j], g.cl[j], g.cu[j]);
                bad[2]++; wrong = true;
            }
        }

        /* 3. every activity sits inside its own row bounds */
        for (int64_t i = 0; i < g.nr; i++) {
            if (act[i] < g.rl[i] - TOL * (1.0 + fabs(g.rl[i])) ||
                act[i] > g.ru[i] + TOL * (1.0 + fabs(g.ru[i]))) {
                printf("ROW t=%" PRId64 " row %" PRId64 " is %.17g outside "
                       "[%g, %g]\n", t, i, act[i], g.rl[i], g.ru[i]);
                bad[3]++; wrong = true;
            }
        }

        /* 4. the objective is the cost row at the published point */
        double co = g.offset;
        for (int64_t j = 0; j < g.nc; j++)
            co += g.cost[j] * x[j];
        if (!near(co, obj, co)) {
            printf("OBJ t=%" PRId64 " published %.17g and the cost row gives "
                   "%.17g\n", t, obj, co);
            bad[4]++; wrong = true;
        }

        /* 5. the reduced cost is the column's own residue */
        for (int64_t j = 0; j < g.nc; j++) {
            double s = g.cost[j];
            for (int64_t i = 0; i < g.nr; i++)
                s -= g.dense[i][j] * y[i];
            if (!near(s, d[j], s)) {
                printf("RC t=%" PRId64 " column %" PRId64 " published %.17g "
                       "and the model gives %.17g\n", t, j, d[j], s);
                bad[5]++; wrong = true;
            }
        }

        /* 6. a status says where the value is */
        for (int64_t j = 0; j < g.nc; j++) {
            seen[(int)cs[j]]++;
            const bool ok =
                cs[j] == JAOS_BASIS_BASIC ? true :
                cs[j] == JAOS_BASIS_AT_LOWER ? isfinite(g.cl[j]) &&
                                               near(x[j], g.cl[j], g.cl[j]) :
                cs[j] == JAOS_BASIS_AT_UPPER ? isfinite(g.cu[j]) &&
                                               near(x[j], g.cu[j], g.cu[j]) :
                !isfinite(g.cl[j]) && !isfinite(g.cu[j]);
            if (!ok) {
                printf("CST t=%" PRId64 " column %" PRId64 " says %s at %.17g "
                       "in [%g, %g]\n", t, j, st_str(cs[j]), x[j], g.cl[j],
                       g.cu[j]);
                bad[6]++; wrong = true;
            }
        }
        for (int64_t i = 0; i < g.nr; i++) {
            rseen[(int)rst[i]]++;
            const bool ok =
                rst[i] == JAOS_BASIS_BASIC ? true :
                rst[i] == JAOS_BASIS_AT_LOWER ? isfinite(g.rl[i]) &&
                                                near(act[i], g.rl[i], g.rl[i]) :
                rst[i] == JAOS_BASIS_AT_UPPER ? isfinite(g.ru[i]) &&
                                                near(act[i], g.ru[i], g.ru[i]) :
                !isfinite(g.rl[i]) && !isfinite(g.ru[i]);
            if (!ok) {
                printf("RST t=%" PRId64 " row %" PRId64 " says %s at %.17g in "
                       "[%g, %g]\n", t, i, st_str(rst[i]), act[i], g.rl[i],
                       g.ru[i]);
                bad[6]++; wrong = true;
            }
        }

        /* 7. one basic variable per row */
        int64_t nbasic = 0;
        for (int64_t j = 0; j < g.nc; j++) nbasic += cs[j] == JAOS_BASIS_BASIC;
        for (int64_t i = 0; i < g.nr; i++) nbasic += rst[i] == JAOS_BASIS_BASIC;
        if (nbasic != g.nr) {
            printf("CNT t=%" PRId64 " the basis holds %" PRId64 " basic of "
                   "%" PRId64 " rows\n", t, nbasic, g.nr);
            bad[7]++; wrong = true;
        }

        /* 8. a basic variable carries a zero dual */
        for (int64_t j = 0; j < g.nc; j++)
            if (cs[j] == JAOS_BASIS_BASIC && fabs(d[j]) > TOL) {
                printf("CSL t=%" PRId64 " column %" PRId64 " is basic and its "
                       "dual is %.17g\n", t, j, d[j]);
                bad[8]++; wrong = true;
            }
        for (int64_t i = 0; i < g.nr; i++)
            if (rst[i] == JAOS_BASIS_BASIC && fabs(y[i]) > TOL) {
                printf("RSL t=%" PRId64 " row %" PRId64 " is basic and its "
                       "dual is %.17g\n", t, i, y[i]);
                bad[8]++; wrong = true;
            }

        /* 10. a copy is the same model, it answers the same, and changing
         * it leaves the model it came from alone */
        jaos_model *cp = nullptr;
        if (jaos_model_copy(m, &cp) != JAOS_OK) {
            printf("CPY t=%" PRId64 " the copy failed\n", t);
            bad[10]++; wrong = true;
        } else {
            copies++;
            bad[10] += readback(&g, cp, "COPY", t);
            if (jaos_set_work_limit(cp, 20000000) == JAOS_OK &&
                jaos_solve(cp) == JAOS_OK &&
                jaos_status_of(cp) == JAOS_SOLVE_OPTIMAL) {
                double oc = 0.0;
                (void)jaos_objective(cp, &oc);
                if (!near(oc, obj, obj)) {
                    printf("CPO t=%" PRId64 " the copy answers %.17g against "
                           "%.17g\n", t, oc, obj);
                    bad[10]++; wrong = true;
                }
            } else {
                printf("CPS t=%" PRId64 " the copy did not reach the optimum "
                       "the model reached\n", t);
                bad[10]++; wrong = true;
            }
            const double before = g.cost[0] + 17.0;
            if (jaos_set_col_cost(cp, 0, before) == JAOS_OK) {
                double back = 0.0;
                if (jaos_col_cost(m, 0, &back) != JAOS_OK ||
                    back != g.cost[0]) {
                    printf("CPI t=%" PRId64 " a change to the copy moved the "
                           "model's own cost to %g\n", t, back);
                    bad[10]++; wrong = true;
                }
            }
            jaos_model_free(cp);
        }

        if (wrong && dump != nullptr)
            show(&g);
        jaos_model_free(m);
    }

    int64_t total = 0;
    for (int k = 1; k <= 10; k++) total += bad[k];

    printf("checked %" PRId64 ", skipped %" PRId64 " (infeasible %" PRId64
           ", unbounded %" PRId64 ", other %" PRId64 "), presolve reduced "
           "%" PRId64 " of them\n", checked, skipped, infeas, unbnd, other,
           reduced);
    printf("rows removed %" PRId64 ", columns removed %" PRId64 "\n",
           cut_row, cut_col);
    printf("column statuses basic %" PRId64 " lower %" PRId64 " upper "
           "%" PRId64 " free %" PRId64 "\n", seen[0], seen[1], seen[2],
           seen[3]);
    printf("row statuses basic %" PRId64 " lower %" PRId64 " upper %" PRId64
           " free %" PRId64 "\n", rseen[0], rseen[1], rseen[2], rseen[3]);
    printf("activity %" PRId64 " box %" PRId64 " rowbound %" PRId64
           " objective %" PRId64 " redcost %" PRId64 " status %" PRId64
           " count %" PRId64 " slack %" PRId64 "\n",
           bad[1], bad[2], bad[3], bad[4], bad[5], bad[6], bad[7], bad[8]);
    printf("load %" PRId64 ", copy %" PRId64 " over %" PRId64 " copies\n",
           bad[9], bad[10], copies);
    printf("wrong %" PRId64 "\n", total);
    return total == 0 ? 0 : 1;
}
