/* The reading behind 02-239: a point another solver produced, judged.
 *
 * The generator is 02-237's: LPs of eight to twenty-four columns and six
 * to sixteen rows, integer data, a planted integer point z inside the
 * boxes with each row's bounds set around it. z is a feasible point no
 * solver produced, and the optimum x* with its duals y* is JAOS's.
 *
 * The harness reads the checker's arithmetic itself, in extended
 * precision and sharing no code with it: activities, bound and row
 * violations, the reduced costs, the sign condition on every multiplier
 * against the side the value rests on, the dual objective and the
 * complementary-slackness gap, with the checker's documented conventions
 * (a row side the model leaves open is the side its columns' boxes
 * imply; a multiplier below the tolerance is negligible).
 *
 * Properties:
 *   P1 the point file and the duals file JAOS writes read back equal, the
 *      checker accepts (x*, y*) on both halves, its primal objective is
 *      the solve's, and a second check gives the same report
 *   P2 the same point in five other solvers' shapes reads back equal:
 *      Gurobi, MIPLIB and SCIP with the zero columns left out, HiGHS with
 *      the duals in its second Rows block, CPLEX XML; the duals from the
 *      HiGHS and CPLEX files read back equal
 *   P3 the planted point z, which no solver produced, is judged primal
 *      feasible with both violations exactly zero and the objective
 *      exactly the integer sum
 *   P4 a point moved off x* or off z is judged by the numbers the
 *      harness reads: max_col_violation and max_row_violation agree to
 *      1e-9, and primal_feasible agrees wherever the margin to the
 *      tolerance is clear
 *   P5 the dual half agrees with the harness's reading on (x*, y*),
 *      (x*, -y* on one row), (x*, 2y*) and (z, y*): max_dual_violation,
 *      dual_objective and objective_gap to 1e-9, dual_feasible wherever
 *      the margin is clear
 *   P6 a file that names a column twice, leaves one out, or names one
 *      the model has not got is refused; the MIPLIB shape with a column
 *      left out reads it as zero
 *
 * Usage: point RUNS SEED DIR [EVERY]
 *   Every EVERY-th model is dumped to DIR as m<idx>.mps, m<idx>.pt (x* in
 *   a foreign shape, rotating), m<idx>.duals (plain), m<idx>.bad.pt (z
 *   with a column outside its box), for the CLI chain in point.sh.
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
#define TOL 1e-7

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool maximise;
    double offset;
    int64_t z[MAXC];
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->maximise = ri(0, 1) == 1;
    g->offset = (double)ri(-3, 3);
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)ri(-9, 9);
        g->cl[j] = 0.0;
        g->cu[j] = (double)ri(1, 10);
        g->z[j] = ri(0, (int64_t)g->cu[j]);
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
            act[i] += (double)a * (double)g->z[j];
        }
    }
    g->ap[g->nc] = g->nz;
    for (int64_t i = 0; i < g->nr; i++) {
        const int64_t kind = ri(0, 9);
        if (kind < 3) {
            g->rl[i] = g->ru[i] = act[i];
        } else if (kind < 6) {
            g->rl[i] = -INFINITY;
            g->ru[i] = act[i] + (double)ri(0, 3);
        } else if (kind < 9) {
            g->rl[i] = act[i] - (double)ri(0, 3);
            g->ru[i] = INFINITY;
        } else {
            g->rl[i] = act[i] - (double)ri(0, 2);
            g->ru[i] = act[i] + (double)ri(0, 2);
        }
    }
}

static void name_of(char *out, size_t cap, bool is_col, int64_t k)
{
    snprintf(out, cap, "%s%" PRId64, is_col ? "X" : "R", k + 1);
}

static jaos_model *load(const gen *g)
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
    char nm[32];
    for (int64_t j = 0; j < g->nc; j++) {
        name_of(nm, sizeof nm, true, j);
        if (jaos_set_col_name(m, j, nm) != JAOS_OK) abort();
    }
    for (int64_t i = 0; i < g->nr; i++) {
        name_of(nm, sizeof nm, false, i);
        if (jaos_set_row_name(m, i, nm) != JAOS_OK) abort();
    }
    return m;
}

/* The harness's own reading of a point and, when y is not null, of its
 * duals, with the checker's conventions. */
