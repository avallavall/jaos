/* The reading behind 02-440: 02-435's mirror on files. Each model is
 * solved as it is, then with each column mirrored and each row negated
 * with chance 1/2. A mirrored column x is -x: its bounds swap sides and
 * change sign, and its cost, coefficients and quadratic terms with one
 * other mirrored column change sign. A negated row a'x in [l, u] is
 * -a'x in [-u, -l], its quadratic part included. Both are exact in
 * floating point and leave the same model. Cone heads, SOS members,
 * semi-continuous columns and indicator columns stay as they are.
 *
 * Usage: mirrorfile WORK_LIMIT SEED FILE...
 *   prints one line a file: the two statuses, objectives (the incumbent
 *   and bound of a tree stopped by the limit), iterations, work units and
 *   nodes, and a verdict: "same" (every number matches), "path" (the
 *   statuses match and an optimum's objectives agree within 1e-9 of
 *   (1 + |a|), but a count or a stopped tree's incumbent differs) or
 *   "DIFF".
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ends(const char *s, const char *t)
{
    const size_t n = strlen(s), k = strlen(t);
    return n >= k && strcmp(s + n - k, t) == 0;
}

static jaos_status load(jaos_model *m, const char *path)
{
    if (ends(path, ".cbf") || ends(path, ".cbf.gz"))
        return jaos_read_cbf(m, path);
    if (ends(path, ".qplib") || ends(path, ".qplib.gz"))
        return jaos_read_qplib(m, path);
    return jaos_read_mps(m, path);
}

static uint64_t draw(uint64_t *s)
{
    uint64_t z = (*s += 0x9e3779b97f4a7c15u);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
    return z ^ (z >> 31);
}

static double neg(double v)
{
    return v == 0.0 ? 0.0 : -v;
}

static jaos_status mirror_quadratic(jaos_model *b, const double *sc,
                                    int64_t *r, int64_t *c, double *v)
{
    const int64_t nq = jaos_quadratic_nz(b);
    if (nq == 0)
        return JAOS_OK;
    jaos_status st = jaos_quadratic(b, r, c, v);
    if (st != JAOS_OK)
        return st;
    for (int64_t p = 0; p < nq; p++)
        if (sc[r[p]] * sc[c[p]] < 0.0)
            v[p] = neg(v[p]);
    return jaos_set_quadratic(b, nq, r, c, v);
}

static jaos_status mirror(jaos_model *b, uint64_t seed)
{
    const int64_t n = jaos_num_col(b), m = jaos_num_row(b);
    int64_t big = jaos_num_nz(b) > jaos_quadratic_nz(b) ? jaos_num_nz(b)
                                                        : jaos_quadratic_nz(b);
    for (int64_t i = 0; i < m; i++)
        if (jaos_row_quadratic_nz(b, i) > big)
            big = jaos_row_quadratic_nz(b, i);
    if (n > big)
        big = n;
    double *sc = malloc((size_t)(n > 0 ? n : 1) * sizeof *sc);
    double *sr = malloc((size_t)(m > 0 ? m : 1) * sizeof *sr);
    int64_t *ri = malloc((size_t)(big > 0 ? big : 1) * sizeof *ri);
    int64_t *ci = malloc((size_t)(big > 0 ? big : 1) * sizeof *ci);
    double *v = malloc((size_t)(big > 0 ? big : 1) * sizeof *v);
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (sc == nullptr || sr == nullptr || ri == nullptr || ci == nullptr ||
        v == nullptr)
        goto out;
    for (int64_t j = 0; j < n; j++)
        sc[j] = (draw(&seed) & 1) != 0 ? -1.0 : 1.0;
    for (int64_t i = 0; i < m; i++)
        sr[i] = (draw(&seed) & 1) != 0 ? -1.0 : 1.0;

    st = JAOS_OK;
    for (int64_t k = 0; st == JAOS_OK && k < jaos_num_cones(b); k++) {
        jaos_cone_type type;
        int64_t len = 0;
        st = jaos_cone(b, k, &type, &len, ri);
        if (st == JAOS_OK && len > 0)
            sc[ri[0]] = 1.0;
        if (st == JAOS_OK && type == JAOS_CONE_ROTATED && len > 1)
            sc[ri[1]] = 1.0;
    }
    for (int64_t k = 0; st == JAOS_OK && k < jaos_num_sos(b); k++) {
        int type = 0;
        int64_t len = 0;
        st = jaos_sos(b, k, &type, &len, ri, v);
        for (int64_t p = 0; p < len; p++)
            sc[ri[p]] = 1.0;
    }
    for (int64_t j = 0; st == JAOS_OK && j < n; j++) {
        bool semi = false;
        st = jaos_col_semicontinuous(b, j, &semi);
        if (semi)
            sc[j] = 1.0;
    }
    for (int64_t i = 0; st == JAOS_OK && i < m; i++) {
        int64_t col = -1;
        st = jaos_row_indicator(b, i, &col, nullptr);
        if (col >= 0)
            sc[col] = 1.0;
    }

    for (int64_t j = 0; st == JAOS_OK && j < n; j++) {
        if (sc[j] < 0.0) {
            double c = 0.0, lo = 0.0, up = 0.0;
            st = jaos_col_cost(b, j, &c);
            if (st == JAOS_OK)
                st = jaos_set_col_cost(b, j, neg(c));
            if (st == JAOS_OK)
                st = jaos_col_bounds(b, j, &lo, &up);
            if (st == JAOS_OK)
                st = jaos_set_col_bounds(b, j, neg(up), neg(lo));
        }
        int64_t cnt = 0;
        if (st == JAOS_OK)
            st = jaos_col_entries(b, j, &cnt, ri, v);
        for (int64_t p = 0; st == JAOS_OK && p < cnt; p++)
            if (sc[j] * sr[ri[p]] < 0.0)
                st = jaos_set_coefficient(b, ri[p], j, neg(v[p]));
    }
    for (int64_t i = 0; st == JAOS_OK && i < m; i++) {
        if (sr[i] < 0.0) {
            double lo = 0.0, up = 0.0;
            st = jaos_row_bounds(b, i, &lo, &up);
            if (st == JAOS_OK)
                st = jaos_set_row_bounds(b, i, neg(up), neg(lo));
        }
        const int64_t nq = jaos_row_quadratic_nz(b, i);
        if (st != JAOS_OK || nq == 0)
            continue;
        st = jaos_row_quadratic(b, i, ri, ci, v);
        for (int64_t p = 0; p < nq; p++)
            if (sr[i] * sc[ri[p]] * sc[ci[p]] < 0.0)
                v[p] = neg(v[p]);
        if (st == JAOS_OK)
            st = jaos_set_row_quadratic(b, i, nq, ri, ci, v);
    }
    if (st == JAOS_OK)
        st = mirror_quadratic(b, sc, ri, ci, v);
out:
    free(sc);
    free(sr);
    free(ri);
    free(ci);
    free(v);
    return st;
}

typedef struct {
    const char *status;
    double obj, inc, bound;
    int64_t iters, work, nodes;
} result;

static void run(jaos_model *m, result *r)
{
    const jaos_status st = jaos_solve(m);
    r->status = st != JAOS_OK ? "error"
                              : jaos_solve_status_str(jaos_status_of(m));
    r->obj = r->inc = r->bound = NAN;
    if (st == JAOS_OK && jaos_status_of(m) == JAOS_SOLVE_OPTIMAL)
        (void)jaos_objective(m, &r->obj);
    jaos_mip_report rep;
    r->nodes = 0;
    if (jaos_mip_result(m, &rep) == JAOS_OK) {
        r->nodes = rep.nodes;
        if (rep.has_incumbent)
            r->inc = rep.incumbent;
        r->bound = rep.bound;
    }
    r->iters = jaos_iterations(m);
    r->work = jaos_work_units(m);
}

static bool same_num(double a, double b)
{
    return (isnan(a) && isnan(b)) || a == b;
}

static bool near_num(double a, double b)
{
    return (isnan(a) && isnan(b)) || fabs(a - b) <= 1e-9 * (1.0 + fabs(a));
}

int main(int argc, char **argv)
{
    if (argc < 4)
        return 2;
    const int64_t limit = strtoll(argv[1], nullptr, 10);
    const uint64_t seed = strtoull(argv[2], nullptr, 10);
    for (int k = 3; k < argc; k++) {
        const char *path = argv[k];
        const char *name = strrchr(path, '/') != nullptr ? strrchr(path, '/') + 1
                                                         : path;
        jaos_model *a = nullptr, *b = nullptr;
        if (jaos_model_new(&a) != JAOS_OK || load(a, path) != JAOS_OK ||
            jaos_model_copy(a, &b) != JAOS_OK || mirror(b, seed) != JAOS_OK ||
            jaos_set_work_limit(a, limit) != JAOS_OK ||
            jaos_set_work_limit(b, limit) != JAOS_OK) {
            printf("%-24s unread\n", name);
            jaos_model_free(a);
            jaos_model_free(b);
            continue;
        }
        result ra, rb;
        run(a, &ra);
        run(b, &rb);
        const bool status = strcmp(ra.status, rb.status) == 0;
        const bool exact = status && same_num(ra.obj, rb.obj) &&
                           same_num(ra.inc, rb.inc) &&
                           same_num(ra.bound, rb.bound) &&
                           ra.iters == rb.iters && ra.work == rb.work &&
                           ra.nodes == rb.nodes;
        const bool near = status && near_num(ra.obj, rb.obj);
        printf("%-24s %-6s %s/%s obj %.17g/%.17g inc %.17g/%.17g iters "
               "%" PRId64 "/%" PRId64 " work %" PRId64 "/%" PRId64
               " nodes %" PRId64 "/%" PRId64 "\n",
               name, exact ? "same" : near ? "path" : "DIFF", ra.status,
               rb.status, ra.obj, rb.obj, ra.inc, rb.inc, ra.iters, rb.iters,
               ra.work, rb.work, ra.nodes, rb.nodes);
        fflush(stdout);
        jaos_model_free(a);
        jaos_model_free(b);
    }
    return 0;
}
