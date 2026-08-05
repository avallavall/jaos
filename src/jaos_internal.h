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

    /* Detail message for the last failed operation; "" when it succeeded.
     * Sits outside the problem data on purpose: setting it never disturbs a
     * loaded model. */
    char err[256];
};

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

#define JM_WORK_NONZERO      1     /* one nonzero touched in a solve   */
#define JM_WORK_ELIMINATED   2     /* one nonzero eliminated in factor */
#define JM_WORK_FACTOR       4096  /* fixed cost of a refactorization  */

static inline void jm_work_add(jm_work *w, int64_t n)
{
    if (w != nullptr)
        w->units += n;
}

/* --------------------------------------------------------------------- */
/* Sparse LU factorization of a basis                                    */
/* --------------------------------------------------------------------- */

/* Factorization of a square matrix B as P B Q = L U, with L unit lower
 * triangular and U upper triangular in pivot order.
 *
 * L is stored as one elimination eta per pivot step (the multipliers below
 * that pivot); U as one row per pivot step, excluding its diagonal, which
 * lives in u_diag. Both carry indices already renumbered into pivot space,
 * so the solves never indirect through a permutation. */
typedef struct {
    int64_t dim;
    int64_t rank;       /* pivots found; rank < dim means singular */

    int64_t *l_start;   /* [dim + 1] */
    int64_t *l_index;   /* pivot-space row positions, all > their step */
    double  *l_value;

    int64_t *u_start;   /* [dim + 1] */
    int64_t *u_index;   /* pivot-space column positions, all > their step */
    double  *u_value;
    double  *u_diag;    /* [dim] */

    int64_t *perm_row;  /* pivot k used original row perm_row[k]    */
    int64_t *perm_col;  /* pivot k used original column perm_col[k] */
    int64_t *inv_row;   /* original row i sits at pivot inv_row[i], or -1 */
    int64_t *inv_col;

    double *tmp;        /* [dim] solve workspace, owned */
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
 * of length dim. Both require rank == dim; callers check once after
 * factoring rather than on every solve, because these run millions of
 * times and the branch would buy nothing. */
void jm_lu_ftran(const jm_lu *lu, double *x, jm_work *w);
void jm_lu_btran(const jm_lu *lu, double *x, jm_work *w);

#endif /* JAOS_INTERNAL_H */
