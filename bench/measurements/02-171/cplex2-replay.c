/* The instance the oracle rejects, replayed. For every member of its IIS
 * that a cold re-solve does not need (the rest re-solves INFEASIBLE
 * without it), up to the first three, rows then columns in the filter's
 * own walk order: rebuild the kept set K the deletion filter held when
 * it reached that side, which is the certificate's support minus the
 * support sides missing from the IIS at earlier positions of the walk,
 * then
 *   A: K fresh, cold -> verdict; the side relaxed on it, warm -> verdict,
 *      and the checker on the point if OPTIMAL or on the ray if not
 *   B: K without the side, fresh, cold -> the same
 *   C: the filter's own walk from the support, warm re-solve by warm
 *      re-solve in the library's order, stopped with this side relaxed:
 *      the verdict the filter itself read, and the checker on its point.
 * The walk is first run to the end and compared with the library's IIS,
 * so the instrument is checked before it is read.
 * Usage: p <mps>  */
#include "jaos.h"
#include "jaos_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const jaos_model *m;
    int64_t nr, nc;
    unsigned char *srs, *scs;   /* the support */
    unsigned char *irs, *ics;   /* the IIS */
    unsigned char *krs, *kcs;   /* the kept set being rebuilt */
} rp;

static jaos_model *build(const rp *g, const unsigned char *rs,
                         const unsigned char *cs)
{
    const jaos_model *m = g->m;
    const int64_t nr = g->nr, nc = g->nc;
    double *zero = calloc((size_t)(nc + 1), sizeof *zero);
    double *rl = malloc((size_t)(nr + 1) * sizeof *rl);
    double *ru = malloc((size_t)(nr + 1) * sizeof *ru);
    double *cl = malloc((size_t)(nc + 1) * sizeof *cl);
    double *cu = malloc((size_t)(nc + 1) * sizeof *cu);
    if (!zero || !rl || !ru || !cl || !cu)
        exit(9);
    for (int64_t i = 0; i < nr; i++) {
        rl[i] = rs[i] & 1 ? m->row_lower[i] : -INFINITY;
        ru[i] = rs[i] & 2 ? m->row_upper[i] : INFINITY;
    }
    for (int64_t j = 0; j < nc; j++) {
        cl[j] = cs[j] & 1 ? m->col_lower[j] : -INFINITY;
        cu[j] = cs[j] & 2 ? m->col_upper[j] : INFINITY;
    }
    jaos_model *s = nullptr;
    if (jaos_model_new(&s) != JAOS_OK ||
        jaos_load_lp(s, nc, nr, JAOS_MINIMIZE, 0.0, zero, cl, cu, rl, ru,
                     m->num_nz, m->a_start, m->a_index, m->a_value) != JAOS_OK)
        exit(9);
    free(zero); free(rl); free(ru); free(cl); free(cu);
    return s;
}

static void point(jaos_model *s, const char *tag)
{
    double *x = malloc((size_t)s->num_col * sizeof *x);
    double *y = malloc((size_t)s->num_row * sizeof *y);
    if (!x || !y || jaos_solution(s, x, nullptr, y, nullptr) != JAOS_OK) {
        printf("  [%s] no solution\n", tag);
        return;
    }
    for (double tol = 1e-7; tol >= 1e-10; tol /= 1000.0) {
        jaos_check_report r;
        if (jaos_check_solution(s, x, y, tol, &r) != JAOS_OK)
            break;
        printf("  [%s] checker tol=%.0e primal_feasible=%d col_viol=%.6g "
               "row_viol=%.6g rel=%.6g\n", tag, tol, (int)r.primal_feasible,
               r.max_col_violation, r.max_row_violation,
               r.max_row_violation_relative);
    }
    printf("  [%s] iters=%lld work=%lld presolve %lld x %lld\n", tag,
           (long long)jaos_iterations(s), (long long)jaos_work_units(s),
           (long long)s->presolve_num_row, (long long)s->presolve_num_col);
    free(x); free(y);
}

static void cert(jaos_model *s, const char *tag)
{
    double *yr = malloc((size_t)s->num_row * sizeof *yr);
    if (!yr || jaos_certificate(s, yr) != JAOS_OK) {
        printf("  [%s] no certificate\n", tag);
        free(yr);
        return;
    }
    jaos_certificate_report c;
    if (jaos_check_certificate(s, yr, 1e-7, &c) == JAOS_OK)
        printf("  [%s] certificate gap=%.6g rel=%.3g certified=%d iters=%lld\n",
               tag, c.gap,
               c.gap / (1.0 + fabs(c.sup_columns) + fabs(c.inf_rows)),
               (int)c.certified, (long long)jaos_iterations(s));
    free(yr);
}

static jaos_solve_status verdict(jaos_model *s)
{
    if (jaos_solve(s) != JAOS_OK)
        exit(9);
    return jaos_status_of(s);
}

