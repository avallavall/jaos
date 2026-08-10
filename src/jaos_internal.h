/* JAOS internals. Everything here can change at any time.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef JAOS_INTERNAL_H
#define JAOS_INTERNAL_H

#include "jaos.h"

#include <stddef.h>

/* What a caller sets, as opposed to what a caller loads. The line between the
 * two is the one D64 drew: this configures the contract — how much precision
 * the data deserves, how long the caller will wait, where output goes, who
 * gets asked whether to carry on — and never the method.
 *
 * Nothing here is problem data, so nothing here is discarded by a load. */
typedef struct {
    int64_t work_limit;      /* <= 0 means unlimited */
    double time_limit;       /* <= 0 means unlimited */

    /* The two tolerances a caller owns, in scaled space where the solver
     * works. Zero means "the built-in default", so a model that never sets
     * them behaves exactly as it did before they existed — which is what
     * makes every digest in the reference sets a test of that claim.
     *
     * These two and no others. They say how much precision the caller's data
     * deserves, which is a question about the problem; everything else in
     * docs/tolerances.md is about the method and stays measured and fixed. */
    double primal_tol;       /* <= 0 means PRIMAL_TOL */
    double dual_tol;         /* <= 0 means DUAL_TOL   */

    /* Where output goes. No callback means no output, whatever the level:
     * a library that writes somewhere the caller did not choose cannot be
     * embedded. */
    jaos_log_fn log_cb;
    void *log_user;
    jaos_log_level log_level;

    /* Who is asked, every PROGRESS_EVERY iterations, whether to carry on.
     * No callback means nobody is asked and the solve runs to its own end. */
    jaos_progress_fn progress_cb;
    void *progress_user;
} jm_config;

struct jaos_model {
    int64_t num_col;
    int64_t num_row;
    int64_t num_nz;

    jaos_obj_sense sense;
    double obj_offset;

    /* Owned arrays, sized by num_col / num_row. */
    double *col_cost;
    double *col_lower, *col_upper;
    double *row_lower, *row_upper;

    /* Constraint matrix, compressed sparse column; entries within a column
     * sorted by row index, no duplicates, no explicit zeros. This is the
     * authoritative copy. */
    int64_t *a_start;   /* [num_col + 1] */
    int64_t *a_index;   /* [num_nz]      */
    double  *a_value;   /* [num_nz]      */

    /* Row-wise mirror (CSR), derived from the CSC copy on demand; the dual
     * simplex prices rows, the checker does not need it. Invalidated by any
     * load. */
    bool rowwise_valid;
    int64_t *ar_start;  /* [num_row + 1] */
    int64_t *ar_index;  /* [num_nz]      */
    double  *ar_value;  /* [num_nz]      */

    /* Scaling factors: row i and column j of A are conceptually multiplied
     * by row_scale[i] and col_scale[j]. The stored matrix is never touched
     * — it stays the authority the checker judges against (PLAN.md 2.5).
     * Every factor is an exact power of two, so applying one is exact in
     * IEEE arithmetic and introduces no rounding error of its own.
     * Invalidated by any load. */
    bool scale_valid;
    double *row_scale;  /* [num_row] */
    double *col_scale;  /* [num_col] */

    /* Set when the exponent range a factor needed exceeded what JAOS is
     * willing to express, so the scaling actually applied is weaker than
     * the one computed. Silence here would let a caller believe the
     * exponent range was fixed when it was not. This is a caveat on a
     * success, so it travels here rather than in err. */
    bool scale_clamped;

    /* Everything the caller configures, in one object, and the reason it is
     * one object is a defect it has already caused twice.
     *
     * Loading a new problem into an existing model must replace the problem
     * and keep the configuration — nobody who writes `set the tolerance, load
     * the file, solve` expects the tolerance to be gone. That used to be a
     * list of fields saved across the wipe and restored after it, and a list
     * is a thing you can forget to add to: the primal tolerance was found
     * missing from it once, and the logging callback was missing from it from
     * D65 until it was noticed here, so a caller who installed a log callback
     * before loading got silence and no way to tell why.
     *
     * As a sub-struct there is nothing to remember. `model_release_arrays`
     * saves this and puts it back, and a setting added to it is preserved
     * because it is inside the thing that gets preserved. */
    jm_config cfg;

