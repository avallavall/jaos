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
 * on one instance and worse on six (D289, refused). With
 * jaos_set_mip_dive_backtrack the dive keeps its siblings on a stack and
 * resumes from the deepest one when a node ends, up to that many times
 * per dive (D308), or while the sibling's bound is within
 * jaos_set_mip_dive_gap's fraction of the best open node's (D311). The
 * search is the same on every machine and every run (D8).
 *
 * The root's relaxation gets rounds of Gomory mixed-integer cuts before
 * the tree starts: one cut per basic integer column whose value is
 * fractional, read off its tableau row as Balas, Ceria, Cornuejols and
 * Natraj state the cut (Gomory cuts revisited, Operations Research
 * Letters 19, 1996), rewritten over the model's own columns and added as
 * a row of the private copy, where it stays for every node under which
 * its slack is never basic: below a node where it is, it leaves, like a
 * node's own cut, unless jaos_set_mip_root_cut_drop keeps it (D306). The
 * rounds end early when one
 * moves the bound by less than jaos_set_mip_cut_stall's fraction (D304).
 * Cuts are valid for the whole tree because they are derived at the root
 * over the model's own bounds. A node inside jaos_set_mip_cut_depth gets one round
 * of the same cuts over its own relaxation (D296); those are read over the
 * node's bounds, so they hold in its subtree only, and they live in a pool
 * and are rows of the copy for exactly the nodes under it; a node whose
 * round moved its bound by less than jaos_set_mip_node_cut_stall's
 * fraction gets no round under it (D305). A cover cut may carry Balas's
 * lifting coefficients (D307) behind jaos_set_mip_cover_lift, and the
 * root gets rounds of mixed-integer rounding cuts read off the model's
 * own rows beside the other two families (D309), each of which may
 * absorb other rows first (D312), and a node inside the
 * cut depth may get the same MIR cuts over its own bounds beside its
 * Gomory round, behind jaos_set_mip_node_mir (D310). A rounding
 * heuristic at every fractional node
 * (D290) is the only heuristic; docs/claims.txt carries the claim that
 * there is no other. A strong-branching probe (D293) may carry a work cap
 * as a multiple of the node's own solve (D294), and the dive's first
 * child is chosen by a rule (D295).
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

#include <assert.h>
#include <float.h>
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
/* Cuts below the root (D296): a node at depth <= MIP_CUT_DEPTH gets one
 * round of cuts on its own relaxation, the root being depth 0 and getting
 * MIP_CUT_ROUNDS. 0 is the root only. jaos_set_mip_cut_depth overrides it;
 * docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_CUT_DEPTH = 3;
/* Cuts a node below the root may add in its round (D301), the most
 * efficacious kept; 0 is no cap. jaos_set_mip_node_cut_cap overrides it;
 * docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_NODE_CUT_CAP = 4;
/* Rounds of knapsack cover cuts at the root (D300), beside the Gomory
 * rounds; a round that adds nothing ends both. jaos_set_mip_cover_rounds
 * overrides it; docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_COVER_ROUNDS = 4;
/* A root round that moves the bound by less than MIP_CUT_STALL times
 * (1 + |bound|) ends the rounds (D304); 0 never ends them on the bound.
 * jaos_set_mip_cut_stall overrides it; docs/tolerances.md carries the
 * sweep. */
constexpr double MIP_CUT_STALL = 0.0;
/* A node whose round moves its bound by less than MIP_NODE_CUT_STALL times
 * (1 + |bound|) gets no round at any node under it (D305); the root's
 * whole cut phase is judged the same way for the nodes under it. 0 never
 * switches a subtree off. jaos_set_mip_node_cut_stall overrides it;
 * docs/tolerances.md carries the sweep. */
constexpr double MIP_NODE_CUT_STALL = 0.0;
/* Whether the root's cuts leave the relaxation below a node where their
 * slack is basic, like a node's own cuts (D306); off, they are rows of
 * every node. On by default. jaos_set_mip_root_cut_drop overrides it;
 * docs/tolerances.md carries the reading. */
constexpr bool MIP_ROOT_CUT_DROP = true;
/* Whether a cover cut carries Balas's lifting coefficients (D307): an item
 * outside the cover at least as heavy as the cover's h heaviest together
 * gets h; off, every item at least as heavy as the cover's heaviest gets
 * 1, the extended cover. jaos_set_mip_cover_lift overrides it;
 * docs/tolerances.md carries the reading. */
constexpr bool MIP_COVER_LIFT = false;
/* Rounds of mixed-integer rounding cuts at the root (D309), one per model
 * row and finite side at most, beside the other families; a round that
 * adds nothing ends them all. jaos_set_mip_mir_rounds overrides it;
 * docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_MIR_ROUNDS = 6;
/* How many scalings a row's MIR cut tries beyond 1: the |a_j| of the
 * integer columns whose shifted value is fractional, in column order.
 * Decides a set of candidates, never a number in an answer; held. */
constexpr int64_t MIP_MIR_DELTAS = 8;
/* The rounding a MIR side's right-hand side may carry and still be cut:
 * the shifted right-hand side is a sum, its fraction f0 is what the cut
 * divides by, and a computed f0 below the true one gives a cut that is
 * not valid, so a side whose sum cannot be computed to within this, in
 * the row's own units (DBL_EPSILON times the sum of the terms' magnitudes
 * times the term count), gets no cut. 1e-9 keeps the coefficient error at
 * most 1e-7 after the cut's 1 / (1 - f0) factor, which MIP_CUT_AWAY bounds
 * by 100: the primal tolerance's own scale. Held; docs/tolerances.md
 * carries the argument. */
constexpr double MIP_MIR_ROUND = 1e-9;
/* How many rows a MIR cut's aggregate may absorb beyond its own (D312):
 * each step substitutes out one continuous column that sits away from
 * both its bounds, using another model row. 0 is the single-row form.
 * jaos_set_mip_mir_aggregate overrides it; docs/tolerances.md carries the
 * sweep. */
constexpr int64_t MIP_MIR_AGGREGATE = 0;
/* How many relaxations the dive heuristic may solve at the root (D313):
 * each one fixes the integer column nearest an integer there and solves
 * again, and a point that comes out integral is an incumbent. 0 is off;
 * 50 is the default, which moves the first incumbent earlier on six of
 * the MIP set's instances and later on none (D313).
 * jaos_set_mip_dive_heuristic overrides it; docs/tolerances.md carries
 * the sweep. */
constexpr int64_t MIP_DIVE_HEURISTIC = 50;
/* The deepest node the dive heuristic runs at, the root being 0: every
 * node at this depth or above gets its own dive on its own relaxation.
 * 0 is the root alone, D313's form. jaos_set_mip_dive_heuristic_depth
 * overrides it; docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_DIVE_HEURISTIC_DEPTH = 0;
/* How many relaxations a RINS dive may solve: the columns the incumbent
 * and the node's relaxation already agree on are fixed at that value and
 * the dive runs on what is left (Danna, Rothberg and Le Pape, Exploring
 * relaxation induced neighborhoods to improve MIP solutions, Mathematical
 * Programming 102, 2005). It runs once per incumbent, at the first
 * fractional node after the incumbent moved, since that is when its input
 * changed. 0 is off. jaos_set_mip_rins overrides it; docs/tolerances.md
 * carries the sweep. */
constexpr int64_t MIP_RINS = 0;
/* The largest multiplier an aggregation step may use, and the reciprocal
 * is the smallest (D312): the step adds lambda times another row, so a
 * lambda far from 1 makes the aggregate's coefficients the difference of
 * numbers of very different size, and what comes out is rounding. Held;
 * cut_finish's dynamism bound is the second line of defence. */
constexpr double MIP_MIR_LAMBDA = 1e6;
/* How many times a dive may resume from the deepest sibling it left on
 * its stack once a node ends (D308); 0 sends every sibling to the open
 * set at once, D289's form. jaos_set_mip_dive_backtrack overrides it;
 * docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_DIVE_BACKTRACK = 0;
/* How far a waiting sibling's bound may sit above the best open node's,
 * as a fraction of (1 + |best|), for the dive to resume from it (D311);
 * 0 puts no bound on it and leaves the count alone. With the dive on and
 * a fraction, the count may be 0 for no count. jaos_set_mip_dive_gap
 * overrides it; docs/tolerances.md carries the sweep. */
constexpr double MIP_DIVE_GAP = 0.0;
/* How far a node's own bound may sit above its parent's, as a fraction of
 * (1 + |parent|), for the dive to go on into one of its children; 0 puts
 * no bound on it, D289's form. This is the third quantity D289's reopen
 * condition named, and the only one that reads the child against the node
 * that made it rather than against the open set. Nothing happens with the
 * dive off. jaos_set_mip_dive_degrade overrides it; docs/tolerances.md
 * carries the sweep. */
constexpr double MIP_DIVE_DEGRADE = 0.0;
/* How many rounds the feasibility pump may run at the root (D318): each
 * round rounds the point it holds and solves for the point of the
 * relaxation nearest that rounding in L1. 0 is off; 20 is the default,
 * the setting that reaches every instance of the MIP set a larger one
 * reaches (D318). jaos_set_mip_feaspump overrides it;
 * docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_FEASPUMP = 20;
/* How many integer columns a stalled pump flips (D318): a rounding that
 * comes back unchanged would repeat for ever, so the columns whose
 * relaxation value sits furthest from the rounding are moved to the other
 * side, the lowest index breaking a tie. Fischetti, Glover and Lodi draw
 * this count at random; drawing it would break D8, so it is fixed and the
 * choice inside it is by distance, which is a total order. Held: it moves
 * which points the pump visits and no number in an answer. */
constexpr int64_t MIP_PUMP_FLIPS = 10;
/* Whether the pump writes the general-integer distance, one auxiliary
 * column and two rows per integer column whose bounds hold more than two
 * integers, on the pump's private copy (Bertacco, Fischetti and Lodi,
 * 2007). jaos_set_mip_pump_general overrides it; docs/tolerances.md
 * carries the measurement, in the prose beside the table. */
constexpr bool MIP_PUMP_GENERAL = false;
/* The objective pump's decay (D321, after Achterberg and Berthold, 2007):
 * each round minimizes (1 - a) times the distance plus a times the
 * model's own objective, the two scaled to comparable norms, and a
 * multiplies by this each round from 1. 0 is the plain pump; 0.5 is the
 * default, the best mean of the four decays swept and the one that moves
 * a first incumbent to the root without moving another away from it
 * (D321). jaos_set_mip_pump_obj overrides it; docs/tolerances.md carries
 * the sweep. */
constexpr double MIP_PUMP_OBJ = 0.5;
/* Whether the feasibility pump runs at the root when something already
 * holds an incumbent there (D322). D318's guard was bought for the plain
 * pump, which looks for any feasible point; the objective pump (D321)
 * looks for a good one, so whether the guard still pays is its own
 * question. jaos_set_mip_pump_always overrides it; docs/tolerances.md
 * carries the reading. */
constexpr bool MIP_PUMP_ALWAYS = false;
/* Whether the root fixes integer columns by their reduced costs once an
 * incumbent exists (D323). jaos_set_mip_rcfix overrides it;
 * docs/tolerances.md carries the reading. */
constexpr bool MIP_RCFIX = false;
/* The slack a reduced-cost fixing keeps before it rounds (D323): the
 * bound it computes is (incumbent - relaxation) / |reduced cost|, a
 * quotient of two quantities the simplex knows to its own tolerance, and
 * rounding it down without slack could pull a bound past an optimal
 * point. Adding this before the floor only ever loosens the new bound, so
 * the deduction stays valid; docs/tolerances.md carries the sweep. */
constexpr double MIP_RCFIX_SLACK = 1e-6;
/* How many passes over the model's rows a node's bound propagation may
 * make (D324); 0 is off. A pass that moves nothing ends the rounds, so
 * the count is a cap and not a schedule. jaos_set_mip_propagate overrides
 * it; docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_PROPAGATE = 0;
/* The deepest node bound propagation runs at, the root being 0 (D324);
 * negative is every node. The root's deductions are made over the model's
 * own bounds and so hold everywhere, and the tree keeps them in ilo and
 * ihi; a deeper node's hold in its subtree alone and are rebuilt at every
 * node, which is what the depth pays for.
 * jaos_set_mip_propagate_depth overrides it; docs/tolerances.md carries
 * the sweep. */
constexpr int64_t MIP_PROPAGATE_DEPTH = -1;
/* The slack a propagated bound keeps before it rounds (D324), the same
 * argument as MIP_RCFIX_SLACK: a row's activity is a sum, so the bound it
 * implies is known to the tolerance of that sum, and loosening before the
 * floor keeps every integer point the row admits. */
constexpr double MIP_PROP_SLACK = 1e-9;
/* How far a row's implied activity must sit outside its own bound before
 * propagation calls the node infeasible (D324), relative to the sizes
 * that went into the comparison. A row's activity is a sum, so it is
 * known to the size of its terms, and this test decides a node with no
 * relaxation solved: it is set well above the slack a bound is rounded
 * with, because a wrong answer here is a pruned node that held the
 * optimum. */
constexpr double MIP_PROP_INFEAS = 1e-7;
/* A propagated bound is written back only when it moves the column by
 * more than this, in the column's own units: a bound that moves by less
 * changes no integer point and would only churn the relaxation. */
constexpr double MIP_PROP_MOVE = 0.5;

/* Whether a node inside the cut depth gets MIR cuts over its own bounds
 * beside its Gomory round (D310), local to its subtree like the rest.
 * jaos_set_mip_node_mir overrides it; docs/tolerances.md carries the
 * reading. */
constexpr bool MIP_NODE_MIR = false;
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

/* The work cap on each probe (D294) as a multiple of the work the node's
 * own relaxation took; a probe that reaches it stops and teaches nothing.
 * 0 is no cap. jaos_set_mip_probe_cap overrides it; docs/tolerances.md
 * carries the sweep. */
constexpr double MIP_PROBE_CAP = 0.0;

/* Where strong branching probes (D298): at nodes whose depth is at most
 * this, the root being 0; negative is every depth. jaos_set_mip_probe_depth
 * overrides it; docs/tolerances.md carries the sweep. */
constexpr int64_t MIP_PROBE_DEPTH = -1;

/* Cuts, rows across: the cuts of one round, held until the tableau they
 * were read from is freed (jaos_add_rows drops the scaling the tableau
 * points into, so a round is generated whole and added whole), and the
 * pool of every local cut the tree has made (D296), which only grows. */
typedef struct {
    int64_t *start, *idx;
    double *val, *lo;
    double *eff;               /* efficacy: violation over the norm (D301);
                                  the pool does not carry it            */
    int64_t n, nnz;
    int64_t cap_start, cap_lo, cap_idx, cap_val, cap_eff;   /* one each:
                                            JM_GROW reads its own cap */
} cutbuf;

static void cutbuf_free(cutbuf *cb)
{
    free(cb->start); free(cb->idx); free(cb->val); free(cb->lo); free(cb->eff);
}

/* Keeps the `cap` cuts of `cb` with the largest efficacy, the earlier on a
 * tie, in their original order; returns how many are left. */