/* One side's bounds on a live model, from the bits kept. */
static void set_side(const rp *g, jaos_model *s, bool is_row, int64_t idx,
                     unsigned char kept)
{
    const jaos_model *m = g->m;
    const double lo = kept & 1 ? (is_row ? m->row_lower[idx] : m->col_lower[idx]) : -INFINITY;
    const double up = kept & 2 ? (is_row ? m->row_upper[idx] : m->col_upper[idx]) : INFINITY;
    const jaos_status rc = is_row ? jaos_set_row_bounds(s, idx, lo, up)
                                  : jaos_set_col_bounds(s, idx, lo, up);
    if (rc != JAOS_OK)
        exit(9);
}

/* C: the deletion filter's own walk, as src/iis.c runs it -- the support
 * solved cold once, then one warm re-solve per side in the same order --
 * stopped at the side (is_row, idx, side) with that side relaxed, so the
 * verdict and the point are the filter's own and not a cold solve's. With
 * idx < 0 the walk runs to the end and its result is compared with the
 * IIS the library returned: the instrument's own check. */
static void walk(rp *g, bool is_row, int64_t idx, unsigned char side)
{
    const int64_t nr = g->nr, nc = g->nc;
    unsigned char *wrs = malloc((size_t)nr + 1), *wcs = malloc((size_t)nc + 1);
    if (!wrs || !wcs)
        exit(9);
    memcpy(wrs, g->srs, (size_t)nr);
    memcpy(wcs, g->scs, (size_t)nc);
    jaos_model *W = build(g, wrs, wcs);
    if (verdict(W) != JAOS_SOLVE_INFEASIBLE) {
        printf("  C: the support re-solved feasible, no walk\n");
        jaos_model_free(W); free(wrs); free(wcs);
        return;
    }
    static const unsigned char order[2] = {1, 2};
    int64_t resolves = 0;
    for (int pass = 0; pass < 2; pass++) {
        const bool rows = pass == 0;
        unsigned char *set = rows ? wrs : wcs;
        const int64_t n = rows ? nr : nc;
        for (int64_t v = 0; v < n; v++)
            for (int k = 0; k < 2; k++) {
                const unsigned char sd = order[k];
                if (!(set[v] & sd))
                    continue;
                set[v] &= (unsigned char)~sd;
                set_side(g, W, rows, v, set[v]);
                const jaos_solve_status st = verdict(W);
                resolves++;
                if (idx >= 0 && rows == is_row && v == idx && sd == side) {
                    printf("  C: the filter's own walk, the side relaxed at re-solve %lld, warm -> %s\n",
                           (long long)resolves, jaos_solve_status_str(st));
                    if (st == JAOS_SOLVE_OPTIMAL) point(W, "C-walk"); else cert(W, "C-walk");
                    jaos_model_free(W); free(wrs); free(wcs);
                    return;
                }
                if (st == JAOS_SOLVE_OPTIMAL) {
                    set[v] |= sd;
                    set_side(g, W, rows, v, set[v]);
                }
            }
    }
    if (idx < 0)
        printf("the walk %s the IIS the library returned, in %lld re-solves\n",
               memcmp(wrs, g->irs, (size_t)nr) == 0 &&
               memcmp(wcs, g->ics, (size_t)nc) == 0 ? "reproduces"
                                                     : "DOES NOT REPRODUCE",
               (long long)resolves);
    jaos_model_free(W); free(wrs); free(wcs);
}

/* K for the side at walk position (is_row, idx, side): the support minus
 * every support side missing from the IIS at an earlier position. */
static int rebuild_k(rp *g, bool is_row, int64_t idx, unsigned char side)
{
    memcpy(g->krs, g->srs, (size_t)g->nr);
    memcpy(g->kcs, g->scs, (size_t)g->nc);
    int dropped = 0;
    const int64_t row_end = is_row ? idx : g->nr;
    for (int64_t r = 0; r < row_end; r++) {
        const unsigned char gone = (unsigned char)(g->srs[r] & ~g->irs[r]);
        if (gone & 1) dropped++;
        if (gone & 2) dropped++;
        g->krs[r] = (unsigned char)(g->srs[r] & ~gone);
    }
    if (is_row) {
        if (side == 2 && (g->srs[idx] & 1) && !(g->irs[idx] & 1)) {
            dropped++;
            g->krs[idx] &= 2;
        }
        return dropped;
    }
    for (int64_t c = 0; c < idx; c++) {
        const unsigned char gone = (unsigned char)(g->scs[c] & ~g->ics[c]);
        if (gone & 1) dropped++;
        if (gone & 2) dropped++;
        g->kcs[c] = (unsigned char)(g->scs[c] & ~gone);
    }
    if (side == 2 && (g->scs[idx] & 1) && !(g->ics[idx] & 1)) {
        dropped++;
        g->kcs[idx] &= 2;
    }
    return dropped;
}

