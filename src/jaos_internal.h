/* JAOS internals. Everything here can change at any time.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef JAOS_INTERNAL_H
#define JAOS_INTERNAL_H

#include "jaos.h"

#include <float.h>
#include <stddef.h>

/* What a caller sets, as opposed to what a caller loads: the contract, never
 * the method (D64). Nothing here is problem data, so nothing here is
 * discarded by a load. */
typedef struct {
    int64_t work_limit;      /* <= 0 means unlimited */
    double time_limit;       /* <= 0 means unlimited */

    /* The two tolerances a caller owns, in scaled space where the solver
     * works. These two and no others; everything else in docs/tolerances.md
     * is about the method and stays measured and fixed. */
    double primal_tol;       /* <= 0 means PRIMAL_TOL */
    double dual_tol;         /* <= 0 means DUAL_TOL   */

    /* Where output goes. No callback means no output, whatever the level. */
    jaos_log_fn log_cb;
    void *log_user;
    jaos_log_level log_level;

    /* Who is asked, every PROGRESS_EVERY iterations, whether to carry on.
     * No callback means nobody is asked and the solve runs to its own end. */
    jaos_progress_fn progress_cb;
    void *progress_user;

    /* Solve this with the primal simplex rather than the dual. Not public
     * API: in-tree tooling built with `-Isrc` reaches it (D64).
     * `jaos_model_new` calloc's the model, so the default is the dual. */
    bool force_primal;
} jm_config;

/* Name -> value map for the readers and for lookup by name: FNV-1a, open
 * addressing, names kept in one arena. Absence and value are separate —
 * values may be negative. */
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
/* An owned copy of a string, or nullptr out of memory. */
char *jm_name_copy(const char *name);
/* The map's names as an array of n owned strings indexed by value: entry
 * v holds the name whose value is v, for every value in 0..n-1, and
 * nullptr where no name has that value. Values outside the range are
 * skipped. nullptr out of memory. This is how a reader hands its names to
 * the model. */
char **jm_nmap_to_names(const jm_nmap *m, int64_t n);

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

    /* Names. Either array is nullptr until a row (column) is given one;
     * inside it an entry is an owned string or nullptr for a row (column)
     * with no name of its own, which is called by its position: R<i+1>,
     * C<j+1>. `obj_name` is the objective row's, nullptr for COST. A name
     * rides with its row or column through an add or a delete and goes
     * with a load (D284). The two maps are the lookup by name, built on the
     * first jaos_col_index / jaos_row_index and dropped by anything that
     * changes a name or a dimension. */
    char **col_name;    /* [num_col] or nullptr */
    char **row_name;    /* [num_row] or nullptr */
    char *obj_name;
    /* The model's own name, MPS's NAME line; nullptr for "JAOS". */
    char *model_name;
    bool name_map_valid;
    jm_nmap col_map, row_map;

    /* Constraint matrix, compressed sparse column; entries within a column
     * sorted by row index, no duplicates, no explicit zeros. This is the
     * authoritative copy. */
    int64_t *a_start;   /* [num_col + 1] */
    int64_t *a_index;   /* [num_nz]      */
    double  *a_value;   /* [num_nz]      */

    /* Row-wise mirror (CSR), derived from the CSC copy on demand.
     * Invalidated by any load. */
    bool rowwise_valid;
    int64_t *ar_start;  /* [num_row + 1] */
    int64_t *ar_index;  /* [num_nz]      */
    double  *ar_value;  /* [num_nz]      */

    /* Scaling factors: row i and column j of A are conceptually multiplied
     * by row_scale[i] and col_scale[j]. The stored matrix is never touched
     * (PLAN.md 2.5). Every factor is an exact power of two. Invalidated by
     * any load. */
    bool scale_valid;
    double *row_scale;  /* [num_row] */
    double *col_scale;  /* [num_col] */

    /* Set when the exponent range a factor needed exceeded what JAOS is
     * willing to express, so the scaling actually applied is weaker than
     * the one computed. A caveat on a success, so it is not in err. */
    bool scale_clamped;

    /* Everything the caller configures, in one object. `model_release_arrays`
     * saves this and puts it back, so a setting added here survives a load. */
    jm_config cfg;

    jaos_solve_status solve_status;
    double objective;
    double *sol_col;         /* [num_col] primal values      */
    double *sol_row;         /* [num_row] row activities     */
    double *sol_dual;        /* [num_row] row duals          */
    double *sol_redcost;     /* [num_col] reduced costs      */
    jaos_basis_status *sol_col_status;  /* [num_col] where each column rests */
    jaos_basis_status *sol_row_status;  /* [num_row] and each row activity   */
    /* The Farkas ray behind INFEASIBLE, in the caller's units (D254).
     * `farkas_ok` is the availability: false on every solve entry, true
     * once a ray stands on this model's own rows — the dual's refusal
     * directly, or a reduced solve's refusal and a presolve site's own
     * bound lifted back through the reductions (D256). A site with no ray
     * to offer, a caller's inverted box, leaves it false. */
    double *sol_farkas;      /* [num_row] */
    bool farkas_ok;
    /* The unbounded ray behind UNBOUNDED, per structural column in the
     * caller's units (D255). Same availability discipline as the Farkas
     * ray, presolve's lift included (D256). */
    double *sol_ray;         /* [num_col] */
    bool ray_ok;
    /* What a proved basis says the answer is, as decimal rationals
     * ("-7/3"), one per column and one dual per row, and the objective
     * (D286). Filled by jaos_verify on OPTIMAL only, and dropped by
     * anything that drops the answer, so a stale proof cannot be read.
     * `exact_obj` alone may be null beside filled arrays: the objective's
     * sum can outgrow the limbs on a model whose values fitted. */
    char **exact_col;        /* [num_col] or nullptr */
    char **exact_dual;       /* [num_row] or nullptr */
    char *exact_obj;
    int64_t solve_work;
    int64_t solve_iters;
    /* How many of `solve_iters` the primal method ran, and how many of THOSE
     * belonged to its phase 1. Both zero on a pure dual solve. Written on
     * EVERY exit from `jm_dual_simplex`, which zeroes all three counts on
     * entry. Reporting only: nothing inside the solver reads them back. Not
     * public API (D64); bench/primal.c and tests/ read them directly. */
    int64_t solve_primal_iters, solve_phase1_iters;
    /* Seconds the last solve took. The one number on this struct that is not
     * reproducible, which is why nothing inside the solver may read it back. */
    double solve_time;

    /* What the last solve's presolve pass gave the simplex: the reduced
     * dimensions, or this model's own when nothing fired. Reporting only,
     * like solve_work/solve_iters. Not public API (D64); bench/run.c and
     * tests/ read these directly. */
    int64_t presolve_num_row, presolve_num_col, presolve_num_nz;

    /* The basis the next solve starts from, or null for the slack basis.
     * Held apart from sol_*_status: those are an answer, discarded when the
     * problem moves; this is a starting point that survives the move.
     * Written by jaos_set_basis and by every solve that reaches an optimum.
     * Dropped only by a load, which changes what the indices refer to. Both
     * arrays or neither. */
    jaos_basis_status *start_col_status;  /* [num_col] */
    jaos_basis_status *start_row_status;  /* [num_row] */

    /* Detail message for the last failed operation; "" when it succeeded.
     * Sits outside the problem data: setting it never disturbs a loaded
     * model. */
    char err[256];
};

