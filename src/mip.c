/* Mixed-integer linear programming: branch and bound over the dual simplex
 * (D288).
 *
 * The plain Land-Doig scheme as Wolsey states it (Integer Programming,
 * Wiley 1998, ch. 7): a node is the root's relaxation with the bounds of
 * some integer columns tightened; its relaxation is solved warm from its
 * parent's basis; a node whose relaxation is infeasible, or no better than
 * the incumbent, is pruned; one whose solution is integral is a new
 * incumbent; any other is split on its most fractional integer column,
 * down and up. Nodes are taken best bound first, ties by creation order,
 * so the search is the same on every machine and every run (D8). No cuts
 * and no heuristics: the two are separate features and each has its own
 * claim of absence in docs/claims.txt.
 *
 * The relaxations are solved on ONE private copy of the model, re-bounded
 * per node, with the log callback off and everything else the caller set
 * carried over, the same arrangement the IIS uses. The work of every node
 * is billed to the model; the caller's work limit is read between nodes
 * and by each node's own solve.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _POSIX_C_SOURCE 200809L

#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* How far a value may sit from the nearest integer and count as integral,
 * in the model's own units. docs/tolerances.md carries the sweep. */
constexpr double MIP_INT_TOL = 1e-6;
/* The relative gap that closes the search: stop when
 * (incumbent - bound) <= MIP_GAP * (1 + |incumbent|), in minimize form.
 * docs/tolerances.md carries the sweep. jaos_set_mip_gap overrides it. */
constexpr double MIP_GAP = 1e-6;

/* One open node: the bound changes along its path from the root, in the
 * order they were made, and the basis its parent's relaxation ended on. */
typedef struct {
    double key;                /* the parent's objective, minimize form */
    int64_t id;                /* creation order, the tie-break         */
    int64_t depth;             /* changes on the path                   */
    int64_t *col;
    double *lo, *hi;
    jaos_basis_status *cs, *rs;
} bnode;

static void node_free(bnode *n)
{
    if (n == nullptr)
        return;
    free(n->col); free(n->lo); free(n->hi); free(n->cs); free(n->rs);
    free(n);
}

/* A binary min-heap of nodes on (key, id). */
typedef struct {
    bnode **v;
    int64_t n, cap;
} bheap;

static bool node_before(const bnode *a, const bnode *b)
{
    return a->key < b->key || (a->key == b->key && a->id < b->id);
}

static bool heap_push(bheap *h, bnode *x)
{
    if (!JM_GROW(h->v, h->cap, h->n + 1))
        return false;
    int64_t i = h->n++;
    h->v[i] = x;
    while (i > 0) {
        const int64_t p = (i - 1) / 2;
        if (!node_before(h->v[i], h->v[p]))
            break;
        bnode *t = h->v[i]; h->v[i] = h->v[p]; h->v[p] = t;
        i = p;
    }
    return true;
}

static bnode *heap_pop(bheap *h)
{
    if (h->n == 0)
        return nullptr;
    bnode *top = h->v[0];
    h->v[0] = h->v[--h->n];
    int64_t i = 0;
    for (;;) {
        const int64_t l = 2 * i + 1, r = l + 1;
        int64_t best = i;
        if (l < h->n && node_before(h->v[l], h->v[best])) best = l;
        if (r < h->n && node_before(h->v[r], h->v[best])) best = r;
        if (best == i)
            break;
        bnode *t = h->v[i]; h->v[i] = h->v[best]; h->v[best] = t;
        i = best;
    }
    return top;
}

/* A child of `parent` (or of the root when parent is null) with one more
 * change, carrying the basis the relaxation just ended on. */
