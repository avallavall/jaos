/* Mixed-integer linear programming: branch and bound over the dual simplex
 * (D288), with a dive from each selected node and Gomory mixed-integer
 * cuts at the root (D289).
 *
 * The Land-Doig scheme as Wolsey states it (Integer Programming, Wiley
 * 1998, ch. 7): a node is the root's relaxation with the bounds of some
 * integer columns tightened; its relaxation is solved warm from its
 * parent's basis; a node whose relaxation is infeasible, or no better
 * than the incumbent, is pruned; one whose solution is integral is a new
 * incumbent; any other is split on its most fractional integer column,
 * down and up. The open set is ordered best bound first, ties by creation
 * order. A dive from each selected node -- the child on the nearer side
 * of the fraction solved next, its sibling into the open set, until a
 * node is pruned or integral -- is behind jaos_set_mip_dive and off:
 * over the MIP set it measured 1.125x the work of the plain order, better
 * on one instance and worse on six (D289, refused). The search is the
 * same on every machine and every run (D8).
 *
 * The root's relaxation gets rounds of Gomory mixed-integer cuts before
 * the tree starts: one cut per basic integer column whose value is
 * fractional, read off its tableau row as Balas, Ceria, Cornuejols and
 * Natraj state the cut (Gomory cuts revisited, Operations Research
 * Letters 19, 1996), rewritten over the model's own columns and added as
 * a row of the private copy, where it stays for every node. Cuts are
 * valid for the whole tree because they are derived at the root over the
 * model's own bounds. A rounding heuristic at every fractional node
 * (D290) is the only heuristic; docs/claims.txt carries the claim that
 * there is no other.
 *
 * The relaxations are solved on ONE private copy of the model, re-bounded
 * per node, with the log callback off and everything else the caller set
 * carried over, the same arrangement the IIS uses. The work of every node
 * and of every cut round is billed to the model; the caller's work limit
 * is read between nodes and by each node's own solve.
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

/* The root cuts' four numbers (D289); docs/tolerances.md carries each
 * one's sweep or the reason it has none. */
/* Rounds of cuts at the root; a round with no cut ends them early.
 * jaos_set_mip_cut_rounds overrides it. */
constexpr int64_t MIP_CUT_ROUNDS = 1;
/* A basic integer column is cut only when its fraction sits inside
 * [MIP_CUT_AWAY, 1 - MIP_CUT_AWAY]: the cut's coefficients divide by the
 * fraction and by its complement, and a fraction near 0 or 1 gives a cut
 * the relaxation cannot hold to tolerance. */
constexpr double MIP_CUT_AWAY = 0.01;
/* A coefficient below MIP_CUT_DROP times the cut's largest is folded into
 * the right-hand side through the column's bound, which keeps the cut
 * valid and drops the entry; kept when that bound is infinite. */
constexpr double MIP_CUT_DROP = 1e-9;
/* The largest ratio of a kept cut's largest to smallest coefficient; a
 * cut past it is not added, since the simplex would hold it to
 * tolerance in the large entries and not at all in the small. */
constexpr double MIP_CUT_DYNAMISM = 1e6;

/* The floor of a direction's pseudocost score (D292): the product of the
 * two directions is the score, and a direction whose gain was 0 would
 * otherwise zero the column out of the choice. Achterberg, Koch and
 * Martin (Branching rules revisited, OR Letters 33, 2005) use the same
 * floor. Decides an order, not a number; docs/tolerances.md lists it. */
constexpr double MIP_PC_EPS = 1e-6;

/* Reliability branching (D293): a column whose pseudocost has fewer than
 * MIP_RELIABILITY branches in a direction has that child solved on the
 * spot, for at most MIP_STRONG_CANDIDATES columns per node, taken in
 * score order. jaos_set_mip_reliability overrides the first;
 * docs/tolerances.md carries the sweep of the first and the reason the
 * second is held. */
constexpr int64_t MIP_RELIABILITY = 0;
constexpr int64_t MIP_STRONG_CANDIDATES = 8;

/* One open node: the bound changes along its path from the root, in the
 * order they were made, and the basis its parent's relaxation ended on. */
typedef struct {
    double key;                /* the parent's objective, minimize form */
    int64_t id;                /* creation order, the tie-break         */
    int64_t depth;             /* changes on the path                   */
    int64_t *col;
    double *lo, *hi;
    jaos_basis_status *cs, *rs;
    double frac;               /* the fraction the branch moved         */
    bool up;                   /* which way                              */
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
                         double hi, double key, int64_t id, double frac,
                         bool up)
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
    n->frac = frac;
    n->up = up;
    if (nc > 0)
        memcpy(n->cs, lp->sol_col_status, (size_t)nc * sizeof *n->cs);
    if (nr > 0)
        memcpy(n->rs, lp->sol_row_status, (size_t)nr * sizeof *n->rs);
    return n;
}