/* Returns 1 when this side was shown (it is not needed by a cold re-solve). */
static int replay_side(rp *g, bool is_row, int64_t idx, unsigned char side)
{
    unsigned char *set = is_row ? g->irs : g->ics;
    if (!(set[idx] & side))
        return 0;
    /* The oracle: the IIS without this side, cold. */
    set[idx] &= (unsigned char)~side;
    jaos_model *o = build(g, g->irs, g->ics);
    const jaos_solve_status ov = verdict(o);
    set[idx] |= side;
    if (ov == JAOS_SOLVE_OPTIMAL) {
        jaos_model_free(o);
        return 0;
    }
    const jaos_model *m = g->m;
    printf("=== %s %lld %s, bounds [%.17g, %.17g]: the IIS without it, cold -> %s\n",
           is_row ? "row" : "column", (long long)idx,
           side == 1 ? "lower" : "upper",
           is_row ? m->row_lower[idx] : m->col_lower[idx],
           is_row ? m->row_upper[idx] : m->col_upper[idx],
           jaos_solve_status_str(ov));
    cert(o, "iis-minus-side");
    jaos_model_free(o);

    const int dropped = rebuild_k(g, is_row, idx, side);
    printf("  sides dropped before it: %d\n", dropped);
    unsigned char *kset = is_row ? g->krs : g->kcs;

    jaos_model *A = build(g, g->krs, g->kcs);
    printf("  A: K cold -> %s\n", jaos_solve_status_str(verdict(A)));
    if (jaos_status_of(A) == JAOS_SOLVE_INFEASIBLE) cert(A, "A-cold");
    const unsigned char kept = (unsigned char)(kset[idx] & ~side);
    const double lo = kept & 1 ? (is_row ? m->row_lower[idx] : m->col_lower[idx]) : -INFINITY;
    const double up = kept & 2 ? (is_row ? m->row_upper[idx] : m->col_upper[idx]) : INFINITY;
    const jaos_status rc = is_row ? jaos_set_row_bounds(A, idx, lo, up)
                                  : jaos_set_col_bounds(A, idx, lo, up);
    if (rc != JAOS_OK) exit(9);
    printf("  A: K minus the side, warm -> %s\n", jaos_solve_status_str(verdict(A)));
    if (jaos_status_of(A) == JAOS_SOLVE_OPTIMAL) point(A, "A-warm"); else cert(A, "A-warm");
    jaos_model_free(A);

    kset[idx] = kept;
    jaos_model *B = build(g, g->krs, g->kcs);
    printf("  B: K minus the side, cold -> %s\n", jaos_solve_status_str(verdict(B)));
    if (jaos_status_of(B) == JAOS_SOLVE_OPTIMAL) point(B, "B-cold"); else cert(B, "B-cold");
    jaos_model_free(B);

    walk(g, is_row, idx, side);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK || jaos_read_mps(m, argv[1]) != JAOS_OK)
        return 9;
    if (jaos_solve(m) != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_INFEASIBLE)
        return 9;
    rp g = {.m = m, .nr = m->num_row, .nc = m->num_col};
    jaos_iis_side *rs = calloc((size_t)g.nr + 1, sizeof *rs);
    jaos_iis_side *cs = calloc((size_t)g.nc + 1, sizeof *cs);
    jaos_iis_report rep;
    if (!rs || !cs || jaos_iis(m, rs, cs, &rep) != JAOS_OK)
        return 9;

    /* The support, by src/iis.c's rule, in double. */
    double *y = malloc((size_t)g.nr * sizeof *y);
    if (!y || jaos_certificate(m, y) != JAOS_OK)
        return 9;
    g.srs = calloc((size_t)g.nr + 1, 1); g.scs = calloc((size_t)g.nc + 1, 1);
    g.irs = malloc((size_t)g.nr + 1);    g.ics = malloc((size_t)g.nc + 1);
    g.krs = malloc((size_t)g.nr + 1);    g.kcs = malloc((size_t)g.nc + 1);
    if (!g.srs || !g.scs || !g.irs || !g.ics || !g.krs || !g.kcs)
        return 9;
    for (int64_t i = 0; i < g.nr; i++) {
        if (y[i] > 0.0 && m->row_lower[i] > -INFINITY) g.srs[i] = 1;
        else if (y[i] < 0.0 && m->row_upper[i] < INFINITY) g.srs[i] = 2;
        g.irs[i] = (unsigned char)rs[i];
    }
    for (int64_t j = 0; j < g.nc; j++) {
        double a = 0.0;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
            a += m->a_value[p] * y[m->a_index[p]];
        if (a > 0.0 && m->col_upper[j] < INFINITY) g.scs[j] = 2;
        else if (a < 0.0 && m->col_lower[j] > -INFINITY) g.scs[j] = 1;
        g.ics[j] = (unsigned char)cs[j];
    }
    printf("members=%lld candidates=%lld solves=%lld\n", (long long)rep.members,
           (long long)rep.candidates, (long long)rep.solves);
    walk(&g, false, -1, 0);

    static const unsigned char sides[2] = {1, 2};
    int shown = 0;
    for (int64_t i = 0; i < g.nr && shown < 3; i++)
        for (int k = 0; k < 2 && shown < 3; k++)
            shown += replay_side(&g, true, i, sides[k]);
    for (int64_t j = 0; j < g.nc && shown < 3; j++)
        for (int k = 0; k < 2 && shown < 3; k++)
            shown += replay_side(&g, false, j, sides[k]);
    if (shown == 0)
        printf("every side of the IIS is needed by a cold re-solve\n");
    jaos_model_free(m);
    return 0;
}