/* Dual simplex ---------------------------------------------------------- */

/* Where a nonbasic variable sits. A basic variable's value comes from the
 * factorization; a nonbasic one is pinned to a bound. */
typedef enum {
    JM_BASIC = 0,
    JM_AT_LOWER,
    JM_AT_UPPER,
    JM_FREE,        /* nonbasic at zero, both bounds infinite */
} jm_var_status;

/* Runs the dual simplex and writes the outcome back into the model
 * (solve_status, objective, sol_*, counters). It works on a scaled copy of
 * the model; the model itself is never modified, and everything written
 * back is in the model's own units. */
JAOS_NODISCARD jaos_status jm_dual_simplex(jaos_model *m);

/* Forrest-Goldfarb dual steepest-edge weight update [8].
 * w[i] tracks ||row i of B^-1||^2. Column q enters the basis at row r.
 * `alpha` is B^-1 M_q and `tau` is B^-1 rho_r with rho_r = B^-T e_r, both
 * dense of length n, both taken against the basis before the change.
 * alpha[r] is the pivot.
 * `exact_r` is the true weight of row r, ||rho_r||^2. It replaces the
 * carried estimate before the recurrence runs, and the disagreement between
 * the two says how far the recurrence has drifted.
 * `drift_factor` is how far apart the carried and exact weights may be
 * before the weights are all reset to one and the recurrence is skipped.
 * The test is symmetric.
 * Returns true when it discarded the whole set rather than carrying it
 * forward (D63). */
bool jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau,
                   double exact_r, double drift_factor,
                   const int64_t *pat, int64_t npat);

/* Harris' two-pass ratio test [7], over n candidate breakpoints.
 * num[k] is how far candidate k's reduced cost still is from crossing into
 * infeasibility, never negative; den[k] is the magnitude of its pivot
 * element, strictly positive. Their quotient is the step at which that
 * candidate blocks. Pass one widens every numerator by dual_tol and takes
 * the smallest quotient. Pass two returns the candidate with the largest
 * pivot whose true, unwidened quotient still fits in that step.
 * Returns an index into num/den, or -1 if n is not positive. The set it
 * chooses from is never empty for n > 0. */