/* Puts the relaxation at `n`: every integer column back at the root's
 * bounds -- the model's, rounded inward to integers (D292) -- then the
 * path's changes in order, then the parent's basis. */
static jaos_status node_apply(jaos_model *lp, const jaos_model *m,
                              const double *ilo, const double *ihi,
                              const bnode *n)
{
    jaos_status st = JAOS_OK;
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++)
        if (m->col_integer[j])
            st = jaos_set_col_bounds(lp, j, ilo[j], ihi[j]);
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


/* The most fractional integer column, lowest index on a tie; -1 when the
 * point is integral to MIP_INT_TOL. */
static int64_t most_fractional(const jaos_model *m, const double *x)
{
    int64_t branch = -1;
    double worst = MIP_INT_TOL;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (!m->col_integer[j])
            continue;
        const double f = fabs(x[j] - round(x[j]));
        if (f > worst) {
            worst = f;
            branch = j;
        }
    }
    return branch;
}

/* --- Pseudocost branching (D292) ---------------------------------------- */

/* What a unit move of column j in direction d (0 down, 1 up) has cost the
 * objective so far, in minimize form: the mean of the gains seen, or the
 * mean over every column that has one when j has none, or 1 when the
 * tree has no history at all, which makes the score below the fraction
 * alone. `pc_sum` and `pc_n` are [2 * num_col], direction-major. */
static double pseudocost(int64_t j, int d, int64_t nc, const double *pc_sum,
                         const int64_t *pc_n)
{
    if (pc_n[d * nc + j] > 0)
        return pc_sum[d * nc + j] / (double)pc_n[d * nc + j];
    double sum = 0.0;
    int64_t cnt = 0;
    for (int64_t k = 0; k < nc; k++)
        if (pc_n[d * nc + k] > 0) {
            sum += pc_sum[d * nc + k] / (double)pc_n[d * nc + k];
            cnt++;
        }
    return cnt > 0 ? sum / (double)cnt : 1.0;
}

/* The column to branch on under `rule`, -1 when the point is integral.
 * Pseudocost: the largest product of the two directions' expected gains,
 * each floored at MIP_PC_EPS, lowest index on a tie. */
static int64_t select_branch(const jaos_model *m, const double *x,
                             jaos_branching rule, const double *pc_sum,
                             const int64_t *pc_n)
{
    if (rule == JAOS_BRANCH_MOST_FRACTIONAL)
        return most_fractional(m, x);
    const int64_t nc = m->num_col;
    int64_t branch = -1;
    double best = -1.0, best_away = -1.0;
    for (int64_t j = 0; j < nc; j++) {
        if (!m->col_integer[j])
            continue;
        const double f = x[j] - floor(x[j]);
        if (f <= MIP_INT_TOL || f >= 1.0 - MIP_INT_TOL)
            continue;
        const double qd = f * pseudocost(j, 0, nc, pc_sum, pc_n);
        const double qu = (1.0 - f) * pseudocost(j, 1, nc, pc_sum, pc_n);
        const double score = (qd > MIP_PC_EPS ? qd : MIP_PC_EPS) *
                             (qu > MIP_PC_EPS ? qu : MIP_PC_EPS);
        /* Equal scores -- every one of them, on a model whose objective is
         * zero, where no gain is ever seen -- fall back to the fraction:
         * the column farthest from an integer, then the lowest index. */
        const double away = f < 1.0 - f ? f : 1.0 - f;
        if (score > best || (score == best && away > best_away)) {
            best = score;
            best_away = away;
            branch = j;
        }
    }
    return branch;
}

/* A solved child teaches its column: the gain over its parent per unit
 * of the fraction it moved, in the direction it moved. A gain below zero
 * is the relaxation's tolerance and counts as none. */
static void pseudocost_learn(const bnode *n, double key, int64_t nc,
                             double *pc_sum, int64_t *pc_n)
{
    if (n == nullptr || n->depth == 0 || n->frac <= 0.0)
        return;
    const int64_t j = n->col[n->depth - 1];
    const int d = n->up ? 1 : 0;
    double gain = key - n->key;
    if (gain < 0.0)
        gain = 0.0;
    pc_sum[d * nc + j] += gain / n->frac;
    pc_n[d * nc + j] += 1;
}

/* The pseudocost score of column j at fraction f, the product select_branch
 * ranks by. */
static double pc_score(int64_t j, double f, int64_t nc, const double *pc_sum,
                       const int64_t *pc_n)
{
    const double qd = f * pseudocost(j, 0, nc, pc_sum, pc_n);
    const double qu = (1.0 - f) * pseudocost(j, 1, nc, pc_sum, pc_n);
    return (qd > MIP_PC_EPS ? qd : MIP_PC_EPS) * (qu > MIP_PC_EPS ? qu : MIP_PC_EPS);
}