static bnode *node_child(const bnode *parent, int64_t nc, int64_t nr,
                         const jaos_model *lp, int64_t col, double lo,
                         double hi, double key, int64_t id)
{
    bnode *n = calloc(1, sizeof *n);
    if (n == nullptr)
        return nullptr;
    const int64_t d = (parent ? parent->depth : 0) + 1;
    n->col = malloc((size_t)d * sizeof *n->col);
    n->lo = malloc((size_t)d * sizeof *n->lo);
    n->hi = malloc((size_t)d * sizeof *n->hi);
    n->cs = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *n->cs);
    n->rs = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *n->rs);
    if (!n->col || !n->lo || !n->hi || !n->cs || !n->rs) {
        node_free(n);
        return nullptr;
    }
    if (parent != nullptr) {
        memcpy(n->col, parent->col, (size_t)parent->depth * sizeof *n->col);
        memcpy(n->lo, parent->lo, (size_t)parent->depth * sizeof *n->lo);
        memcpy(n->hi, parent->hi, (size_t)parent->depth * sizeof *n->hi);
    }
    n->col[d - 1] = col;
    n->lo[d - 1] = lo;
    n->hi[d - 1] = hi;
    n->depth = d;
    n->key = key;
    n->id = id;
    if (nc > 0)
        memcpy(n->cs, lp->sol_col_status, (size_t)nc * sizeof *n->cs);
    if (nr > 0)
        memcpy(n->rs, lp->sol_row_status, (size_t)nr * sizeof *n->rs);
    return n;
}

/* Puts the relaxation at `n`: every integer column back at the root's
 * bounds, then the path's changes in order, then the parent's basis. */
static jaos_status node_apply(jaos_model *lp, const jaos_model *m,
                              const bnode *n)
{
    jaos_status st = JAOS_OK;
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++)
        if (m->col_integer[j])
            st = jaos_set_col_bounds(lp, j, m->col_lower[j], m->col_upper[j]);
    for (int64_t k = 0; st == JAOS_OK && n != nullptr && k < n->depth; k++)
        st = jaos_set_col_bounds(lp, n->col[k], n->lo[k], n->hi[k]);
    if (st == JAOS_OK && n != nullptr)
        st = jaos_set_basis(lp, n->cs, n->rs);
    return st;
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* The incumbent: the relaxation's whole published answer, copied when it
 * is found, because the copy of the model moves on to other nodes. */
typedef struct {
    bool have;
    double obj, key;
    double *x, *ra, *rd, *cd;
    jaos_basis_status *cs, *rs;
} incumbent;

static bool incumbent_take(incumbent *inc, const jaos_model *lp, double key)
{
    const int64_t nc = lp->num_col, nr = lp->num_row;
    if (inc->x == nullptr) {
        inc->x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *inc->x);
        inc->cd = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *inc->cd);
        inc->cs = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *inc->cs);
        inc->ra = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *inc->ra);
        inc->rd = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *inc->rd);
        inc->rs = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *inc->rs);
        if (!inc->x || !inc->cd || !inc->cs || !inc->ra || !inc->rd || !inc->rs)
            return false;
    }
    if (nc > 0) {
        memcpy(inc->x, lp->sol_col, (size_t)nc * sizeof *inc->x);
        memcpy(inc->cd, lp->sol_redcost, (size_t)nc * sizeof *inc->cd);
        memcpy(inc->cs, lp->sol_col_status, (size_t)nc * sizeof *inc->cs);
    }
    if (nr > 0) {
        memcpy(inc->ra, lp->sol_row, (size_t)nr * sizeof *inc->ra);
        memcpy(inc->rd, lp->sol_dual, (size_t)nr * sizeof *inc->rd);
        memcpy(inc->rs, lp->sol_row_status, (size_t)nr * sizeof *inc->rs);
    }
    inc->obj = lp->objective;
    inc->key = key;
    inc->have = true;
    return true;
}

static void incumbent_free(incumbent *inc)
{
    free(inc->x); free(inc->ra); free(inc->rd); free(inc->cd);
    free(inc->cs); free(inc->rs);
}

bool jm_model_has_integer(const jaos_model *m)
{
    if (m->col_integer == nullptr)
        return false;
    for (int64_t j = 0; j < m->num_col; j++)
        if (m->col_integer[j])
            return true;
    return false;
}