    jaos_solve_status solve_status;
    double objective;
    double *sol_col;         /* [num_col] primal values      */
    double *sol_row;         /* [num_row] row activities     */
    double *sol_dual;        /* [num_row] row duals          */
    double *sol_redcost;     /* [num_col] reduced costs      */
    jaos_basis_status *sol_col_status;  /* [num_col] where each column rests */
    jaos_basis_status *sol_row_status;  /* [num_row] and each row activity   */
    int64_t solve_work;
    int64_t solve_iters;

    /* The basis the next solve starts from, or null for the slack basis.
     *
     * Held apart from sol_*_status above, and that separation is the whole of
     * warm re-solve: those two are an *answer*, and an answer is discarded the
     * moment the problem moves. This is a *starting point*, and surviving
     * exactly that move is what it is for — a caller who shifts one bound and
     * re-solves is asking the dual simplex to do the thing it is best at.
     *
     * Written two ways and they mean the same thing: jaos_set_basis, and every
     * solve that reaches an optimum. Dropped only by a load, which changes
     * what the indices refer to. Both arrays or neither. */
    jaos_basis_status *start_col_status;  /* [num_col] */
    jaos_basis_status *start_row_status;  /* [num_row] */

    /* Detail message for the last failed operation; "" when it succeeded.
     * Sits outside the problem data on purpose: setting it never disturbs a
     * loaded model. */
    char err[256];
};

/* --------------------------------------------------------------------- */
/* Dual simplex                                                          */
/* --------------------------------------------------------------------- */

/* Where a nonbasic variable sits. A basic variable's value comes from the
 * factorization; a nonbasic one is pinned to a bound, which is what makes
 * the basis determine the point. */
typedef enum {
    JM_BASIC = 0,
    JM_AT_LOWER,
    JM_AT_UPPER,
    JM_FREE,        /* nonbasic at zero, both bounds infinite */
} jm_var_status;

/* Runs the dual simplex and writes the outcome back into the model
 * (solve_status, objective, sol_*, counters).
 *
 * It works on a scaled copy of the model, computing the scaling first if
 * the model does not already carry one; the model itself is never
 * modified, and everything written back is in the model's own units. */
JAOS_NODISCARD jaos_status jm_dual_simplex(jaos_model *m);

/* Forrest-Goldfarb dual steepest-edge weight update [8].
 *
 * w[i] tracks ||row i of B^-1||^2, the length of the direction the dual
 * method would move along if it chose row i. Dividing a bound violation by
 * it turns a raw distance-to-feasibility into a distance measured in the
 * units the step actually takes, which is what makes the row choice
 * insensitive to how the model happens to be written.
 *
 * Column q enters the basis at row r. `alpha` is B^-1 M_q and `tau` is
 * B^-1 rho_r with rho_r = B^-T e_r, both dense of length n, both taken
 * against the basis *before* the change. alpha[r] is the pivot.
 *
 * `exact_r` is the true weight of row r, ||rho_r||^2. Nothing else is ever
 * known exactly — recomputing all n weights costs n solves — but this one
 * is free, because rho_r had to be built to price the row. It is used two
 * ways: it replaces the carried estimate before the recurrence runs, so
 * every weight the step produces is derived from a true value rather than
 * from an estimate of one; and the disagreement between the two says how
 * far the recurrence has drifted.
 *
 * `drift_factor` is how far apart the carried and exact weights may be
 * before the estimates are declared worthless. Past it the weights are all
 * reset to one and the recurrence is skipped: a neutral prior beats
 * propagating numbers that have been shown to be wrong, and the exact
 * value injected each iteration rebuilds the estimates from there. The
 * test is symmetric, because a weight that has shrunk makes its row look
 * urgent and is the more dangerous direction.
 *
 * Reachable from outside the simplex for one reason: a wrong weight costs
 * iterations and never a wrong answer, so no solve-level test can catch
 * it and both the formula and the restart have to be checked against norms
 * recomputed from scratch.
 *
 * Returns true when it discarded the whole set rather than carrying it
 * forward. That is not a detail: it is the difference between pricing by
 * steepest edge and pricing by largest infeasibility, it happens on 80-93%
 * of the iterations of four reference instances, and until D63 measured it
 * nothing in the solver could say so. A caller watching a solve is told how
 * many times it happened. */