/* Strong branching on the node the copy holds (D293): the fractional
 * integer columns with fewer than `reliability` branches in some
 * direction, the best MIP_STRONG_CANDIDATES by score, have each such
 * child solved from the node's optimal basis, and the gain seen teaches
 * the pseudocost as a real branch would. An infeasible child teaches
 * nothing. The node is then put back -- its bounds, its basis, one solve
 * that ends where it started -- so what follows sees the node as it was.
 * `cs`, `rs` and `cand` are scratch of the copy's size; every probe is
 * billed to `work` and counted in `solves`. */
static jaos_status strong_probe(jaos_model *lp, const jaos_model *m,
                                const double *x, double key, double sigma,
                                int64_t reliability, double *pc_sum,
                                int64_t *pc_n, int64_t *work, int64_t *solves,
                                jaos_basis_status *cs, jaos_basis_status *rs,
                                int64_t *cand)
{
    const int64_t nc = m->num_col, nr = lp->num_row;
    int64_t ncand = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (!m->col_integer[j])
            continue;
        const double f = x[j] - floor(x[j]);
        if (f <= MIP_INT_TOL || f >= 1.0 - MIP_INT_TOL)
            continue;
        if (pc_n[j] >= reliability && pc_n[nc + j] >= reliability)
            continue;
        /* Insertion by score, descending, earlier index first on a tie:
         * the list is short and the order must be the same everywhere. */
        const double sc = pc_score(j, f, nc, pc_sum, pc_n);
        int64_t p = ncand < MIP_STRONG_CANDIDATES ? ncand : MIP_STRONG_CANDIDATES - 1;
        if (p == MIP_STRONG_CANDIDATES - 1 && ncand == MIP_STRONG_CANDIDATES) {
            const int64_t last = cand[p];
            const double fl = x[last] - floor(x[last]);
            if (sc <= pc_score(last, fl, nc, pc_sum, pc_n))
                continue;
        }
        while (p > 0) {
            const int64_t k = cand[p - 1];
            const double fk = x[k] - floor(x[k]);
            if (pc_score(k, fk, nc, pc_sum, pc_n) >= sc)
                break;
            cand[p] = cand[p - 1];
            p--;
        }
        cand[p] = j;
        if (ncand < MIP_STRONG_CANDIDATES)
            ncand++;
    }
    if (ncand == 0)
        return JAOS_OK;
    if (nc > 0)
        memcpy(cs, lp->sol_col_status, (size_t)nc * sizeof *cs);
    if (nr > 0)
        memcpy(rs, lp->sol_row_status, (size_t)nr * sizeof *rs);
    jaos_status st = JAOS_OK;
    for (int64_t c = 0; c < ncand && st == JAOS_OK; c++) {
        const int64_t j = cand[c];
        const double f = x[j] - floor(x[j]);
        const double lo0 = lp->col_lower[j], hi0 = lp->col_upper[j];
        for (int d = 0; d < 2 && st == JAOS_OK; d++) {
            if (pc_n[d * nc + j] >= reliability)
                continue;
            st = d == 0 ? jaos_set_col_bounds(lp, j, lo0, floor(x[j]))
                        : jaos_set_col_bounds(lp, j, ceil(x[j]), hi0);
            if (st == JAOS_OK)
                st = jaos_set_basis(lp, cs, rs);
            if (st == JAOS_OK)
                st = jaos_solve(lp);
            (*solves)++;
            *work += jaos_work_units(lp);
            if (st == JAOS_OK && jaos_status_of(lp) == JAOS_SOLVE_OPTIMAL) {
                double obj = 0.0;
                if (jaos_objective(lp, &obj) == JAOS_OK) {
                    double gain = sigma * obj - key;
                    if (gain < 0.0)
                        gain = 0.0;
                    pc_sum[d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                    pc_n[d * nc + j] += 1;
                }
            }
            const jaos_status back = jaos_set_col_bounds(lp, j, lo0, hi0);
            if (st == JAOS_OK)
                st = back;
        }
    }
    if (st != JAOS_OK)
        return st;
    /* The node as it was: its optimal basis is a start that ends at once. */
    st = jaos_set_basis(lp, cs, rs);
    if (st == JAOS_OK)
        st = jaos_solve(lp);
    (*solves)++;
    *work += jaos_work_units(lp);
    if (st == JAOS_OK && jaos_status_of(lp) != JAOS_SOLVE_OPTIMAL)
        st = JAOS_ERR_NUMERICAL;
    return st;
}

/* --- Gomory mixed-integer cuts at the root ----------------------------- */

