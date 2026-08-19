/* Can jaos_set_basis alone make HEAD publish a wrong optimum?
 *
 * jaos.h promises a hostile basis "costs time and cannot produce a wrong
 * verdict". D145 manufactured count-valid bases inside a candidate and got
 * eight wrong optima through the termination hole; this asks whether a
 * CALLER can do the same at HEAD with nothing but the public API.
 *
 * For each instance: one cold solve for the reference objective, then S
 * deterministic hostile bases — exactly nrow columns BASIC starting at a
 * shifting offset, every other status AT_LOWER — each set through
 * jaos_set_basis and solved. A basis the presolve mapping rejects just
 * starts cold and reads as correct; one it accepts starts hostile. Any
 * line with wrong=1 refutes the promise on its own.
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int run_instance(const char *path, const char *name, int shifts)
{
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 1;
    if (jaos_read_mps(m, path) != JAOS_OK) {
        fprintf(stderr, "HOSTILE inst=%s error=read\n", name);
        jaos_model_free(m);
        return 1;
    }
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    if (jaos_solve(m) != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
        fprintf(stderr, "HOSTILE inst=%s error=cold\n", name);
        jaos_model_free(m);
        return 1;
    }
    double ref = 0.0;
    (void)jaos_objective(m, &ref);

    jaos_basis_status *cs = malloc((size_t)nc * sizeof *cs);
    jaos_basis_status *rs = malloc((size_t)nr * sizeof *rs);
    double *x = malloc((size_t)nc * sizeof *x);
    double *y = malloc((size_t)nr * sizeof *y);
    if (!cs || !rs || !x || !y) {
        free(cs); free(rs); free(x); free(y);
        jaos_model_free(m);
        return 1;
    }

    for (int s = 1; s <= shifts; s++) {
        /* Reload so every trial starts from the same model state: a solve
         * leaves its own published basis behind, and set_basis must be the
         * only warm memory in play. */
        if (jaos_read_mps(m, path) != JAOS_OK)
            break;
        for (int64_t j = 0; j < nc; j++)
            cs[j] = JAOS_BASIS_AT_LOWER;
        for (int64_t i = 0; i < nr; i++)
            rs[i] = JAOS_BASIS_AT_LOWER;
        int64_t placed = 0;
        for (int64_t k = 0; placed < nr && k < nc; k++) {
            const int64_t j = (k * (int64_t)s * 7 + s) % nc;
            if (cs[j] != JAOS_BASIS_BASIC) {
                cs[j] = JAOS_BASIS_BASIC;
                placed++;
            }
        }
        for (int64_t i = 0; placed < nr && i < nr; i++) {
            rs[i] = JAOS_BASIS_BASIC;
            placed++;
        }
        if (jaos_set_basis(m, cs, rs) != JAOS_OK) {
            fprintf(stderr, "HOSTILE inst=%s s=%d error=set\n", name, s);
            continue;
        }
        if (jaos_solve(m) != JAOS_OK) {
            fprintf(stderr, "HOSTILE inst=%s s=%d error=solve\n", name, s);
            continue;
        }
        const int st = (int)jaos_status_of(m);
        double obj = 0.0;
        int wrong = 0, chk = -1;
        if (st == (int)JAOS_SOLVE_OPTIMAL) {
            (void)jaos_objective(m, &obj);
            const double denom = fabs(ref) > 1.0 ? fabs(ref) : 1.0;
            wrong = fabs(obj - ref) / denom > 1e-6;
            if (jaos_solution(m, x, NULL, y, NULL) == JAOS_OK) {
                jaos_check_report r;
                if (jaos_check_solution(m, x, y, 1e-6, &r) == JAOS_OK)
                    chk = r.primal_feasible && r.dual_feasible;
            }
        }
        fprintf(stderr,
                "HOSTILE inst=%s s=%d status=%d wrong=%d chk=%d "
                "obj=%.17g ref=%.17g iters=%lld\n",
                name, s, st, wrong, chk, obj, ref,
                (long long)jaos_iterations(m));
    }
    free(cs); free(rs); free(x); free(y);
    jaos_model_free(m);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <shifts> <name=path>...\n", argv[0]);
        return 2;
    }
    const int shifts = atoi(argv[1]);
    for (int a = 2; a < argc; a++) {
        char *eq = NULL;
        for (char *p = argv[a]; *p; p++)
            if (*p == '=') { eq = p; break; }
        if (!eq)
            continue;
        *eq = '\0';
        (void)run_instance(eq + 1, argv[a], shifts);
    }
    return 0;
}