int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       double dual_tol);

/* Bland's rule over the same candidate set, for when Harris' has cycled.
 * Same num/den, and `var` carries each candidate's variable index. The rule
 * is the smallest index among the candidates attaining the exact minimum
 * quotient, no widening (D26). The minimum is compared exactly rather than
 * within a tolerance. Returns an index into the arrays, or -1 if n is not
 * positive. */
int64_t jm_bland_pick(int64_t n, const int64_t *var, const double *num,
                      const double *den);

/* The same decision on the primal side: does a candidate row displace the
 * incumbent in a primal ratio test? The primal's Bland's rule falls on the
 * LEAVING variable: the smallest basis variable index among the rows
 * attaining the exact minimum ratio.
 * `step` and `var` describe the candidate; `best_step` and `best_var` the
 * incumbent, with `best_var < 0` meaning there is not one yet. Callers scan
 * ascending, so `best_step` only ever falls.
 * With `bland` false this is a plain strict minimum, and the first row
 * scanned keeps a tie. That is not Bland's rule: the tie then falls to a row
 * POSITION. The minimum is compared exactly. */
bool jm_primal_row_wins(double step, int64_t var,
                        double best_step, int64_t best_var, bool bland);

/* Turns a list of positions into the sorted, distinct list of them.
 * `pos[0..n)` is unordered and may repeat. The result is written over the
 * input, ascending, each position once, and the count of them returned.
 * `words` reports how many bitmap words the enumeration had to look at,
 * which the caller bills.
 * `mark` is a bitmap of at least (limit + 63) / 64 words, where `limit`
 * bounds the positions; anything outside [0, limit) is dropped. It must be
 * all zero on entry and is all zero again on return.
 * Ascending order is not a preference: every consumer of a pricing row
 * breaks its ties by scan position. */
int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words);

/* The nonbasic set as a bitmap, and the four operations that keep it equal
 * to `{v : status[v] != JM_BASIC}`.
 * `mark` is a bitmap of (nvar + 63) / 64 words with bit v set exactly when
 * variable v is not basic. `jm_nonbasic_build` writes every one of those
 * words from `status[0..nvar)` and returns how many bits it set;
 * `jm_nonbasic_insert` and `jm_nonbasic_remove` set and clear bit v;
 * `jm_nonbasic_expand` writes the set positions ascending into `out` and
 * returns how many there were. A non-positive `nvar` has no words, so both
 * of the two that take one build nothing and return zero.
 * Membership, and never "has a finite bound": a nonbasic free variable is
 * in the set exactly as a bounded one is.
 * Persistent, unlike jm_pattern_order's bitmap: no reader clears it, and
 * jm_nonbasic_build is the only thing that ever writes it wholesale. The
 * sites that move a variable into or out of the basis maintain it by hand. */
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
 * The macro answers the common case without a cross-unit call (D55).
 * `need` and `cap` are evaluated twice. */
bool jm_grow(void **arr, int64_t *cap, int64_t need, size_t elsize);
#define JM_GROW(a, cap, need) \
    ((need) <= (cap) ? true : jm_grow((void **)&(a), &(cap), (need), sizeof *(a)))

/* Names (src/model.c). A row's or column's name is its own when it has one
 * and its position otherwise, R<i+1> or C<j+1>, written into `buf`, which
 * holds JM_NAME_BUF bytes; the pointer returned is the stored name or
 * `buf`. Both are what every writer prints and what the solution reader
 * checks against, so a file and the model agree on what a row is called
 * whether or not anyone named it. `jm_obj_name` is the objective row's,
 * "COST" when none was given. */
constexpr int JM_NAME_BUF = 24;
const char *jm_col_name(const jaos_model *m, int64_t j, char *buf);
const char *jm_row_name(const jaos_model *m, int64_t i, char *buf);
const char *jm_obj_name(const jaos_model *m);

/* Installs names a reader collected, taking ownership of every string and
 * of both arrays, which are sized by the model's current dimensions and may
 * hold nullptr entries; any of the three may be nullptr. Replaces what the
 * model held. Called after the reader's jaos_load_lp, which dropped the
 * previous names. */
void jm_model_take_names(jaos_model *m, char **col, char **row, char *obj);

/* Whether a string may be a name: not empty, at most JAOS_NAME_MAX bytes,
 * no whitespace and no control character, because every file this library
 * writes separates its fields by whitespace. The readers apply it to the
 * names they meet and the setters to the names they are given. */
bool jm_name_ok(const char *name);

/* Whether the LP reader's scanner reads `s` back as one name that is not a
 * keyword (src/lpfmt.c): the LP writer's test for a name it may print. */
bool jm_lp_name_ok(const char *s);