/* The cuts of one round, rows across, held until the tableau they were
 * read from is freed: jaos_add_rows drops the scaling the tableau points
 * into, so a round is generated whole and added whole. */
typedef struct {
    int64_t *start, *idx;
    double *val, *lo;
    int64_t n, nnz;
    int64_t cap_start, cap_lo, cap_idx, cap_val;   /* one each: JM_GROW
                                                      reads its own cap */
} cutbuf;

static void cutbuf_free(cutbuf *cb)
{
    free(cb->start); free(cb->idx); free(cb->val); free(cb->lo);
}

static bool cutbuf_push(cutbuf *cb, const double *cut, int64_t nc, double lo)
{
    if (!JM_GROW(cb->start, cb->cap_start, cb->n + 2) ||
        !JM_GROW(cb->lo, cb->cap_lo, cb->n + 2))
        return false;
    int64_t nz = 0;
    for (int64_t k = 0; k < nc; k++)
        nz += cut[k] != 0.0;
    if (!JM_GROW(cb->idx, cb->cap_idx, cb->nnz + nz) ||
        !JM_GROW(cb->val, cb->cap_val, cb->nnz + nz))
        return false;
    cb->start[cb->n] = cb->nnz;
    for (int64_t k = 0; k < nc; k++) {
        if (cut[k] == 0.0)
            continue;
        cb->idx[cb->nnz] = k;
        cb->val[cb->nnz] = cut[k];
        cb->nnz++;
    }
    cb->lo[cb->n] = lo;
    cb->n++;
    cb->start[cb->n] = cb->nnz;
    return true;
}

/* One round: for every basic integer column of the copy's optimal basis
 * whose value is fractional, in column order, the Gomory mixed-integer
 * cut of its tableau row
 *
 *     x_B + sum_v a_v x_v = b,   f0 = b - floor(b),
 *
 * over the nonbasics shifted to their bounds, x'_v = x_v - l_v at lower or
 * u_v - x_v at upper (which negates a_v):
 *
 *     sum_{v int, f_v <= f0} f_v / f0 x'_v
 *   + sum_{v int, f_v >  f0} (1 - f_v) / (1 - f0) x'_v
 *   + sum_{v cont, a_v >= 0}  a_v / f0 x'_v
 *   + sum_{v cont, a_v <  0} -a_v / (1 - f0) x'_v  >=  1,
 *
 * a nonbasic counting as integer when it is an integer column resting on
 * an integer bound; a logical is its row's activity and always
 * continuous. The shifts go to the right-hand side and a logical's term
 * is spread over its row, so the cut is a row over the model's columns.
 * A row with a free nonbasic in it is skipped: x' has no sign there.
 * Returns the number of cuts put in `cb`, or -1 on failure. */
