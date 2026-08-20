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
    /* Seconds the last solve took. The one number on this struct that is not
     * reproducible, which is why nothing inside the solver may read it back:
     * it is written at the end of a solve and only ever handed to a caller. */
    double solve_time;

    /* What the last solve's presolve pass actually gave the simplex: the
     * reduced dimensions, or this model's own when nothing fired — which is
     * always, under a JAOS_NO_PRESOLVE build. Reporting only, exactly like
     * solve_work/solve_iters above: written at the end of every solve and
     * read back by nothing inside the solver. Not part of the public API
     * (D-13, D64) — bench/run.c reads these directly because it is in-tree
     * tooling, not a consumer, the same reason tests/ may (Makefile). */
    int64_t presolve_num_row, presolve_num_col, presolve_num_nz;

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

/* The nonbasic set as a bitmap, and the four operations that keep it equal
 * to `{v : status[v] != JM_BASIC}`.
 *
 * `mark` is a bitmap of (nvar + 63) / 64 words with bit v set exactly when
 * variable v is not basic. `jm_nonbasic_build` writes every one of those
 * words from `status[0..nvar)` and returns how many bits it set;
 * `jm_nonbasic_insert` and `jm_nonbasic_remove` set and clear bit v, which
 * is what one variable leaving and one entering the basis each cost;
 * `jm_nonbasic_expand` writes the set positions ascending into `out` and
 * returns how many there were. A non-positive `nvar` has no words, so both
 * of the two that take one build nothing and return zero.
 *
 * Membership, and never "has a finite bound". A nonbasic free variable is
 * in the set exactly as a bounded one is — it reaches the ratio test with a
 * zero numerator and may move either way — so a rule keyed on bounds rather
 * than on JM_BASIC would drop every one of them and the drop would look
 * like a different answer rather than a wrong one.
 *
 * Unlike jm_pattern_order's bitmap this one is persistent: it holds its
 * contents between iterations, no reader clears it, and jm_nonbasic_build
 * is the only thing that ever writes it wholesale. The caller allocates it
 * once and the sites that move a variable into or out of the basis maintain
 * it by hand.
 *
 * Reachable from outside the simplex for the reason jm_pattern_order is: a
 * bitmap that has drifted from `status` is invisible from the solve. A
 * variable missing from it is left out of a ratio test that would have been
 * correct with it, so the solve runs on to an answer that is merely
 * different, and nothing short of a solution digest reports it.
 * jm_nonbasic_expand exists to be the testable mirror of the walk in the
 * ratio test; the ratio test walks the words itself rather than calling it,
 * because materialising an index array is the traffic the bitmap removes. */
int64_t jm_nonbasic_build(int64_t nvar, const jm_var_status *status,
                          uint64_t *mark);
void jm_nonbasic_insert(uint64_t *mark, int64_t v);
void jm_nonbasic_remove(uint64_t *mark, int64_t v);
int64_t jm_nonbasic_expand(int64_t nvar, const uint64_t *mark, int64_t *out);

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

/* Allocates m's six sol_* arrays if any is missing, all six or none (a
 * partial set left behind by an earlier failed allocation must not read as
 * "already there"). Shared by publish() and jm_postsolve_expand, since a
 * presolve-reduced solve has to ensure them on the caller's own model
 * exactly as an unreduced one ensures them on itself. */
JAOS_NODISCARD jaos_status jm_model_ensure_solution_arrays(jaos_model *m);

/* Keeps the basis just published as the one the next solve starts from.
 * A model with no published basis is left alone and reports success: there is
 * nothing to remember, which is not a failure. */
JAOS_NODISCARD jaos_status jm_model_remember_basis(jaos_model *m);

/* Sets m->objective from m's own published solution, with a compensated sum
 * (D169). Every publication path ends here, so the number the caller reads is
 * the objective of the point the caller reads and not of some earlier one. */
void jm_model_publish_objective(jaos_model *m);

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
/* Presolve and postsolve (D-01)                                         */
/* --------------------------------------------------------------------- */

/* What each reduction removed. Declared in full now, though this plan makes
 * only fixed_col and rounds move — every other field stays zero until the
 * plan that makes its family fire. That is deliberate: the record format
 * settles once here rather than changing under the campaign baselines nine
 * times (D-13). Logged at JAOS_LOG_SUMMARY and printed by bench/run.c;
 * tests are white-box and read the fields directly. */
typedef struct {
    int64_t fixed_col;
    int64_t empty_row;
    int64_t empty_col;
    int64_t singleton_row;
    int64_t singleton_col;          /* bounded singleton column only */
    int64_t free_col_singleton;     /* the free-column-singleton, counted
                                      * apart from singleton_col above since
                                      * it is a bigger structural win (one
                                      * row and one column, not one column) —
                                      * folding the two together would make
                                      * the D-13 counts unreadable */
    int64_t forcing_row;
    int64_t redundant_row;
    int64_t implied_free_col;       /* the implied free column singleton: a
                                      * column whose one entry's row already
                                      * confines it strictly inside its own
                                      * box, substituted out exactly (D105) */
    int64_t tightened_bound;
    int64_t duplicate_row;
    int64_t duplicate_col;
    int64_t dominated_col;
    int64_t rounds;
} jm_presolve_counts;