/* Frees the exact values a proof left on the model (src/verify.c). Called
 * by everything that drops the answer. */
void jm_model_drop_exact(jaos_model *m);

/* Reads a whole input file. A file that starts with the gzip magic is
 * inflated by src/inflate.c; any other file comes back byte for byte. On
 * success *out is a NUL-terminated buffer the caller frees and *out_len is
 * its length, which may itself contain NUL bytes. On failure *out is NULL
 * and the model carries the message. */
JAOS_NODISCARD jaos_status jm_slurp(jaos_model *m, const char *path,
                                    char **out, int64_t *out_len);

/* Builds the CSR mirror if it is not current. */
/* Whether a lower bound sits above its upper by more than presolve's own
 * rounding window (src/presolve.c). The solve's entry refuses such a box
 * as infeasible in every build (D259). */
bool jm_box_inverted(double lower, double upper);

JAOS_NODISCARD jaos_status jm_model_ensure_rowwise(jaos_model *m);

/* Allocates m's six sol_* arrays if any is missing, all six or none.
 * Shared by publish() and jm_postsolve_expand. */
JAOS_NODISCARD jaos_status jm_model_ensure_solution_arrays(jaos_model *m);

/* Keeps the basis just published as the one the next solve starts from.
 * A model with no published basis is left alone and reports success. */
JAOS_NODISCARD jaos_status jm_model_remember_basis(jaos_model *m);

/* Sets m->objective from m's own published solution, with a compensated sum
 * (D169). Every publication path ends here. */
void jm_model_publish_objective(jaos_model *m);

/* The two steps a `sum c_j x_j` needs to lose nothing, shared by the
 * published objective and `settled_objective` in the simplex.
 * `jm_obj_add` is one Neumaier step into a running sum and its compensation.
 * `sum` and `comp` must not be the same object; asserted at the definition.
 * `jm_two_product_residue` is what `a * b` lost when it rounded to `p`, by
 * Dekker's split. Beyond a factor magnitude of 2^996 the split would
 * overflow, so it reports a zero residue there and the caller keeps the
 * plain product (D172, D175).
 * The split needs double arithmetic evaluated at double, which the assert
 * below states wherever these are used. */
static_assert(FLT_EVAL_METHOD == 0,
              "Dekker's split needs double arithmetic evaluated at double");
/* The other half of the same precondition, and the build could not see it
 * until D219. `-ffast-math` and `-Ofast` enable `-fassociative-math`, which
 * lets the compiler rewrite `(a - u) + t` and delete the residue the split
 * exists to compute; the result stays plausible and is silently wrong. The
 * Makefile uses neither flag, and this is what refuses one added later. */
#ifdef __FAST_MATH__
#error "JAOS cannot be built with -ffast-math or -Ofast: associative maths deletes the compensated-summation residue (D34, D175, D219)"
#endif

void jm_obj_add(double *sum, double *comp, double t);
JAOS_NODISCARD double jm_two_product_residue(double a, double b, double p);

/* Formats into m->err. NULL model is tolerated (message dropped). */
[[gnu::format(printf, 2, 3)]]
void jm_set_err(jaos_model *m, const char *fmt, ...);

/* Is anyone listening at this level? Every logging site tests this first, so
 * a solve nobody is watching pays one comparison per site. */
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

/* Deterministic work counter (D16) --------------------------------------- */

/* The reproducible budget's currency. Counted in the kernels, never
 * derived from a clock, so a run consumes the same units on every machine. */
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

/* Presolve and postsolve ------------------------------------------------ */

/* What each reduction removed. Logged at JAOS_LOG_SUMMARY and printed by
 * bench/run.c; tests are white-box and read the fields directly. */
typedef struct {
    int64_t fixed_col;
    int64_t empty_row;
    int64_t empty_col;
    int64_t singleton_row;
    int64_t singleton_col;          /* bounded singleton column only */
    int64_t free_col_singleton;     /* counted apart from singleton_col: it
                                      * removes one row and one column */
    int64_t forcing_row;
    int64_t redundant_row;
    int64_t implied_free_col;       /* the implied free column singleton,
                                      * substituted out exactly (D105) */
    int64_t tightened_bound;
    int64_t duplicate_row;
    int64_t duplicate_col;
    int64_t dominated_col;
    int64_t rounds;
} jm_presolve_counts;

