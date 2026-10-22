/* The reading behind 02-439: 02-433's sense flip on files. Each model is
 * solved as it is, then with its costs, its quadratic objective and its
 * offset negated and its sense the other one. Negating a double is exact,
 * so a solver that reads the sense the same way everywhere takes the same
 * path on both: the same status, the objective with the other sign, the
 * same iterations, work units and nodes.
 *
 * Usage: flipfile WORK_LIMIT FILE...
 *   prints one line a file: the two statuses, objectives (the incumbent
 *   and bound of a tree stopped by the limit), iterations, work units and
 *   nodes, and a verdict: "same" (every number matches), "path" (the
 *   statuses match and the objectives agree within 1e-9 of (1 + |a|) but
 *   a count differs) or "DIFF".
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

static jaos_status negate(jaos_model *b)
{
    const int64_t n = jaos_num_col(b);
    for (int64_t j = 0; j < n; j++) {
        double c = 0.0;
        jaos_status st = jaos_col_cost(b, j, &c);
        if (st == JAOS_OK)
            st = jaos_set_col_cost(b, j, c == 0.0 ? 0.0 : -c);
        if (st != JAOS_OK)
            return st;
    }
    const int64_t nq = jaos_quadratic_nz(b);
    if (nq > 0) {
        int64_t *r = malloc((size_t)nq * sizeof *r);
        int64_t *c = malloc((size_t)nq * sizeof *c);
        double *v = malloc((size_t)nq * sizeof *v);
        jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
        if (r != nullptr && c != nullptr && v != nullptr)
            st = jaos_quadratic(b, r, c, v);
        if (st == JAOS_OK) {
            for (int64_t p = 0; p < nq; p++)
                v[p] = -v[p];
            st = jaos_set_quadratic(b, nq, r, c, v);
        }
        free(r);
        free(c);
        free(v);
        if (st != JAOS_OK)
            return st;
    }
    double off = 0.0;
    jaos_obj_sense sense = JAOS_MINIMIZE;
    jaos_status st = jaos_objective_offset(b, &off);
    if (st == JAOS_OK)
        st = jaos_set_objective_offset(b, off == 0.0 ? 0.0 : -off);
    if (st == JAOS_OK)
        st = jaos_objective_sense(b, &sense);
    if (st == JAOS_OK)
        st = jaos_set_objective_sense(b, sense == JAOS_MINIMIZE ? JAOS_MAXIMIZE
                                                                : JAOS_MINIMIZE);
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
    return (isnan(a) && isnan(b)) || a == -b;
}

static bool near_num(double a, double b)
{
    return (isnan(a) && isnan(b)) || fabs(a + b) <= 1e-9 * (1.0 + fabs(a));
}

int main(int argc, char **argv)
{
    if (argc < 3)
        return 2;
    const int64_t limit = strtoll(argv[1], nullptr, 10);
    for (int k = 2; k < argc; k++) {
        const char *path = argv[k];
        const char *name = strrchr(path, '/') != nullptr ? strrchr(path, '/') + 1
                                                         : path;
        jaos_model *a = nullptr, *b = nullptr;
        if (jaos_model_new(&a) != JAOS_OK || load(a, path) != JAOS_OK ||
            jaos_model_copy(a, &b) != JAOS_OK || negate(b) != JAOS_OK ||
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
        const bool near = status && near_num(ra.obj, rb.obj) &&
                          near_num(ra.inc, rb.inc);
        printf("%-24s %-6s %s/%s obj %.17g/%.17g inc %.17g/%.17g iters "
               "%" PRId64 "/%" PRId64 " work %" PRId64 "/%" PRId64
               " nodes %" PRId64 "/%" PRId64 "\n",
               name, exact ? "same" : near ? "path" : "DIFF", ra.status,
               rb.status, ra.obj, -rb.obj, ra.inc, -rb.inc, ra.iters,
               rb.iters, ra.work, rb.work, ra.nodes, rb.nodes);
        fflush(stdout);
        jaos_model_free(a);
        jaos_model_free(b);
    }
    return 0;
}