bool jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau,
                   double exact_r, double drift_factor,
                   const int64_t *pat, int64_t npat);

/* Harris' two-pass ratio test [7], over n candidate breakpoints.
 *
 * num[k] is how far candidate k's reduced cost still is from crossing into
 * infeasibility, never negative; den[k] is the magnitude of its pivot
 * element, strictly positive. Their quotient is the step at which that
 * candidate blocks.
 *
 * Pass one widens every numerator by dual_tol and takes the smallest
 * quotient: the largest step that leaves no candidate more than the
 * tolerance beyond feasible. Pass two returns the candidate with the
 * largest pivot whose true, unwidened quotient still fits in that step.
 * The trade is a bounded amount of dual infeasibility for a pivot that can
 * be orders of magnitude better conditioned, and on a degenerate vertex —
 * where many quotients are zero and the choice is otherwise arbitrary —
 * that is the difference between progress and a stall.
 *
 * Returns an index into num/den, or -1 if n is not positive. The set it
 * chooses from is never empty for n > 0: whichever candidate sets the
 * window in pass one is inside its own window by construction.
 *
 * Separate from the simplex, and reachable, because it is a decision made
 * entirely out of numbers: which candidates are eligible at all is solver
 * state and sign conventions, but which eligible one wins is this, and a
 * wrong answer here costs conditioning rather than correctness — nothing
 * at the solve level would report it. */
int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       double dual_tol);

/* Bland's rule over the same candidate set, for when Harris' has cycled.
 *
 * Same num/den, and `var` carries each candidate's variable index. The rule
 * is the smallest index among the candidates attaining the exact minimum
 * quotient — no widening, so no candidate is left past feasible and the
 * choice at a degenerate vertex stops being arbitrary. That is what makes it
 * terminate, and it is also why it cannot be the default: without the
 * window there is no room to prefer a better-conditioned pivot, and the
 * iteration counts say what that costs (DECISIONS.md D26).
 *
 * The minimum is compared exactly rather than within a tolerance. A
 * tolerance would hand back the freedom the rule exists to remove, and the
 * quotients are a deterministic function of the same inputs, so exact
 * equality is a meaningful test here rather than a hopeful one.
 *
 * Returns an index into the arrays, or -1 if n is not positive. */
int64_t jm_bland_pick(int64_t n, const int64_t *var, const double *num,
                      const double *den);

/* Turns a list of positions into the sorted, distinct list of them.
 *
 * `pos[0..n)` is what a scatter recorded on its way past: unordered, because
 * the scatter visits rows and a row's columns are anywhere, and repeating,
 * because a slot that cancels back to zero and is written again is recorded
 * twice. The result is written over the input, ascending, each position
 * once, and the count of them returned. `words` reports how many bitmap
 * words the enumeration had to look at, which is the part of the cost that
 * does not scale with n and which the caller bills.
 *
 * `mark` is a bitmap of at least (limit + 63) / 64 words, where `limit`
 * bounds the positions; anything outside [0, limit) has nowhere to be
 * recorded and is dropped. It must be all zero on entry and is all zero again
 * on return — the routine clears exactly the words it set, so the caller
 * allocates it once and never has to.
 *
 * A bitmap rather than a sort, and that is the whole reason this is worth
 * having: a sort of k positions costs k log k comparisons and the pattern
 * this exists for is read once per iteration, while a bitmap pass costs k
 * to mark, one scan of the touched word range to read back, and hands over
 * the duplicate removal for nothing. Ascending order is not a preference:
 * every consumer of a pricing row breaks its ties by scan position, so a
 * pattern in any other order silently moves the trajectory.
 *
 * Reachable from outside the simplex because a defect here is invisible
 * from the solve: a dropped position leaves a variable out of a ratio test
 * that would have been correct without it, and the answer is merely
 * different rather than wrong. */
int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words);

