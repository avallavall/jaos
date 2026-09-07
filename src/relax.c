/* The smallest change to the bounds that makes an infeasible model
 * feasible (D331).
 *
 * An IIS says WHERE a model contradicts itself. This says HOW MUCH the
 * caller would have to give up to stop the contradiction, and on which
 * sides. The two answer different questions and neither replaces the
 * other: an IIS can be a set of ten rows nobody may move, and a
 * relaxation can name one row that has to move by 3.
 *
 * The formulation is the elastic one every textbook gives. A row
 * `rl <= a'x <= ru` in the relaxation's scope becomes
 * `rl <= a'x + s - t <= ru` with `s, t >= 0`, so the true activity may
 * fall `s` below `rl` or rise `t` above `ru`; a column's two bounds get
 * the same treatment through a row of their own, because a bound is not
 * a row and there is nothing to add a column to. The objective is the
 * sum of every `s` and `t`, minimized, and the caller's own objective is
 * dropped: this asks what feasibility costs, not what it costs at an
 * optimum. So the answer is the smallest TOTAL violation, the L1 one,
 * which is the measure that keeps the relaxation an LP. It is not the
 * smallest NUMBER of bounds moved -- that one is NP-hard and is not what
 * this call computes.
 *
 * Everything runs on a private copy, so the caller's model, answer,
 * certificate and basis are untouched, and nothing is billed to
 * jaos_work_units: the report carries the total.
 *
 * One model has no relaxation here and says so. An inverted box, a lower
 * bound above its upper, is a contradiction between two of the caller's
 * own numbers on ONE row; `s` and `t` move that row's two ends together
 * and cannot open it, so the elastic solve answers INFEASIBLE and the
 * report repeats it. The bounds are the diagnosis in that case, the way
 * they are for the certificate (D256). */

#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Which sides of one row or column this relaxation may move. A side that
 * is not there -- an infinite bound -- constrains nothing and gets no
 * elastic column, so a free row costs the relaxation nothing at all. */
typedef struct {
    bool lo, hi;
} rx_sides;

static rx_sides rx_sides_of(double lower, double upper, bool in_scope)
{
    rx_sides s = {false, false};
    if (!in_scope)
        return s;
    s.lo = lower > -INFINITY;
    s.hi = upper < INFINITY;
    return s;
}

/* The elastic model, built as CSC in one pass so no column is appended
 * twice. The layout is fixed and index-ordered, which is what makes the
 * relaxation the same on every machine (D8):
 *
 *   columns   0 .. nc-1              the caller's own, bounds intact for a
 *                                    column out of scope and opened for one
 *                                    in it, since its row below now holds them
 *             nc ..                  the elastic pairs, rows before columns,
 *                                    and within each the lower side's `s`
 *                                    before the upper side's `t`
 *   rows      0 .. nr-1              the caller's own, bounds intact
 *             nr ..                  one per relaxed column, carrying that
 *                                    column's own two bounds
 */
typedef struct {
    jaos_model *c;
    int64_t nc, nr;        /* the caller's dimensions */
    int64_t ecol;          /* elastic columns added */
    int64_t erow;          /* rows added, one per relaxed column */
    /* Where each side's elastic column landed, or -1. Read back after the
     * solve to turn a point into a per-row and per-column move. */
    int64_t *row_s, *row_t;   /* [nr] */
    int64_t *col_s, *col_t;   /* [nc] */
} rx;

static void rx_free(rx *g)
{
    if (g->c != nullptr)
        jaos_model_free(g->c);
    g->c = nullptr;
    free(g->row_s); free(g->row_t);
    free(g->col_s); free(g->col_t);
    g->row_s = g->row_t = g->col_s = g->col_t = nullptr;
}

/* Builds the elastic copy. `m` is read only; every failure leaves the
 * caller's model as it was and puts the reason in its error text. */