static int64_t gomory_round(jaos_model *lp, const jaos_model *m,
                            const double *x, cutbuf *cb, double *row,
                            double *cut, int64_t *work)
{
    const int64_t nc = lp->num_col, nr = lp->num_row, nv = nc + nr;
    jm_tableau *tb = nullptr;
    if (jm_tableau_build(lp, &tb) != JAOS_OK)
        return -1;
    int64_t added = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (!m->col_integer[j])
            continue;
        const int64_t p = jm_tableau_position(tb, j);
        if (p < 0)
            continue;
        const double xb = jm_tableau_value(tb, p);
        const double f0 = xb - floor(xb);
        if (f0 < MIP_CUT_AWAY || f0 > 1.0 - MIP_CUT_AWAY)
            continue;
        if (jm_tableau_row(tb, p, row) != JAOS_OK) {
            added = -1;
            break;
        }
        memset(cut, 0, (size_t)nc * sizeof *cut);
        double rhs = 1.0;
        bool ok = true;
        for (int64_t v = 0; v < nv && ok; v++) {
            const double a = row[v];
            if (a == 0.0)
                continue;
            const bool structural = v < nc;
            const int64_t i = v - nc;
            const jaos_basis_status s = structural ? lp->sol_col_status[v]
                                                   : lp->sol_row_status[i];
            double ap, shift;
            bool at_upper;
            if (s == JAOS_BASIS_AT_LOWER) {
                at_upper = false;
                ap = a;
                shift = structural ? lp->col_lower[v] : lp->row_lower[i];
            } else if (s == JAOS_BASIS_AT_UPPER) {
                at_upper = true;
                ap = -a;
                shift = structural ? lp->col_upper[v] : lp->row_upper[i];
            } else {
                ok = false;
                break;
            }
            if (!isfinite(shift)) {
                ok = false;
                break;
            }
            const bool integral = structural && m->col_integer[v] &&
                                  floor(shift) == shift;
            double g;
            if (integral) {
                const double f = ap - floor(ap);
                g = f <= f0 ? f / f0 : (1.0 - f) / (1.0 - f0);
            } else {
                g = ap >= 0.0 ? ap / f0 : -ap / (1.0 - f0);
            }
            if (g == 0.0)
                continue;
            /* g x'_v = g (x_v - l) or g (u - x_v); the constant crosses. */
            const double sgn = at_upper ? -g : g;
            rhs += sgn * shift;
            if (structural) {
                cut[v] += sgn;
            } else {
                for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++)
                    cut[lp->ar_index[k]] += sgn * lp->ar_value[k];
            }
        }
        if (!ok)
            continue;
        double amax = 0.0;
        for (int64_t k = 0; k < nc; k++)
            if (fabs(cut[k]) > amax)
                amax = fabs(cut[k]);
        if (amax == 0.0)
            continue;
        double amin = INFINITY;
        int64_t nnz = 0;
        for (int64_t k = 0; k < nc; k++) {
            const double c = cut[k];
            if (c == 0.0)
                continue;
            if (fabs(c) < MIP_CUT_DROP * amax) {
                /* c x_k >= rhs - rest; the term is largest at the bound
                 * its sign points to, and a finite one absorbs it. */
                const double b = c > 0.0 ? lp->col_upper[k] : lp->col_lower[k];
                if (isfinite(b)) {
                    rhs -= c * b;
                    cut[k] = 0.0;
                    continue;
                }
            }
            if (fabs(c) < amin)
                amin = fabs(c);
            nnz++;
        }
        if (nnz == 0 || amax / amin > MIP_CUT_DYNAMISM)
            continue;
        double act = 0.0;
        for (int64_t k = 0; k < nc; k++)
            act += cut[k] * x[k];
        if (!(rhs - act > 0.0))
            continue;
        if (!cutbuf_push(cb, cut, nc, rhs)) {
            added = -1;
            break;
        }
        added++;
    }
    *work += jm_tableau_work(tb);
    jm_tableau_free(tb);
    return added;
}

static jaos_status cuts_add(jaos_model *lp, const cutbuf *cb)
{
    double *up = malloc((size_t)cb->n * sizeof *up);
    if (up == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t r = 0; r < cb->n; r++)
        up[r] = INFINITY;
    const jaos_status st = jaos_add_rows(lp, cb->n, cb->lo, up, cb->nnz,
                                         cb->start, cb->idx, cb->val);
    free(up);
    return st;
}

/* --- The rounding heuristic (D290) ------------------------------------- */

/* The relaxation's point with every integer column rounded to the nearest
 * integer, judged against the model's own bounds and rows to the primal
 * tolerance; true when it is feasible, with its objective in `obj` (the
 * model's sense) and its row activities in `ra`. The cut rows are not
 * consulted: a point inside the model's rows is a point of the integer
 * program whatever the cuts say. Every sum runs in index order (D8). */
static bool rounded_point(const jaos_model *m, const double *x, double *xr,
                          double *ra, double *obj)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    const double tol = jm_primal_tolerance(m);
    for (int64_t j = 0; j < nc; j++) {
        double v = m->col_integer[j] ? round(x[j]) : x[j];
        if (v < m->col_lower[j] - tol || v > m->col_upper[j] + tol)
            return false;
        xr[j] = v;
    }
    for (int64_t i = 0; i < nr; i++)
        ra[i] = 0.0;
    double z = m->obj_offset;
    for (int64_t j = 0; j < nc; j++) {
        const double v = xr[j];
        if (v == 0.0)
            continue;
        z += m->col_cost[j] * v;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            ra[m->a_index[k]] += m->a_value[k] * v;
    }
    for (int64_t i = 0; i < nr; i++)
        if (ra[i] < m->row_lower[i] - tol || ra[i] > m->row_upper[i] + tol)
            return false;
    *obj = z;
    return true;
}

/* Takes a heuristic point as the incumbent: its own values and activities,
 * with the duals, reduced costs and statuses of the relaxation it was
 * rounded from, since a rounded point has no basis of its own. */
static bool incumbent_take_point(incumbent *inc, const jaos_model *lp,
                                 const jaos_model *m, const double *xr,
                                 const double *ra, double obj, double key)
{
    if (!incumbent_take(inc, lp, key))
        return false;
    const int64_t nc = m->num_col, nr = m->num_row;
    if (nc > 0)
        memcpy(inc->x, xr, (size_t)nc * sizeof *inc->x);
    if (nr > 0)
        memcpy(inc->ra, ra, (size_t)nr * sizeof *inc->ra);
    inc->obj = obj;
    return true;
}

/* How often the tree says where it is, at JAOS_LOG_PROGRESS: every this
 * many nodes. Decides nothing. */
