/* An irreducible infeasible subsystem of the last INFEASIBLE model (D264).
 *
 * The members are bound SIDES: a row's lower or upper bound, a column's
 * lower or upper bound. A subsystem is a choice of sides kept, every
 * other side set to the infinity that relaxes it. It is an IIS when it is
 * infeasible on its own and keeping any one side less makes it feasible.
 *
 * Two filters from Chinneck and Dravnieks, ORSA Journal on Computing 3(2),
 * 1991, run one after the other. The sensitivity filter reads the Farkas
 * ray the solve published: a row side enters where its multiplier points
 * at it (y_i > 0 the lower, y_i < 0 the upper), a column side where
 * (A'y)_j points at it (positive the upper, negative the lower). Those
 * are the sides the certificate's own proof leans on, the sign rule
 * jaos_check_certificate reads it by without its floor, so the set is an infeasible
 * subsystem already and usually a small one. The deletion filter then
 * walks it in index order, rows before columns and lower before upper:
 * relax one side, re-solve warm; still infeasible, the side is out for
 * good; feasible, it is a member and goes back. What is left is an IIS.
 * The walk decides which IIS a model with several has, and a fixed walk
 * is what makes the answer reproducible (D8).
 *
 * Every solve runs on a private copy of the model with zero costs, so
 * feasibility is the only question a solve can answer (UNBOUNDED cannot
 * arise) and the caller's model, answer, certificate and basis are
 * untouched. Nothing here is billed to jaos_work_units; the report
 * carries the total. */

#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const jaos_model *m;
    jaos_model *c;              /* the private copy, zero costs */
    int64_t nrow, ncol;
    unsigned char *rs;          /* [nrow] sides kept, JAOS_IIS_* bits */
    unsigned char *cs;          /* [ncol] the same for columns */
    int64_t solves;
    int64_t work;
} iis;

/* A side is present when it constrains: a lower bound above -inf, an
 * upper bound below +inf. A lower bound of +inf is present, and inverted
 * against any upper; jaos_load_lp accepts that model on purpose. */
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

/* The copy's bounds for one row or column, from the sides it keeps. */
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

/* One re-solve of the copy, warm from wherever the last one stopped.
 * INFEASIBLE and OPTIMAL are the two answers the filter can use; a
 * budget stop, an interruption or a numerical failure is neither, and
 * the filter cannot decide the side it was asked about. */
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

/* Every present side, which is the whole model: the candidate set when
 * the certificate's support is not usable. */
static void mark_every_side(iis *g)
{
    for (int64_t i = 0; i < g->nrow; i++)
        g->rs[i] = sides_present(g->m->row_lower[i], g->m->row_upper[i]);
    for (int64_t j = 0; j < g->ncol; j++)
        g->cs[j] = sides_present(g->m->col_lower[j], g->m->col_upper[j]);
}

/* The sensitivity filter: the sides the certificate leans on, by the
 * sign rule jaos_check_certificate reads a ray with. (A'y)_j is summed
 * in double, in column order, and never in long double: the checker's
 * wider sum is a checker's privilege, and a published set may not
 * depend on a type whose width differs by machine (D8, D34; the review
 * of D264 measured seven of the 29 reference IISs changing with it).
 * The checker's floor, tol times the column's own traffic, is not
 * applied here: it would be a constant to sweep, a side that is only
 * roundoff costs one re-solve and the deletion filter removes it, and a
 * support that is not a proof falls back to every side. A side the ray
 * points at that the model has not got cannot be a member and is
 * skipped. */
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

/* The copy: the caller's matrix, zero costs, and the bounds the kept
 * sides give. The caller's limits and tolerances go with it, so one
 * re-solve may cost what the caller allows one solve; so does the
 * progress callback, so a watcher can stop the filter. The log callback
 * does not: the re-solves are this call's own business. */
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

/* The deletion filter over the kept sides, in the fixed order the file
 * comment states. */
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

    /* The candidates. An inverted box is refused by the solve before any
     * ray exists (D259) and has none to read, and its two sides are an
     * infeasible subsystem by themselves. Otherwise the certificate's
     * support; failing both, every side there is. */
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

    /* The candidates must be infeasible on their own, or the deletion
     * filter has nothing to reduce. A support that is not -- a ray the
     * copy's own tolerance reads differently -- falls back to the whole
     * model once; the whole model re-solving feasible is the original
     * verdict failing to repeat, which is reported and not repaired. */
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