/* Overflow-checked array allocation: n elements of elsize bytes.
 * Returns NULL on n < 0, size overflow, or exhaustion. n == 0 still returns
 * a valid non-NULL allocation, so success is always non-NULL. */
void *jm_alloc_array(int64_t n, size_t elsize);
void *jm_calloc_array(int64_t n, size_t elsize);

/* Grows *arr (elements of elsize) to hold at least need elements; on
 * failure *arr is untouched, so cleanup still frees the old block.
 *
 * The macro answers the common case here rather than in `jm_grow`, and that
 * is not a micro-optimisation: `jm_grow` lives in another translation unit,
 * so in the build JAOS ships — `-O2`, no link-time optimisation — every
 * append pays a call to discover it had capacity all along. On `fit2p` that
 * discovery was 24% of every instruction the solver executed, and not one
 * work unit charges for it (D55).
 *
 * `need` and `cap` are evaluated twice. Every call site passes plain field
 * arithmetic, checked, and this header is not a public one. */
bool jm_grow(void **arr, int64_t *cap, int64_t need, size_t elsize);
#define JM_GROW(a, cap, need) \
    ((need) <= (cap) ? true : jm_grow((void **)&(a), &(cap), (need), sizeof *(a)))

/* Name -> value map for the readers: FNV-1a, open addressing, names kept in
 * one arena. Absence and value are separate — values may be negative. */
typedef struct {
    char *pool;              /* all names, NUL-separated */
    int64_t pool_len, pool_cap;
    int64_t *off, *val;      /* entry e: name at pool+off[e], value val[e] */
    int64_t n, cap;
    int64_t *slot;           /* entry indices, -1 empty; power-of-two size */
    int64_t nslot;
} jm_nmap;

void jm_nmap_free(jm_nmap *m);
bool jm_nmap_get(const jm_nmap *m, const char *name, int64_t *val);
/* The caller must have checked the name is absent. */
bool jm_nmap_insert(jm_nmap *m, const char *name, int64_t value);

/* Builds the CSR mirror if it is not current. */
JAOS_NODISCARD jaos_status jm_model_ensure_rowwise(jaos_model *m);

/* Keeps the basis just published as the one the next solve starts from.
 * A model with no published basis is left alone and reports success: there is
 * nothing to remember, which is not a failure. */
JAOS_NODISCARD jaos_status jm_model_remember_basis(jaos_model *m);

/* Formats into m->err. NULL model is tolerated (message dropped). */
[[gnu::format(printf, 2, 3)]]
void jm_set_err(jaos_model *m, const char *fmt, ...);

/* Is anyone listening at this level? Every logging site tests this first, so
 * that a solve nobody is watching pays one comparison per site and not one
 * formatted string. Inline and in the header for that reason. */
static inline bool jm_logging_at(const jaos_model *m, jaos_log_level level)
{
    return m != nullptr && m->cfg.log_cb != nullptr && m->cfg.log_level >= level;
}

/* Formats one line and hands it to the caller's callback. Does nothing when
 * jm_logging_at is false, so a site may call it unguarded where the arguments
 * are already at hand; guard it where computing them would cost anything. */
[[gnu::format(printf, 3, 4)]]
void jm_log(const jaos_model *m, jaos_log_level level, const char *fmt, ...);

typedef enum {
    JM_SCALE_NONE = 0,      /* all factors 1 */
    JM_SCALE_CURTIS_REID,   /* default */
    JM_SCALE_GEOMETRIC,     /* geometric-mean equilibration */
} jm_scale_mode;

/* Computes m->row_scale and m->col_scale. Deterministic: fixed iteration
 * counts and fixed summation order, no clock, no randomness (D8). */
JAOS_NODISCARD jaos_status jm_model_scale(jaos_model *m, jm_scale_mode mode);

/* |a_k| after scaling, for the entry at index k of column j. */
double jm_scaled_abs(const jaos_model *m, int64_t j, int64_t k);

/* --------------------------------------------------------------------- */
/* Deterministic work counter (D16)                                      */
/* --------------------------------------------------------------------- */

/* The reproducible budget's currency. Counted in the kernels, never
 * derived from a clock, so a run consumes the same units on every machine.
 * Weights are drafts until calibrated; the definition is public contract
 * from 1.0 on. */