/* One postsolve record kind per reduction family. */
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
 * needs to be restored. `index` is always an ORIGINAL row or column index,
 * never a reduced one. Every other field is read differently by different
 * tags, where jm_postsolve_expand and jm_postsolve_solved read them:
 *   JM_PS_FIXED_COL, JM_PS_EMPTY_COL: index=column, value=the fixed value,
 *     cost=the column's own cost. A JM_PS_FIXED_COL pushed by a forcing row
 *     also carries coef=its coefficient in that row; every other producer
 *     of this tag leaves coef at zero.
 *   JM_PS_EMPTY_ROW: index=row. Nothing else is read.
 *   JM_PS_SINGLETON_ROW: index=row, index2=the column its one entry named,
 *     coef=that entry's coefficient. row_tightens_lo/hi record which side(s)
 *     of the column's bound the row's own implied bound won at the moment
 *     of intersection, and lo/hi are the bounds the column was left
 *     carrying after that intersection.
 *   JM_PS_SINGLETON_COL: index=row (which survives, relaxed), index2=the
 *     column removed, coef=the entry's coefficient, lo/hi=the column's own
 *     bounds at the moment it was removed, row_lo/row_hi=the row's own
 *     current bounds at that same moment, BEFORE this record's relaxation.
 *     The replay reads sol_row[index] as sum plus carry, never the sum
 *     alone (see ps_replay_one).
 *   JM_PS_FREE_COL_SINGLETON: index=row (removed), index2=column (removed),
 *     coef=the entry's coefficient, lo/hi=the row's own current bounds at
 *     the moment both were removed.
 *   JM_PS_REDUNDANT_ROW: index=row. Nothing else is read.
 *   JM_PS_FORCING_ROW: index=row (removed), index2=how many records
 *     IMMEDIATELY BEFORE this one in the arena are the columns this row
 *     fixed. row_tightens_hi says which of the row's own bounds the range
 *     attained: true for the upper bound (minimum activity reached it),
 *     false for the lower (maximum activity reached it).
 *   JM_PS_IMPLIED_FREE_COL: index=row (removed), index2=column (removed),
 *     coef=the entry's coefficient, value=the row's own current bound at
 *     the moment it fired, cost=the column's cost in the CURRENT objective
 *     at that same moment. lo/hi are the implied box the row put on the
 *     column; nothing reads them back.
 * There is no tag for bound tightening. `jm_presolve_counts.tightened_bound`
 * stays declared and stays zero. */
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
 * and SOLVED all short-circuit the simplex entirely. UNBOUNDED comes from
 * the empty column only, the one family permitted to report it (D19). */
typedef enum {
    JM_PRESOLVE_NONE,
    JM_PRESOLVE_REDUCED,
    JM_PRESOLVE_INFEASIBLE,
    JM_PRESOLVE_UNBOUNDED,
    JM_PRESOLVE_SOLVED,
} jm_presolve_outcome;

/* The solve-local presolve workspace: built inside jm_dual_simplex,
 * consumed before publish returns, freed with sx.
 * `reduced` is a jaos_model VALUE, not a pointer. Every array on it is
 * presolve-owned; none aliases the caller's model, so jm_presolve_free never
 * hands `reduced` to jaos_model_free.
 * orig_col/orig_row map a reduced index to the original one it came from;
 * col_map/row_map are the inverse, original index to reduced index or -1
 * when a reduction removed it.
 * `orig` is the caller's own model: the non-const write target
 * jm_postsolve_expand needs. jm_dual_simplex sets it directly; nothing
 * inside presolve.c needs to. */
typedef struct {
    jaos_model reduced;
    int64_t *orig_col, *orig_row;
    int64_t *col_map, *row_map;
    jm_presolve_rec *arena;
    int64_t arena_len, arena_cap;
    jm_presolve_counts counts;
    jm_presolve_outcome outcome;
    /* The seed of a presolve-proved verdict's certificate (D256): the
     * ORIGINAL row whose own bound refused (INFEASIBLE) or the ORIGINAL
     * column whose cost runs off an open side (UNBOUNDED), and the sign
     * the ray takes there. -1 when the site has no ray to offer, which is
     * a caller's inverted box. jm_postsolve_infeasible_or_unbounded lifts
     * it through the arena. */
    int64_t proof_index;
    double proof_sign;
    jaos_model *orig;
} jm_presolve;

void jm_presolve_init(jm_presolve *p);
void jm_presolve_free(jm_presolve *p);

/* Builds the reduced model in p->reduced from m as loaded, unscaled, before
 * sx_init runs, and never writes to m. */
JAOS_NODISCARD jaos_status jm_presolve_run(const jaos_model *m, jm_presolve *p,
                                           jm_work *w);

/* Walks p's arena strictly LIFO and writes p->orig's sol_col, sol_row,
 * sol_dual, sol_redcost, sol_col_status and sol_row_status in ORIGINAL
 * indices, from whatever the reduced solve left in p->reduced. Also copies
 * solve_status/iters/work/time and, on an optimal outcome, remembers the
 * postsolved basis on p->orig. Called from publish(), before it returns,
 * only when p->outcome == JM_PRESOLVE_REDUCED. */