typedef struct {
    long double col_viol, row_viol, pobj;
    long double dual_viol, dobj, gap;
    bool primal_feasible, dual_feasible;
} reading;

static long double outside(long double v, double lo, double hi)
{
    long double d = 0.0L;
    if (isfinite(lo) && v < (long double)lo) d = (long double)lo - v;
    if (isfinite(hi) && v > (long double)hi && v - (long double)hi > d) d = v - (long double)hi;
    return d;
}

typedef struct { long double viol, dobj, cs; } side_out;

static void sign_condition(long double v, long double lo, long double hi,
                           long double w, long double scale, bool lo_imp,
                           bool hi_imp, side_out *o)
{
    const long double window = (long double)TOL * scale;
    const bool at_lo = isfinite((double)lo) && v <= lo + window;
    const bool at_hi = isfinite((double)hi) && v >= hi - window;
    const bool negligible = fabsl(w) <= (long double)TOL;
    o->viol = 0.0L;
    if (w > 0.0L) {
        if (!isfinite((double)lo)) abort();
        o->dobj += w * lo;
        if (!lo_imp) o->cs += w * (v - lo);
        o->viol = (negligible || at_lo) ? 0.0L : w;
    } else if (w < 0.0L) {
        if (!isfinite((double)hi)) abort();
        o->dobj += w * hi;
        if (!hi_imp) o->cs += w * (v - hi);
        o->viol = (negligible || at_hi) ? 0.0L : -w;
    }
}

static void read_point(const gen *g, const double *x, const double *y, reading *r)
{
    memset(r, 0, sizeof *r);
    const long double sigma = g->maximise ? -1.0L : 1.0L;
    long double act[MAXR], traffic[MAXR], rlo[MAXR], rup[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = traffic[i] = rlo[i] = rup[i] = 0.0L;
    r->pobj = (long double)g->offset;
    for (int64_t j = 0; j < g->nc; j++) {
        r->pobj += (long double)g->cost[j] * (long double)x[j];
        const long double v = outside((long double)x[j], g->cl[j], g->cu[j]);
        if (v > r->col_viol) r->col_viol = v;
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++) {
            const long double t = (long double)g->av[p] * (long double)x[j];
            act[g->ai[p]] += t;
            traffic[g->ai[p]] += fabsl(t);
            rlo[g->ai[p]] += (long double)g->av[p] * (g->av[p] > 0.0 ? g->cl[j] : g->cu[j]);
            rup[g->ai[p]] += (long double)g->av[p] * (g->av[p] > 0.0 ? g->cu[j] : g->cl[j]);
        }
    }
    for (int64_t i = 0; i < g->nr; i++) {
        const long double v = outside(act[i], g->rl[i], g->ru[i]);
        if (v > r->row_viol) r->row_viol = v;
    }
    r->primal_feasible = r->col_viol <= (long double)TOL && r->row_viol <= (long double)TOL;
    if (y == nullptr) return;

    side_out o = {0};
    o.dobj = sigma * (long double)g->offset;
    long double viol = 0.0L;
    for (int64_t i = 0; i < g->nr; i++) {
        long double lo = (long double)g->rl[i], hi = (long double)g->ru[i];
        bool lo_imp = false, hi_imp = false;
        if (!isfinite(g->rl[i])) { lo = rlo[i]; lo_imp = true; }
        if (!isfinite(g->ru[i])) { hi = rup[i]; hi_imp = true; }
        const long double scale = traffic[i] > 1.0L ? traffic[i] : 1.0L;
        sign_condition(act[i], lo, hi, sigma * (long double)y[i], scale, lo_imp, hi_imp, &o);
        if (o.viol > viol) viol = o.viol;
    }
    for (int64_t j = 0; j < g->nc; j++) {
        long double d = (long double)g->cost[j];
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++)
            d -= (long double)g->av[p] * (long double)y[g->ai[p]];
        const long double ax = fabsl((long double)x[j]);
        sign_condition((long double)x[j], (long double)g->cl[j], (long double)g->cu[j],
                       sigma * d, ax > 1.0L ? ax : 1.0L, false, false, &o);
        if (o.viol > viol) viol = o.viol;
    }
    r->dual_viol = viol;
    r->dobj = sigma * o.dobj;
    r->gap = fabsl(o.cs) / (1.0L + fabsl(r->pobj) + fabsl(r->dobj));
    r->dual_feasible = viol <= (long double)TOL && r->gap <= (long double)TOL;
}