jaos_status jm_branch_and_bound(jaos_model *m)
{
    const double t0 = now_seconds();
    const int64_t nc = m->num_col, nr = m->num_row;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double gap = m->cfg.mip_gap > 0.0 ? m->cfg.mip_gap : MIP_GAP;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    jaos_model *lp = nullptr;
    bheap heap = {0};
    incumbent inc = {0};
    bnode *cur = nullptr;
    double *x = nullptr;
    int64_t next_id = 0, nodes = 0, solves = 0;
    int64_t work = 0, iters = 0;
    double best_bound = -INFINITY;     /* minimize form */
    jaos_solve_status outcome = JAOS_SOLVE_NOT_RUN;

    /* The answer the model holds is about the previous problem. */
    free(m->mip_inc_x);
    m->mip_inc_x = nullptr;
    m->mip_nodes = m->mip_solves = 0;
    m->mip_bound = 0.0;
    m->mip_has_incumbent = false;

    if (jaos_model_copy(m, &lp) != JAOS_OK)
        goto done;
    free(lp->col_integer);
    lp->col_integer = nullptr;
    lp->cfg.log_cb = nullptr;
    jaos_clear_basis(lp);

    x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x);
    if (x == nullptr)
        goto done;

    /* The root is the node with no changes. */
    for (;;) {
        /* Which node: the root first, then the best open one; a node whose
         * key no longer beats the incumbent is dropped unsolved. */
        if (nodes > 0) {
            node_free(cur);
            cur = heap_pop(&heap);
            if (cur == nullptr) {
                outcome = inc.have ? JAOS_SOLVE_OPTIMAL : JAOS_SOLVE_INFEASIBLE;
                break;
            }
            best_bound = cur->key;
            if (inc.have &&
                inc.key - cur->key <= gap * (1.0 + fabs(inc.key))) {
                outcome = JAOS_SOLVE_OPTIMAL;
                break;
            }
        }
        /* The caller's budgets, read between nodes. Every node's own solve
         * reads them too. */
        if (m->cfg.work_limit > 0 && work >= m->cfg.work_limit) {
            outcome = JAOS_SOLVE_WORK_LIMIT;
            break;
        }
        if (m->cfg.time_limit > 0.0 && now_seconds() - t0 >= m->cfg.time_limit) {
            outcome = JAOS_SOLVE_TIME_LIMIT;
            break;
        }

        if (node_apply(lp, m, nodes > 0 ? cur : nullptr) != JAOS_OK)
            goto done;
        nodes++;
        const jaos_status st = jaos_solve(lp);
        solves++;
        work += jaos_work_units(lp);
        iters += jaos_iterations(lp);
        if (st != JAOS_OK) {
            if (st == JAOS_ERR_NUMERICAL) {
                outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                jm_set_err(m, "node %lld: %s", (long long)nodes,
                           jaos_model_error(lp));
                break;
            }
            goto done;
        }
        const jaos_solve_status ns = jaos_status_of(lp);
        if (ns == JAOS_SOLVE_INFEASIBLE)
            continue;
        if (ns == JAOS_SOLVE_UNBOUNDED) {
            /* The relaxation is unbounded. For rational data that makes
             * the integer program unbounded or infeasible, and this call
             * reports the relaxation's fact, which jaos_unbounded_ray
             * carries from the root. */
            outcome = JAOS_SOLVE_UNBOUNDED;
            break;
        }
        if (ns != JAOS_SOLVE_OPTIMAL) {
            outcome = ns;              /* a limit or an interrupt */
            break;
        }

        double obj = 0.0;
        if (jaos_objective(lp, &obj) != JAOS_OK ||
            jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
            goto done;
        const double key = sigma * obj;
        if (nodes == 1)
            best_bound = key;
        if (inc.have && inc.key - key <= gap * (1.0 + fabs(inc.key)))
            continue;                  /* cannot improve enough */

        /* The most fractional integer column, lowest index on a tie. */
        int64_t branch = -1;
        double worst = MIP_INT_TOL;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j])
                continue;
            const double f = fabs(x[j] - round(x[j]));
            if (f > worst) {
                worst = f;
                branch = j;
            }
        }
        if (branch < 0) {
            if (!incumbent_take(&inc, lp, key))
                goto done;
            for (int64_t j = 0; j < nc; j++)
                if (m->col_integer[j])
                    inc.x[j] = round(inc.x[j]);
            continue;
        }
        const double v = x[branch];
        bnode *down = node_child(nodes > 0 ? cur : nullptr, nc, nr, lp, branch,
                                 lp->col_lower[branch], floor(v), key,
                                 next_id++);
        bnode *up = node_child(nodes > 0 ? cur : nullptr, nc, nr, lp, branch,
                               ceil(v), lp->col_upper[branch], key,
                               next_id++);
        if (down == nullptr || up == nullptr ||
            !heap_push(&heap, down) || !heap_push(&heap, up)) {
            node_free(down);
            node_free(up);
            goto done;
        }
    }
    /* Publish. */
    rc = JAOS_OK;
    m->solve_status = outcome;
    m->solve_work = work;
    m->solve_iters = iters;
    m->solve_time = now_seconds() - t0;
    m->mip_nodes = nodes;
    m->mip_solves = solves;
    m->mip_bound = sigma * (heap.n > 0 && heap.v[0]->key < best_bound
                            ? heap.v[0]->key : best_bound);
    if (outcome == JAOS_SOLVE_OPTIMAL)
        m->mip_bound = inc.obj;
    if (inc.have) {
        m->mip_has_incumbent = true;
        m->mip_inc_obj = inc.obj;
        m->mip_inc_x = inc.x;
        inc.x = nullptr;
    }
    if (outcome == JAOS_SOLVE_OPTIMAL) {
        if (jm_model_ensure_solution_arrays(m) != JAOS_OK) {
            rc = JAOS_ERR_OUT_OF_MEMORY;
            m->solve_status = JAOS_SOLVE_NOT_RUN;
            goto done;
        }
        if (nc > 0) {
            memcpy(m->sol_col, m->mip_inc_x, (size_t)nc * sizeof *m->sol_col);
            memcpy(m->sol_redcost, inc.cd, (size_t)nc * sizeof *m->sol_redcost);
            memcpy(m->sol_col_status, inc.cs, (size_t)nc * sizeof *m->sol_col_status);
        }
        if (nr > 0) {
            memcpy(m->sol_row, inc.ra, (size_t)nr * sizeof *m->sol_row);
            memcpy(m->sol_dual, inc.rd, (size_t)nr * sizeof *m->sol_dual);
            memcpy(m->sol_row_status, inc.rs, (size_t)nr * sizeof *m->sol_row_status);
        }
        jm_model_publish_objective(m);
    }

