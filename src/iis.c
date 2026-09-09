#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const jaos_model *m;
    jaos_model *c;
    int64_t nrow, ncol;
    unsigned char *rs;
    unsigned char *cs;
    int64_t solves;
    int64_t work;
} iis;

static unsigned char sides_present(double lower, double upper)
{
    unsigned char s = JAOS_IIS_NONE;
    if (lower > -INFINITY)
        s |= JAOS_IIS_LOWER;
    if (upper < INFINITY)
        s |= JAOS_IIS_UPPER;
    return s;
}

static int64_t count_sides(const unsigned char *s, int64_t n)
{
    int64_t k = 0;
    for (int64_t i = 0; i < n; i++)
        k += (s[i] & JAOS_IIS_LOWER ? 1 : 0) + (s[i] & JAOS_IIS_UPPER ? 1 : 0);
    return k;
}

static jaos_status apply_row(iis *g, int64_t i)
{
    const double lo = g->rs[i] & JAOS_IIS_LOWER ? g->m->row_lower[i] : -INFINITY;
    const double up = g->rs[i] & JAOS_IIS_UPPER ? g->m->row_upper[i] : INFINITY;
    return jaos_set_row_bounds(g->c, i, lo, up);
}

static jaos_status apply_col(iis *g, int64_t j)
{
    const double lo = g->cs[j] & JAOS_IIS_LOWER ? g->m->col_lower[j] : -INFINITY;
    const double up = g->cs[j] & JAOS_IIS_UPPER ? g->m->col_upper[j] : INFINITY;
    return jaos_set_col_bounds(g->c, j, lo, up);
}

static jaos_status resolve(iis *g, jaos_model *m, jaos_solve_status *st)
{
    const jaos_status rc = jaos_solve(g->c);
    if (rc != JAOS_OK) {
        jm_set_err(m, "an IIS re-solve failed: %s (%s)", jaos_status_str(rc),
                   jaos_model_error(g->c));
        return rc;
    }
    g->solves++;
    g->work += jaos_work_units(g->c);
    *st = jaos_status_of(g->c);
    if (*st != JAOS_SOLVE_INFEASIBLE && *st != JAOS_SOLVE_OPTIMAL) {
        jm_set_err(m, "an IIS re-solve stopped %s after %lld solves, and "
                   "the filter cannot decide a side from that",
                   jaos_solve_status_str(*st), (long long)g->solves);
        return JAOS_ERR_NUMERICAL;
    }
    return JAOS_OK;
}

static void mark_every_side(iis *g)
{
    for (int64_t i = 0; i < g->nrow; i++)
        g->rs[i] = sides_present(g->m->row_lower[i], g->m->row_upper[i]);
    for (int64_t j = 0; j < g->ncol; j++)
        g->cs[j] = sides_present(g->m->col_lower[j], g->m->col_upper[j]);
}

static void mark_support(iis *g, const double *y)
{
    const jaos_model *m = g->m;
    for (int64_t i = 0; i < g->nrow; i++) {
        unsigned char s = JAOS_IIS_NONE;
        if (y[i] > 0.0 && m->row_lower[i] > -INFINITY)
            s = JAOS_IIS_LOWER;
        else if (y[i] < 0.0 && m->row_upper[i] < INFINITY)
            s = JAOS_IIS_UPPER;
        g->rs[i] = s;
    }
    for (int64_t j = 0; j < g->ncol; j++) {
        double a = 0.0;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
            a += m->a_value[p] * y[m->a_index[p]];
        unsigned char s = JAOS_IIS_NONE;
        if (a > 0.0 && m->col_upper[j] < INFINITY)
            s = JAOS_IIS_UPPER;
        else if (a < 0.0 && m->col_lower[j] > -INFINITY)
            s = JAOS_IIS_LOWER;
        g->cs[j] = s;
    }
}