typedef struct { int64_t units; } jm_work;

constexpr int64_t JM_WORK_NONZERO    = 1;     /* nonzero touched in a solve */
constexpr int64_t JM_WORK_ELIMINATED = 2;     /* nonzero eliminated, factor */
constexpr int64_t JM_WORK_FACTOR     = 4096;  /* fixed cost, refactorization */
constexpr int64_t JM_WORK_UPDATE     = 64;    /* fixed cost, basis update    */

static inline void jm_work_add(jm_work *w, int64_t n)
{
    if (w != nullptr)
        w->units += n;
}

/* --------------------------------------------------------------------- */
/* Sparse LU factorization of a basis                                    */
/* --------------------------------------------------------------------- */

/* A growable sparse vector: parallel index and value arrays. */
typedef struct {
    int64_t *idx;
    double  *val;
    int64_t n, cap;
} jm_svec;

void jm_svec_free(jm_svec *v);
bool jm_svec_push(jm_svec *v, int64_t i, double x);
void jm_svec_erase(jm_svec *v, int64_t i);   /* removes index i if present */

/* Factorization of a square matrix B as
 *
 *     B = P' L E^-1 U Q'
 *
 * where P and Q are permutations, L is unit lower triangular, U is upper
 * triangular, and E is the product of the row transformations accumulated
 * by Forrest-Tomlin updates (empty right after factoring).
 *
 * Everything is indexed by *slot*: slot s is the pivot taken at step s of
 * the factorization, and it keeps its original row and its basis column
 * for life. What an update changes is a slot's *position* in the
 * triangular order, tracked by slot_at and pos_of. That indirection is
 * what makes the cyclic permutation of an update cost O(dim) instead of
 * touching every nonzero of U.
 *
 * U is held in both orientations at once because an update needs both: by
 * column to install the spike, by row to eliminate it. */
typedef struct {
    int64_t dim;
    int64_t rank;        /* pivots found; rank < dim means singular */

    /* L: one elimination eta per slot, indices in slot space. Fixed once
     * factored — updates never touch L. */
    int64_t *l_start;    /* [dim + 1] */
    int64_t *l_index;
    double  *l_value;

    /* U, by row and by column, indexed by slot. Diagonals live apart. */
    jm_svec *urow;       /* [dim] */
    jm_svec *ucol;       /* [dim] */
    double  *u_diag;     /* [dim] */

    /* Forrest-Tomlin row transformations, in creation order. Each one is
     * "y[target] -= factor * y[source]" during FTRAN: ft holds
     * (target, factor), ft_source the matching sources. */
    jm_svec ft;
    int64_t *ft_source;
    int64_t ft_source_cap;
    int64_t n_updates;   /* updates applied since the last factorization */

    int64_t *slot_at;    /* slot_at[k] = slot currently at position k */
    int64_t *pos_of;     /* pos_of[s] = current position of slot s     */

    int64_t *perm_row;   /* slot s owns original row perm_row[s]       */
    int64_t *perm_col;   /* slot s owns basis column perm_col[s]       */
    int64_t *inv_col;    /* basis column j belongs to slot inv_col[j]  */

    /* What counts as a structural zero, anchored to the basis matrix at
     * factorization time. Updates reuse it rather than re-deriving a
     * threshold from whichever column happens to be entering: otherwise
     * "structurally absent" would drift with each spike's own norm
     * instead of meaning one thing for one factorization. */
    double drop;

    double *tmp;         /* [dim] solve workspace, owned */
    double *spike;       /* [dim] update workspace, owned */

    /* Reachability workspace for BTRAN's U' pass. The slots whose value
     * comes out zero are exactly those not reachable from the right-hand
     * side's support in U's dependency graph, and on real bases that is
     * almost all of them — so they are found by search rather than by
     * computing zeros [9]. `mark` is stamped rather than cleared, because
     * clearing it would cost the O(dim) the search exists to avoid. */
    int64_t *mark;       /* [dim] visit stamp                            */
    int64_t stamp;       /* current stamp; mark[s] == stamp means visited */
    int64_t *dfs_node;   /* [dim] explicit DFS stack: node at each level  */
    int64_t *dfs_next;   /* [dim] and how far into its row it has got     */
    int64_t *pattern;    /* [dim] reachable slots, in topological order   */
} jm_lu;