static int64_t cutbuf_keep_best(cutbuf *cb, int64_t cap)
{
    if (cap <= 0 || cb->n <= cap)
        return cb->n;
    bool *keep = calloc((size_t)cb->n, sizeof *keep);
    if (keep == nullptr)
        return -1;
    for (int64_t k = 0; k < cap; k++) {
        int64_t best = -1;
        for (int64_t r = 0; r < cb->n; r++)
            if (!keep[r] && (best < 0 || cb->eff[r] > cb->eff[best]))
                best = r;
        keep[best] = true;
    }
    int64_t n = 0, nnz = 0;
    for (int64_t r = 0; r < cb->n; r++) {
        if (!keep[r])
            continue;
        const int64_t s = cb->start[r], e = cb->start[r + 1];
        if (nnz != s) {
            memmove(cb->idx + nnz, cb->idx + s, (size_t)(e - s) * sizeof *cb->idx);
            memmove(cb->val + nnz, cb->val + s, (size_t)(e - s) * sizeof *cb->val);
        }
        cb->start[n] = nnz;
        cb->lo[n] = cb->lo[r];
        cb->eff[n] = cb->eff[r];
        nnz += e - s;
        n++;
    }
    cb->start[n] = nnz;
    cb->n = n;
    cb->nnz = nnz;
    free(keep);
    return n;
}

/* Cut `r` of `src` appended to `dst`. */
static bool cutbuf_append(cutbuf *dst, const cutbuf *src, int64_t r)
{
    const int64_t s = src->start[r], nz = src->start[r + 1] - s;
    if (!JM_GROW(dst->start, dst->cap_start, dst->n + 2) ||
        !JM_GROW(dst->lo, dst->cap_lo, dst->n + 2) ||
        !JM_GROW(dst->idx, dst->cap_idx, dst->nnz + nz) ||
        !JM_GROW(dst->val, dst->cap_val, dst->nnz + nz))
        return false;
    dst->start[dst->n] = dst->nnz;
    if (nz > 0) {
        memcpy(dst->idx + dst->nnz, src->idx + s, (size_t)nz * sizeof *dst->idx);
        memcpy(dst->val + dst->nnz, src->val + s, (size_t)nz * sizeof *dst->val);
    }
    dst->nnz += nz;
    dst->lo[dst->n] = src->lo[r];
    dst->n++;
    dst->start[dst->n] = dst->nnz;
    return true;
}

/* Cuts `which[0..n)` of the pool added to the copy as rows, >= their
 * right-hand sides. */
static jaos_status pool_add(jaos_model *lp, const cutbuf *pool,
                            const int64_t *which, int64_t n)
{
    int64_t nnz = 0;
    for (int64_t k = 0; k < n; k++)
        nnz += pool->start[which[k] + 1] - pool->start[which[k]];
    int64_t *start = malloc((size_t)(n + 1) * sizeof *start);
    int64_t *idx = malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof *idx);
    double *val = malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof *val);
    double *lo = malloc((size_t)n * sizeof *lo);
    double *up = malloc((size_t)n * sizeof *up);
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (start == nullptr || idx == nullptr || val == nullptr ||
        lo == nullptr || up == nullptr)
        goto out;
    int64_t p = 0;
    for (int64_t k = 0; k < n; k++) {
        const int64_t c = which[k], s = pool->start[c], e = pool->start[c + 1];
        start[k] = p;
        for (int64_t q = s; q < e; q++) {
            idx[p] = pool->idx[q];
            val[p] = pool->val[q];
            p++;
        }
        lo[k] = pool->lo[c];
        up[k] = INFINITY;
    }
    start[n] = p;
    st = jaos_add_rows(lp, n, lo, up, p, start, idx, val);
out:
    free(start); free(idx); free(val); free(lo); free(up);
    return st;
}

/* One open node: the bound changes along its path from the root, in the
 * order they were made, the local cuts in force there (D296), and the
 * basis its parent's relaxation ended on. */
typedef struct {
    double key;                /* the parent's objective, minimize form */
    int64_t id;                /* creation order, the tie-break         */
    int64_t depth;             /* changes on the path                   */
    int64_t *col;
    double *lo, *hi;
    jaos_basis_status *cs, *rs;
    double frac;               /* the fraction the branch moved         */
    bool up;                   /* which way                              */
    int64_t *cuts;             /* pool indices of the local cuts        */
    int64_t ncuts;
    bool no_cuts;              /* no round here or under it (D305)      */
} bnode;

static void node_free(bnode *n)
{
    if (n == nullptr)
        return;
    free(n->col); free(n->lo); free(n->hi); free(n->cs); free(n->rs);
    free(n->cuts);
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

/* The best key among the open nodes not being solved: the heap's top and
 * the dive's stack (D308); INFINITY when there are none. */
static double open_key(const bheap *h, bnode *const *stack, int64_t n)
{
    double k = h->n > 0 ? h->v[0]->key : INFINITY;
    for (int64_t i = 0; i < n; i++)
        if (stack[i]->key < k)
            k = stack[i]->key;
    return k;
}

/* Whether the dive may resume from `n` (D311): always when no fraction is
 * set; else only while n's key is within `frac` of (1 + |best|) of the
 * best key any open node has, the dive's own waiting siblings included.
 * The heap alone is not that best: during a dive every sibling goes on
 * the stack and the heap can be empty for the whole dive, which is what
 * made the first form of this rule fire at every fraction alike. `n` is
 * itself on the stack, so a candidate that IS the best passes, which is
 * the intent. */
static bool resume_within(const bnode *n, const bheap *h, bnode *const *stack,
                          int64_t stack_n, double frac)
{
    if (frac <= 0.0)
        return true;
    const double best = open_key(h, stack, stack_n);
    if (!isfinite(best))
        return true;
    return n->key - best <= frac * (1.0 + fabs(best));
}

/* A child of `parent` (or of the root when parent is null) with one more
 * change, carrying the local cuts in force, `act[0..act_n)`, and the basis
 * the relaxation just ended on, whose `nr` rows are the copy's now. */
static bnode *node_child(const bnode *parent, int64_t nc, int64_t nr,
                         const jaos_basis_status *cs,
                         const jaos_basis_status *rs, int64_t col, double lo,
                         double hi, double key, int64_t id, double frac,
                         bool up, const int64_t *act, int64_t act_n,
                         bool no_cuts)
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
    n->cuts = malloc((size_t)(act_n > 0 ? act_n : 1) * sizeof *n->cuts);
    if (!n->col || !n->lo || !n->hi || !n->cs || !n->rs || !n->cuts) {
        node_free(n);
        return nullptr;
    }
    if (act_n > 0)
        memcpy(n->cuts, act, (size_t)act_n * sizeof *n->cuts);
    n->ncuts = act_n;
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
    n->no_cuts = no_cuts;
    if (nc > 0)
        memcpy(n->cs, cs, (size_t)nc * sizeof *n->cs);
    if (nr > 0)
        memcpy(n->rs, rs, (size_t)nr * sizeof *n->rs);
    return n;
}

/* The local cuts the copy holds right now, as pool indices in row order
 * (D297): node_apply compares a node's list against it and moves no row
 * when they are the same. */
typedef struct {
    int64_t *v;
    int64_t n, cap;
} cutlist;

static bool cutlist_set(cutlist *l, const int64_t *v, int64_t n)
{
    if (!JM_GROW(l->v, l->cap, n > 0 ? n : 1))
        return false;
    if (n > 0)
        memcpy(l->v, v, (size_t)n * sizeof *v);
    l->n = n;
    return true;
}

static bool cutlist_same(const cutlist *l, const int64_t *v, int64_t n)
{
    return l->n == n && (n == 0 || memcmp(l->v, v, (size_t)n * sizeof *v) == 0);
}

/* Puts the relaxation at `n`: the local cuts of the node before it out of
 * the copy and n's in (D296), every integer column back at the root's
 * bounds -- the model's, rounded inward to integers (D292) -- then the
 * path's changes in order, then the parent's basis. `nfixed` is the row
 * count the copy has with no local cut in it, and `*in_copy` how many
 * local rows it holds on entry and on exit. */