static int64_t broke[8], ambiguous;

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

static bool near(long double mine, double theirs)
{
    return fabsl(mine - (long double)theirs) <= 1e-9L * (1.0L + fabsl((long double)theirs));
}

static bool clear_of(long double v)
{
    return fabsl(v - (long double)TOL) > 1e-9L;
}

/* P4/P5: compare the checker's report with the harness's reading. */
static void compare(const gen *g, jaos_model *m, const double *x,
                    const double *y, int64_t idx, const char *tag, int p)
{
    jaos_check_report rep;
    if (jaos_check_solution(m, x, y, TOL, &rep) != JAOS_OK) {
        fail(p, idx, "check-call-failed", 0.0, 0.0);
        return;
    }
    reading r;
    read_point(g, x, y, &r);
    char what[96];
    if (!near(r.col_viol, rep.max_col_violation)) {
        snprintf(what, sizeof what, "%s-col-violation-differs", tag);
        fail(p, idx, what, (double)r.col_viol, rep.max_col_violation);
    }
    if (!near(r.row_viol, rep.max_row_violation)) {
        snprintf(what, sizeof what, "%s-row-violation-differs", tag);
        fail(p, idx, what, (double)r.row_viol, rep.max_row_violation);
    }
    if (!near(r.pobj, rep.primal_objective)) {
        snprintf(what, sizeof what, "%s-primal-objective-differs", tag);
        fail(p, idx, what, (double)r.pobj, rep.primal_objective);
    }
    if (clear_of(r.col_viol) && clear_of(r.row_viol) &&
        r.primal_feasible != rep.primal_feasible) {
        snprintf(what, sizeof what, "%s-primal-verdict-differs", tag);
        fail(p, idx, what, (double)r.col_viol, (double)r.row_viol);
    } else if (!(clear_of(r.col_viol) && clear_of(r.row_viol))) {
        ambiguous++;
    }
    if (y == nullptr) {
        if (rep.checked_duals) fail(p, idx, "duals-checked-without-duals", 0.0, 0.0);
        return;
    }
    if (!rep.checked_duals) {
        fail(p, idx, "duals-not-checked", 0.0, 0.0);
        return;
    }
    if (!near(r.dual_viol, rep.max_dual_violation)) {
        snprintf(what, sizeof what, "%s-dual-violation-differs", tag);
        fail(p, idx, what, (double)r.dual_viol, rep.max_dual_violation);
    }
    if (!near(r.dobj, rep.dual_objective)) {
        snprintf(what, sizeof what, "%s-dual-objective-differs", tag);
        fail(p, idx, what, (double)r.dobj, rep.dual_objective);
    }
    if (!near(r.gap, rep.objective_gap)) {
        snprintf(what, sizeof what, "%s-gap-differs", tag);
        fail(p, idx, what, (double)r.gap, rep.objective_gap);
    }
    if (!rep.gap_certified) {
        snprintf(what, sizeof what, "%s-gap-not-certified", tag);
        fail(p, idx, what, (double)rep.dropped_terms, rep.max_dropped_multiplier);
    }
    if (clear_of(r.dual_viol) && clear_of(r.gap)) {
        if (r.dual_feasible != rep.dual_feasible) {
            snprintf(what, sizeof what, "%s-dual-verdict-differs", tag);
            fail(p, idx, what, (double)r.dual_viol, (double)r.gap);
        }
    } else {
        ambiguous++;
    }
}