static jaos_status make_copy(iis *g, jaos_model *m)
{
    const int64_t nr = g->nrow, nc = g->ncol;
    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    double *zero = calloc((size_t)(nc > 0 ? nc : 1), sizeof *zero);
    double *rl = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *rl);
    double *ru = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *ru);
    double *cl = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *cl);
    double *cu = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *cu);
    if (zero == nullptr || rl == nullptr || ru == nullptr ||
        cl == nullptr || cu == nullptr)
        goto out;
    for (int64_t i = 0; i < nr; i++) {
        rl[i] = g->rs[i] & JAOS_IIS_LOWER ? m->row_lower[i] : -INFINITY;
        ru[i] = g->rs[i] & JAOS_IIS_UPPER ? m->row_upper[i] : INFINITY;
    }
    for (int64_t j = 0; j < nc; j++) {
        cl[j] = g->cs[j] & JAOS_IIS_LOWER ? m->col_lower[j] : -INFINITY;
        cu[j] = g->cs[j] & JAOS_IIS_UPPER ? m->col_upper[j] : INFINITY;
    }
    rc = jaos_model_new(&g->c);
    if (rc != JAOS_OK)
        goto out;
    rc = jaos_load_lp(g->c, nc, nr, JAOS_MINIMIZE, 0.0, zero, cl, cu, rl, ru,
                      m->num_nz, m->a_start, m->a_index, m->a_value);
    if (rc != JAOS_OK) {
        jm_set_err(m, "the IIS copy refused the model: %s",
                   jaos_model_error(g->c));
        goto out;
    }
    g->c->cfg.work_limit = m->cfg.work_limit;
    g->c->cfg.time_limit = m->cfg.time_limit;
    g->c->cfg.primal_tol = m->cfg.primal_tol;
    g->c->cfg.dual_tol = m->cfg.dual_tol;
    g->c->cfg.progress_cb = m->cfg.progress_cb;
    g->c->cfg.progress_user = m->cfg.progress_user;
    g->c->cfg.force_primal = m->cfg.force_primal;
out:
    free(zero);
    free(rl);
    free(ru);
    free(cl);
    free(cu);
    return rc;
}

static jaos_status delete_filter(iis *g, jaos_model *m)
{
    static const unsigned char order[2] = {JAOS_IIS_LOWER, JAOS_IIS_UPPER};
    jaos_solve_status st;
    for (int64_t i = 0; i < g->nrow; i++) {
        for (int k = 0; k < 2; k++) {
            const unsigned char side = order[k];
            if (!(g->rs[i] & side))
                continue;
            g->rs[i] &= (unsigned char)~side;
            jaos_status rc = apply_row(g, i);
            if (rc == JAOS_OK)
                rc = resolve(g, m, &st);
            if (rc != JAOS_OK)
                return rc;
            if (st == JAOS_SOLVE_OPTIMAL) {
                g->rs[i] |= side;
                rc = apply_row(g, i);
                if (rc != JAOS_OK)
                    return rc;
            }
        }
    }
    for (int64_t j = 0; j < g->ncol; j++) {
        for (int k = 0; k < 2; k++) {
            const unsigned char side = order[k];
            if (!(g->cs[j] & side))
                continue;
            g->cs[j] &= (unsigned char)~side;
            jaos_status rc = apply_col(g, j);
            if (rc == JAOS_OK)
                rc = resolve(g, m, &st);
            if (rc != JAOS_OK)
                return rc;
            if (st == JAOS_SOLVE_OPTIMAL) {
                g->cs[j] |= side;
                rc = apply_col(g, j);
                if (rc != JAOS_OK)
                    return rc;
            }
        }
    }
    return JAOS_OK;
}