static jaos_status node_apply(jaos_model *lp, const jaos_model *m,
                              const double *ilo, const double *ihi,
                              const bnode *n, const cutbuf *pool,
                              int64_t nfixed, cutlist *in_copy)
{
    jaos_status st = JAOS_OK;
    const int64_t want_n = n != nullptr ? n->ncuts : 0;
    const int64_t *want = n != nullptr ? n->cuts : nullptr;
    /* The same rows in the same order are the same relaxation (D297): the
     * scaling and the mirror a delete and an add would drop are rebuilt
     * from the same matrix, so nothing moves and nothing is moved. */
    if (!cutlist_same(in_copy, want, want_n)) {
        if (in_copy->n > 0) {
            int64_t *del = malloc((size_t)in_copy->n * sizeof *del);
            if (del == nullptr)
                return JAOS_ERR_OUT_OF_MEMORY;
            for (int64_t k = 0; k < in_copy->n; k++)
                del[k] = nfixed + k;
            st = jaos_delete_rows(lp, in_copy->n, del);
            free(del);
            in_copy->n = 0;
        }
        if (st == JAOS_OK && want_n > 0)
            st = pool_add(lp, pool, want, want_n);
        if (st == JAOS_OK && !cutlist_set(in_copy, want, want_n))
            return JAOS_ERR_OUT_OF_MEMORY;
    }
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
 * is found, because the copy of the model moves on to other nodes. The
 * row arrays hold the MODEL's rows, `nr`: a cut's row is never published,
 * and the copy's count moves with the local cuts (D296). */
typedef struct {
    bool have;
    double obj, key;
    double *x, *ra, *rd, *cd;
    jaos_basis_status *cs, *rs;
} incumbent;

static bool incumbent_take(incumbent *inc, const jaos_model *lp, int64_t nr,
                           double key)
{
    const int64_t nc = lp->num_col;
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

/* A basis of the MODEL behind a proved incumbent (D334).
 *
 * `incumbent_take` copies the node LP's statuses truncated to the
 * caller's rows, and the node LP carries the cut rows. A cut that binds
 * at that node has a nonbasic logical, and truncation drops the status
 * while keeping the basic it paid for, so the published count comes out
 * one too high per binding cut -- 19 of the 24 MIPLIB instances, and the
 * ordinary case rather than a corner since cuts are on by default (D330,
 * `bench/measurements/02-212/mip-basis-count.txt`). The duals and the
 * reduced costs have the same shape of defect: they are the node's, over
 * a row set that has rows the caller's model does not.
 *
 * The repair is a solve and not a copy, and it is the smallest one that
 * exists. Fix every integer column at the incumbent's value, drop the
 * cuts, and solve the model that is left. That LP's optimum IS the
 * incumbent's: every cut the tree added is valid for the integer hull, so
 * it removes no point that satisfies the model with the integers at those
 * values, and the feasible set of the fixed model is exactly those
 * points. So the objective cannot move, and what comes back is a basis,
 * a set of duals and a set of reduced costs of the model as the caller
 * loaded it.
 *
 * It runs once, at publication, and only where the truncation actually
 * broke the count -- five of the 24 need nothing. A re-solve that does
 * not reach an optimum changes nothing and leaves the flag false, which
 * is the honest state and the one this function replaces. */
static bool republish_at_the_incumbent(jaos_model *m, const double *point,
                                       int64_t nc, int64_t nr,
                                       int64_t *extra_work)
{
    jaos_model *fin = nullptr;
    bool ok = false;
    if (jaos_model_copy(m, &fin) != JAOS_OK)
        return false;
    /* No integrality on the copy: this is one LP and not a second tree. */
    free(fin->col_integer);
    fin->col_integer = nullptr;
    /* And no log: the re-solve is this function's own business, the rule
     * the tree's own copy follows. */
    fin->cfg.log_cb = nullptr;

    for (int64_t j = 0; j < nc; j++) {
        if (m->col_integer == nullptr || !m->col_integer[j])
            continue;
        /* The incumbent's value is integral to the tree's tolerance; the
         * bound it is fixed at is the integer itself, so the fixed model
         * is stated in integers and not in a rounding of them. */
        const double v = floor(point[j] + 0.5);
        if (jaos_set_col_bounds(fin, j, v, v) != JAOS_OK)
            goto out;
    }
    if (jaos_solve(fin) != JAOS_OK)
        goto out;
    *extra_work += jaos_work_units(fin);
    if (jaos_status_of(fin) != JAOS_SOLVE_OPTIMAL || !fin->sol_basis_ok)
        goto out;

    if (nc > 0) {
        memcpy(m->sol_col, fin->sol_col, (size_t)nc * sizeof *m->sol_col);
        memcpy(m->sol_redcost, fin->sol_redcost,
               (size_t)nc * sizeof *m->sol_redcost);
        memcpy(m->sol_col_status, fin->sol_col_status,
               (size_t)nc * sizeof *m->sol_col_status);
    }
    if (nr > 0) {
        memcpy(m->sol_row, fin->sol_row, (size_t)nr * sizeof *m->sol_row);
        memcpy(m->sol_dual, fin->sol_dual, (size_t)nr * sizeof *m->sol_dual);
        memcpy(m->sol_row_status, fin->sol_row_status,
               (size_t)nr * sizeof *m->sol_row_status);
    }
    /* The incumbent's own values go back over the columns the fixing
     * pinned, so the published point is the one the tree proved and not a
     * re-solve's spelling of it. They are equal by construction -- the
     * bound IS the value -- and this says so rather than assuming it. */
    for (int64_t j = 0; j < nc; j++)
        if (m->col_integer != nullptr && m->col_integer[j])
            m->sol_col[j] = floor(point[j] + 0.5);
    ok = true;
out:
    jaos_model_free(fin);
    return ok;
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
 * billed to `work` and counted in `solves` and `probes`. A probe whose
 * solve reaches `cap` work units (D294; 0 for no cap) stops there and
 * teaches nothing, and is counted in `capped`; the caller's own work
 * limit still applies where it is the smaller. */
static jaos_status strong_probe(jaos_model *lp, const jaos_model *m,
                                const double *x, double key, double sigma,
                                int64_t reliability, int64_t cap,
                                double *pc_sum, int64_t *pc_n, int64_t *work,
                                int64_t *solves, int64_t *probes,
                                int64_t *capped, jaos_basis_status *cs,
                                jaos_basis_status *rs, int64_t *cand)
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
            const int64_t caller_limit = lp->cfg.work_limit;
            const bool capping = cap > 0 &&
                                 (caller_limit <= 0 || cap < caller_limit);
            if (capping)
                lp->cfg.work_limit = cap;
            if (st == JAOS_OK)
                st = jaos_solve(lp);
            lp->cfg.work_limit = caller_limit;
            (*solves)++;
            (*probes)++;
            *work += jaos_work_units(lp);
            if (st == JAOS_OK && capping &&
                jaos_status_of(lp) == JAOS_SOLVE_WORK_LIMIT)
                (*capped)++;
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

static bool cutbuf_push(cutbuf *cb, const double *cut, int64_t nc, double lo,
                        double eff)
{
    if (!JM_GROW(cb->start, cb->cap_start, cb->n + 2) ||
        !JM_GROW(cb->lo, cb->cap_lo, cb->n + 2) ||
        !JM_GROW(cb->eff, cb->cap_eff, cb->n + 2))
        return false;
    cb->eff[cb->n] = eff;
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

/* A cut in the form cut . x >= rhs over the copy's columns, finished the
 * way every family's is: an entry below MIP_CUT_DROP of the largest is
 * folded into the right-hand side through its column's finite bound and
 * dropped; a cut past MIP_CUT_DYNAMISM, or one the point does not
 * violate, is not added. Every sum runs in column order (D8). Returns 1
 * when the cut was pushed, 0 when not, -1 when out of memory. */
static int cut_finish(const jaos_model *lp, cutbuf *cb, double *cut,
                      double rhs, const double *x)
{
    const int64_t nc = lp->num_col;
    double amax = 0.0;
    for (int64_t k = 0; k < nc; k++)
        if (fabs(cut[k]) > amax)
            amax = fabs(cut[k]);
    if (amax == 0.0)
        return 0;
    double amin = INFINITY;
    int64_t nnz = 0;
    for (int64_t k = 0; k < nc; k++) {
        const double c = cut[k];
        if (c == 0.0)
            continue;
        if (fabs(c) < MIP_CUT_DROP * amax) {
            /* c x_k >= rhs - rest; the term is largest at the bound its
             * sign points to, and a finite one absorbs it. */
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
        return 0;
    double act = 0.0, nrm = 0.0;
    for (int64_t k = 0; k < nc; k++) {
        act += cut[k] * x[k];
        nrm += cut[k] * cut[k];
    }
    if (!(rhs - act > 0.0))
        return 0;
    if (!cutbuf_push(cb, cut, nc, rhs, (rhs - act) / sqrt(nrm)))
        return -1;
    return 1;
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
        const int pushed = cut_finish(lp, cb, cut, rhs, x);
        if (pushed < 0) {
            added = -1;
            break;
        }
        added += pushed;
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

/* --- Knapsack cover cuts at the root (D300) ---------------------------- */

/* One item of a knapsack row: the column, its weight (positive once a
 * negative coefficient is complemented), the relaxation's value of the
 * literal, x_j or 1 - x_j, and which literal it is. */
typedef struct {
    int64_t col;
    double a, xv;
    bool compl;
} kitem;

/* The greedy order: the literal nearest 1 per unit of weight first,
 * (1 - xv) / a ascending, the lower column on a tie, so the order is a
 * total one and the same on every machine (D8) whatever qsort does. */
static int kitem_cmp(const void *pa, const void *pb)
{
    const kitem *p = pa, *q = pb;
    const double kp = (1.0 - p->xv) / p->a, kq = (1.0 - q->xv) / q->a;
    if (kp < kq) return -1;
    if (kp > kq) return 1;
    return p->col < q->col ? -1 : p->col > q->col;
}

/* The cover's items heaviest first, the lower column on a tie (D307). */
static int kitem_weight_cmp(const void *pa, const void *pb)
{
    const kitem *p = pa, *q = pb;
    if (p->a > q->a) return -1;
    if (p->a < q->a) return 1;
    return p->col < q->col ? -1 : p->col > q->col;
}

/* One round of cover cuts (Wolsey, Integer Programming, 1998, ch. 9.3):
 * every model row whose columns are all binary integer columns, each
 * finite side read as
 *
 *     sum a_j y_j <= b   over literals y_j = x_j or 1 - x_j, a_j > 0,
 *
 * a negative coefficient complemented and its weight moved into b. The
 * greedy cover C takes items in kitem_cmp's order until their weight
 * passes b by more than the primal tolerance's margin, so a rounded sum
 * of decimal weights cannot make a cover of a set that fits (a cover
 * short of the margin is a lost cut, never a wrong one); extended by
 * every item at least as heavy as C's heaviest, it
 * gives sum_{E} y_j <= |C| - 1, rewritten over the model's own columns and
 * added when the point violates it. With `lift` (D307) the coefficient of
 * an item outside C is Balas's (Facets of the knapsack polytope,
 * Mathematical Programming 8, 1975): with C's weights in descending order
 * and mu_h the first h summed, an item of weight in [mu_h, mu_{h+1}) gets
 * h, so the extended cover is the case h = 1. It is valid for any cover:
 * if items T outside C are at 1 with H = sum of their h's, their weight is
 * at least mu_H since mu is concave, so at most |C| - H - 1 items of C fit
 * beside them, else C's own weight would be at most b. The bounds it
 * reads are the root's rounded ones, so the cut is valid for the whole
 * tree. `items` and `cut` are scratch of num_col, `mu` of num_col + 1;
 * the pass is billed once. Returns the count, or -1 when the mirror could
 * not be built. */
static int64_t cover_round(const jaos_model *m, jaos_model *lp,
                           const double *x, const double *ilo,
                           const double *ihi, cutbuf *cb, kitem *items,
                           double *cut, double *mu, bool lift,
                           int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    const double tol = jm_primal_tolerance(m);
    int64_t added = 0;
    /* The row-wise mirror is rebuilt on demand after any change to the
     * matrix, which every round of cuts is. */
    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return -1;
    *work += m->num_nz + nc + nr;
    for (int64_t i = 0; i < nr; i++) {
        bool binary = true;
        for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1] && binary; k++) {
            const int64_t j = lp->ar_index[k];
            binary = m->col_integer[j] && ilo[j] == 0.0 && ihi[j] == 1.0;
        }
        if (!binary)
            continue;
        for (int side = 0; side < 2; side++) {
            const double bound = side == 0 ? lp->row_upper[i] : lp->row_lower[i];
            if (!isfinite(bound))
                continue;
            const double sg = side == 0 ? 1.0 : -1.0;
            double b = sg * bound, total = 0.0;
            int64_t n = 0;
            for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++) {
                const int64_t j = lp->ar_index[k];
                const double a = sg * lp->ar_value[k];
                if (a == 0.0)
                    continue;
                if (a > 0.0) {
                    items[n] = (kitem){ .col = j, .a = a, .xv = x[j], .compl = false };
                } else {
                    items[n] = (kitem){ .col = j, .a = -a, .xv = 1.0 - x[j],
                                        .compl = true };
                    b += -a;
                }
                total += items[n].a;
                n++;
            }
            /* A cover's excess over b must clear the margin the tree
             * judges feasibility to, or the cover is not one. */
            const double over = b + tol * (1.0 + fabs(b));
            if (n == 0 || !(total > over) || !(b >= 0.0))
                continue;
            qsort(items, (size_t)n, sizeof *items, kitem_cmp);
            double weight = 0.0;
            int64_t c = 0;
            while (c < n && !(weight > over))
                weight += items[c++].a;
            if (!(weight > over))
                continue;
            double amax = 0.0;
            for (int64_t k = 0; k < c; k++)
                if (items[k].a > amax)
                    amax = items[k].a;
            if (lift) {
                /* mu_h over the cover heaviest first; the sum runs in that
                 * order on every machine, since the order is total. */
                qsort(items, (size_t)c, sizeof *items, kitem_weight_cmp);
                mu[0] = 0.0;
                for (int64_t k = 0; k < c; k++)
                    mu[k + 1] = mu[k] + items[k].a;
            }
            /* sum_E alpha y <= c - 1, E = C at 1 each plus every item
             * outside it with a positive coefficient: 1 when at least as
             * heavy as C's heaviest, or Balas's h when lifted. */
            double act = 0.0, nrm = 0.0, shift = 0.0;
            memset(cut, 0, (size_t)nc * sizeof *cut);
            for (int64_t k = 0; k < n; k++) {
                double alpha = 1.0;
                if (k >= c) {
                    if (lift) {
                        int64_t h = 0;
                        while (h < c && mu[h + 1] <= items[k].a)
                            h++;
                        alpha = (double)h;
                    } else {
                        alpha = items[k].a >= amax ? 1.0 : 0.0;
                    }
                    if (alpha == 0.0)
                        continue;
                }
                act += alpha * items[k].xv;
                cut[items[k].col] = items[k].compl ? alpha : -alpha;
                if (items[k].compl)
                    shift += alpha;
                nrm += alpha * alpha;
            }
            if (!(act > (double)(c - 1)))
                continue;
            /* Over the columns: sum_{compl} alpha x - sum_{plain} alpha x
             * >= sum_{compl} alpha - (c - 1). */
            if (!cutbuf_push(cb, cut, nc, shift - (double)(c - 1),
                             (act - (double)(c - 1)) / sqrt(nrm)))
                return -1;
            added++;
        }
    }
    return added;
}

/* --- Mixed-integer rounding cuts on the model's rows (D309) ------------ */

/* Which bound column j is shifted to: the nearer to its value, or the
 * finite one; the caller has excluded a column with neither. */
static bool shift_to_upper(double lo, double hi, double xj)
{
    if (!isfinite(lo))
        return true;
    if (!isfinite(hi))
        return false;
    return hi - xj < xj - lo;
}

/* One round of MIR cuts: one per model row and finite side at most
 * (Marchand and Wolsey, Aggregation and mixed integer rounding to solve
 * MIPs, Operations Research 49, 2001, without the aggregation; Wolsey,
 * Integer Programming, 1998, ch. 8.6). A side is read as
 *
 *     sum a_j x_j <= b,
 *
 * every column shifted to the bound nearer its value, x'_j = x_j - l_j or
 * u_j - x_j, so x' >= 0 (a side with a column that has no finite bound is
 * skipped); scaled by a delta from {1} and the |a'_j| of the integer
 * columns whose shifted value is fractional, at most MIP_MIR_DELTAS of
 * those in column order; then rounded. With f0 the fraction of b'/delta,
 * inside [MIP_CUT_AWAY, 1 - MIP_CUT_AWAY], and f_j that of a'_j/delta,
 *
 *     sum_int (floor(a'_j/delta) + max(f_j - f0, 0) / (1 - f0)) x'_j
 *   + sum_{cont, a'_j < 0} a'_j / (delta (1 - f0)) x'_j  <=  floor(b'/delta),
 *
 * valid because a continuous term with a positive coefficient only
 * loosens the base row when dropped and what is left is the MIR
 * inequality of sum_int c_j x_j - t <= c_0 with t >= 0. The delta with
 * the largest efficacy is kept, rewritten over the model's columns as a
 * >= row and finished like every cut. The cut holds wherever the `ilo`
 * and `ihi` it read hold: the root's rounded bounds at the root, so for
 * the whole tree, and a node's own bounds under jaos_set_mip_node_mir
 * (D310), so for that node's subtree only, which is where the pool puts
 * it. Both are integral for an integer column, which node_apply keeps and
 * the assert below states. `cut` and `best` are
 * scratch of num_col, `delta` of MIP_MIR_DELTAS + 1; the pass is billed
 * once per delta. Returns the count, or -1 on failure. */
/* One side, `sum a_j x_j <= b` with `a` dense over the columns, put
 * through the rounding above. `mag_in` and `terms_in` carry the magnitude
 * and the term count that already went into `b` before this call, and
 * `cmag`, when it is not null, the magnitude that went into each
 * coefficient the same way: an aggregate has both and a model row has
 * neither, since its coefficients are the data. A coefficient the sum
 * could not place to MIP_MIR_ROUND is refused for the same reason the
 * right-hand side is -- the rounding rides through the cut's map, which
 * multiplies it by up to 1 / MIP_CUT_AWAY. Returns 1 when a cut was
 * pushed, 0 when none was, -1 out of memory. */
static int mir_side(const jaos_model *m, jaos_model *lp, const double *x,
                    const double *ilo, const double *ihi, const double *a_in,
                    double b_in, double mag_in, int64_t terms_in,
                    const double *cmag, cutbuf *cb, double *cut, double *best,
                    double *delta)
{
    const int64_t nc = m->num_col;
    if (cmag != nullptr)
        for (int64_t j = 0; j < nc; j++)
            if (DBL_EPSILON * cmag[j] * (double)terms_in > MIP_MIR_ROUND)
                return 0;
    {
        {
            double b = b_in, mag = fabs(b_in) + mag_in;
            int64_t terms = 1 + terms_in;
            bool ok = true;
            int64_t nd = 1;
            delta[0] = 1.0;
            for (int64_t j = 0; j < nc; j++) {
                const double a = a_in[j];
                if (a == 0.0)
                    continue;
                if (!isfinite(ilo[j]) && !isfinite(ihi[j])) {
                    ok = false;
                    break;
                }
                const bool at_up = shift_to_upper(ilo[j], ihi[j], x[j]);
                const double shift = at_up ? a * ihi[j] : a * ilo[j];
                b -= shift;
                mag += fabs(shift);
                terms++;
                if (!m->col_integer[j] || nd > MIP_MIR_DELTAS)
                    continue;
                const double xs = at_up ? ihi[j] - x[j] : x[j] - ilo[j];
                const double fr = xs - floor(xs);
                if (fr <= MIP_INT_TOL || fr >= 1.0 - MIP_INT_TOL)
                    continue;
                const double d = fabs(a);
                bool seen = false;
                for (int64_t q = 0; q < nd && !seen; q++)
                    seen = delta[q] == d;
                if (!seen)
                    delta[nd++] = d;
            }
            /* A right-hand side the sum could not place to MIP_MIR_ROUND
             * has a fraction the cut cannot trust (the review's case: a
             * column whose one finite bound is 1e15 makes f0 a multiple
             * of 1/8 whatever the data). */
            if (!ok || DBL_EPSILON * mag * (double)terms > MIP_MIR_ROUND)
                return 0;
            double best_eff = 0.0, best_rhs = 0.0;
            bool have = false;
            for (int64_t q = 0; q < nd; q++) {
                const double d = delta[q], b0 = b / d;
                const double f0 = b0 - floor(b0);
                if (f0 < MIP_CUT_AWAY || f0 > 1.0 - MIP_CUT_AWAY)
                    continue;
                /* The rounded row over x', then over x: cut . x <= rhs. */
                memset(cut, 0, (size_t)nc * sizeof *cut);
                double rhs = floor(b0);
                for (int64_t j = 0; j < nc; j++) {
                    const double a0 = a_in[j];
                    if (a0 == 0.0)
                        continue;
                    const bool at_up = shift_to_upper(ilo[j], ihi[j], x[j]);
                    const double a = (at_up ? -a0 : a0) / d;
                    double c;
                    if (m->col_integer[j]) {
                        const double fa = floor(a), fj = a - fa;
                        c = fa + (fj > f0 ? (fj - f0) / (1.0 - f0) : 0.0);
                    } else {
                        c = a < 0.0 ? a / (1.0 - f0) : 0.0;
                    }
                    if (c == 0.0)
                        continue;
                    if (at_up) {
                        cut[j] -= c;
                        rhs -= c * ihi[j];
                    } else {
                        cut[j] += c;
                        rhs += c * ilo[j];
                    }
                }
                double act = 0.0, nrm = 0.0;
                for (int64_t k = 0; k < nc; k++) {
                    act += cut[k] * x[k];
                    nrm += cut[k] * cut[k];
                }
                if (nrm == 0.0)
                    continue;
                const double eff = (act - rhs) / sqrt(nrm);
                if (eff > best_eff) {
                    best_eff = eff;
                    best_rhs = rhs;
                    have = true;
                    memcpy(best, cut, (size_t)nc * sizeof *best);
                }
            }
            if (!have)
                return 0;
            for (int64_t k = 0; k < nc; k++)
                cut[k] = -best[k];
            return cut_finish(lp, cb, cut, -best_rhs, x);
        }
    }
}

/* One round of MIR cuts, one per model row and finite side at most: each
 * side read into a dense array and put through mir_side. `agg` is scratch
 * of num_col. Returns the count, or -1 on failure. */
static int64_t mir_round(const jaos_model *m, jaos_model *lp, const double *x,
                         const double *ilo, const double *ihi, cutbuf *cb,
                         double *cut, double *best, double *delta,
                         double *agg, int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    int64_t added = 0;
    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return -1;
    /* x' is integral only when the bound it is measured from is: the
     * whole rounding rests on that, and a fractional bound would cut off
     * feasible integer points with nothing to show for it. */
    for (int64_t j = 0; j < nc; j++)
        assert(!m->col_integer[j] ||
               ((!isfinite(ilo[j]) || floor(ilo[j]) == ilo[j]) &&
                (!isfinite(ihi[j]) || floor(ihi[j]) == ihi[j])));
    *work += (m->num_nz + nc + nr) * (MIP_MIR_DELTAS + 1);
    for (int64_t i = 0; i < nr; i++) {
        for (int side = 0; side < 2; side++) {
            const double bound = side == 0 ? lp->row_upper[i] : lp->row_lower[i];
            if (!isfinite(bound))
                continue;
            const double sg = side == 0 ? 1.0 : -1.0;
            memset(agg, 0, (size_t)nc * sizeof *agg);
            for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++)
                agg[lp->ar_index[k]] += sg * lp->ar_value[k];
            const int pushed = mir_side(m, lp, x, ilo, ihi, agg, sg * bound,
                                        0.0, 0, nullptr, cb, cut, best, delta);
            if (pushed < 0)
                return -1;
            added += pushed;
        }
    }
    return added;
}

/* One round of aggregated MIR cuts (D312), Marchand and Wolsey's
 * aggregation without their heuristic search: each model row and finite
 * side is the start of an aggregate, and each of `steps` steps
 * substitutes out one continuous column that sits away from both its
 * bounds and has not been substituted out already -- the one with the
 * largest coefficient in the aggregate, the lowest index on a tie --
 * using the lowest-indexed other row that holds it with a coefficient
 * worth pivoting on and a finite bound on the side the multiplier
 * needs. With lambda = a_j / c_rj the aggregate becomes
 * `agg - lambda row_r <= b - lambda B_r`, where `B_r` is row r's lower
 * bound when lambda is positive and its upper bound when negative, which
 * is what keeps the inequality true. A multiplier outside
 * [1/MIP_MIR_LAMBDA, MIP_MIR_LAMBDA] is refused: the step would be the
 * difference of numbers of very different size. The magnitude that went
 * into every coefficient is carried beside it and mir_side refuses a
 * coefficient it cannot place, which is what keeps an aggregated cut
 * valid: the pivot's own column is left in the aggregate with whatever
 * residue the cancellation left, and a mask keeps it out of later picks,
 * since dropping a term whose coefficient may be negative would
 * strengthen the cut past what the rows say. Every aggregate is put
 * through mir_side after each step, so a row contributes at most `steps`
 * + 1 cuts per side. `agg`, `cut`, `best` and `cmag` are scratch of
 * num_col, `used` and `picked` of num_row and num_col. Returns the count,
 * or -1 on failure. */
static int64_t mir_aggregate_round(const jaos_model *m, jaos_model *lp,
                                   const double *x, const double *ilo,
                                   const double *ihi, cutbuf *cb, double *cut,
                                   double *best, double *delta, double *agg,
                                   double *cmag, bool *used, bool *picked,
                                   int64_t steps, int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    int64_t added = 0;
    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return -1;
    if (steps > nr)
        steps = nr;                    /* used[] bounds the steps anyway */
    for (int64_t i = 0; i < nr; i++) {
        for (int side = 0; side < 2; side++) {
            const double bound = side == 0 ? lp->row_upper[i] : lp->row_lower[i];
            if (!isfinite(bound))
                continue;
            const double sg = side == 0 ? 1.0 : -1.0;
            memset(agg, 0, (size_t)nc * sizeof *agg);
            for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++)
                agg[lp->ar_index[k]] += sg * lp->ar_value[k];
            memset(used, 0, (size_t)(nr > 0 ? nr : 1) * sizeof *used);
            memset(picked, 0, (size_t)(nc > 0 ? nc : 1) * sizeof *picked);
            memset(cmag, 0, (size_t)(nc > 0 ? nc : 1) * sizeof *cmag);
            used[i] = true;
            double b = sg * bound, mag = 0.0;
            int64_t terms = 0;
            for (int64_t s = 0; s < steps; s++) {
                /* One pass over the matrix and the columns per step. */
                *work += m->num_nz + nc + nr;
                /* The column to substitute out: continuous, in the
                 * aggregate, and away from both its bounds. */
                int64_t pick = -1;
                double pick_a = 0.0;
                for (int64_t j = 0; j < nc; j++) {
                    if (agg[j] == 0.0 || m->col_integer[j] || picked[j])
                        continue;
                    const double lo = ilo[j], hi = ihi[j];
                    if ((isfinite(lo) && x[j] - lo <= MIP_INT_TOL) ||
                        (isfinite(hi) && hi - x[j] <= MIP_INT_TOL))
                        continue;
                    if (fabs(agg[j]) > pick_a) {
                        pick_a = fabs(agg[j]);
                        pick = j;
                    }
                }
                if (pick < 0)
                    break;
                /* The row to substitute with: the lowest index that is
                 * not in the aggregate yet, holds the column with a
                 * coefficient worth pivoting on, and has the bound the
                 * multiplier's sign needs. */
                int64_t rrow = -1;
                double lambda = 0.0, rbound = 0.0;
                for (int64_t k = lp->a_start[pick];
                     k < lp->a_start[pick + 1] && rrow < 0; k++) {
                    const int64_t r = lp->a_index[k];
                    if (r >= nr || used[r])
                        continue;
                    const double crj = lp->a_value[k];
                    if (crj == 0.0)
                        continue;
                    double rmax = 0.0;
                    for (int64_t q = lp->ar_start[r]; q < lp->ar_start[r + 1]; q++)
                        if (fabs(lp->ar_value[q]) > rmax)
                            rmax = fabs(lp->ar_value[q]);
                    if (fabs(crj) < MIP_CUT_DROP * rmax)
                        continue;
                    const double lam = agg[pick] / crj;
                    if (!isfinite(lam) || lam == 0.0 ||
                        fabs(lam) > MIP_MIR_LAMBDA ||
                        fabs(lam) < 1.0 / MIP_MIR_LAMBDA)
                        continue;
                    const double bnd = lam > 0.0 ? lp->row_lower[r]
                                                 : lp->row_upper[r];
                    if (!isfinite(bnd))
                        continue;
                    rrow = r;
                    lambda = lam;
                    rbound = bnd;
                }
                if (rrow < 0)
                    break;
                for (int64_t q = lp->ar_start[rrow]; q < lp->ar_start[rrow + 1]; q++) {
                    const int64_t j = lp->ar_index[q];
                    agg[j] -= lambda * lp->ar_value[q];
                    cmag[j] += fabs(lambda * lp->ar_value[q]);
                }
                picked[pick] = true;   /* not exactly zero; not picked again */
                b -= lambda * rbound;
                mag += fabs(lambda * rbound);
                terms++;
                used[rrow] = true;
                const int pushed = mir_side(m, lp, x, ilo, ihi, agg, b, mag,
                                            terms, cmag, cb, cut, best, delta);
                if (pushed < 0)
                    return -1;
                added += pushed;
            }
        }
    }
    return added;
}

/* --- The dive heuristic (D313) ----------------------------------------- */

/* A dive for a first incumbent: on a copy of the root's relaxation, the
 * integer column nearest an integer is fixed there and the relaxation is
 * solved again, up to `solves` times. The point that comes out is the
 * caller's to judge -- this only says whether every integer column of it
 * is integral. The copy carries the root's cuts and bounds, so a point it
 * reaches satisfies the model's rows, and the caller runs it through
 * rounded_point anyway, which is the same acceptance every heuristic
 * point goes through. A relaxation that comes back infeasible or stops on
 * a budget ends the dive: a heuristic gives up, it does not fail. The
 * choice is the smallest distance to an integer, the lowest column on a
 * tie, so the dive is the same on every machine (D8). With `agree_a` and
 * `agree_b` both given, every integer column the two points already place
 * at the same integer is fixed there before the first solve, which is
 * RINS's neighbourhood: the dive then searches only the columns they
 * disagree on. A value outside the relaxation's own bounds fixes nothing,
 * since the copy's bounds are the node's and a fix outside them would
 * make an infeasible relaxation out of a feasible one. Neither point is
 * required to be finite: a relaxation may publish a NaN, and every
 * comparison below is false for one, so the test has to ask for finite
 * values rather than assume them. Returns 1 with the point in `out`, 0
 * when there is none, -1 only when the copy could not be made. Every
 * solve is billed and counted. */
static int dive_for_point(const jaos_model *m, const jaos_model *lp,
                          int64_t solves, const double *agree_a,
                          const double *agree_b, double *out, int64_t *work,
                          int64_t *solves_done)
{
    const int64_t nc = m->num_col;
    jaos_model *hv = nullptr;
    if (jaos_model_copy(lp, &hv) != JAOS_OK)
        return -1;
    int rc = 0;
    if (agree_a != nullptr && agree_b != nullptr) {
        *work += nc;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j])
                continue;
            if (!isfinite(agree_a[j]) || !isfinite(agree_b[j]))
                continue;
            const double a = round(agree_a[j]);
            if (fabs(agree_a[j] - a) > MIP_INT_TOL ||
                fabs(agree_b[j] - a) > MIP_INT_TOL)
                continue;
            if (a < hv->col_lower[j] || a > hv->col_upper[j])
                continue;
            /* A fix the model refuses stops the fixing and the dive runs
             * on what is fixed so far, which is a wider neighbourhood and
             * still a valid one: a heuristic gives up, it does not fail. */
            if (jaos_set_col_bounds(hv, j, a, a) != JAOS_OK)
                break;
        }
    }
    for (int64_t s = 0; s < solves; s++) {
        if (jaos_solve(hv) != JAOS_OK)
            break;
        *work += jaos_work_units(hv);
        (*solves_done)++;
        if (jaos_status_of(hv) != JAOS_SOLVE_OPTIMAL)
            break;
        if (jaos_solution(hv, out, nullptr, nullptr, nullptr) != JAOS_OK)
            break;
        int64_t pick = -1;
        double near = 2.0;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j])
                continue;
            const double f = out[j] - floor(out[j]);
            const double d = f < 0.5 ? f : 1.0 - f;
            if (d <= MIP_INT_TOL)
                continue;
            if (d < near) {
                near = d;
                pick = j;
            }
        }
        if (pick < 0) {
            rc = 1;                    /* every integer column is integral */
            break;
        }
        double v = round(out[pick]);
        if (v < hv->col_lower[pick])
            v = hv->col_lower[pick];
        if (v > hv->col_upper[pick])
            v = hv->col_upper[pick];
        if (jaos_set_col_bounds(hv, pick, v, v) != JAOS_OK)
            break;
    }
    jaos_model_free(hv);
    return rc;
}

