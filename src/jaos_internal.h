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

#endif /* JAOS_INTERNAL_H */