JAOS_NODISCARD jaos_status jm_postsolve_expand(jm_presolve *p);

/* Publishes a presolve-only answer with no sx built and no simplex
 * iteration, for the outcome where every column presolve fixed
 * (JM_PRESOLVE_SOLVED). */
JAOS_NODISCARD jaos_status jm_postsolve_solved(jm_presolve *p);

/* Publishes JAOS_SOLVE_INFEASIBLE or JAOS_SOLVE_UNBOUNDED with no sx built
 * and no simplex iteration, for JM_PRESOLVE_INFEASIBLE/JM_PRESOLVE_UNBOUNDED,
 * and the certificate the refusing site seeded, lifted into the caller's
 * space (D256). */
JAOS_NODISCARD jaos_status jm_postsolve_infeasible_or_unbounded(
    jm_presolve *p, jaos_solve_status status);

/* Sparse LU factorization of a basis ------------------------------------ */

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
 * Everything is indexed by slot: slot s is the pivot taken at step s of
 * the factorization, and it keeps its original row and its basis column
 * for life. What an update changes is a slot's position in the triangular
 * order, tracked by slot_at and pos_of.
 * U is held in both orientations at once because an update needs both: by
 * column to install the spike, by row to eliminate it. */
typedef struct {
    int64_t dim;
    int64_t rank;        /* pivots found; rank < dim means singular */

    /* L: one elimination eta per slot, indices in slot space. Fixed once
     * factored; updates never touch L. */
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
     * threshold from the entering column. */
    double drop;

    double *tmp;         /* [dim] solve workspace, owned */
    double *spike;       /* [dim] update workspace, owned */

    /* Reachability workspace for BTRAN's U' and L' passes [9]. `mark` is
     * stamped rather than cleared. */
    int64_t *mark;       /* [dim] visit stamp                            */
    int64_t stamp;       /* current stamp; mark[s] == stamp means visited */
    int64_t *dfs_node;   /* [dim] explicit DFS stack: node at each level  */
    int64_t *dfs_next;   /* [dim] and how far into its row it has got     */
    int64_t *pattern;    /* [dim] reachable slots, in topological order   */

    /* L by row, structure only, for the L' reachability pass: row t lists
     * the slots s whose column of L carries row t (each s < t), ascending.
     * The solve's arithmetic still reads the column form; this is only who
     * feeds whom. Built once per full-rank factorization; updates never
     * touch L (D253). */
    int64_t *lrow_start; /* [dim + 1] */
    int64_t *lrow_index; /* [nnz(L)]  */
} jm_lu;

/* Markowitz threshold: a pivot must be at least this fraction of the
 * largest magnitude in its column (docs/tolerances.md). Shared by the
 * solve's factorizations and by ranging's factorization of the published
 * basis (D258), so both read one owner. */
constexpr double LU_PIVOT_TOL = 0.1;

void jm_lu_init(jm_lu *lu);
void jm_lu_free(jm_lu *lu);

/* Factors a dim x dim matrix given in compressed sparse column form.
 * Markowitz ordering under a threshold stability test: a candidate pivot
 * must be at least pivot_tol times the largest magnitude in its column.
 * A singular matrix is not an error. JAOS_OK is returned with rank < dim,
 * and the pivoted rows and columns are the first `rank` entries of
 * perm_row and perm_col. */
JAOS_NODISCARD jaos_status jm_lu_factor(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w);

/* Solves B x = b (FTRAN) and B' x = b (BTRAN), in place on a dense vector
 * of length dim. The factorization is taken non-const because both solves
 * scribble on its internal workspace: they are not reentrant and two of
 * them may not overlap on the same jm_lu. A factorization that is not full
 * rank leaves x untouched. */
void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w);
void jm_lu_btran(jm_lu *lu, double *x, jm_work *w);

/* The forward solve, additionally reporting where the answer is nonzero.
 * `pat` takes at least `dim` entries and comes back holding `*npat` indices
 * in whatever order the column permutation produced them. A null `pat` is
 * exactly jm_lu_ftran. */
void jm_lu_ftran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat);

/* The same solve, additionally reporting where the answer is nonzero.
 * `pat` takes at least `dim` entries and comes back holding `*npat` row
 * indices, in whatever order the row permutation produced them, not
 * ascending; a caller that needs ascending has jm_pattern_order for it.
 * A null `pat` is exactly jm_lu_btran. */
void jm_lu_btran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat);

/* Forrest-Tomlin update [5]: basis column `col_out` is replaced by
 * `new_col`, a dense vector indexed by original row. Three passes are
 * O(dim), so the work counter charges JM_WORK_UPDATE for that floor (D16).
 * Returns JAOS_ERR_NUMERICAL when the replacement leaves a pivot too small
 * to trust. By then the update has already rewritten U, so the
 * factorization is left marked unusable (rank < 0) and the caller must
 * refactorize.
 * `min_pivot_ratio` is the floor on the new diagonal relative to the
 * spike's largest magnitude, far looser than Markowitz's threshold.
 * Updates accumulate both error and fill; the caller refactorizes on an
 * interval, watching lu->n_updates. */