/* --- The feasibility pump (D318) --------------------------------------- */

/* Move `MIP_PUMP_FLIPS` of the rounding's integer columns to the other
 * side of the value the relaxation gave, furthest first and the lowest
 * index on a tie, which is what a stalled pump needs to leave the
 * rounding it keeps coming back to. A column at its lower bound goes up
 * one, one at its upper bound goes down one, and one at neither goes away
 * from the value the relaxation gave it. A column whose bounds hold no
 * second integer has no other side and is skipped, and so is a move that
 * lands outside the bounds or that changes nothing -- the last happens
 * where the value is large enough that adding one to it is a no-op, and
 * reporting it as progress would leave the pump re-solving an identical
 * relaxation for every round it has left. Returns false when nothing
 * moved, which ends the pump. */
static bool pump_flip(const jaos_model *m, const double *x, double *rnd)
{
    const int64_t nc = m->num_col;
    int64_t chosen[MIP_PUMP_FLIPS];
    int64_t n_chosen = 0;
    bool moved = false;
    for (int64_t k = 0; k < MIP_PUMP_FLIPS; k++) {
        int64_t pick = -1;
        double far = -1.0;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j] || !isfinite(x[j]))
                continue;
            if (m->col_upper[j] - m->col_lower[j] < 1.0 - MIP_INT_TOL)
                continue;              /* fixed: no other side */
            bool taken = false;
            for (int64_t q = 0; q < n_chosen; q++)
                if (chosen[q] == j) {
                    taken = true;
                    break;
                }
            if (taken)
                continue;
            const double d = fabs(x[j] - rnd[j]);
            if (d > far) {             /* strict: the lowest index on a tie */
                far = d;
                pick = j;
            }
        }
        if (pick < 0)
            break;
        chosen[n_chosen++] = pick;
        const double lo = m->col_lower[pick], hi = m->col_upper[pick];
        double v = rnd[pick];
        if (isfinite(hi) && v >= hi - MIP_INT_TOL)
            v -= 1.0;
        else if (isfinite(lo) && v <= lo + MIP_INT_TOL)
            v += 1.0;
        else
            v = x[pick] >= v ? v + 1.0 : v - 1.0;
        if (v < lo || v > hi || v == rnd[pick])
            continue;                  /* this one cannot move */
        rnd[pick] = v;
        moved = true;
    }
    return moved;
}

/* An integer column whose bounds hold more than two integers, so its
 * rounding can sit away from both and the binary distance loses it. A
 * bound at infinity counts as such a column. The one predicate the
 * auxiliaries, their rows and the cost loop all read, so a column cannot
 * get both an auxiliary and a +-1 term, or neither. */
static inline bool pump_general_col(const jaos_model *m, int64_t j)
{
    return m->col_integer[j] &&
           m->col_upper[j] - m->col_lower[j] > 1.0 + MIP_INT_TOL;
}

