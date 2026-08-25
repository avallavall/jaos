/* Does the published POINT move, or only the DUALS?
 *
 * `bench/run.c` hashes x and y into one digest, so a record that shows two
 * settings publishing the same objective and different digests cannot say
 * which half moved. `pilot87` does exactly that at REFACTOR_EVERY 8 and 256:
 * obj=301.71035883543192 both times, digests 1fc1800d2e00788c and
 * 7c4d40db1433ac5e (D180, bench/measurements/02-92/).
 *
 * The standing debt this is for: "pilot87's suboptimality bound is not
 * understood — gap_positive moves 0.0068 to 26.7 across D92's variants while
 * every answer is inside tolerance." gap_positive is built from the duals, so
 * if only y moves the bound moving is a property of a non-unique dual
 * solution and not of a wandering answer.
 *
 * Prints x and y separately, to a file, so two builds can be diffed exactly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "jaos.h"

static uint64_t digest(const double *v, int64_t n, uint64_t h)
{
    for (int64_t i = 0; i < n; i++) {
        uint64_t b;
        memcpy(&b, &v[i], sizeof b);
        for (int k = 0; k < 8; k++) {
            h ^= (b >> (k * 8)) & 0xffu;
            h *= 1099511628211u;
        }
    }
    return h;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s model.mps out\n", argv[0]); return 2; }
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) { jaos_model_free(m); return 2; }
    if (jaos_solve(m) != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
        fprintf(stderr, "not optimal\n"); jaos_model_free(m); return 2;
    }
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)nc, sizeof *x);
    double *y = calloc((size_t)nr, sizeof *y);
    double *d = calloc((size_t)nc, sizeof *d);
    jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
    if (!x || !y || !d || !cs || !rs ||
        jaos_solution(m, x, NULL, y, d) != JAOS_OK ||
        jaos_basis(m, cs, rs) != JAOS_OK) { jaos_model_free(m); return 2; }
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    jaos_check_report rep;
    memset(&rep, 0, sizeof rep);
    (void)jaos_check_solution(m, x, y, 1e-7, &rep);

    printf("obj      %.17g\n", obj);
    printf("digest-x %016" PRIx64 "\n", digest(x, nc, 1469598103934665603u));
    printf("digest-y %016" PRIx64 "\n", digest(y, nr, 1469598103934665603u));
    printf("digest-d %016" PRIx64 "\n", digest(d, nc, 1469598103934665603u));
    printf("gap+     %.6g\n", rep.gap_positive);
    printf("rsub     %.6g\n", rep.relative_suboptimality);
    printf("rays     %lld\n", (long long)rep.unquantified_rays);
    int64_t nb = 0;
    for (int64_t j = 0; j < nc; j++) nb += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < nr; i++) nb += rs[i] == JAOS_BASIS_BASIC;
    printf("basic    %lld of nrow %lld\n", (long long)nb, (long long)nr);

    FILE *f = fopen(argv[2], "w");
    if (f == NULL) { jaos_model_free(m); return 2; }
    /* The cost goes in the dump too. 987 columns moving while c'x holds to
     * the last bit is a claim that needs the cost beside it: either those
     * columns cost nothing, or the changes cancel exactly, and only one of
     * those is an ordinary degenerate face. */
    for (int64_t j = 0; j < nc; j++) {
        double c = 0.0;
        (void)jaos_col_cost(m, j, &c);
        fprintf(f, "x %lld %a %d %a\n", (long long)j, x[j], (int)cs[j], c);
    }
    for (int64_t i = 0; i < nr; i++) fprintf(f, "y %lld %a %d\n", (long long)i, y[i], (int)rs[i]);
    fclose(f);
    free(x); free(y); free(d); free(cs); free(rs);
    jaos_model_free(m);
    return 0;
}