/* Foreign shapes, written from x and y by the harness. */
enum shape { SH_GUROBI, SH_MIPLIB, SH_SCIP, SH_HIGHS, SH_CPLEX, SH_COUNT };
static const char *const SHAPE_NAME[SH_COUNT] = {"gurobi", "miplib", "scip", "highs", "cplex"};

static void write_shape(const char *path, const gen *g, const double *x,
                        const double *y, enum shape sh)
{
    FILE *f = fopen(path, "w");
    if (f == nullptr) abort();
    int64_t order[MAXC], rorder[MAXR];
    for (int64_t j = 0; j < g->nc; j++) order[j] = j;
    for (int64_t j = g->nc - 1; j > 0; j--) {
        const int64_t t = ri(0, j);
        const int64_t s = order[j]; order[j] = order[t]; order[t] = s;
    }
    for (int64_t i = 0; i < g->nr; i++) rorder[i] = i;
    for (int64_t i = g->nr - 1; i > 0; i--) {
        const int64_t t = ri(0, i);
        const int64_t s = rorder[i]; rorder[i] = rorder[t]; rorder[t] = s;
    }
    long double obj = (long double)g->offset;
    for (int64_t j = 0; j < g->nc; j++) obj += (long double)g->cost[j] * (long double)x[j];
    char cn[32], rn[32];
    switch (sh) {
    case SH_GUROBI:
        fprintf(f, "# Solution for model m\n# Objective value = %.17Lg\n", obj);
        for (int64_t k = 0; k < g->nc; k++) {
            const int64_t j = order[k];
            name_of(cn, sizeof cn, true, j);
            if (ri(0, 5) == 0) fprintf(f, "\n");
            fprintf(f, "%s%s%.17g%s\n", cn, ri(0, 1) ? " " : "\t", x[j],
                    ri(0, 3) == 0 ? "   # a remark" : "");
        }
        break;
    case SH_MIPLIB:
        fprintf(f, "=obj= %.17Lg\n", obj);
        for (int64_t k = 0; k < g->nc; k++) {
            const int64_t j = order[k];
            if (x[j] == 0.0) continue;
            name_of(cn, sizeof cn, true, j);
            fprintf(f, "%-12s %.17g\n", cn, x[j]);
        }
        break;
    case SH_SCIP:
        fprintf(f, "solution status: optimal solution found\n"
                   "objective value:                                   %.17Lg\n", obj);
        for (int64_t k = 0; k < g->nc; k++) {
            const int64_t j = order[k];
            if (x[j] == 0.0) continue;
            name_of(cn, sizeof cn, true, j);
            fprintf(f, "%-50s %20.17g \t(obj:%g)\n", cn, x[j], g->cost[j]);
        }
        break;
    case SH_HIGHS:
        fprintf(f, "Model status\nOptimal\n\n# Primal solution values\nFeasible\n"
                   "Objective %.17Lg\n# Columns %" PRId64 "\n", obj, g->nc);
        for (int64_t k = 0; k < g->nc; k++) {
            name_of(cn, sizeof cn, true, order[k]);
            fprintf(f, "%s %.17g\n", cn, x[order[k]]);
        }
        fprintf(f, "# Rows %" PRId64 "\n", g->nr);
        for (int64_t i = 0; i < g->nr; i++) {
            name_of(rn, sizeof rn, false, i);
            fprintf(f, "%s 0\n", rn);
        }
        fprintf(f, "\n# Dual solution values\nFeasible\n# Columns %" PRId64 "\n", g->nc);
        for (int64_t j = 0; j < g->nc; j++) {
            name_of(cn, sizeof cn, true, j);
            fprintf(f, "%s 0\n", cn);
        }
        fprintf(f, "# Rows %" PRId64 "\n", g->nr);
        for (int64_t k = 0; k < g->nr; k++) {
            name_of(rn, sizeof rn, false, rorder[k]);
            fprintf(f, "%s %.17g\n", rn, y[rorder[k]]);
        }
        fprintf(f, "\n# Basis\nHiGHS v1\nValid\n");
        break;
    case SH_CPLEX:
        fprintf(f, "<?xml version = \"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                   "<CPLEXSolution version=\"1.2\">\n"
                   " <header problemName=\"m\" objectiveValue=\"%.17Lg\" solutionStatusString=\"optimal\"/>\n"
                   " <linearConstraints>\n", obj);
        for (int64_t k = 0; k < g->nr; k++) {
            const int64_t i = rorder[k];
            name_of(rn, sizeof rn, false, i);
            fprintf(f, "  <constraint name=\"%s\" index=\"%" PRId64 "\" slack=\"0\" dual=\"%.17g\"/>\n", rn, i, y[i]);
        }
        fprintf(f, " </linearConstraints>\n <variables>\n");
        for (int64_t k = 0; k < g->nc; k++) {
            const int64_t j = order[k];
            name_of(cn, sizeof cn, true, j);
            fprintf(f, "  <variable name=\"%s\" index=\"%" PRId64 "\" value=\"%.17g\" reducedCost=\"0\"/>\n", cn, j, x[j]);
        }
        fprintf(f, " </variables>\n</CPLEXSolution>\n");
        break;
    default:
        abort();
    }
    fclose(f);
}