constexpr int64_t MIP_LOG_EVERY = 100;

/* Tells the caller of a new incumbent (D291) and returns false when it
 * asks the search to stop. The bound is the best any open node could
 * still reach, in the model's sense. */
static bool incumbent_announce(const jaos_model *m, const incumbent *inc,
                               int64_t node, double bound, bool by_rounding)
{
    if (m->cfg.incumbent_cb == nullptr)
        return true;
    const jaos_incumbent ev = {
        .node = node,
        .objective = inc->obj,
        .bound = bound,
        .col_value = inc->x,
        .num_col = m->num_col,
        .by_rounding = by_rounding,
    };
    return m->cfg.incumbent_cb(&ev, m->cfg.incumbent_user) !=
           JAOS_CALLBACK_STOP;
}

/* --- The tree ---------------------------------------------------------- */

jaos_status jm_branch_and_bound(jaos_model *m)
{
    const double t0 = now_seconds();
    const int64_t nc = m->num_col, nr = m->num_row;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double gap = m->cfg.mip_gap > 0.0 ? m->cfg.mip_gap : MIP_GAP;
    const int64_t rounds = m->cfg.mip_cut_rounds_set ? m->cfg.mip_cut_rounds
                                                     : MIP_CUT_ROUNDS;
    const bool dive = m->cfg.mip_dive;
    const bool heur = !m->cfg.mip_no_heuristics;
    const jaos_branching rule = (jaos_branching)m->cfg.mip_branching;
    const int64_t reliability = rule == JAOS_BRANCH_PSEUDOCOST
        ? (m->cfg.mip_reliability_set ? m->cfg.mip_reliability : MIP_RELIABILITY)
        : 0;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    jaos_model *lp = nullptr;
    bheap heap = {0};
    incumbent inc = {0};
    cutbuf cb = {0};
    bnode *cur = nullptr, *next = nullptr;
    double *x = nullptr, *row = nullptr, *cut = nullptr;
    double *xr = nullptr, *ra = nullptr;
    double *ilo = nullptr, *ihi = nullptr, *pc_sum = nullptr;
    int64_t *pc_n = nullptr, *cand = nullptr;
    jaos_basis_status *pcs = nullptr, *prs = nullptr;
    int64_t next_id = 0, nodes = 0, solves = 0, cuts = 0, heur_points = 0;
    int64_t first_inc = 0;             /* the node of the first incumbent */
    int64_t work = 0, iters = 0;
    double best_bound = -INFINITY;     /* minimize form */
    jaos_solve_status outcome = JAOS_SOLVE_NOT_RUN;

    /* The answer the model holds is about the previous problem. */
    free(m->mip_inc_x);
    m->mip_inc_x = nullptr;
    m->mip_nodes = m->mip_solves = m->mip_cuts = m->mip_heur = 0;
    m->mip_first_inc = 0;
    m->mip_bound = 0.0;
    m->mip_has_incumbent = false;

    if (jaos_model_copy(m, &lp) != JAOS_OK)
        goto done;
    free(lp->col_integer);
    lp->col_integer = nullptr;
    lp->cfg.log_cb = nullptr;
    jaos_clear_basis(lp);

    x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x);
    xr = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *xr);
    ra = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *ra);
    ilo = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *ilo);
    ihi = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *ihi);
    pc_sum = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_sum);
    pc_n = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_n);
    cand = malloc((size_t)MIP_STRONG_CANDIDATES * sizeof *cand);
    pcs = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pcs);
    if (x == nullptr || xr == nullptr || ra == nullptr || ilo == nullptr ||
        ihi == nullptr || pc_sum == nullptr || pc_n == nullptr ||
        cand == nullptr || pcs == nullptr)
        goto done;
    /* An integer column's bounds rounded inward (D292): a fractional bound
     * admits no integer between it and the next one, and a column whose
     * two rounded bounds cross has no integer at all, which is the whole
     * program's answer before a relaxation is solved. */
    for (int64_t j = 0; j < nc; j++) {
        ilo[j] = m->col_integer[j] ? ceil(m->col_lower[j]) : m->col_lower[j];
        ihi[j] = m->col_integer[j] ? floor(m->col_upper[j]) : m->col_upper[j];
        if (m->col_integer[j] && ilo[j] > ihi[j])
            outcome = JAOS_SOLVE_INFEASIBLE;
    }
    if (jm_logging_at(m, JAOS_LOG_SUMMARY)) {
        int64_t nint = 0;
        for (int64_t j = 0; j < nc; j++)
            nint += m->col_integer[j];
        jm_log(m, JAOS_LOG_SUMMARY,
               "branch and bound: %lld integer columns of %lld, %lld rounds "
               "of cuts, dive %s, rounding %s, %s branching, reliability %lld",
               (long long)nint, (long long)nc, (long long)rounds,
               dive ? "on" : "off", heur ? "on" : "off",
               rule == JAOS_BRANCH_MOST_FRACTIONAL ? "most-fractional"
                                                   : "pseudocost",
               (long long)reliability);
    }

    /* The root is the node with no changes. An outcome already known --
     * a column with no integer inside its bounds -- skips the tree. */
    for (; outcome == JAOS_SOLVE_NOT_RUN;) {
        /* Which node: the root first; then the dive's child, or the best
         * open one; a node whose key no longer beats the incumbent is
         * dropped unsolved, which ends a dive. */
        if (nodes > 0) {
            node_free(cur);
            for (;;) {
                if (next != nullptr) {
                    cur = next;
                    next = nullptr;
                } else {
                    cur = heap_pop(&heap);
                    if (cur == nullptr)
                        break;
                    best_bound = cur->key;
                }
                if (inc.have &&
                    inc.key - cur->key <= gap * (1.0 + fabs(inc.key))) {
                    node_free(cur);
                    cur = nullptr;
                    continue;
                }
                break;
            }
            if (cur == nullptr) {
                outcome = inc.have ? JAOS_SOLVE_OPTIMAL : JAOS_SOLVE_INFEASIBLE;
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
        if (m->cfg.mip_node_limit > 0 && nodes >= m->cfg.mip_node_limit) {
            outcome = JAOS_SOLVE_NODE_LIMIT;
            break;
        }

        if (node_apply(lp, m, ilo, ihi, nodes > 0 ? cur : nullptr) != JAOS_OK)
            goto done;
        nodes++;
        jaos_status st = jaos_solve(lp);
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
        jaos_solve_status ns = jaos_status_of(lp);
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
        double key = sigma * obj;
        if (nodes > 1)
            pseudocost_learn(cur, key, nc, pc_sum, pc_n);
        int64_t branch = select_branch(m, x, rule, pc_sum, pc_n);

        /* The root's cuts: rounds until one adds nothing or the point is
         * integral. A relaxation the cuts make infeasible is an infeasible
         * integer program, since every cut is valid for it. The tableau
         * row spans the columns and every row the copy can come to hold. */
        if (nodes == 1 && branch >= 0 && rounds > 0) {
            row = malloc((size_t)(nc + lp->num_row + 1 +
                                  (nc > 0 ? nc : 1) * rounds) * sizeof *row);
            cut = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *cut);
            if (row == nullptr || cut == nullptr)
                goto done;
            bool stop = false;
            for (int64_t r = 0; r < rounds && !stop; r++) {
                cb.n = cb.nnz = 0;
                const int64_t got = gomory_round(lp, m, x, &cb, row, cut,
                                                 &work);
                if (got < 0)
                    goto done;
                if (got == 0)
                    break;
                if (cuts_add(lp, &cb) != JAOS_OK)
                    goto done;
                cuts += got;
                st = jaos_solve(lp);
                solves++;
                work += jaos_work_units(lp);
                iters += jaos_iterations(lp);
                if (st != JAOS_OK) {
                    if (st == JAOS_ERR_NUMERICAL) {
                        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                        jm_set_err(m, "root cuts, round %lld: %s",
                                   (long long)(r + 1), jaos_model_error(lp));
                        stop = true;
                        break;
                    }
                    goto done;
                }
                ns = jaos_status_of(lp);
                if (ns != JAOS_SOLVE_OPTIMAL) {
                    outcome = ns;
                    stop = true;
                    break;
                }
                if (jaos_objective(lp, &obj) != JAOS_OK ||
                    jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
                    goto done;
                key = sigma * obj;
                branch = select_branch(m, x, rule, pc_sum, pc_n);
                if (branch < 0)
                    break;
            }
            if (stop)
                break;
        }
        if (nodes == 1) {
            best_bound = key;
            jm_log(m, JAOS_LOG_SUMMARY, "root: relaxation %.17g after %lld cuts",
                   obj, (long long)cuts);
        }
        /* The rounding heuristic, on a fractional node: a feasible rounding
         * that beats the incumbent is taken before the node is judged, so
         * the node itself is pruned when its bound is now inside the gap. */
        if (heur && branch >= 0) {
            double hobj = 0.0;
            /* One pass over the matrix, billed like any kernel (D16). */
            work += m->num_nz + nc + nr;
            if (rounded_point(m, x, xr, ra, &hobj)) {
                const double hkey = sigma * hobj;
                if (!inc.have || hkey < inc.key) {
                    if (!incumbent_take_point(&inc, lp, m, xr, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "node %lld: incumbent %.17g by rounding",
                           (long long)nodes, hobj);
                    if (!incumbent_announce(m, &inc, nodes,
                            sigma * (heap.n > 0 && heap.v[0]->key < key
                                     ? heap.v[0]->key : key), true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
        }
        if (nodes % MIP_LOG_EVERY == 0)
            jm_log(m, JAOS_LOG_PROGRESS,
                   "node %lld: %lld open, bound %.17g, incumbent %s",
                   (long long)nodes, (long long)heap.n,
                   sigma * (heap.n > 0 && heap.v[0]->key < best_bound
                            ? heap.v[0]->key : best_bound),
                   inc.have ? "yes" : "none");
        if (inc.have && inc.key - key <= gap * (1.0 + fabs(inc.key)))
            continue;                  /* cannot improve enough */

        if (branch < 0) {
            if (!incumbent_take(&inc, lp, key))
                goto done;
            if (first_inc == 0)
                first_inc = nodes;
            for (int64_t j = 0; j < nc; j++)
                if (m->col_integer[j])
                    inc.x[j] = round(inc.x[j]);
            jm_log(m, JAOS_LOG_PROGRESS, "node %lld: incumbent %.17g, integral",
                   (long long)nodes, obj);
            if (!incumbent_announce(m, &inc, nodes,
                    sigma * (heap.n > 0 && heap.v[0]->key < key
                             ? heap.v[0]->key : key), false)) {
                outcome = JAOS_SOLVE_INTERRUPTED;
                break;
            }
            continue;
        }
        const int64_t nrl = lp->num_row;
        /* Strong branching on the unreliable candidates, then the choice
         * again with what they taught (D293). The row scratch follows the
         * copy, which the root's cuts may have widened. */
        if (reliability > 0) {
            jaos_basis_status *grown = realloc(prs, (size_t)(nrl > 0 ? nrl : 1)
                                                        * sizeof *prs);
            if (grown == nullptr)
                goto done;
            prs = grown;
            const jaos_status ps = strong_probe(lp, m, x, key, sigma,
                                                reliability, pc_sum, pc_n,
                                                &work, &solves, pcs, prs, cand);
            if (ps != JAOS_OK) {
                if (ps == JAOS_ERR_NUMERICAL) {
                    outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                    jm_set_err(m, "node %lld, strong branching: %s",
                               (long long)nodes, jaos_model_error(lp));
                    break;
                }
                goto done;
            }
            branch = select_branch(m, x, rule, pc_sum, pc_n);
        }
        const double v = x[branch];
        bnode *down = node_child(nodes > 0 ? cur : nullptr, nc, nrl, lp,
                                 branch, lp->col_lower[branch], floor(v), key,
                                 next_id++, v - floor(v), false);
        bnode *up = node_child(nodes > 0 ? cur : nullptr, nc, nrl, lp, branch,
                               ceil(v), lp->col_upper[branch], key,
                               next_id++, ceil(v) - v, true);
        if (down == nullptr || up == nullptr) {
            node_free(down);
            node_free(up);
            goto done;
        }
        /* The dive takes the nearer side next; a fraction of one half goes
         * down. Off, both children join the open set. */
        bnode *first = v - floor(v) < 0.5 ? down : up;
        bnode *other = first == down ? up : down;
        if (dive) {
            if (!heap_push(&heap, other)) {
                node_free(down);
                node_free(up);
                goto done;
            }
            next = first;
        } else if (!heap_push(&heap, down)) {
            node_free(down);
            node_free(up);
            goto done;
        } else if (!heap_push(&heap, up)) {
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
    m->mip_cuts = cuts;
    m->mip_heur = heur_points;
    m->mip_first_inc = first_inc;
    m->mip_bound = sigma * (heap.n > 0 && heap.v[0]->key < best_bound
                            ? heap.v[0]->key : best_bound);
    if (outcome == JAOS_SOLVE_OPTIMAL)
        m->mip_bound = inc.obj;
    jm_log(m, JAOS_LOG_SUMMARY,
           "branch and bound: %s after %lld nodes, %lld solves, %lld cuts, "
           "%lld points by rounding", jaos_solve_status_str(outcome),
           (long long)nodes, (long long)solves, (long long)cuts,
           (long long)heur_points);
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
        /* The model's own rows come first in the copy; a cut's row is
         * after them and is not published. */
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
    free(xr);
    free(ra);
    free(ilo);
    free(ihi);
    free(pc_sum);
    free(pc_n);
    free(cand);
    free(pcs);
    free(prs);
    free(row);
    free(cut);
    cutbuf_free(&cb);
    while (heap.n > 0)
        node_free(heap_pop(&heap));
    free(heap.v);
    node_free(cur);
    node_free(next);
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
    out->cuts = m->mip_cuts;
    out->heuristic_points = m->mip_heur;
    out->first_incumbent_node = m->mip_first_inc;
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