static jaos_status rx_build(rx *g, jaos_model *m, jaos_relax_scope scope)
{
    const int64_t nc = g->nc, nr = g->nr;
    const bool do_rows = (scope & JAOS_RELAX_ROWS) != 0;
    const bool do_cols = (scope & JAOS_RELAX_COLS) != 0;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    double *cost = nullptr, *cl = nullptr, *cu = nullptr;
    double *rl = nullptr, *ru = nullptr;
    int64_t *ap = nullptr, *ai = nullptr;
    double *av = nullptr;

    g->row_s = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *g->row_s);
    g->row_t = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *g->row_t);
    g->col_s = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *g->col_s);
    g->col_t = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *g->col_t);
    if (g->row_s == nullptr || g->row_t == nullptr ||
        g->col_s == nullptr || g->col_t == nullptr)
        goto out;
    for (int64_t i = 0; i < nr; i++) g->row_s[i] = g->row_t[i] = -1;
    for (int64_t j = 0; j < nc; j++) g->col_s[j] = g->col_t[j] = -1;

    /* Pass one: count, so the arrays are allocated once at their final
     * size and the second pass writes them in the layout above. */
    int64_t ecol = 0, erow = 0;
    for (int64_t i = 0; i < nr; i++) {
        const rx_sides s = rx_sides_of(m->row_lower[i], m->row_upper[i],
                                       do_rows);
        ecol += (s.lo ? 1 : 0) + (s.hi ? 1 : 0);
    }
    for (int64_t j = 0; j < nc; j++) {
        const rx_sides s = rx_sides_of(m->col_lower[j], m->col_upper[j],
                                       do_cols);
        if (!s.lo && !s.hi)
            continue;
        erow++;
        ecol += (s.lo ? 1 : 0) + (s.hi ? 1 : 0);
    }
    g->ecol = ecol;
    g->erow = erow;

    const int64_t tc = nc + ecol, tr = nr + erow;
    /* Every elastic column has exactly one entry, and every relaxed
     * column gains one in the row that now carries its bounds. */
    const int64_t tnz = m->num_nz + ecol + erow;

    cost = calloc((size_t)(tc > 0 ? tc : 1), sizeof *cost);
    cl   = malloc((size_t)(tc > 0 ? tc : 1) * sizeof *cl);
    cu   = malloc((size_t)(tc > 0 ? tc : 1) * sizeof *cu);
    rl   = malloc((size_t)(tr > 0 ? tr : 1) * sizeof *rl);
    ru   = malloc((size_t)(tr > 0 ? tr : 1) * sizeof *ru);
    ap   = malloc((size_t)(tc + 1) * sizeof *ap);
    ai   = malloc((size_t)(tnz > 0 ? tnz : 1) * sizeof *ai);
    av   = malloc((size_t)(tnz > 0 ? tnz : 1) * sizeof *av);
    if (cost == nullptr || cl == nullptr || cu == nullptr || rl == nullptr ||
        ru == nullptr || ap == nullptr || ai == nullptr || av == nullptr)
        goto out;

    for (int64_t i = 0; i < nr; i++) {
        rl[i] = m->row_lower[i];
        ru[i] = m->row_upper[i];
    }

    /* The caller's columns, each with its own entries and, where its
     * bounds moved out to a row, one entry in that row. The row indices
     * are assigned here in column order, which is the same order pass
     * one counted them in. */
    int64_t nz = 0, r = nr;
    for (int64_t j = 0; j < nc; j++) {
        ap[j] = nz;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            ai[nz] = m->a_index[k];
            av[nz] = m->a_value[k];
            nz++;
        }
        const rx_sides s = rx_sides_of(m->col_lower[j], m->col_upper[j],
                                       do_cols);
        if (s.lo || s.hi) {
            rl[r] = s.lo ? m->col_lower[j] : -INFINITY;
            ru[r] = s.hi ? m->col_upper[j] : INFINITY;
            ai[nz] = r;
            av[nz] = 1.0;
            nz++;
            /* The column itself is opened on exactly the sides its new
             * row took over, and keeps any side that row did not. */
            cl[j] = s.lo ? -INFINITY : m->col_lower[j];
            cu[j] = s.hi ?  INFINITY : m->col_upper[j];
            r++;
        } else {
            cl[j] = m->col_lower[j];
            cu[j] = m->col_upper[j];
        }
    }
    assert(r == tr);

    /* The elastic columns. `s` raises the row's activity so the true one
     * may sit `s` below the lower bound, `t` lowers it so the true one
     * may sit `t` above the upper. Cost 1 each: the objective is the
     * total violation and nothing else. */
    int64_t e = nc;
    for (int64_t i = 0; i < nr; i++) {
        const rx_sides s = rx_sides_of(m->row_lower[i], m->row_upper[i],
                                       do_rows);
        if (s.lo) {
            ap[e] = nz; ai[nz] = i; av[nz] = 1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->row_s[i] = e; e++;
        }
        if (s.hi) {
            ap[e] = nz; ai[nz] = i; av[nz] = -1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->row_t[i] = e; e++;
        }
    }
    r = nr;
    for (int64_t j = 0; j < nc; j++) {
        const rx_sides s = rx_sides_of(m->col_lower[j], m->col_upper[j],
                                       do_cols);
        if (!s.lo && !s.hi)
            continue;
        if (s.lo) {
            ap[e] = nz; ai[nz] = r; av[nz] = 1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->col_s[j] = e; e++;
        }
        if (s.hi) {
            ap[e] = nz; ai[nz] = r; av[nz] = -1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->col_t[j] = e; e++;
        }
        r++;
    }
    assert(e == tc && nz == tnz);
    ap[tc] = nz;

    rc = jaos_model_new(&g->c);
    if (rc != JAOS_OK)
        goto out;
    /* Minimize, and no objective constant: the total violation is the
     * objective, so a constant would be added to an answer that has to
     * read zero on a feasible model. */
    rc = jaos_load_lp(g->c, tc, tr, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                      tnz, ap, ai, av);
    if (rc != JAOS_OK) {
        jm_set_err(m, "the relaxation's copy refused the model: %s",
                   jaos_model_error(g->c));
        goto out;
    }
    /* An integer column stays integer, so a relaxation of a mixed-integer
     * model answers about that model and not about its relaxation. The
     * elastic columns are continuous, which is what makes the violation
     * a size rather than a count. */
    for (int64_t j = 0; j < nc; j++) {
        if (m->col_integer != nullptr && m->col_integer[j]) {
            rc = jaos_set_col_integer(g->c, j, true);
            if (rc != JAOS_OK)
                goto out;
        }
    }
    /* The caller's limits, tolerances and progress callback ride along,
     * so one relaxation may cost what the caller allows one solve and a
     * watcher can stop it. The log callback does not: these solves are
     * this call's own business. */
    g->c->cfg.work_limit = m->cfg.work_limit;
    g->c->cfg.time_limit = m->cfg.time_limit;
    g->c->cfg.primal_tol = m->cfg.primal_tol;
    g->c->cfg.dual_tol = m->cfg.dual_tol;
    g->c->cfg.progress_cb = m->cfg.progress_cb;
    g->c->cfg.progress_user = m->cfg.progress_user;
    g->c->cfg.force_primal = m->cfg.force_primal;
    rc = JAOS_OK;