void jm_lu_init(jm_lu *lu);
void jm_lu_free(jm_lu *lu);

/* Factors a dim x dim matrix given in compressed sparse column form.
 * Markowitz ordering under a threshold stability test: a candidate pivot
 * must be at least pivot_tol times the largest magnitude in its column,
 * and among those the one whose elimination creates the least expected
 * fill-in wins.
 *
 * A singular matrix is not an error — it is a fact the caller acts on, by
 * replacing basis columns. JAOS_OK is returned with rank < dim, and the
 * pivoted rows and columns are the first `rank` entries of perm_row and
 * perm_col. */
JAOS_NODISCARD jaos_status jm_lu_factor(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w);

/* Solves B x = b (FTRAN) and B' x = b (BTRAN), in place on a dense vector
 * of length dim.
 *
 * The factorization is taken non-const because both solves scribble on its
 * internal workspace. That also means they are not reentrant and two of
 * them may not overlap on the same jm_lu — when M5 brings parallelism the
 * workspace becomes a caller-supplied argument, but promising const now
 * would be a lie a caller could act on.
 *
 * A factorization that is not full rank leaves x untouched: solving with
 * one is meaningless, and a wrecked factorization is exactly what
 * jm_lu_update leaves behind when it fails. */
void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w);
void jm_lu_btran(jm_lu *lu, double *x, jm_work *w);

/* The forward solve, additionally reporting where the answer is nonzero.
 *
 * `pat` takes at least `dim` entries and comes back holding `*npat` indices
 * in whatever order the column permutation produced them. A null `pat` is
 * exactly jm_lu_ftran. Unordered is enough here and ordered is not worth
 * buying: everything that reads an FTRAN result is elementwise. */
void jm_lu_ftran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat);

/* The same solve, additionally reporting where the answer is nonzero.
 *
 * `pat` takes at least `dim` entries and comes back holding `*npat` row
 * indices, in whatever order the row permutation produced them — not
 * ascending, and a caller that needs ascending has jm_pattern_order for it.
 * A null `pat` is exactly jm_lu_btran.
 *
 * It exists because the alternative is worse. The solve's last pass already
 * visits every slot to permute it back, so it can say which ones carry a
 * value for the price of a comparison; a caller left to find out for itself
 * scans the whole vector again. On the Kennington set that second scan was
 * 27% of everything the solver billed. */
void jm_lu_btran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat);

/* Forrest-Tomlin update: basis column `col_out` is replaced by `new_col`,
 * a dense vector indexed by original row. Refactorizing from scratch after
 * every simplex iteration would cost more than the iteration; this repairs
 * the factorization instead [5].
 *
 * The elimination work is proportional to the change, but three passes are
 * unavoidably O(dim) — clearing the dense row buffer, measuring the spike,
 * and shifting positions — so the cost is that plus a floor. The work
 * counter charges JM_WORK_UPDATE for the floor, the way jm_lu_factor
 * charges JM_WORK_FACTOR, because a budget that ignored it would promise
 * a run far cheaper than it is (D16).
 *
 * Returns JAOS_ERR_NUMERICAL when the replacement leaves a pivot too small
 * to trust — the new basis is singular or nearly so. By then the update
 * has already rewritten U, so the factorization is left marked unusable
 * (rank < 0) and the caller must refactorize. Preserving the old
 * factorization instead would mean simulating the whole elimination before
 * committing to it, which costs more than the refactorization it saves.
 *
 * `min_pivot_ratio` is the floor on the new diagonal relative to the
 * spike's largest magnitude. It is not Markowitz's threshold and should be
 * far looser — after elimination a legitimate pivot can be orders of
 * magnitude below the spike — so something like 1e-9 rather than 0.1.
 *
 * Updates accumulate both error and fill; the caller refactorizes on an
 * interval, watching lu->n_updates. */
JAOS_NODISCARD jaos_status jm_lu_update(jm_lu *lu, int64_t col_out,
    const double *new_col, double min_pivot_ratio, jm_work *w);

#endif /* JAOS_INTERNAL_H */
