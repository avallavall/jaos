/* D271. How much of the gate an exact verifier could prove, asked before any
 * elimination is written.
 *
 * An exact verifier factors the final basis B over the integers. Bareiss's
 * entries are minors of B, so Hadamard bounds every one of them, and the
 * largest thing the factorization ever holds is det B itself:
 *
 *     log2 |det B|  <=  sum_j log2 ||b_j||_2
 *
 * where b_j is a column of B. That is one pass over the basis columns, in
 * floating point, before a single limb is allocated. Past the limb capacity
 * the verifier can refuse a priori and say so, which is what this project
 * wants instead of running out of memory halfway.
 *
 * So the question this asks is: over the 139 gate instances, how many bases
 * fit in JM_EXACT_LIMBS, and how far outside the rest are. It reads the
 * PUBLISHED basis, which since D257 has exactly num_row basics.
 *
 * A slack column is a unit vector, so its log2 norm is zero and it costs the
 * bound nothing. That is why the answer is not simply "no".
 *
 * Not a gate tool. Built and run by run-hadamard.sh.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The bound in bits, and the basis's shape. Everything here is a double
 * rounded the ordinary way: the figure is a budget, and a budget half an ulp
 * out changes no decision this asks. */
typedef struct {
    double  bits;        /* sum of log2 of the column norms */
    int64_t structurals; /* basic columns that are not slacks */
    int64_t slacks;
    int64_t nnz;         /* nonzeros in the structural basic columns */
    double  worst_col;   /* the largest single log2 norm */
    int64_t worst_at;
} basis_bound;

static bool bound_of(jaos_model *m, basis_bound *out)
{
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    jaos_basis_status *cs = calloc((size_t)(nc > 0 ? nc : 1), sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)(nr > 0 ? nr : 1), sizeof *rs);
    if (cs == nullptr || rs == nullptr) {
        free(cs); free(rs);
        return false;
    }
    if (jaos_basis(m, cs, rs) != JAOS_OK) {
        free(cs); free(rs);
        return false;
    }

    memset(out, 0, sizeof *out);
    out->worst_at = -1;

    for (int64_t i = 0; i < nr; i++)
        if (rs[i] == JAOS_BASIS_BASIC)
            out->slacks++;

    for (int64_t j = 0; j < nc; j++) {
        if (cs[j] != JAOS_BASIS_BASIC)
            continue;
        out->structurals++;
        /* ||b_j||_2, from the model's own CSC. */
        double ss = 0.0;
        int64_t n = 0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const double v = m->a_value[k];
            if (v == 0.0)
                continue;
            ss += v * v;
            n++;
        }
        out->nnz += n;
        /* A basic column with no entries contributes a zero norm, which
         * would make the determinant zero and the basis singular. It cannot
         * happen on a real basis; skipped rather than taking log2(0). */
        if (ss <= 0.0)
            continue;
        const double lg = 0.5 * log2(ss);
        out->bits += lg;
        if (lg > out->worst_col) {
            out->worst_col = lg;
            out->worst_at = j;
        }
    }

    free(cs);
    free(rs);
    return true;
}

int main(int argc, char **argv)
{
    const double cap_bits = 32.0 * (double)JM_EXACT_LIMBS;

    printf("# an exact verifier's a-priori budget, per gate instance\n");
    printf("# capacity: JM_EXACT_LIMBS=%d, %.0f bits\n",
           (int)JM_EXACT_LIMBS, cap_bits);
    printf("# bits: Hadamard bound on log2|det B|, one pass over the basis\n");
    printf("# fits: whether that bound is inside the capacity\n\n");
    printf("%-14s %-11s %8s %8s %8s %12s %10s %6s\n",
           "instance", "status", "rows", "basic", "slacks", "bits", "worstcol",
           "fits");

    int64_t seen = 0, fits = 0, skipped = 0;
    double worst_bits = 0.0;
    char worst_name[64] = "";

    for (int a = 1; a < argc; a++) {
        const char *path = argv[a];
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;

        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) {
            printf("%-14s new failed\n", name);
            continue;
        }
        if (jaos_read_mps(m, path) != JAOS_OK) {
            printf("%-14s read failed\n", name);
            jaos_model_free(m);
            continue;
        }
        const jaos_status st = jaos_solve(m);
        if (st != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            printf("%-14s %-11s (no optimum, no basis to bound)\n",
                   name, jaos_solve_status_str(jaos_status_of(m)));
            skipped++;
            jaos_model_free(m);
            continue;
        }

        basis_bound b;
        if (!bound_of(m, &b)) {
            printf("%-14s basis failed\n", name);
            jaos_model_free(m);
            continue;
        }

        seen++;
        const bool ok = b.bits <= cap_bits;
        if (ok)
            fits++;
        if (b.bits > worst_bits) {
            worst_bits = b.bits;
            snprintf(worst_name, sizeof worst_name, "%s", name);
        }
        printf("%-14s %-11s %8lld %8lld %8lld %12.1f %10.2f %6s\n",
               name, "optimal", (long long)jaos_num_row(m),
               (long long)b.structurals, (long long)b.slacks,
               b.bits, b.worst_col, ok ? "yes" : "NO");
        jaos_model_free(m);
    }

    printf("\n%lld bases bounded, %lld skipped for having no optimum\n",
           (long long)seen, (long long)skipped);
    printf("%lld of %lld fit in %.0f bits; %lld do not\n",
           (long long)fits, (long long)seen, cap_bits,
           (long long)(seen - fits));
    printf("largest bound: %s at %.1f bits, %.1fx the capacity\n",
           worst_name, worst_bits, worst_bits / cap_bits);
    return 0;
}