/* One postsolve record kind per reduction family. 02-01 shipped the first;
 * this plan (02-03) adds five more — the four structural families plus the
 * free-column-singleton's own tag, since it restores a row and a column
 * from one record and is not a variant of any other tag (D-01, D-07). */
typedef enum {
    JM_PS_FIXED_COL,            /* a column presolve fixed as loaded */
    JM_PS_EMPTY_ROW,            /* a row with no live entry */
    JM_PS_EMPTY_COL,            /* a column with no live entry */
    JM_PS_SINGLETON_ROW,        /* a row with exactly one live entry */
    JM_PS_SINGLETON_COL,        /* a bounded column with one live, cost-0
                                  * entry; its row survives, relaxed */
    JM_PS_FREE_COL_SINGLETON,   /* a free, cost-0 column whose one live row
                                  * is itself a mutual singleton on it;
                                  * removes both in one record */
    JM_PS_REDUNDANT_ROW,        /* a row whose whole activity range lies
                                  * inside its own bounds: it can never bind */
    JM_PS_FORCING_ROW,          /* a row whose activity range touches one of
                                  * its own bounds exactly, fixing every live
                                  * column in it at the bound that attains
                                  * that extreme */
    JM_PS_IMPLIED_FREE_COL,     /* a column with one matrix entry, in an
                                  * equality row that already confines it
                                  * strictly inside its own box: substituted
                                  * out exactly, and the row goes with it */
} jm_presolve_tag;

/* One tagged, append-only postsolve record: what an original row or column
 * needs to be restored. `index` is always an ORIGINAL row or column index —
 * never a reduced one. Every other field is read differently by different
 * tags, documented per tag where jm_postsolve_expand and jm_postsolve_solved
 * read them:
 *
 *   JM_PS_FIXED_COL, JM_PS_EMPTY_COL: index=column, value=the fixed value,
 *     cost=the column's own cost. A JM_PS_FIXED_COL pushed by a forcing row
 *     also carries coef=its coefficient in that row, which the forcing row's
 *     own record reads back to size its multiplier; every other producer of
 *     this tag leaves coef at zero and no reader of those looks at it.
 *   JM_PS_EMPTY_ROW: index=row. Nothing else is read — an empty row's
 *     activity, dual and status are always zero/zero/basic.
 *   JM_PS_SINGLETON_ROW: index=row, index2=the column its one entry named,
 *     coef=that entry's coefficient. row_tightens_lo/hi record which side(s)
 *     of the column's bound the row's own implied bound actually won at the
 *     moment of intersection, and lo/hi are the bounds the column was left
 *     carrying after that intersection. Several rows can fold into one
 *     column, so which side won is not on its own enough to say which row
 *     owns the bound x_j finally rests on; the recorded pair is what
 *     separates them (see ps_replay_one).
 *   JM_PS_SINGLETON_COL: index=row (which survives, relaxed), index2=the
 *     column removed, coef=the entry's coefficient, lo/hi=the column's own
 *     bounds at the moment it was removed, row_lo/row_hi=the row's own
 *     current bounds at that same moment, BEFORE this record's relaxation.
 *     The replay runs mid-LIFO, when the pair (sol_row[index], its carry —
 *     ps_row_add's compensation, read as sum plus carry, never the sum
 *     alone) holds the columns
 *     that were live at push time except index2 itself, so the push-time
 *     pair is the only one that activity can be judged against; the
 *     original pair only becomes the right target once every
 *     earlier-pushed record has replayed (see ps_replay_one, which
 *     carries the containment argument).
 *   JM_PS_FREE_COL_SINGLETON: index=row (removed), index2=column (removed),
 *     coef=the entry's coefficient, lo/hi=the row's own *current* (already
 *     shifted by every value-determined column removed before it) bounds at
 *     the moment both were removed.
 *   JM_PS_REDUNDANT_ROW: index=row. Nothing else is read. Its dual is zero
 *     by construction — a row that can never bind carries no multiplier —
 *     and its activity is summed at replay from the columns that survived.
 *   JM_PS_FORCING_ROW: index=row (removed), index2=how many records
 *     IMMEDIATELY BEFORE this one in the arena are the columns this row
 *     fixed. row_tightens_hi says which of the row's own bounds the range
 *     attained: true for the upper bound (minimum activity reached it),
 *     false for the lower (maximum activity reached it). That is the sign
 *     the row's own multiplier has to take, and the preceding records are
 *     what its magnitude is computed from — see ps_replay_one.
 *   JM_PS_IMPLIED_FREE_COL: index=row (removed), index2=column (removed),
 *     coef=the entry's coefficient, value=the row's own *current* bound at
 *     the moment it fired (the row is an equality, so its two bounds are
 *     equal, and both are already net of every value-determined column
 *     removed from it before this), cost=the column's cost in the CURRENT
 *     objective at that same moment. lo/hi are the implied box the row put
 *     on the column; nothing reads them back and they are recorded so a
 *     failing instance can be read without re-deriving them.
 *
 * There is no tag for bound tightening. 02-04 built the family, measured it
 * and did not ship it — see the block that says so in src/presolve.c and
 * 02-04-SUMMARY.md. `jm_presolve_counts.tightened_bound` stays declared and
 * stays zero, the same way every other unfired family's field did between
 * 02-01 and the plan that lit it. */