done:
    if (rc != JAOS_OK && m->solve_status != outcome)
        jm_set_err(m, "%s", m->err[0] ? m->err : "out of memory in branch and bound");
    free(x);
    while (heap.n > 0)
        node_free(heap_pop(&heap));
    free(heap.v);
    node_free(cur);
    incumbent_free(&inc);
    jaos_model_free(lp);
    return rc;
}

jaos_status jaos_mip_result(const jaos_model *m, jaos_mip_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    out->nodes = m->mip_nodes;
    out->lp_solves = m->mip_solves;
    out->has_incumbent = m->mip_has_incumbent;
    out->incumbent = m->mip_has_incumbent ? m->mip_inc_obj : 0.0;
    out->bound = m->mip_bound;
    return JAOS_OK;
}

jaos_status jaos_mip_incumbent(const jaos_model *m, double *col_value,
                               double *objective)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!m->mip_has_incumbent || m->mip_inc_x == nullptr) {
        jm_set_err((jaos_model *)m, "no incumbent: the last solve found no "
                   "integer point");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (col_value != nullptr && m->num_col > 0)
        memcpy(col_value, m->mip_inc_x, (size_t)m->num_col * sizeof *col_value);
    if (objective != nullptr)
        *objective = m->mip_inc_obj;
    return JAOS_OK;
}