JAOS_NODISCARD jaos_status jm_lu_update(jm_lu *lu, int64_t col_out,
    const double *new_col, double min_pivot_ratio, jm_work *w);


/* Exact integer and rational arithmetic (src/exact.c) -------------------- */

/* Limbs in a magnitude, base 2^32.
 *
 * The floor is what one double costs. A finite double is m * 2^e with e no
 * smaller than -1074, so its denominator needs up to 1075 bits and its
 * numerator up to 1024: 34 limbs holds either one. Everything above that is
 * headroom for sums, products and the elimination that reads them, and an
 * operation that runs out of it returns false rather than wrapping. 128
 * limbs is 4096 bits and makes a jm_rational 1048 bytes.
 *
 * Sweep it with -DJM_EXACT_LIMBS=N. Nothing in a shipping build reads
 * any other value, and the value only moves how much can be proved: no
 * setting changes an answer, it changes whether there is one. */
#ifndef JM_EXACT_LIMBS
#define JM_EXACT_LIMBS 128
#endif

/* A magnitude, least significant limb first, no leading zero limb. n == 0
 * is zero and is its only representation. */
typedef struct {
    uint32_t w[JM_EXACT_LIMBS];
    int64_t  n;
} jm_nat;

/* A signed integer. sign is -1, 0 or 1, and is 0 exactly when mag is. */
typedef struct {
    jm_nat  mag;
    int32_t sign;
} jm_bigint;

/* num / den, with den > 0 and gcd(|num|, den) == 1. Zero is 0/1. */
typedef struct {
    jm_bigint num;
    jm_nat    den;
} jm_rational;

void    jm_nat_set_zero(jm_nat *a);
void    jm_nat_set_u64(jm_nat *a, uint64_t v);
bool    jm_nat_is_zero(const jm_nat *a);
int64_t jm_nat_bits(const jm_nat *a);
int     jm_nat_cmp(const jm_nat *a, const jm_nat *b);
/* r = a - b, and the caller has established a >= b. */
void    jm_nat_sub(jm_nat *r, const jm_nat *a, const jm_nat *b);
void    jm_nat_shr(jm_nat *r, const jm_nat *a, int64_t bits);
/* These five return false when the result does not fit in JM_EXACT_LIMBS,
 * leaving the destination unspecified. jm_nat_divmod also returns false on
 * a zero divisor; either of its outputs may be null, and neither may alias
 * an input. */
JAOS_NODISCARD bool jm_nat_add(jm_nat *r, const jm_nat *a, const jm_nat *b);
JAOS_NODISCARD bool jm_nat_mul(jm_nat *r, const jm_nat *a, const jm_nat *b);
JAOS_NODISCARD bool jm_nat_shl(jm_nat *r, const jm_nat *a, int64_t bits);
JAOS_NODISCARD bool jm_nat_divmod(jm_nat *q, jm_nat *rem, const jm_nat *a,
                                  const jm_nat *b);
JAOS_NODISCARD bool jm_nat_gcd(jm_nat *r, const jm_nat *a, const jm_nat *b);

void    jm_bigint_set_zero(jm_bigint *a);
void    jm_bigint_set_i64(jm_bigint *a, int64_t v);
bool    jm_bigint_is_zero(const jm_bigint *a);
int32_t jm_bigint_sign(const jm_bigint *a);
void    jm_bigint_neg(jm_bigint *a);
int     jm_bigint_cmp(const jm_bigint *a, const jm_bigint *b);
JAOS_NODISCARD bool jm_bigint_add(jm_bigint *r, const jm_bigint *a,
                                  const jm_bigint *b);
JAOS_NODISCARD bool jm_bigint_sub(jm_bigint *r, const jm_bigint *a,
                                  const jm_bigint *b);
JAOS_NODISCARD bool jm_bigint_mul(jm_bigint *r, const jm_bigint *a,
                                  const jm_bigint *b);
/* a * 2^bits, bits >= 0 only: this makes a row of doubles integral and a
 * right shift that dropped a set bit would be a silent rounding. */
JAOS_NODISCARD bool jm_bigint_shl(jm_bigint *r, const jm_bigint *a,
                                  int64_t bits);
/* a / b where b divides a exactly. False on a zero divisor, on a result
 * that does not fit, and on a nonzero remainder -- which in a fraction-free
 * elimination means the elimination is wrong, not that the input is hard. */
JAOS_NODISCARD bool jm_bigint_divexact(jm_bigint *q, const jm_bigint *a,
                                       const jm_bigint *b);