jaos_status jaos_iis(jaos_model *m, jaos_iis_side *row_side,
                     jaos_iis_side *col_side, jaos_iis_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    memset(out, 0, sizeof *out);
    if (m->solve_status != JAOS_SOLVE_INFEASIBLE) {
        jm_set_err(m, "an IIS needs the last solve to have answered "
                   "INFEASIBLE, and it answered %s",
                   jaos_solve_status_str(m->solve_status));
        return JAOS_ERR_INVALID_INPUT;
    }
    m->err[0] = '\0';

    iis g = {.m = m, .nrow = m->num_row, .ncol = m->num_col};
    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    g.rs = calloc((size_t)(g.nrow > 0 ? g.nrow : 1), sizeof *g.rs);
    g.cs = calloc((size_t)(g.ncol > 0 ? g.ncol : 1), sizeof *g.cs);
    double *y = malloc((size_t)(g.nrow > 0 ? g.nrow : 1) * sizeof *y);
    if (g.rs == nullptr || g.cs == nullptr || y == nullptr)
        goto out;

    bool from_cert = false;
    bool found = false;
    for (int64_t i = 0; i < g.nrow && !found; i++)
        if (jm_box_inverted(m->row_lower[i], m->row_upper[i])) {
            g.rs[i] = sides_present(m->row_lower[i], m->row_upper[i]);
            found = true;
        }
    for (int64_t j = 0; j < g.ncol && !found; j++)
        if (jm_box_inverted(m->col_lower[j], m->col_upper[j])) {
            g.cs[j] = sides_present(m->col_lower[j], m->col_upper[j]);
            found = true;
        }
    if (!found && jaos_certificate(m, y) == JAOS_OK) {
        mark_support(&g, y);
        from_cert = count_sides(g.rs, g.nrow) + count_sides(g.cs, g.ncol) > 0;
        found = from_cert;
    }
    if (!found)
        mark_every_side(&g);

    rc = make_copy(&g, m);
    if (rc != JAOS_OK)
        goto out;

    jaos_solve_status st;
    rc = resolve(&g, m, &st);
    if (rc != JAOS_OK)
        goto out;
    if (st != JAOS_SOLVE_INFEASIBLE && from_cert) {
        from_cert = false;
        mark_every_side(&g);
        for (int64_t i = 0; i < g.nrow && rc == JAOS_OK; i++)
            rc = apply_row(&g, i);
        for (int64_t j = 0; j < g.ncol && rc == JAOS_OK; j++)
            rc = apply_col(&g, j);
        if (rc == JAOS_OK)
            rc = resolve(&g, m, &st);
        if (rc != JAOS_OK)
            goto out;
    }
    if (st != JAOS_SOLVE_INFEASIBLE) {
        bool relaxed = m->num_sos > 0 || m->row_ind_col != nullptr;
        for (int64_t j = 0; !relaxed && j < g.ncol; j++)
            relaxed = (m->col_integer != nullptr && m->col_integer[j]) ||
                      (m->col_semi != nullptr && m->col_semi[j]) ||
                      (m->col_quad != nullptr && m->col_quad[j] != 0.0);
        if (relaxed)
            jm_set_err(m, "a subsystem is a set of bound sides, so "
                       "integrality, semi-continuity, the SOS sets, the "
                       "indicator rows and the quadratic term are dropped; "
                       "with them dropped the model re-solved %s, so no set "
                       "of bounds explains its INFEASIBLE",
                       jaos_solve_status_str(st));
        else
            jm_set_err(m, "the model with every bound kept and no objective "
                       "re-solved %s, so its INFEASIBLE does not repeat",
                       jaos_solve_status_str(st));
        rc = JAOS_ERR_NUMERICAL;
        goto out;
    }
    out->candidates = count_sides(g.rs, g.nrow) + count_sides(g.cs, g.ncol);
    out->from_certificate = from_cert;

    rc = delete_filter(&g, m);
    if (rc != JAOS_OK)
        goto out;

    out->members = count_sides(g.rs, g.nrow) + count_sides(g.cs, g.ncol);
    if (row_side)
        for (int64_t i = 0; i < g.nrow; i++)
            row_side[i] = (jaos_iis_side)g.rs[i];
    if (col_side)
        for (int64_t j = 0; j < g.ncol; j++)
            col_side[j] = (jaos_iis_side)g.cs[j];
    rc = JAOS_OK;

out:
    out->solves = g.solves;
    out->work_units = g.work;
    jaos_model_free(g.c);
    free(g.rs);
    free(g.cs);
    free(y);
    return rc;
}

