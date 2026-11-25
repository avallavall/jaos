#include "jaos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct { int64_t dim, nnz; double tol; int64_t *start, *index; double *value; } mat;
static uint64_t H = 1469598103934665603ULL;
static void hb(const void *p, size_t n) { const unsigned char *c = p; for (size_t i = 0; i < n; i++) { H ^= c[i]; H *= 1099511628211ULL; } }
static void hash_lu(const jm_lu *lu) {
    int64_t d = lu->dim;
    hb(&lu->rank, 8);
    hb(lu->perm_row, 8 * d); hb(lu->perm_col, 8 * d); hb(lu->u_diag, 8 * d);
    hb(lu->l_start, 8 * (d + 1));
    hb(lu->l_index, 8 * lu->l_start[d]); hb(lu->l_value, 8 * lu->l_start[d]);
    if (lu->rank == d) {
        for (int64_t s = 0; s < d; s++) { hb(&lu->urow[s].n, 8); hb(lu->urow[s].idx, 8 * lu->urow[s].n); hb(lu->urow[s].val, 8 * lu->urow[s].n); }
        for (int64_t s = 0; s < d; s++) { hb(&lu->ucol[s].n, 8); hb(lu->ucol[s].idx, 8 * lu->ucol[s].n); hb(lu->ucol[s].val, 8 * lu->ucol[s].n); }
        if (d > 0) { hb(lu->lrow_start, 8 * (d + 1)); hb(lu->lrow_index, 8 * lu->lrow_start[d]); }
    }
}
int main(int argc, char **argv) {
    int reps = argc > 2 ? atoi(argv[2]) : 3;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    mat *m = NULL; int nm = 0, cap = 0;
    for (;;) {
        mat x;
        if (fread(&x.dim, 8, 1, f) != 1) break;
        if (fread(&x.nnz, 8, 1, f) != 1 || fread(&x.tol, 8, 1, f) != 1) return 2;
        x.start = malloc(8 * (x.dim + 1)); x.index = malloc(8 * (x.nnz + 1)); x.value = malloc(8 * (x.nnz + 1));
        if (fread(x.start, 8, x.dim + 1, f) != (size_t)x.dim + 1 || fread(x.index, 8, x.nnz, f) != (size_t)x.nnz || fread(x.value, 8, x.nnz, f) != (size_t)x.nnz) return 3;
        if (nm == cap) { cap = cap ? 2 * cap : 16; m = realloc(m, sizeof *m * cap); }
        m[nm++] = x;
    }
    fclose(f);
    jm_lu lu; jm_lu_init(&lu);
    jm_work w = {0};
    struct timespec t0, t1;
    double best = 1e30;
    for (int r = 0; r < reps; r++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < nm; k++) {
            if (jm_lu_factor(&lu, m[k].dim, m[k].start, m[k].index, m[k].value, m[k].tol, &w) != JAOS_OK) { printf("fail %d\n", k); }
            if (r == 0) hash_lu(&lu);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double t = (t1.tv_sec - t0.tv_sec) + 1e-9 * (t1.tv_nsec - t0.tv_nsec);
        if (r > 0 && t < best) best = t;
    }
    printf("mats %d dim %lld hash %016llx work %lld best %.4f ms/factor %.3f\n", nm, (long long)m[0].dim, (unsigned long long)H, (long long)w.units, best, 1e3 * best / nm);
    jm_lu_free(&lu);
    return 0;
}