void    jm_rational_set_zero(jm_rational *r);
void    jm_rational_set_i64(jm_rational *r, int64_t v);
bool    jm_rational_is_zero(const jm_rational *r);
int32_t jm_rational_sign(const jm_rational *r);
void    jm_rational_neg(jm_rational *r);
/* Sign of a - c. Both are normalised, so this is exact. */
/* Sign of a - c. Out of limbs this reports 0, the same answer it gives for
 * a real equality, so **a caller that cannot rule the overflow out must use
 * the checked form**: reading a failed comparison as "equal" is how a bound
 * test certifies a value it never compared. */
int     jm_rational_cmp(const jm_rational *a, const jm_rational *c);
/* The same, writing the sign and returning false when a cross-multiply does
 * not fit. On false *out is untouched. */
JAOS_NODISCARD bool jm_rational_cmp_checked(const jm_rational *a,
                                            const jm_rational *c, int *out);
/* The nearest double, ties to even. For a report only: a proof never
 * leaves the rationals. */
double  jm_rational_to_double(const jm_rational *r);
/* The rational spelled in decimal, "-7/3", "12", "0", as an owned string;
 * nullptr out of memory. A denominator of one is not written. */
char   *jm_rational_decimal(const jm_rational *r);
/* False for an infinity or a NaN; every finite double is exact here. */
JAOS_NODISCARD bool jm_rational_from_double(jm_rational *r, double d);
/* False when the result does not fit, and jm_rational_div also when the
 * divisor is zero. */
JAOS_NODISCARD bool jm_rational_add(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);
JAOS_NODISCARD bool jm_rational_sub(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);
JAOS_NODISCARD bool jm_rational_mul(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);
JAOS_NODISCARD bool jm_rational_div(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);

/* A dyadic rational, m * 2^e. Every finite double is one and sums and
 * products of them are, so evaluating a row never leaves the type. Kept
 * with m odd, or m zero and e zero, which is its only canonical form.
 *
 * It sits beside jm_rational rather than replacing it: a general rational
 * normalises with a gcd and two divisions, and this normalises by
 * stripping trailing zero bits. Both are exact. Division is what needs
 * jm_rational, and evaluating a point does not divide. */
typedef struct {
    jm_bigint m;
    int64_t   e;
} jm_dyadic;

void    jm_dyadic_set_zero(jm_dyadic *d);
bool    jm_dyadic_is_zero(const jm_dyadic *d);
int32_t jm_dyadic_sign(const jm_dyadic *d);
/* The nearest double, ties to even: one rounding, not a chain of them --
 * which for a subnormal result means rounding at 2^-1074 and not at 53
 * bits, or ldexp would round a second time (D268). */
double  jm_dyadic_to_double(const jm_dyadic *d);
/* False for an infinity or a NaN. Every finite double is exact. */
JAOS_NODISCARD bool jm_dyadic_from_double(jm_dyadic *d, double v);
/* False when the result does not fit in JM_EXACT_LIMBS. jm_dyadic_cmp
 * writes the sign of a - b and fails only when that difference does not
 * fit, rather than guessing an order. */
JAOS_NODISCARD bool jm_dyadic_add(jm_dyadic *r, const jm_dyadic *a,
                                  const jm_dyadic *b);
JAOS_NODISCARD bool jm_dyadic_sub(jm_dyadic *r, const jm_dyadic *a,
                                  const jm_dyadic *b);
JAOS_NODISCARD bool jm_dyadic_mul(jm_dyadic *r, const jm_dyadic *a,
                                  const jm_dyadic *b);
JAOS_NODISCARD bool jm_dyadic_cmp(const jm_dyadic *a, const jm_dyadic *b,
                                  int *out);

/* What a claimed point is worth, with no rounding anywhere in the walk and
 * one at the end of it. `objective` is the model's own form, offset
 * included and the sense not applied, so it compares directly with
 * jaos_objective. The two violations are the largest over all rows and all
 * columns, and the two indices name where, or -1 when nothing is violated.
 * `terms` counts the products formed, which is what the cost scales with. */
typedef struct {
    double  objective;
    double  row_violation;
    double  col_violation;
    int64_t row_at;
    int64_t col_at;
    int64_t terms;
} jm_exact_point;

/* False when the model has no row-wise mirror to build, or when some
 * operation ran out of limbs. There is no partial answer: a verifier that
 * could not finish has to say so. On false the three doubles are NaN and
 * both indices are -1, because a half-written walk would otherwise read
 * as a point with nothing wrong with it (D268). `terms` still says how
 * far it got. */
JAOS_NODISCARD bool jm_exact_evaluate(jaos_model *m, const double *x,
                                      jm_exact_point *out);
#endif /* JAOS_INTERNAL_H */