/* The pump of Fischetti, Glover and Lodi (The feasibility pump,
 * Mathematical Programming 104, 2005), at the root, on a copy of the
 * relaxation as the cuts left it. One round rounds the point it holds to
 * the nearest integer inside the model's bounds, then replaces the copy's
 * objective with the L1 distance to that rounding and solves: the point
 * that comes back is the nearest point of the relaxation to an integer
 * point, and it is rounded again. A point that comes back integral is the
 * caller's to judge, through `rounded_point` like every other heuristic
 * point.
 *
 * The distance is written the way it is written for binary columns, one
 * term per column and no auxiliary variable: a column rounded to its
 * lower bound costs `+x_j` and one rounded to its upper bound costs
 * `-x_j`, so the sum is the L1 distance up to a constant. A general
 * integer column rounded to neither of its bounds has no such term and is
 * left out of the distance, since writing it would want a variable per
 * column and this pump does not add rows or columns. The paper's own
 * pump is stated for binaries and handles the general case with those
 * auxiliaries; what is here is the binary pump, exactly, and a general
 * integer column pulls on it only while its rounding sits on a bound.
 *
 * `general` writes those auxiliaries (Bertacco, Fischetti and Lodi, A
 * feasibility pump heuristic for general mixed-integer problems, Discrete
 * Optimization 4, 2007): every integer column whose bounds hold more than
 * two integers gets one column `d_q >= 0` with cost 1 and the two rows
 * `x_j - d_q <= r_j` and `x_j + d_q >= r_j` on the pump's private copy,
 * so minimizing pays exactly `|x_j - r_j|` wherever the rounding `r_j`
 * sits. The rows keep their coefficients and only their bounds move each
 * round, which keeps the copy's basis. Such a column has no +-1 term.
 *
 * `obj_decay`, when positive, is the objective pump (Achterberg and
 * Berthold, Improving the feasibility pump, Discrete Optimization 4,
 * 2007): the round's cost is `(1 - a)` times the distance plus
 * `a * |dist| / |c|` times the model's own minimized objective, the norms
 * Euclidean, and `a` multiplies by `obj_decay` each round from 1, so the
 * blend fades to the plain distance. `a` decays by multiplication, never
 * through pow(), whose last bit is libm's. A model with no objective
 * blends nothing. The norm squares each cost, so a cost past 1.3e154
 * overflows it to infinity and the blend's weight on the objective is
 * then zero, a plain pump scaled by `(1 - a)`; a cost under 1.5e-162
 * squares to zero. No instance of the MIP set is within a hundred
 * orders of either, and the exposure is stated rather than repaired.
 *
 * A rounding that comes back unchanged would repeat for ever, so
 * `MIP_PUMP_FLIPS` of its columns are moved to the other side, chosen by
 * how far the relaxation's value sits from the rounding, the lowest index
 * breaking a tie. The paper draws that count at random; a draw would
 * break D8, so the count is fixed and the choice inside it is a total
 * order.
 *
 * The rounding is compared against the last two the relaxation produced,
 * not against the last one after a flip: a pump that alternates between
 * two roundings would otherwise never look stalled, which is the case the
 * paper's restart exists for. `out` carries the point, `rnd`, `prev` and
 * `prev2` are scratch of num_col.
 * Returns 1 with an integral point in `out`, 0 when there is none, -1
 * only when the copy or its auxiliaries could not be made. Every solve is
 * billed and counted. */
static int pump_for_point(const jaos_model *m, const jaos_model *lp,
                          const double *x, int64_t rounds, bool general,
                          double obj_decay, double *out, double *rnd,
                          double *prev, double *prev2, int64_t *work,
                          int64_t *solves_done)
{
    const int64_t nc = m->num_col;
    const int64_t nr0 = lp->num_row;
    jaos_model *pv = nullptr;
    double *sol = nullptr;             /* the copy's point, nc + g wide */
    int64_t *gcol = nullptr;           /* the general columns, index order */
    int64_t g = 0;
    if (jaos_model_copy(lp, &pv) != JAOS_OK)
        return -1;
    int rc = 0;
    if (jaos_set_objective_sense(pv, JAOS_MINIMIZE) != JAOS_OK ||
        jaos_set_objective_offset(pv, 0.0) != JAOS_OK)
        goto out_free;
    if (general) {
        *work += nc;                   /* one pass to count them (D16) */
        for (int64_t j = 0; j < nc; j++)
            if (pump_general_col(m, j))
                g++;
    }
    if (g > 0) {
        /* The auxiliaries, once: the rows' bounds move each round, their
         * coefficients never do, so the copy keeps its basis. One pass
         * to place them, and the two adds each rebuild the copy's matrix
         * (D16). */
        *work += nc;
        gcol = malloc((size_t)g * sizeof *gcol);
        double *ac = malloc((size_t)g * sizeof *ac);
        double *alo = malloc((size_t)g * sizeof *alo);
        double *ahi = malloc((size_t)g * sizeof *ahi);
        double *rlo = malloc((size_t)(2 * g) * sizeof *rlo);
        double *rhi = malloc((size_t)(2 * g) * sizeof *rhi);
        int64_t *rs = malloc((size_t)(2 * g + 1) * sizeof *rs);
        int64_t *ri = malloc((size_t)(4 * g) * sizeof *ri);
        double *rv = malloc((size_t)(4 * g) * sizeof *rv);
        bool ok = gcol != nullptr && ac != nullptr && alo != nullptr &&
                  ahi != nullptr && rlo != nullptr && rhi != nullptr &&
                  rs != nullptr && ri != nullptr && rv != nullptr;
        if (ok) {
            int64_t t = 0;
            for (int64_t j = 0; j < nc; j++)
                if (pump_general_col(m, j))
                    gcol[t++] = j;
            for (int64_t q = 0; q < g; q++) {
                ac[q] = 1.0;
                alo[q] = 0.0;
                ahi[q] = INFINITY;
                rlo[2 * q] = -INFINITY;    /* set before every solve */
                rhi[2 * q] = INFINITY;
                rlo[2 * q + 1] = -INFINITY;
                rhi[2 * q + 1] = INFINITY;
                rs[2 * q] = 4 * q;
                rs[2 * q + 1] = 4 * q + 2;
                ri[4 * q] = gcol[q];
                rv[4 * q] = 1.0;
                ri[4 * q + 1] = nc + q;
                rv[4 * q + 1] = -1.0;
                ri[4 * q + 2] = gcol[q];
                rv[4 * q + 2] = 1.0;
                ri[4 * q + 3] = nc + q;
                rv[4 * q + 3] = 1.0;
            }
            rs[2 * g] = 4 * g;
            ok = jaos_add_cols(pv, g, ac, alo, ahi, 0, nullptr, nullptr,
                               nullptr) == JAOS_OK &&
                 jaos_add_rows(pv, 2 * g, rlo, rhi, 4 * g, rs, ri, rv)
                     == JAOS_OK;
            if (ok)
                *work += 2 * (pv->num_nz + pv->num_col);
        }
        free(ac);
        free(alo);
        free(ahi);
        free(rlo);
        free(rhi);
        free(rs);
        free(ri);
        free(rv);
        if (!ok) {
            rc = -1;
            goto out_free;
        }
    }
    /* The copy's point is `out` itself unless the auxiliaries widened it,
     * so the plain pump touches exactly the memory it always did. */
    sol = out;
    if (g > 0) {
        sol = malloc((size_t)(nc + g) * sizeof *sol);
        if (sol == nullptr) {
            rc = -1;
            goto out_free;
        }
    }
    double cnorm = 0.0;
    if (obj_decay > 0.0) {
        *work += nc;                   /* the norm's one pass (D16) */
        for (int64_t j = 0; j < nc; j++)
            cnorm += m->col_cost[j] * m->col_cost[j];
        cnorm = sqrt(cnorm);
    }
    const bool blend = obj_decay > 0.0 && cnorm > 0.0;
    const double sgn = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    double alpha = 1.0;
    memcpy(out, x, (size_t)(nc > 0 ? nc : 1) * sizeof *out);
    for (int64_t r = 0; r < rounds; r++) {
        /* One pass to round, one to write the objective (D16). */
        *work += 2 * nc;
        bool integral = true, same1 = r > 0, same2 = r > 1;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j] || !isfinite(out[j])) {
                rnd[j] = out[j];
                if (m->col_integer[j])
                    integral = false;
                continue;
            }
            double v = round(out[j]);
            if (v < m->col_lower[j])
                v = ceil(m->col_lower[j]);
            if (v > m->col_upper[j])
                v = floor(m->col_upper[j]);
            if (fabs(out[j] - v) > MIP_INT_TOL)
                integral = false;
            if (r > 0 && v != prev[j])
                same1 = false;
            if (r > 1 && v != prev2[j])
                same2 = false;
            rnd[j] = v;
        }
        if (integral) {
            rc = 1;
            break;
        }
        /* The history is the rounding as the relaxation made it, before
         * any flip, so the next round's comparison means something. */
        memcpy(prev2, prev, (size_t)(nc > 0 ? nc : 1) * sizeof *prev2);
        memcpy(prev, rnd, (size_t)(nc > 0 ? nc : 1) * sizeof *prev);
        if (same1 || same2) {
            /* Up to MIP_PUMP_FLIPS passes over the columns, billed (D16). */
            *work += MIP_PUMP_FLIPS * nc;
            if (!pump_flip(m, out, rnd))
                break;                 /* nothing left to flip */
        }
        if (g > 0) {
            /* The auxiliaries' rows follow the rounding, after any flip. A
             * column the relaxation left at NaN has no rounding, and its
             * pair stays free, as the +-1 form gives such a column no
             * term. */
            *work += 2 * g;
            for (int64_t q = 0; q < g; q++) {
                const double r = rnd[gcol[q]];
                const double lo = isfinite(r) ? r : -INFINITY;
                const double hi = isfinite(r) ? r : INFINITY;
                if (jaos_set_row_bounds(pv, nr0 + 2 * q, -INFINITY, hi)
                        != JAOS_OK ||
                    jaos_set_row_bounds(pv, nr0 + 2 * q + 1, lo, INFINITY)
                        != JAOS_OK)
                    goto out_free;
            }
        }
        double factor = 0.0, a = 0.0;
        if (blend) {
            /* One extra pass to count the distance's terms (D16). */
            *work += nc;
            alpha *= obj_decay;
            a = alpha;
            int64_t terms = g;
            for (int64_t j = 0; j < nc; j++) {
                if (!m->col_integer[j])
                    continue;
                if (general && pump_general_col(m, j))
                    continue;
                if ((isfinite(m->col_lower[j]) &&
                     rnd[j] <= m->col_lower[j] + MIP_INT_TOL) ||
                    (isfinite(m->col_upper[j]) &&
                     rnd[j] >= m->col_upper[j] - MIP_INT_TOL))
                    terms++;
            }
            factor = a * sqrt((double)terms) / cnorm;
        }
        for (int64_t j = 0; j < nc; j++) {
            double d = 0.0;
            if (m->col_integer[j] && !(general && pump_general_col(m, j))) {
                if (isfinite(m->col_lower[j]) &&
                    rnd[j] <= m->col_lower[j] + MIP_INT_TOL)
                    d = 1.0;
                else if (isfinite(m->col_upper[j]) &&
                         rnd[j] >= m->col_upper[j] - MIP_INT_TOL)
                    d = -1.0;
            }
            const double c = (1.0 - a) * d + factor * sgn * m->col_cost[j];
            if (jaos_set_col_cost(pv, j, c) != JAOS_OK)
                goto out_free;
        }
        if (blend && g > 0) {
            *work += g;
            for (int64_t q = 0; q < g; q++)
                if (jaos_set_col_cost(pv, nc + q, 1.0 - a) != JAOS_OK)
                    goto out_free;
        }
        if (jaos_solve(pv) != JAOS_OK)
            break;
        *work += jaos_work_units(pv);
        (*solves_done)++;
        if (jaos_status_of(pv) != JAOS_SOLVE_OPTIMAL)
            break;
        if (jaos_solution(pv, sol, nullptr, nullptr, nullptr) != JAOS_OK)
            break;
        if (g > 0)
            memcpy(out, sol, (size_t)nc * sizeof *out);
    }
out_free:
    if (g > 0)
        free(sol);
    free(gcol);
    jaos_model_free(pv);
    return rc;
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
    if (!incumbent_take(inc, lp, m->num_row, key))
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

/* --- The solution pool (D299) ------------------------------------------ */

/* The best `cap` distinct integer points offered so far, best first by
 * key (minimize form), the earlier first on a tie; `obj` is each one's
 * objective in the model's sense. Every comparison is exact, so the pool
 * is the same on every machine (D8). */
typedef struct {
    double *x, *key, *obj;
    int64_t n, cap, nc;
} spool;

static bool spool_init(spool *sp, int64_t cap, int64_t nc)
{
    sp->n = 0;
    sp->cap = cap;
    sp->nc = nc;
    sp->x = malloc((size_t)(cap * (nc > 0 ? nc : 1)) * sizeof *sp->x);
    sp->key = malloc((size_t)cap * sizeof *sp->key);
    sp->obj = malloc((size_t)cap * sizeof *sp->obj);
    return sp->x != nullptr && sp->key != nullptr && sp->obj != nullptr;
}

static void spool_free(spool *sp)
{
    free(sp->x); free(sp->key); free(sp->obj);
}

static void spool_offer(spool *sp, const double *x, double key, double obj)
{
    const int64_t nc = sp->nc;
    if (sp->n == sp->cap && !(key < sp->key[sp->n - 1]))
        return;
    for (int64_t i = 0; i < sp->n; i++)
        if (sp->key[i] == key &&
            (nc == 0 || memcmp(sp->x + i * nc, x, (size_t)nc * sizeof *x) == 0))
            return;                    /* held already */
    int64_t pos = 0;
    while (pos < sp->n && !(key < sp->key[pos]))
        pos++;
    const int64_t last = sp->n < sp->cap ? sp->n : sp->cap - 1;
    if (last > pos) {
        memmove(sp->key + pos + 1, sp->key + pos, (size_t)(last - pos) * sizeof *sp->key);
        memmove(sp->obj + pos + 1, sp->obj + pos, (size_t)(last - pos) * sizeof *sp->obj);
        if (nc > 0)
            memmove(sp->x + (pos + 1) * nc, sp->x + pos * nc,
                    (size_t)((last - pos) * nc) * sizeof *sp->x);
    }
    sp->key[pos] = key;
    sp->obj[pos] = obj;
    if (nc > 0)
        memcpy(sp->x + pos * nc, x, (size_t)nc * sizeof *x);
    if (sp->n < sp->cap)
        sp->n++;
}

static const char *dive_child_str(int rule)
{
    switch (rule) {
    case JAOS_DIVE_UP:         return "up first";
    case JAOS_DIVE_DOWN:       return "down first";
    case JAOS_DIVE_PSEUDOCOST: return "pseudocost side first";
    default:                   return "nearer side first";
    }
}

/* --- The tree ---------------------------------------------------------- */

/* Bound propagation at a node (D324). It reads the model's own rows, not
 * the copy's: a node carries cut rows, and reading the copy would rebuild
 * the row-wise mirror at every node, which is the bill D320 found the
 * general pump paying. The bounds it reads are the ones the copy holds
 * right now, which node_apply has just put there.
 *
 * A row's smallest possible activity is the sum over its entries of the
 * coefficient times the end of its column the coefficient's sign points
 * at, and its largest is the same sum over the other ends. A row whose
 * smallest activity is already above its upper bound admits no point at
 * all, and the node is infeasible with no relaxation solved. Where it
 * admits points, the same two sums bound each of the row's own columns:
 * take the sum without column j, and what is left of the row's width is
 * what a_ij x_j may be.
 *
 * Only integer columns are pulled in, and only when the move is a whole
 * integer, so nothing here depends on a continuous bound being reached to
 * the last bit. Only integer columns are written back, and node_apply
 * rebuilds every one of them from ilo/ihi at the next node, so no
 * deduction leaks out of the node that made it.
 *
 * Returns the number of bounds it moved, -1 when the node is infeasible
 * and -2 on an error. */