jaos_status jaos_iis_model(const jaos_model *m, const jaos_iis_side *row_side,
                           const jaos_iis_side *col_side, jaos_model **out)
{
    if (m == nullptr || row_side == nullptr || col_side == nullptr ||
        out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t i = 0; i < m->num_row; i++)
        if ((unsigned)row_side[i] > (unsigned)JAOS_IIS_BOTH)
            return JAOS_ERR_INVALID_INPUT;
    for (int64_t j = 0; j < m->num_col; j++)
        if ((unsigned)col_side[j] > (unsigned)JAOS_IIS_BOTH)
            return JAOS_ERR_INVALID_INPUT;

    jaos_model *c = nullptr;
    jaos_status st = jaos_model_copy(m, &c);
    if (st != JAOS_OK)
        return st;

    int64_t *drop = nullptr;
    int64_t ndrop = 0;

    free(c->col_integer);  c->col_integer = nullptr;
    free(c->col_semi);     c->col_semi = nullptr;
    free(c->col_quad);     c->col_quad = nullptr;
    free(c->row_ind_col);  c->row_ind_col = nullptr;
    free(c->row_ind_val);  c->row_ind_val = nullptr;
    c->num_sos = 0;

    st = jaos_set_objective_offset(c, 0.0);
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++)
        st = jaos_set_col_cost(c, j, 0.0);

    for (int64_t i = 0; st == JAOS_OK && i < m->num_row; i++) {
        const double lo = (row_side[i] & JAOS_IIS_LOWER) ? m->row_lower[i]
                                                         : -INFINITY;
        const double up = (row_side[i] & JAOS_IIS_UPPER) ? m->row_upper[i]
                                                         : INFINITY;
        st = jaos_set_row_bounds(c, i, lo, up);
    }
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++) {
        const double lo = (col_side[j] & JAOS_IIS_LOWER) ? m->col_lower[j]
                                                         : -INFINITY;
        const double up = (col_side[j] & JAOS_IIS_UPPER) ? m->col_upper[j]
                                                         : INFINITY;
        st = jaos_set_col_bounds(c, j, lo, up);
    }

    if (st == JAOS_OK) {
        drop = jm_alloc_array(m->num_row, sizeof *drop);
        if (drop == nullptr)
            st = JAOS_ERR_OUT_OF_MEMORY;
    }
    if (st == JAOS_OK) {
        for (int64_t i = 0; i < m->num_row; i++)
            if (row_side[i] == JAOS_IIS_NONE)
                drop[ndrop++] = i;
        if (ndrop > 0)
            st = jaos_delete_rows(c, ndrop, drop);
        free(drop);
        drop = nullptr;
    }

    if (st == JAOS_OK) {
        const int64_t ncol = c->num_col;
        drop = jm_alloc_array(ncol, sizeof *drop);
        if (drop == nullptr)
            st = JAOS_ERR_OUT_OF_MEMORY;
        ndrop = 0;
        for (int64_t j = 0; st == JAOS_OK && j < ncol; j++) {
            int64_t nz = 0;
            st = jaos_col_entries(c, j, &nz, nullptr, nullptr);
            if (st == JAOS_OK && nz == 0 && col_side[j] == JAOS_IIS_NONE)
                drop[ndrop++] = j;
        }
        if (st == JAOS_OK && ndrop > 0)
            st = jaos_delete_cols(c, ndrop, drop);
        free(drop);
    }

    if (st != JAOS_OK) {
        jaos_model_free(c);
        return st;
    }
    *out = c;
    return JAOS_OK;
}