typedef struct {
    jm_presolve_tag tag;
    int64_t index;
    int64_t index2;
    double value;
    double cost;
    double coef;
    double lo, hi;
    double row_lo, row_hi;
    bool row_tightens_lo, row_tightens_hi;
} jm_presolve_rec;

/* What presolve decided about the model as a whole. NONE and REDUCED are
 * the two a caller-visible solve can end up taking; INFEASIBLE, UNBOUNDED
 * and SOLVED all short-circuit the simplex entirely — declared complete
 * from 02-01's first commit so every switch over this enum never grows a
 * case later; INFEASIBLE and UNBOUNDED are produced starting 02-03 (empty
 * row / singleton row for the former, empty column for the latter — the
 * only family permitted to report it, D19). */
typedef enum {
    JM_PRESOLVE_NONE,
    JM_PRESOLVE_REDUCED,
    JM_PRESOLVE_INFEASIBLE,
    JM_PRESOLVE_UNBOUNDED,
    JM_PRESOLVE_SOLVED,
} jm_presolve_outcome;

/* The solve-local presolve workspace (D-08): built inside jm_dual_simplex,
 * consumed before publish returns, freed with sx.
 *
 * `reduced` is a jaos_model VALUE, not a pointer — sx_init already knows how
 * to build a working copy of a problem from a jaos_model, and this is that
 * same move one layer up, before scaling exists (D-04, D-06). Every array on
 * it is presolve-owned; none aliases the caller's model, which is what makes
 * jm_presolve_free safe without ever handing `reduced` to jaos_model_free.
 *
 * orig_col/orig_row map a reduced index to the original one it came from;
 * col_map/row_map are the inverse, original index to reduced index or -1
 * when a reduction removed it — row_map stopped being the identity in
 * 02-03, once empty rows, singleton rows and the free-column-singleton's
 * paired row all became reductions that remove one.
 *
 * `orig` is the caller's own model: the non-const write target
 * jm_postsolve_expand needs and jm_presolve_run — which only ever reads
 * through a const pointer, matching D-06 at the type level — cannot supply.
 * jm_dual_simplex sets it directly; nothing inside presolve.c needs to. */
typedef struct {
    jaos_model reduced;
    int64_t *orig_col, *orig_row;
    int64_t *col_map, *row_map;
    jm_presolve_rec *arena;
    int64_t arena_len, arena_cap;
    jm_presolve_counts counts;
    jm_presolve_outcome outcome;
    jaos_model *orig;
} jm_presolve;

void jm_presolve_init(jm_presolve *p);
void jm_presolve_free(jm_presolve *p);

/* Builds the reduced model in p->reduced from m as loaded — unscaled,
 * before sx_init runs (D-04) — and never writes to m (D-06). `w` is billed
 * starting 02-02; this plan accepts it and charges nothing. */
JAOS_NODISCARD jaos_status jm_presolve_run(const jaos_model *m, jm_presolve *p,
                                           jm_work *w);

/* Walks p's arena strictly LIFO (D-07) and writes p->orig's sol_col,
 * sol_row, sol_dual, sol_redcost, sol_col_status and sol_row_status in
 * ORIGINAL indices, from whatever the reduced solve left in p->reduced.
 * Also copies solve_status/iters/work/time and, on an optimal outcome,
 * remembers the postsolved basis on p->orig — D-08's mapping back into
 * original indices. Called from publish(), before it returns, only when
 * p->outcome == JM_PRESOLVE_REDUCED. */
JAOS_NODISCARD jaos_status jm_postsolve_expand(jm_presolve *p);

/* Publishes a presolve-only answer with no sx built and no simplex
 * iteration, for the outcome where every column presolve fixed
 * (JM_PRESOLVE_SOLVED). */
JAOS_NODISCARD jaos_status jm_postsolve_solved(jm_presolve *p);

/* Publishes JAOS_SOLVE_INFEASIBLE or JAOS_SOLVE_UNBOUNDED with no sx built
 * and no simplex iteration, for JM_PRESOLVE_INFEASIBLE/JM_PRESOLVE_UNBOUNDED
 * — the short-circuit D-12 asks this plan to confirm bench/run.c's
 * double-solve determinism check already reaches. */
JAOS_NODISCARD jaos_status jm_postsolve_infeasible_or_unbounded(
    jm_presolve *p, jaos_solve_status status);

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