static int64_t propagate_node(jaos_model *m, jaos_model *lp, double *plo,
                              double *phi, int64_t rounds, int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    if (jm_model_ensure_rowwise(m) != JAOS_OK)
        return -2;
    for (int64_t j = 0; j < nc; j++) {
        plo[j] = lp->col_lower[j];
        phi[j] = lp->col_upper[j];
    }
    int64_t moved = 0;
    for (int64_t r = 0; r < rounds; r++) {
        int64_t moved_here = 0;
        /* One pass over the matrix and both vectors, billed like any
         * kernel (D16). */
        *work += m->num_nz + nc + nr;
        for (int64_t i = 0; i < nr; i++) {
            const double rlo = m->row_lower[i], rhi = m->row_upper[i];
            if (rlo == -INFINITY && rhi == INFINITY)
                continue;
            /* The finite part of each sum, and how many ends are not. */
            double smin = 0.0, smax = 0.0;
            int64_t nmin = 0, nmax = 0;
            for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                const int64_t j = m->ar_index[k];
                const double a = m->ar_value[k];
                const double e = a > 0.0 ? plo[j] : phi[j];
                const double f = a > 0.0 ? phi[j] : plo[j];
                if (isinf(e))
                    nmin++;
                else
                    smin += a * e;
                if (isinf(f))
                    nmax++;
                else
                    smax += a * f;
            }
            /* A row no point can satisfy: the node is infeasible, and the
             * comparison is relative to what went into the sum, since a
             * sum is known to the size of its own terms. */
            if (nmin == 0 && rhi < INFINITY &&
                smin - rhi > MIP_PROP_INFEAS * (1.0 + fabs(rhi) + fabs(smin)))
                return -1;
            if (nmax == 0 && rlo > -INFINITY &&
                rlo - smax > MIP_PROP_INFEAS * (1.0 + fabs(rlo) + fabs(smax)))
                return -1;
            for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                const int64_t j = m->ar_index[k];
                if (!m->col_integer[j])
                    continue;
                const double a = m->ar_value[k];
                const double e = a > 0.0 ? plo[j] : phi[j];
                const double f = a > 0.0 ? phi[j] : plo[j];
                /* The two sums without this column. An end that is itself
                 * infinite is the one the count was for, so the residual
                 * is finite exactly when no OTHER end is. */
                const bool rmin_ok = isinf(e) ? nmin == 1 : nmin == 0;
                const bool rmax_ok = isinf(f) ? nmax == 1 : nmax == 0;
                const double rmin = isinf(e) ? smin : smin - a * e;
                const double rmax = isinf(f) ? smax : smax - a * f;
                double ub = INFINITY, lb = -INFINITY;
                if (rmin_ok && rhi < INFINITY) {
                    const double t = (rhi - rmin) / a;
                    if (a > 0.0)
                        ub = t;
                    else
                        lb = t;
                }
                if (rmax_ok && rlo > -INFINITY) {
                    const double t = (rlo - rmax) / a;
                    if (a > 0.0) {
                        if (t > lb)
                            lb = t;
                    } else if (t < ub) {
                        ub = t;
                    }
                }
                /* Loosened before it is rounded, so the integer the row
                 * really admits is never pulled away. */
                if (ub < INFINITY) {
                    const double nh =
                        floor(ub + MIP_PROP_SLACK * (1.0 + fabs(ub)));
                    if (nh < phi[j] - MIP_PROP_MOVE) {
                        phi[j] = nh;
                        moved_here++;
                    }
                }
                if (lb > -INFINITY) {
                    const double nl =
                        ceil(lb - MIP_PROP_SLACK * (1.0 + fabs(lb)));
                    if (nl > plo[j] + MIP_PROP_MOVE) {
                        plo[j] = nl;
                        moved_here++;
                    }
                }
                if (plo[j] > phi[j] + MIP_PROP_MOVE)
                    return -1;   /* no integer left in the column */
            }
        }
        moved += moved_here;
        if (moved_here == 0)
            break;              /* a pass that moved nothing ends it */
    }
    for (int64_t j = 0; moved > 0 && j < nc; j++) {
        if (!m->col_integer[j])
            continue;
        if (plo[j] == lp->col_lower[j] && phi[j] == lp->col_upper[j])
            continue;
        if (jaos_set_col_bounds(lp, j, plo[j], phi[j]) != JAOS_OK)
            return -2;
    }
    return moved;
}