out:
    free(cost); free(cl); free(cu); free(rl); free(ru);
    free(ap); free(ai); free(av);
    return rc;
}

/* One side's value out of the elastic point, and zero where that side has
 * no column. A tolerance is not applied: the value is what the solve
 * published and rounding it here would be a threshold nobody swept. */
static double rx_at(const double *x, int64_t e)
{
    return e < 0 ? 0.0 : x[e];
}

jaos_status jaos_feasrelax(jaos_model *m, jaos_relax_scope scope,
                           double *row_move, double *col_move,
                           jaos_relax_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (scope != JAOS_RELAX_ROWS && scope != JAOS_RELAX_COLS &&
        scope != JAOS_RELAX_BOTH)
        return JAOS_ERR_INVALID_INPUT;
    memset(out, 0, sizeof *out);
    out->at_row = -1;
    out->at_col = -1;
    out->status = JAOS_SOLVE_NOT_RUN;
    m->err[0] = '\0';

    if (row_move != nullptr && m->num_row > 0)
        memset(row_move, 0, (size_t)m->num_row * sizeof *row_move);
    if (col_move != nullptr && m->num_col > 0)
        memset(col_move, 0, (size_t)m->num_col * sizeof *col_move);

    rx g = {.nc = m->num_col, .nr = m->num_row};
    double *x = nullptr;
    jaos_status rc = rx_build(&g, m, scope);
    if (rc != JAOS_OK)
        goto out;

    rc = jaos_solve(g.c);
    if (rc != JAOS_OK) {
        jm_set_err(m, "the relaxation's solve failed: %s (%s)",
                   jaos_status_str(rc), jaos_model_error(g.c));
        goto out;
    }
    out->work_units = jaos_work_units(g.c);
    out->status = jaos_status_of(g.c);
    if (out->status != JAOS_SOLVE_OPTIMAL) {
        /* No verdict is manufactured from a solve that did not finish.
         * INFEASIBLE is the one status that can legitimately arrive
         * here, and it has two causes rather than one. An inverted box
         * is a contradiction on a single row that `s` and `t` move
         * together and cannot open, and it defeats every scope. The
         * other is a scope narrower than the contradiction: under
         * JAOS_RELAX_COLS the caller's rows keep their bounds and get no
         * elastic pair, so `x0 + x1 = 5` beside `x0 + x1 = 7` has no
         * relaxation over the columns even though it has one over the
         * rows. The report says INFEASIBLE either way, and which of the
         * two it is is read from the scope. */
        jm_set_err(m, "the relaxation's solve answered %s, so there is no "
                      "smallest violation to report",
                   jaos_solve_status_str(out->status));
        rc = JAOS_ERR_NUMERICAL;
        goto out;
    }

    const int64_t tc = g.nc + g.ecol;
    x = malloc((size_t)(tc > 0 ? tc : 1) * sizeof *x);
    if (x == nullptr) {
        rc = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    rc = jaos_solution(g.c, x, nullptr, nullptr, nullptr);
    if (rc != JAOS_OK)
        goto out;

    /* The objective IS the total, and it is taken from the solve rather
     * than re-summed here, so one number has one owner. */
    rc = jaos_objective(g.c, &out->total);
    if (rc != JAOS_OK)
        goto out;

    /* A move is signed: below zero the lower bound has to come down by
     * that much, above zero the upper bound has to go up by it. At an
     * optimum at most one of a side's two columns is positive, since a
     * pair that both moved would cost two and buy nothing, so the
     * difference names the side without ambiguity. */
    for (int64_t i = 0; i < g.nr; i++) {
        const double v = rx_at(x, g.row_t[i]) - rx_at(x, g.row_s[i]);
        if (row_move != nullptr)
            row_move[i] = v;
        if (v == 0.0)
            continue;
        out->rows_moved++;
        if (fabs(v) > out->largest) {
            out->largest = fabs(v);
            out->at_row = i;
            out->at_col = -1;
        }
    }
    for (int64_t j = 0; j < g.nc; j++) {
        const double v = rx_at(x, g.col_t[j]) - rx_at(x, g.col_s[j]);
        if (col_move != nullptr)
            col_move[j] = v;
        if (v == 0.0)
            continue;
        out->cols_moved++;
        if (fabs(v) > out->largest) {
            out->largest = fabs(v);
            out->at_row = -1;
            out->at_col = j;
        }
    }
    rc = JAOS_OK;
out:
    free(x);
    rx_free(&g);
    return rc;
}
