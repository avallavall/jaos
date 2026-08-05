/* JAOS internals. Everything here can change at any time.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef JAOS_INTERNAL_H
#define JAOS_INTERNAL_H

#include "jaos.h"

#include <stddef.h>

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

    /* Budgets and the record of the last solve. Kept on the model so a
     * caller can query results without holding a solver handle. */
    int64_t work_limit;      /* <= 0 means unlimited */
    double time_limit;       /* <= 0 means unlimited */

    jaos_solve_status solve_status;
    double objective;
    double *sol_col;         /* [num_col] primal values      */
    double *sol_row;         /* [num_row] row activities     */
    double *sol_dual;        /* [num_row] row duals          */
    double *sol_redcost;     /* [num_col] reduced costs      */
    int64_t solve_work;
    int64_t solve_iters;

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
 * It works on the model as loaded, not on a scaled copy: jm_model_scale
 * exists and is tested but is not wired into the solve path yet, which
 * also means the tolerances in simplex.c act on raw magnitudes rather
 * than scaled ones. PLAN.md Q7 carries it. */
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
 * Reachable from outside the simplex for one reason: a wrong recurrence
 * costs iterations and never a wrong answer, so no solve-level test can
 * catch it and the formula has to be checked against norms recomputed from
 * scratch. */
void jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau);

/* Overflow-checked array allocation: n elements of elsize bytes.
 * Returns NULL on n < 0, size overflow, or exhaustion. n == 0 still returns
 * a valid non-NULL allocation, so success is always non-NULL. */
void *jm_alloc_array(int64_t n, size_t elsize);
void *jm_calloc_array(int64_t n, size_t elsize);

/* Grows *arr (elements of elsize) to hold at least need elements; on
 * failure *arr is untouched, so cleanup still frees the old block. */
bool jm_grow(void **arr, int64_t *cap, int64_t need, size_t elsize);
#define JM_GROW(a, cap, need) jm_grow((void **)&(a), &(cap), (need), sizeof *(a))

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

/* Formats into m->err. NULL model is tolerated (message dropped). */
[[gnu::format(printf, 2, 3)]]
void jm_set_err(jaos_model *m, const char *fmt, ...);

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