jaos_status jm_branch_and_bound(jaos_model *m)
{
    const double t0 = now_seconds();
    const int64_t nc = m->num_col, nr = m->num_row;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double gap = m->cfg.mip_gap > 0.0 ? m->cfg.mip_gap : MIP_GAP;
    const int64_t rounds = m->cfg.mip_cut_rounds_set ? m->cfg.mip_cut_rounds
                                                     : MIP_CUT_ROUNDS;
    const int64_t cover_rounds = m->cfg.mip_cover_rounds_set
        ? m->cfg.mip_cover_rounds : MIP_COVER_ROUNDS;
    const int64_t mir_rounds = m->cfg.mip_mir_rounds_set ? m->cfg.mip_mir_rounds
                                                         : MIP_MIR_ROUNDS;
    int64_t root_rounds = rounds > cover_rounds ? rounds : cover_rounds;
    if (mir_rounds > root_rounds)
        root_rounds = mir_rounds;
    const int64_t backtrack = m->cfg.mip_dive_backtrack_set
        ? m->cfg.mip_dive_backtrack : MIP_DIVE_BACKTRACK;
    const double dive_gap = m->cfg.mip_dive_gap_set ? m->cfg.mip_dive_gap
                                                    : MIP_DIVE_GAP;
    const bool node_mir = m->cfg.mip_node_mir_set ? m->cfg.mip_node_mir
                                                  : MIP_NODE_MIR;
    const int64_t mir_aggregate = m->cfg.mip_mir_aggregate_set
        ? m->cfg.mip_mir_aggregate : MIP_MIR_AGGREGATE;
    const int64_t dive_heur = m->cfg.mip_dive_heuristic_set
        ? m->cfg.mip_dive_heuristic : MIP_DIVE_HEURISTIC;
    const int64_t dive_heur_depth = m->cfg.mip_dive_heuristic_depth_set
        ? m->cfg.mip_dive_heuristic_depth : MIP_DIVE_HEURISTIC_DEPTH;
    const int64_t rins = m->cfg.mip_rins_set ? m->cfg.mip_rins : MIP_RINS;
    const int64_t feaspump = m->cfg.mip_feaspump_set ? m->cfg.mip_feaspump
                                                     : MIP_FEASPUMP;
    const bool pump_general = m->cfg.mip_pump_general_set
        ? m->cfg.mip_pump_general : MIP_PUMP_GENERAL;
    const double pump_obj = m->cfg.mip_pump_obj_set ? m->cfg.mip_pump_obj
                                                    : MIP_PUMP_OBJ;
    const bool pump_always = m->cfg.mip_pump_always_set
        ? m->cfg.mip_pump_always : MIP_PUMP_ALWAYS;
    const bool rcfix = m->cfg.mip_rcfix_set ? m->cfg.mip_rcfix : MIP_RCFIX;
    const int64_t propagate = m->cfg.mip_propagate_set ? m->cfg.mip_propagate
                                                       : MIP_PROPAGATE;
    const int64_t propagate_depth = m->cfg.mip_propagate_depth_set
        ? m->cfg.mip_propagate_depth : MIP_PROPAGATE_DEPTH;
    /* The caller's cutoff (D326), in the tree's own minimize form; +inf
     * when there is none, so nothing below needs a second flag. It does
     * two things, and both are needed for the answer to mean what the
     * header says. It prunes: a node that cannot reach past it is dropped
     * unsolved, from node 1 and with no incumbent needed. And it gates
     * what may BECOME the incumbent: a heuristic point no better than the
     * cutoff is not taken, so a search that ends with nothing ends
     * INFEASIBLE, which is the honest answer to "is there a solution
     * better than this?". Without the second half a point found before
     * the pruning started would be published as an answer that does not
     * satisfy the question that was asked. An integral node needs no gate
     * of its own: its key IS its objective, so the prune above has
     * already dropped it. */
    const double cut_key = m->cfg.mip_cutoff_set
        ? sigma * m->cfg.mip_cutoff : INFINITY;
    const double degrade = m->cfg.mip_dive_degrade_set
        ? m->cfg.mip_dive_degrade : MIP_DIVE_DEGRADE;
    const bool dive = m->cfg.mip_dive;
    /* The dive keeps its siblings on a stack when either rule may bring
     * it back for one (D308, D311); D289's form otherwise. */
    const bool stack_dive = dive && (backtrack > 0 || dive_gap > 0.0);
    const bool heur = !m->cfg.mip_no_heuristics;
    const jaos_branching rule = (jaos_branching)m->cfg.mip_branching;
    const int64_t reliability = rule == JAOS_BRANCH_PSEUDOCOST
        ? (m->cfg.mip_reliability_set ? m->cfg.mip_reliability : MIP_RELIABILITY)
        : 0;
    const double probe_cap = m->cfg.mip_probe_cap_set ? m->cfg.mip_probe_cap
                                                      : MIP_PROBE_CAP;
    const int dive_child = m->cfg.mip_dive_child;
    const int64_t cut_depth = m->cfg.mip_cut_depth_set ? m->cfg.mip_cut_depth
                                                       : MIP_CUT_DEPTH;
    const bool cut_drop = !m->cfg.mip_no_cut_drop;
    const int64_t node_cut_cap = m->cfg.mip_node_cut_cap_set
        ? m->cfg.mip_node_cut_cap : MIP_NODE_CUT_CAP;
    const int64_t probe_depth = m->cfg.mip_probe_depth_set
        ? m->cfg.mip_probe_depth : MIP_PROBE_DEPTH;
    const int64_t pool_size = m->cfg.mip_pool_size > 0 ? m->cfg.mip_pool_size : 1;
    const double cut_stall = m->cfg.mip_cut_stall_set ? m->cfg.mip_cut_stall
                                                      : MIP_CUT_STALL;
    const double node_cut_stall = m->cfg.mip_node_cut_stall_set
        ? m->cfg.mip_node_cut_stall : MIP_NODE_CUT_STALL;
    const bool root_cut_drop = m->cfg.mip_root_cut_drop_set
        ? m->cfg.mip_root_cut_drop : MIP_ROOT_CUT_DROP;
    const bool cover_lift = m->cfg.mip_cover_lift_set ? m->cfg.mip_cover_lift
                                                      : MIP_COVER_LIFT;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    jaos_model *lp = nullptr;
    bheap heap = {0};
    incumbent inc = {0};
    cutbuf cb = {0}, pool = {0};       /* one round; every local cut   */
    int64_t *act = nullptr;            /* the local cuts in force, pool
                                          indices, and their count/cap  */
    int64_t act_n = 0, act_cap = 0;
    int64_t nfixed = nr;               /* rows with no local cut in the copy */
    cutlist in_copy = {0};             /* the local cuts the copy holds now */
    jaos_basis_status *crs = nullptr;  /* a child's row statuses when a cut
                                          is dropped (D297)                */
    int64_t crs_cap = 0;
    spool sp = {0};
    int64_t local_cuts = 0;
    bnode *cur = nullptr, *next = nullptr;
    double *x = nullptr, *row = nullptr, *cut = nullptr;
    int64_t row_cap = 0;
    double *xr = nullptr, *ra = nullptr, *x2 = nullptr;
    double *ilo = nullptr, *ihi = nullptr, *pc_sum = nullptr;
    double *plo = nullptr, *phi = nullptr;   /* propagation's bounds (D324) */
    double *rcd = nullptr;                   /* the root's reduced costs    */
    int64_t *pc_n = nullptr, *cand = nullptr;
    jaos_basis_status *pcs = nullptr, *prs = nullptr;
    int64_t next_id = 0, nodes = 0, solves = 0, cuts = 0, heur_points = 0;
    int64_t probes = 0, capped = 0;    /* strong branching's, D293/D294 */
    int64_t dive_points = 0;           /* the dive heuristic's, D313      */
    int64_t rins_points = 0;           /* RINS's, D315                    */
    int64_t pump_points = 0;           /* the feasibility pump's, D318    */
    double rins_key = 0.0;             /* the incumbent RINS last saw     */
    bool rins_seen = false;
    int64_t covers = 0;                /* cover cuts at the root, D300    */
    kitem *items = nullptr;
    double *mu = nullptr;              /* the cover's partial sums (D307) */
    double *mbest = nullptr, *mdelta = nullptr;   /* MIR scratch (D309) */
    double *prnd = nullptr;            /* the pump's rounding (D318)     */
    double *pprev = nullptr;           /* the one before it              */
    double *pprev2 = nullptr;          /* and the one before that        */
    double *magg = nullptr;            /* the MIR side, dense (D309)     */
    double *mcmag = nullptr;           /* what went into each of its
                                          coefficients (D312)            */
    bool *mused = nullptr;             /* the rows an aggregate holds    */
    bool *mpicked = nullptr;           /* the columns it substituted out */
    int64_t mirs = 0;                  /* MIR cuts at the root, D309      */
    bnode **dstack = nullptr;          /* the dive's siblings (D308)      */
    int64_t dstack_n = 0, dstack_cap = 0, backtracks = 0;
    int64_t first_inc = 0;             /* the node of the first incumbent */
    int64_t rcfixed = 0;               /* bounds the root's fixing moved  */
    int64_t tightened = 0;             /* bounds propagation moved        */
    int64_t work = 0, iters = 0;
    double best_bound = -INFINITY;     /* minimize form */
    jaos_solve_status outcome = JAOS_SOLVE_NOT_RUN;

    /* The answer the model holds is about the previous problem. */
    free(m->mip_inc_x);
    m->mip_inc_x = nullptr;
    free(m->mip_pool_x);
    m->mip_pool_x = nullptr;
    free(m->mip_pool_obj);
    m->mip_pool_obj = nullptr;
    m->mip_pool_n = 0;
    m->mip_nodes = m->mip_solves = m->mip_cuts = m->mip_heur = 0;
    m->mip_first_inc = 0;
    m->mip_bound = 0.0;
    m->mip_has_incumbent = false;
    /* At entry, so the two solve entry points agree: `jm_dual_simplex`
     * clears the flag before it runs, and a tree that fails on an
     * allocation before it publishes would otherwise leave the previous
     * solve's basis readable (D330). */
    m->sol_basis_ok = false;

    if (jaos_model_copy(m, &lp) != JAOS_OK)
        goto done;
    free(lp->col_integer);
    lp->col_integer = nullptr;
    lp->cfg.log_cb = nullptr;
    jaos_clear_basis(lp);

    x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x);
    xr = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *xr);
    x2 = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x2);
    ra = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *ra);
    ilo = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *ilo);
    ihi = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *ihi);
    pc_sum = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_sum);
    pc_n = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_n);
    cand = malloc((size_t)MIP_STRONG_CANDIDATES * sizeof *cand);
    pcs = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pcs);
    if (propagate > 0) {
        plo = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *plo);
        phi = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *phi);
        if (plo == nullptr || phi == nullptr)
            goto done;
    }
    if (x == nullptr || xr == nullptr || x2 == nullptr || ra == nullptr ||
        ilo == nullptr ||
        ihi == nullptr || pc_sum == nullptr || pc_n == nullptr ||
        cand == nullptr || pcs == nullptr || !spool_init(&sp, pool_size, nc))
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
               "of cuts, %lld of MIR over %lld aggregated rows, cuts to "
               "depth %lld%s, dive %s with %lld "
               "backtracks and a resume gap of %g, rounding %s, %s "
               "branching, reliability %lld, probe cap %gx, cut stall %g "
               "at the root and %g below it, root cuts %s, covers %s",
               (long long)nint, (long long)nc, (long long)rounds,
               (long long)mir_rounds, (long long)mir_aggregate,
               (long long)cut_depth,
               node_mir ? " with MIR" : "",
               dive ? dive_child_str(dive_child) : "off",
               (long long)backtrack, dive_gap, heur ? "on" : "off",
               rule == JAOS_BRANCH_MOST_FRACTIONAL ? "most-fractional"
                                                   : "pseudocost",
               (long long)reliability, probe_cap, cut_stall, node_cut_stall,
               root_cut_drop ? "dropped when slack" : "kept",
               cover_lift ? "lifted" : "extended");
    }

    /* The root is the node with no changes. An outcome already known --
     * a column with no integer inside its bounds -- skips the tree. */
    for (; outcome == JAOS_SOLVE_NOT_RUN;) {
        /* Which node: the root first; then the dive's child, or the deepest
         * sibling the dive left on its stack while it may still backtrack
         * (D308), or the best open one, the stack emptied into the open
         * set first; a node whose key no longer beats the incumbent is
         * dropped unsolved, which ends a dive. */
        if (nodes > 0) {
            node_free(cur);
            for (;;) {
                bool resumed = false;
                if (next != nullptr) {
                    cur = next;
                    next = nullptr;
                } else if (dstack_n > 0 &&
                           (backtrack == 0 || backtracks < backtrack) &&
                           resume_within(dstack[dstack_n - 1], &heap, dstack,
                                         dstack_n, dive_gap)) {
                    cur = dstack[--dstack_n];
                    resumed = true;
                } else {
                    while (dstack_n > 0) {
                        if (!heap_push(&heap, dstack[dstack_n - 1]))
                            goto done;
                        dstack_n--;
                    }
                    backtracks = 0;
                    cur = heap_pop(&heap);
                    if (cur == nullptr)
                        break;
                    best_bound = cur->key;
                }
                /* What this node must beat: the incumbent, the caller's
                 * cutoff, or the better of the two (D326). */
                const double bk = inc.have && inc.key < cut_key ? inc.key
                                                                : cut_key;
                if (bk < INFINITY &&
                    bk - cur->key <= gap * (1.0 + fabs(bk))) {
                    node_free(cur);
                    cur = nullptr;
                    continue;
                }
                /* A resume counts once its node is solved: a sibling the
                 * gap drops unsolved spends none of the budget (D308). */
                if (resumed)
                    backtracks++;
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

        if (node_apply(lp, m, ilo, ihi, nodes > 0 ? cur : nullptr, &pool,
                       nfixed, &in_copy) != JAOS_OK)
            goto done;
        const int64_t depth_here = nodes > 0 ? cur->depth : 0;
        bool stalled = false;          /* this node's round moved nothing */
        act_n = 0;
        if (nodes > 0 && cur->ncuts > 0) {
            if (!JM_GROW(act, act_cap, cur->ncuts))
                goto done;
            memcpy(act, cur->cuts, (size_t)cur->ncuts * sizeof *act);
            act_n = cur->ncuts;
        }
        nodes++;
        /* Bound propagation (D324): the node's own bounds read over the
         * model's rows, before any relaxation is solved. A node it proves
         * infeasible costs no solve at all; the bounds it moves make the
         * relaxation this node does solve a tighter one. */
        if (propagate > 0 &&
            (propagate_depth < 0 || depth_here <= propagate_depth)) {
            const int64_t got = propagate_node(m, lp, plo, phi, propagate,
                                               &work);
            if (got == -2)
                goto done;
            if (got == -1) {
                jm_log(m, JAOS_LOG_PROGRESS,
                       "node %lld: infeasible by propagation",
                       (long long)nodes);
                continue;
            }
            tightened += got;
            /* The root's deductions were made over the model's own bounds,
             * so they hold for every integer point of the model and not
             * only under this node: ilo and ihi take them, and every node
             * of the tree gets them from node_apply for nothing. A deeper
             * node's are read over ITS bounds and stay where they were
             * made. */
            if (nodes == 1 && got > 0) {
                for (int64_t j = 0; j < nc; j++) {
                    if (!m->col_integer[j])
                        continue;
                    ilo[j] = lp->col_lower[j];
                    ihi[j] = lp->col_upper[j];
                }
            }
        }
        jaos_status st = jaos_solve(lp);
        solves++;
        const int64_t node_work = jaos_work_units(lp);
        work += node_work;
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
        /* What the branch itself cost, before this node's own cut round
         * raises the bound (D316): a node whose cuts worked would
         * otherwise read as a node whose branch went badly. */
        const double branch_key = key;
        if (nodes > 1)
            pseudocost_learn(cur, key, nc, pc_sum, pc_n);
        int64_t branch = select_branch(m, x, rule, pc_sum, pc_n);

        /* The root's cuts: rounds until one adds nothing or the point is
         * integral. A relaxation the cuts make infeasible is an infeasible
         * integer program, since every cut is valid for it. The tableau
         * row spans the columns and every row the copy can come to hold. */
        if (nodes == 1 && branch >= 0 && root_rounds > 0) {
            /* A round adds at most a Gomory cut per column and two covers
             * and two MIR cuts per row, and the row scratch must span them
             * all. */
            const int64_t need = nc + lp->num_row + 1 +
                                 (nc + 4 * nr + 1) * root_rounds;
            double *grown = realloc(row, (size_t)need * sizeof *row);
            if (grown == nullptr)
                goto done;
            row = grown;
            row_cap = need;
            if (cut == nullptr)
                cut = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *cut);
            if (cut == nullptr)
                goto done;
            if (cover_rounds > 0 && items == nullptr)
                items = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *items);
            if (cover_rounds > 0 && mu == nullptr)
                mu = malloc((size_t)(nc + 1) * sizeof *mu);
            if (cover_rounds > 0 && (items == nullptr || mu == nullptr))
                goto done;
            if (mir_rounds > 0 && mbest == nullptr)
                mbest = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mbest);
            if (mir_rounds > 0 && mdelta == nullptr)
                mdelta = malloc((size_t)(MIP_MIR_DELTAS + 1) * sizeof *mdelta);
            if (mir_rounds > 0 && magg == nullptr)
                magg = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *magg);
            if (mir_rounds > 0 && mir_aggregate > 0 && mused == nullptr) {
                mused = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *mused);
                mpicked = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mpicked);
                mcmag = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mcmag);
            }
            if (mir_rounds > 0 && (mbest == nullptr || mdelta == nullptr ||
                                   magg == nullptr ||
                                   (mir_aggregate > 0 &&
                                    (mused == nullptr || mpicked == nullptr ||
                                     mcmag == nullptr))))
                goto done;
            const double key_first = key;  /* before any cut (D305) */
            bool stop = false;
            for (int64_t r = 0; r < root_rounds && !stop; r++) {
                cb.n = cb.nnz = 0;
                int64_t got = 0;
                if (r < rounds) {
                    got = gomory_round(lp, m, x, &cb, row, cut, &work);
                    if (got < 0)
                        goto done;
                }
                if (r < cover_rounds) {
                    const int64_t cv = cover_round(m, lp, x, ilo, ihi, &cb,
                                                   items, cut, mu, cover_lift,
                                                   &work);
                    if (cv < 0)
                        goto done;
                    covers += cv;
                    got += cv;
                }
                if (r < mir_rounds) {
                    const int64_t mv = mir_round(m, lp, x, ilo, ihi, &cb, cut,
                                                 mbest, mdelta, magg, &work);
                    if (mv < 0)
                        goto done;
                    mirs += mv;
                    got += mv;
                    if (mir_aggregate > 0) {
                        const int64_t av = mir_aggregate_round(
                            m, lp, x, ilo, ihi, &cb, cut, mbest, mdelta, magg,
                            mcmag, mused, mpicked, mir_aggregate, &work);
                        if (av < 0)
                            goto done;
                        mirs += av;
                        got += av;
                    }
                }
                if (got == 0)
                    break;
                if (cuts_add(lp, &cb) != JAOS_OK)
                    goto done;
                /* Root cuts that may leave (D306) are pool cuts in force
                 * at the root, like a node's own; the fixed rows stay the
                 * model's. */
                if (root_cut_drop) {
                    for (int64_t k = 0; k < cb.n; k++) {
                        if (!cutbuf_append(&pool, &cb, k) ||
                            !JM_GROW(act, act_cap, act_n + 1) ||
                            !JM_GROW(in_copy.v, in_copy.cap, in_copy.n + 1))
                            goto done;
                        act[act_n++] = pool.n - 1;
                        in_copy.v[in_copy.n++] = pool.n - 1;
                    }
                }
                cuts += got;
                const double key_before = key;
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
                /* The stall (D304): a round that moved the bound by less
                 * than the fraction is the last. */
                if (cut_stall > 0.0 &&
                    key - key_before < cut_stall * (1.0 + fabs(key_before)))
                    break;
            }
            if (stop)
                break;
            /* The root's whole cut phase, judged for the nodes under it
             * (D305): no cut is no evidence. */
            if (node_cut_stall > 0.0 && cuts > 0 &&
                key - key_first < node_cut_stall * (1.0 + fabs(key_first)))
                stalled = true;
        }
        if (nodes == 1) {
            best_bound = key;
            /* The root's cuts stay for good, unless they may leave (D306),
             * in which case the copy's fixed rows are the model's. */
            nfixed = root_cut_drop ? nr : lp->num_row;
            jm_log(m, JAOS_LOG_SUMMARY,
                   "root: relaxation %.17g after %lld cuts, %lld of them "
                   "covers and %lld MIR",
                   obj, (long long)cuts, (long long)covers, (long long)mirs);
        }
        /* The caller's own integer point (D326), once, at the root and
         * before every heuristic, so a heuristic that would find a worse
         * one never becomes the incumbent and the root's own bound is
         * judged against it. It goes through `rounded_point` like every
         * other point offered to the tree, so a point that is not integral
         * inside the tolerance, or not inside every bound and every row,
         * is refused rather than taken: a starting point the caller got
         * wrong must not be published as an answer. `first_inc` stays 0,
         * because no node found it. */
        if (nodes == 1 && m->mip_start != nullptr) {
            double hobj = 0.0;
            work += m->num_nz + nc + nr;
            if (rounded_point(m, m->mip_start, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);
                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    jm_log(m, JAOS_LOG_SUMMARY,
                           "root: incumbent %.17g from the caller's starting "
                           "point", hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key,
                                            true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            } else {
                jm_log(m, JAOS_LOG_SUMMARY,
                       "root: the caller's starting point is not a feasible "
                       "integer point of this model, and is not taken");
            }
        }
        /* The dive heuristic (D313), on the root's relaxation as the cuts
         * left it and on every node down to its own depth (D314): a point
         * it reaches is judged by rounded_point, the same acceptance the
         * rounding heuristic's point gets. `cur` is null at the root. */
        if (dive_heur > 0 && branch >= 0 &&
            (cur == nullptr || cur->depth <= dive_heur_depth)) {
            const int got = dive_for_point(m, lp, dive_heur, nullptr, nullptr,
                                           xr, &work, &solves);
            if (got < 0)
                goto done;
            double hobj = 0.0;
            /* One pass over the matrix, billed like any kernel (D16), and
             * only when the pass runs: the dive fires at every node inside
             * its depth (D314) and most of them reach no point. */
            if (got == 1)
                work += m->num_nz + nc + nr;
            if (got == 1 && rounded_point(m, xr, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);
                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    dive_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "node %lld: incumbent %.17g by the dive heuristic",
                           (long long)nodes, hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key, true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
            /* The root's own point is what the tree branches on. */
            if (jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
                goto done;
        }
        /* The feasibility pump (D318), once, at the root, on the
         * relaxation as the cuts left it, and only while nothing has an
         * answer yet. This is the plain pump of the 2005 paper: it looks
         * for a feasible point, not a good one, so where the rounding
         * heuristic or the dive already put an incumbent at the root it
         * can only cost. Eight of the MIP set's 24 have one there and the
         * six the pump helps have none, so the guard keeps every gain
         * (bench/measurements/02-208/). */
        if (nodes == 1 && feaspump > 0 && branch >= 0 &&
            (!inc.have || pump_always)) {
            /* Its own scratch, and not the root cut block's: that block
             * runs only when a cut round does, and the pump is a heuristic
             * that stands on its own. */
            if (prnd == nullptr) {
                prnd = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *prnd);
                pprev = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pprev);
                pprev2 = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pprev2);
                if (prnd == nullptr || pprev == nullptr || pprev2 == nullptr)
                    goto done;
            }
            const int got = pump_for_point(m, lp, x, feaspump, pump_general,
                                           pump_obj, xr, prnd, pprev, pprev2,
                                           &work, &solves);
            if (got < 0)
                goto done;
            double hobj = 0.0;
            if (got == 1)
                work += m->num_nz + nc + nr;
            if (got == 1 && rounded_point(m, xr, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);
                /* With the guard as D318 left it there is no incumbent
                 * here and this test always passes; with the pump let
                 * through anyway (D322) it is what keeps a worse point
                 * out. It is the acceptance every heuristic point goes
                 * through. */
                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    pump_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "root: incumbent %.17g by the feasibility pump",
                           hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key, true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
        }
        /* RINS (D315): a dive over the columns the incumbent and this
         * node's relaxation do not already agree on, the rest fixed where
         * both put them. It runs once per incumbent -- the first
         * fractional node after the incumbent moved -- since the
         * neighbourhood is a function of the incumbent and the node, and
         * a second run on the same incumbent would search the same set
         * from a point that has not moved much. The dive's own budget is
         * separate, so RINS pays for itself and not for D313. */
        if (rins > 0 && inc.have && branch >= 0 &&
            (!rins_seen || inc.key != rins_key)) {
            rins_seen = true;
            rins_key = inc.key;
            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves);
            if (got < 0)
                goto done;
            double hobj = 0.0;
            if (got == 1)
                work += m->num_nz + nc + nr;
            if (got == 1 && rounded_point(m, xr, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);
                if (hkey < cut_key && hkey < inc.key) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    rins_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "node %lld: incumbent %.17g by RINS",
                           (long long)nodes, hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key, true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
        }
        /* One round of local cuts at a node inside the depth (D296): read
         * over the node's bounds, so valid in its subtree; into the pool,
         * the active list and the copy, then the relaxation again. A
         * relaxation the cuts make infeasible prunes the node, since every
         * cut holds for every integer point under it. The tableau row
         * spans the columns and every row the copy can come to hold. */
        if (nodes > 1 && branch >= 0 && cur->depth <= cut_depth &&
            !cur->no_cuts) {
            const int64_t need = nc + lp->num_row + 1 + (nc > 0 ? nc : 1);
            if (need > row_cap) {
                double *grown = realloc(row, (size_t)need * sizeof *row);
                if (grown == nullptr)
                    goto done;
                row = grown;
                row_cap = need;
            }
            if (cut == nullptr)
                cut = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *cut);
            if (cut == nullptr)
                goto done;
            cb.n = cb.nnz = 0;
            int64_t got = gomory_round(lp, m, x, &cb, row, cut, &work);
            if (got < 0)
                goto done;
            /* MIR cuts over the node's own bounds (D310), into the same
             * round and under the same cap. */
            if (node_mir) {
                if (mbest == nullptr)
                    mbest = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mbest);
                if (mdelta == nullptr)
                    mdelta = malloc((size_t)(MIP_MIR_DELTAS + 1) * sizeof *mdelta);
                if (magg == nullptr)
                    magg = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *magg);
                if (mbest == nullptr || mdelta == nullptr || magg == nullptr)
                    goto done;
                const int64_t mv = mir_round(m, lp, x, lp->col_lower,
                                             lp->col_upper, &cb, cut, mbest,
                                             mdelta, magg, &work);
                if (mv < 0)
                    goto done;
                got += mv;
            }
            /* The cap (D301): the most efficacious cuts of the round stay. */
            if (got > 0 && node_cut_cap > 0)
                got = cutbuf_keep_best(&cb, node_cut_cap);
            if (got < 0)
                goto done;
            if (got > 0) {
                if (cuts_add(lp, &cb) != JAOS_OK)
                    goto done;
                for (int64_t r = 0; r < got; r++) {
                    if (!cutbuf_append(&pool, &cb, r) ||
                        !JM_GROW(act, act_cap, act_n + 1) ||
                        !JM_GROW(in_copy.v, in_copy.cap, in_copy.n + 1))
                        goto done;
                    act[act_n++] = pool.n - 1;
                    in_copy.v[in_copy.n++] = pool.n - 1;
                }
                cuts += got;
                local_cuts += got;
                const double key_before = key;
                st = jaos_solve(lp);
                solves++;
                work += jaos_work_units(lp);
                iters += jaos_iterations(lp);
                if (st != JAOS_OK) {
                    if (st == JAOS_ERR_NUMERICAL) {
                        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                        jm_set_err(m, "node %lld, cuts: %s", (long long)nodes,
                                   jaos_model_error(lp));
                        break;
                    }
                    goto done;
                }
                ns = jaos_status_of(lp);
                if (ns == JAOS_SOLVE_INFEASIBLE)
                    continue;
                if (ns != JAOS_SOLVE_OPTIMAL) {
                    outcome = ns;
                    break;
                }
                if (jaos_objective(lp, &obj) != JAOS_OK ||
                    jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
                    goto done;
                key = sigma * obj;
                branch = select_branch(m, x, rule, pc_sum, pc_n);
                /* The round moved nothing: no round under this node
                 * (D305). */
                if (node_cut_stall > 0.0 &&
                    key - key_before < node_cut_stall * (1.0 + fabs(key_before)))
                    stalled = true;
            }
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
                spool_offer(&sp, xr, hkey, hobj);
                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, xr, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "node %lld: incumbent %.17g by rounding",
                           (long long)nodes, hobj);
                    const double ok = open_key(&heap, dstack, dstack_n);
                    if (!incumbent_announce(m, &inc, nodes,
                            sigma * (ok < key ? ok : key), true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
        }
        /* Reduced-cost fixing at the root (D323), once every heuristic
         * that runs there has had its turn, so it reads the best
         * incumbent the root has. A nonbasic integer column resting at a
         * bound cannot move t away from it without the objective rising
         * by at least |d| t, so t is at most (incumbent - relaxation) /
         * |d|, and the other bound is pulled in to the integer that
         * reaches. The deduction holds for every integer point of the
         * model, since every cut the root carries does, so ilo and ihi
         * take it and every node under here inherits it through
         * node_apply. The branch column is basic and is never touched,
         * which is what keeps the children's recorded bounds true. */
        if (nodes == 1 && rcfix && inc.have && branch >= 0 &&
            inc.key >= key) {
            if (rcd == nullptr) {
                rcd = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *rcd);
                if (rcd == nullptr)
                    goto done;
            }
            if (jaos_solution(lp, nullptr, nullptr, nullptr, rcd) != JAOS_OK)
                goto done;
            /* One pass over the columns, billed like any kernel (D16). */
            work += nc;
            const double room = inc.key - key;
            for (int64_t j = 0; j < nc; j++) {
                if (!m->col_integer[j])
                    continue;
                const double d = sigma * rcd[j];
                const jaos_basis_status bs = lp->sol_col_status[j];
                if (bs == JAOS_BASIS_AT_LOWER && d > 0.0 &&
                    ilo[j] > -INFINITY) {
                    const double t = floor(room / d + MIP_RCFIX_SLACK);
                    const double nh = ilo[j] + t;
                    if (nh < ihi[j]) {
                        ihi[j] = nh;
                        rcfixed++;
                    }
                } else if (bs == JAOS_BASIS_AT_UPPER && d < 0.0 &&
                           ihi[j] < INFINITY) {
                    const double t = floor(room / -d + MIP_RCFIX_SLACK);
                    const double nl = ihi[j] - t;
                    if (nl > ilo[j]) {
                        ilo[j] = nl;
                        rcfixed++;
                    }
                }
            }
            if (rcfixed > 0)
                jm_log(m, JAOS_LOG_SUMMARY,
                       "root: %lld column bounds fixed by their reduced costs",
                       (long long)rcfixed);
        }
        if (nodes % MIP_LOG_EVERY == 0) {
            const double ok = open_key(&heap, dstack, dstack_n);
            jm_log(m, JAOS_LOG_PROGRESS,
                   "node %lld: %lld open, bound %.17g, incumbent %s",
                   (long long)nodes, (long long)(heap.n + dstack_n),
                   sigma * (ok < best_bound ? ok : best_bound),
                   inc.have ? "yes" : "none");
        }
        {
            const double bk = inc.have && inc.key < cut_key ? inc.key
                                                            : cut_key;
            if (bk < INFINITY && bk - key <= gap * (1.0 + fabs(bk)))
                continue;              /* cannot improve enough */
        }

        if (branch < 0) {
            if (!incumbent_take(&inc, lp, nr, key))
                goto done;
            if (first_inc == 0)
                first_inc = nodes;
            for (int64_t j = 0; j < nc; j++)
                if (m->col_integer[j])
                    inc.x[j] = round(inc.x[j]);
            spool_offer(&sp, inc.x, key, obj);
            jm_log(m, JAOS_LOG_PROGRESS, "node %lld: incumbent %.17g, integral",
                   (long long)nodes, obj);
            const double ok = open_key(&heap, dstack, dstack_n);
            if (!incumbent_announce(m, &inc, nodes,
                    sigma * (ok < key ? ok : key), false)) {
                outcome = JAOS_SOLVE_INTERRUPTED;
                break;
            }
            continue;
        }
        const int64_t nrl = lp->num_row;
        /* Strong branching on the unreliable candidates, then the choice
         * again with what they taught (D293). The row scratch follows the
         * copy, which the root's cuts may have widened. */
        if (reliability > 0 && (probe_depth < 0 || depth_here <= probe_depth)) {
            jaos_basis_status *grown = realloc(prs, (size_t)(nrl > 0 ? nrl : 1)
                                                        * sizeof *prs);
            if (grown == nullptr)
                goto done;
            prs = grown;
            /* The cap in units: the node's own solve times the multiple,
             * rounded up, and never below one unit. */
            int64_t cap = 0;
            if (probe_cap > 0.0) {
                const double c = ceil(probe_cap * (double)node_work);
                cap = c < 1.0 ? 1 : (int64_t)c;
            }
            const jaos_status ps = strong_probe(lp, m, x, key, sigma,
                                                reliability, cap, pc_sum, pc_n,
                                                &work, &solves, &probes,
                                                &capped, pcs, prs, cand);
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
        /* A local cut whose slack is basic here does not bind, and is not
         * carried under this node (D297): the children's list and row
         * statuses skip it, and what they carry is still a basis, since a
         * row and its basic slack leave together. `act[k]` is the cut in
         * row nfixed + k, in order, which its three writers keep true:
         * node_apply, the root's rounds when their cuts may leave (D306)
         * and the node's round above. */
        assert(lp->num_row == nfixed + act_n && act_n == in_copy.n);
        int64_t nr_child = nrl;
        const jaos_basis_status *child_rs = lp->sol_row_status;
        if (cut_drop && act_n > 0) {
            if (nrl > crs_cap) {
                jaos_basis_status *g = realloc(crs, (size_t)nrl * sizeof *crs);
                if (g == nullptr)
                    goto done;
                crs = g;
                crs_cap = nrl;
            }
            memcpy(crs, lp->sol_row_status, (size_t)nfixed * sizeof *crs);
            int64_t kept = 0;
            for (int64_t k = 0; k < act_n; k++) {
                const jaos_basis_status s = lp->sol_row_status[nfixed + k];
                if (s == JAOS_BASIS_BASIC)
                    continue;
                act[kept] = act[k];
                crs[nfixed + kept] = s;
                kept++;
            }
            act_n = kept;
            nr_child = nfixed + kept;
            child_rs = crs;
        }
        const double v = x[branch];
        /* The children get no round when this node's did not pay, or when
         * this node had none for the same reason (D305). */
        const bool child_no_cuts = stalled || (nodes > 1 && cur->no_cuts);
        bnode *down = node_child(nodes > 0 ? cur : nullptr, nc, nr_child,
                                 lp->sol_col_status, child_rs, branch,
                                 lp->col_lower[branch], floor(v), key,
                                 next_id++, v - floor(v), false, act, act_n,
                                 child_no_cuts);
        bnode *up = node_child(nodes > 0 ? cur : nullptr, nc, nr_child,
                               lp->sol_col_status, child_rs, branch, ceil(v),
                               lp->col_upper[branch], key, next_id++,
                               ceil(v) - v, true, act, act_n, child_no_cuts);
        if (down == nullptr || up == nullptr) {
            node_free(down);
            node_free(up);
            goto done;
        }
        /* The dive's first child, by the rule (D295): the nearer side of
         * the fraction, a half going up; a fixed side; or the direction
         * whose expected loss is smaller, the nearer side on a tie. Off,
         * both children join the open set. */
        /* The dive goes on from here only while this node's own bound has
         * not fallen away from its parent's by more than a fraction of
         * (1 + |parent|) (D316); the root has no parent and always dives.
         * This is the one quantity D289's reopen condition named that
         * neither the resume count nor the resume gap reads: it judges the
         * branch that was just made, not the open set. */
        const bool dive_here = dive &&
            (degrade <= 0.0 || cur == nullptr ||
             branch_key - cur->key <= degrade * (1.0 + fabs(cur->key)));
        const double f = v - floor(v);
        bnode *first = f < 0.5 ? down : up;
        if (dive_child == JAOS_DIVE_UP) {
            first = up;
        } else if (dive_child == JAOS_DIVE_DOWN) {
            first = down;
        } else if (dive_child == JAOS_DIVE_PSEUDOCOST) {
            const double gd = f * pseudocost(branch, 0, nc, pc_sum, pc_n);
            const double gu = (1.0 - f) * pseudocost(branch, 1, nc, pc_sum, pc_n);
            if (gd < gu)
                first = down;
            else if (gu < gd)
                first = up;
        }
        bnode *other = first == down ? up : down;
        if (dive_here) {
            /* The sibling waits on the dive's stack when the dive may come
             * back for it (D308, D311), in the open set otherwise. */
            if (stack_dive) {
                if (!JM_GROW(dstack, dstack_cap, dstack_n + 1)) {
                    node_free(down);
                    node_free(up);
                    goto done;
                }
                dstack[dstack_n++] = other;
            } else if (!heap_push(&heap, other)) {
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
    /* Only the incumbent's basis crosses back (below), and only when the
     * tree proved it. Every other outcome leaves the tree's own node
     * bases behind on the private copy, and none of them is a basis of
     * the model the caller holds (D330). Already false from entry;
     * repeated because the publish block is where it turns on. */
    m->sol_basis_ok = false;
    m->solve_work = work;
    m->solve_iters = iters;
    m->solve_time = now_seconds() - t0;
    m->mip_nodes = nodes;
    m->mip_solves = solves;
    m->mip_cuts = cuts;
    m->mip_heur = heur_points;
    m->mip_first_inc = first_inc;
    m->mip_rcfix_n = rcfixed;
    m->mip_prop_n = tightened;
    {
        const double ok = open_key(&heap, dstack, dstack_n);
        m->mip_bound = sigma * (ok < best_bound ? ok : best_bound);
    }
    if (outcome == JAOS_SOLVE_OPTIMAL)
        m->mip_bound = inc.obj;
    jm_log(m, JAOS_LOG_SUMMARY,
           "branch and bound: %s after %lld nodes, %lld solves, %lld cuts "
           "(%lld below the root), %lld points by rounding, %lld of them "
           "by the dive heuristic, %lld by RINS and %lld by the pump, "
           "%lld probes, "
           "%lld of them capped",
           jaos_solve_status_str(outcome), (long long)nodes,
           (long long)solves, (long long)cuts, (long long)local_cuts,
           (long long)heur_points, (long long)dive_points,
           (long long)rins_points, (long long)pump_points,
           (long long)probes, (long long)capped);
    /* The pool goes to the model whole, proved or not, like the incumbent. */
    m->mip_pool_n = sp.n;
    m->mip_pool_x = sp.x;
    m->mip_pool_obj = sp.obj;
    sp.x = sp.obj = nullptr;
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
        /* The truncation, and the solve that repairs it (D334). The
         * count is asked first because five of the 24 MIPLIB instances
         * do not need the repair, and a re-solve that changes nothing
         * is a re-solve nobody should pay for. */
        m->sol_basis_ok = jm_model_basis_count_ok(m);
        if (!m->sol_basis_ok) {
            int64_t extra = 0;
            if (republish_at_the_incumbent(m, m->mip_inc_x, nc, nr, &extra))
                m->sol_basis_ok = jm_model_basis_count_ok(m);
            /* Billed, because it runs on the caller's own solve and is
             * not an analysis call the caller asked for separately. */
            m->solve_work += extra;
        }
        jm_model_publish_objective(m);
        assert(!m->sol_basis_ok || jm_model_basis_count_ok(m));
    }

done:
    if (rc != JAOS_OK && m->solve_status != outcome)
        jm_set_err(m, "%s", m->err[0] ? m->err : "out of memory in branch and bound");
    free(x);
    free(xr);
    free(x2);
    free(ra);
    free(ilo);
    free(ihi);
    free(plo);
    free(phi);
    free(rcd);
    free(pc_sum);
    free(pc_n);
    free(cand);
    free(pcs);
    free(prs);
    free(row);
    free(cut);
    free(act);
    free(items);
    free(mu);
    free(mbest);
    free(mdelta);
    free(prnd);
    free(pprev);
    free(pprev2);
    free(magg);
    free(mcmag);
    free(mused);
    free(mpicked);
    free(crs);
    free(in_copy.v);
    spool_free(&sp);
    cutbuf_free(&cb);
    cutbuf_free(&pool);
    while (heap.n > 0)
        node_free(heap_pop(&heap));
    free(heap.v);
    while (dstack_n > 0)
        node_free(dstack[--dstack_n]);
    free(dstack);
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
    out->fixed_cols = m->mip_rcfix_n;
    out->tightened = m->mip_prop_n;
    return JAOS_OK;
}

jaos_status jaos_mip_pool_count(const jaos_model *m, int64_t *count)
{
    if (m == nullptr || count == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *count = m->mip_pool_n;
    return JAOS_OK;
}

jaos_status jaos_mip_pool_solution(const jaos_model *m, int64_t k,
                                   double *col_value, double *objective)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (k < 0 || k >= m->mip_pool_n || m->mip_pool_x == nullptr) {
        jm_set_err((jaos_model *)m, "the solution pool holds %lld point(s); "
                   "there is no point %lld", (long long)m->mip_pool_n,
                   (long long)k);
        return JAOS_ERR_INVALID_INPUT;
    }
    if (col_value != nullptr && m->num_col > 0)
        memcpy(col_value, m->mip_pool_x + k * m->num_col,
               (size_t)m->num_col * sizeof *col_value);
    if (objective != nullptr)
        *objective = m->mip_pool_obj[k];
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