static bool same_values(const double *a, const double *b, int64_t n)
{
    for (int64_t k = 0; k < n; k++)
        if (a[k] != b[k]) return false;
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: point RUNS SEED DIR [EVERY]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t every = argc > 4 ? atoll(argv[4]) : 0;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, shapes_read = 0, foreign_points = 0,
            foreign_refused = 0, dual_points = 0, dual_refused = 0,
            dumped = 0;
    char path[4096], path2[4096];

    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }
        optimal++;
        double x[MAXC], y[MAXR], obj, rx[MAXC], ry[MAXR];
        if (jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK) abort();
        if (jaos_objective(m, &obj) != JAOS_OK) abort();

        /* P1 */
        snprintf(path, sizeof path, "%s/own-%" PRId64 ".pt", dir, idx);
        snprintf(path2, sizeof path2, "%s/own-%" PRId64 ".duals", dir, idx);
        if (jaos_write_point(m, path) != JAOS_OK) fail(1, idx, "point-not-written", 0.0, 0.0);
        else if (jaos_read_point(m, path, rx) != JAOS_OK) fail(1, idx, "point-not-read", 0.0, 0.0);
        else if (!same_values(rx, x, g.nc)) fail(1, idx, "point-differs", 0.0, 0.0);
        if (jaos_write_duals(m, path2) != JAOS_OK) fail(1, idx, "duals-not-written", 0.0, 0.0);
        else if (jaos_read_duals(m, path2, ry) != JAOS_OK) fail(1, idx, "duals-not-read", 0.0, 0.0);
        else if (!same_values(ry, y, g.nr)) fail(1, idx, "duals-differ", 0.0, 0.0);
        {
            jaos_check_report a, b;
            if (jaos_check_solution(m, x, y, TOL, &a) != JAOS_OK ||
                jaos_check_solution(m, x, y, TOL, &b) != JAOS_OK)
                fail(1, idx, "check-failed", 0.0, 0.0);
            else {
                if (!a.primal_feasible || !a.dual_feasible || !a.checked_duals || !a.gap_certified)
                    fail(1, idx, "own-answer-not-accepted", a.max_row_violation, a.max_dual_violation);
                if (!near((long double)obj, a.primal_objective))
                    fail(1, idx, "own-objective-differs", obj, a.primal_objective);
                if (memcmp(&a, &b, sizeof a) != 0)
                    fail(1, idx, "second-check-differs", 0.0, 0.0);
            }
        }

        /* P2 */
        for (int sh = 0; sh < SH_COUNT; sh++) {
            snprintf(path, sizeof path, "%s/%s-%" PRId64 ".sol", dir, SHAPE_NAME[sh], idx);
            write_shape(path, &g, x, y, (enum shape)sh);
            char what[64];
            if (jaos_read_point(m, path, rx) != JAOS_OK) {
                snprintf(what, sizeof what, "%s-point-refused", SHAPE_NAME[sh]);
                fail(2, idx, what, 0.0, 0.0);
                if (broke[2] <= 5) printf("   %s\n", jaos_model_error(m));
            } else if (!same_values(rx, x, g.nc)) {
                snprintf(what, sizeof what, "%s-point-differs", SHAPE_NAME[sh]);
                fail(2, idx, what, 0.0, 0.0);
            } else {
                shapes_read++;
            }
            if (sh == SH_HIGHS || sh == SH_CPLEX) {
                if (jaos_read_duals(m, path, ry) != JAOS_OK) {
                    snprintf(what, sizeof what, "%s-duals-refused", SHAPE_NAME[sh]);
                    fail(2, idx, what, 0.0, 0.0);
                    if (broke[2] <= 5) printf("   %s\n", jaos_model_error(m));
                } else if (!same_values(ry, y, g.nr)) {
                    snprintf(what, sizeof what, "%s-duals-differ", SHAPE_NAME[sh]);
                    fail(2, idx, what, 0.0, 0.0);
                }
            }
            if (!(every > 0 && idx % every == 0)) remove(path);
        }

        /* P3 */
        double z[MAXC];
        for (int64_t j = 0; j < g.nc; j++) z[j] = (double)g.z[j];
        {
            jaos_check_report rep;
            if (jaos_check_solution(m, z, nullptr, TOL, &rep) != JAOS_OK) abort();
            long double zobj = (long double)g.offset;
            for (int64_t j = 0; j < g.nc; j++) zobj += (long double)g.cost[j] * (long double)g.z[j];
            if (!rep.primal_feasible) fail(3, idx, "planted-point-refused", rep.max_col_violation, rep.max_row_violation);
            if (rep.max_col_violation != 0.0 || rep.max_row_violation != 0.0)
                fail(3, idx, "planted-violation-not-zero", rep.max_col_violation, rep.max_row_violation);
            if ((long double)rep.primal_objective != zobj)
                fail(3, idx, "planted-objective-not-exact", rep.primal_objective, (double)zobj);
            if (rep.checked_duals) fail(3, idx, "planted-duals-checked", 0.0, 0.0);
        }

        /* P4 */
        double w[MAXC];
        double bad[MAXC];
        {
            memcpy(w, x, sizeof w);
            const int64_t j = ri(0, g.nc - 1);
            w[j] += (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
            compare(&g, m, w, nullptr, idx, "moved-x", 4);
            foreign_points++;
            jaos_check_report rep;
            if (jaos_check_solution(m, w, nullptr, TOL, &rep) == JAOS_OK && !rep.primal_feasible) foreign_refused++;

            memcpy(bad, z, sizeof bad);
            const int64_t k = ri(0, g.nc - 1);
            bad[k] = g.cu[k] + (double)ri(1, 3);
            compare(&g, m, bad, nullptr, idx, "moved-z", 4);
            foreign_points++;
            if (jaos_check_solution(m, bad, nullptr, TOL, &rep) == JAOS_OK && !rep.primal_feasible) foreign_refused++;
            else fail(4, idx, "outside-box-accepted", bad[k], g.cu[k]);
        }

        /* P5 */
        {
            compare(&g, m, x, y, idx, "own", 5);
            double yf[MAXR];
            memcpy(yf, y, sizeof yf);
            int64_t big = -1;
            for (int64_t i = 0; i < g.nr; i++)
                if (y[i] != 0.0 && (big < 0 || fabs(y[i]) > fabs(y[big]))) big = i;
            if (big >= 0) {
                yf[big] = -yf[big];
                compare(&g, m, x, yf, idx, "flipped-y", 5);
                dual_points++;
                jaos_check_report rep;
                if (jaos_check_solution(m, x, yf, TOL, &rep) == JAOS_OK && !rep.dual_feasible) dual_refused++;
            }
            for (int64_t i = 0; i < g.nr; i++) yf[i] = 2.0 * y[i];
            compare(&g, m, x, yf, idx, "doubled-y", 5);
            compare(&g, m, z, y, idx, "z-with-y", 5);
        }

        /* P6 */
        {
            char cn[32];
            snprintf(path, sizeof path, "%s/six-%" PRId64 ".pt", dir, idx);
            FILE *f = fopen(path, "w");
            if (f == nullptr) abort();
            for (int64_t j = 0; j < g.nc; j++) {
                name_of(cn, sizeof cn, true, j);
                fprintf(f, "%s %.17g\n", cn, x[j]);
            }
            name_of(cn, sizeof cn, true, 0);
            fprintf(f, "%s %.17g\n", cn, x[0]);
            fclose(f);
            if (jaos_read_point(m, path, rx) == JAOS_OK) fail(6, idx, "twice-named-accepted", 0.0, 0.0);

            f = fopen(path, "w");
            if (f == nullptr) abort();
            for (int64_t j = 1; j < g.nc; j++) {
                name_of(cn, sizeof cn, true, j);
                fprintf(f, "%s %.17g\n", cn, x[j]);
            }
            fclose(f);
            if (jaos_read_point(m, path, rx) == JAOS_OK) fail(6, idx, "missing-column-accepted", 0.0, 0.0);

            f = fopen(path, "w");
            if (f == nullptr) abort();
            fprintf(f, "=obj= 0\n");
            for (int64_t j = 1; j < g.nc; j++) {
                name_of(cn, sizeof cn, true, j);
                fprintf(f, "%s %.17g\n", cn, x[j]);
            }
            fclose(f);
            if (jaos_read_point(m, path, rx) != JAOS_OK) fail(6, idx, "miplib-missing-column-refused", 0.0, 0.0);
            else if (rx[0] != 0.0 || !same_values(rx + 1, x + 1, g.nc - 1)) fail(6, idx, "miplib-missing-column-not-zero", rx[0], 0.0);

            f = fopen(path, "w");
            if (f == nullptr) abort();
            for (int64_t j = 0; j < g.nc; j++) {
                name_of(cn, sizeof cn, true, j);
                fprintf(f, "%s %.17g\n", cn, x[j]);
            }
            fprintf(f, "NOSUCH 1\n");
            fclose(f);
            if (jaos_read_point(m, path, rx) == JAOS_OK) fail(6, idx, "unknown-name-accepted", 0.0, 0.0);
            remove(path);
        }

        if (every > 0 && idx % every == 0) {
            snprintf(path, sizeof path, "%s/m%" PRId64 ".mps", dir, idx);
            if (jaos_write_mps(m, path) != JAOS_OK) abort();
            const int sh = (int)((idx / every) % SH_COUNT);
            snprintf(path, sizeof path, "%s/m%" PRId64 ".pt", dir, idx);
            snprintf(path2, sizeof path2, "%s/%s-%" PRId64 ".sol", dir, SHAPE_NAME[sh], idx);
            if (rename(path2, path) != 0) abort();
            for (int s = 0; s < SH_COUNT; s++) {
                if (s == sh) continue;
                snprintf(path2, sizeof path2, "%s/%s-%" PRId64 ".sol", dir, SHAPE_NAME[s], idx);
                remove(path2);
            }
            snprintf(path, sizeof path, "%s/m%" PRId64 ".duals", dir, idx);
            if (jaos_write_duals(m, path) != JAOS_OK) abort();
            snprintf(path, sizeof path, "%s/m%" PRId64 ".bad.pt", dir, idx);
            if (jaos_write_point_values(m, path, bad) != JAOS_OK) abort();
            dumped++;
        }
        snprintf(path, sizeof path, "%s/own-%" PRId64 ".pt", dir, idx);
        remove(path);
        snprintf(path, sizeof path, "%s/own-%" PRId64 ".duals", dir, idx);
        remove(path);
        jaos_model_free(m);
    }

    int64_t total = 0;
    for (int p = 1; p <= 6; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " shapes_read %" PRId64
           " foreign_points %" PRId64 " refused %" PRId64
           " flipped_duals %" PRId64 " refused %" PRId64
           " ambiguous %" PRId64 " dumped %" PRId64 "\n",
           runs, optimal, shapes_read, foreign_points, foreign_refused,
           dual_points, dual_refused, ambiguous, dumped);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " total=%" PRId64 "\n", broke[1],
           broke[2], broke[3], broke[4], broke[5], broke[6], total);
    return total == 0 ? 0 : 1;
}
