#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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

static rx_sides rx_col_sides(const jaos_model *m, int64_t j, bool in_scope)
{
    rx_sides s = rx_sides_of(m->col_lower[j], m->col_upper[j], in_scope);
    if (m->col_semi != nullptr && m->col_semi[j])
        s.lo = false;
    return s;
}

typedef struct {
    jaos_model *c;
    int64_t nc, nr;
    int64_t ecol;
    int64_t erow;

    int64_t *row_s, *row_t;
    int64_t *col_s, *col_t;
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

    int64_t ecol = 0, erow = 0;
    for (int64_t i = 0; i < nr; i++) {
        const rx_sides s = rx_sides_of(m->row_lower[i], m->row_upper[i],
                                       do_rows);
        ecol += (s.lo ? 1 : 0) + (s.hi ? 1 : 0);
    }
    for (int64_t j = 0; j < nc; j++) {
        const rx_sides s = rx_col_sides(m, j, do_cols);
        if (!s.lo && !s.hi)
            continue;
        erow++;
        ecol += (s.lo ? 1 : 0) + (s.hi ? 1 : 0);
    }
    g->ecol = ecol;
    g->erow = erow;

    const int64_t tc = nc + ecol, tr = nr + erow;

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

    int64_t nz = 0, r = nr;
    for (int64_t j = 0; j < nc; j++) {
        ap[j] = nz;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            ai[nz] = m->a_index[k];
            av[nz] = m->a_value[k];
            nz++;
        }
        const rx_sides s = rx_col_sides(m, j, do_cols);
        if (s.lo || s.hi) {
            rl[r] = s.lo ? m->col_lower[j] : -INFINITY;
            ru[r] = s.hi ? m->col_upper[j] : INFINITY;
            ai[nz] = r;
            av[nz] = 1.0;
            nz++;

            cl[j] = s.lo ? -INFINITY : m->col_lower[j];
            cu[j] = s.hi ?  INFINITY : m->col_upper[j];
            r++;
        } else {
            cl[j] = m->col_lower[j];
            cu[j] = m->col_upper[j];
        }
    }
    assert(r == tr);

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
        const rx_sides s = rx_col_sides(m, j, do_cols);
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

    rc = jaos_load_lp(g->c, tc, tr, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                      tnz, ap, ai, av);
    if (rc != JAOS_OK) {
        jm_set_err(m, "the relaxation's copy refused the model: %s",
                   jaos_model_error(g->c));
        goto out;
    }

    for (int64_t j = 0; j < nc; j++) {
        if (m->col_integer != nullptr && m->col_integer[j]) {
            rc = jaos_set_col_integer(g->c, j, true);
            if (rc != JAOS_OK)
                goto out;
        }
        if (m->col_semi != nullptr && m->col_semi[j]) {
            rc = jaos_set_col_semicontinuous(g->c, j, true);
            if (rc != JAOS_OK)
                goto out;
        }
    }

    for (int64_t i = 0; i < nr; i++) {
        if (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0) {
            rc = jaos_set_row_indicator(g->c, i, m->row_ind_col[i],
                                        m->row_ind_val[i]);
            if (rc != JAOS_OK)
                goto out;
        }
    }

    for (int64_t k = 0; k < m->num_sos; k++) {
        const int64_t n = m->sos_start[k + 1] - m->sos_start[k];
        rc = jaos_add_sos(g->c, m->sos_type[k], n,
                          m->sos_col + m->sos_start[k],
                          m->sos_weight + m->sos_start[k]);
        if (rc != JAOS_OK)
            goto out;
    }

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

    rc = jaos_objective(g.c, &out->total);
    if (rc != JAOS_OK)
        goto out;

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
