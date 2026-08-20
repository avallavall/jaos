/* What tightening the dual tolerance costs, over a whole set, without a
 * rebuild.
 *
 * `DUAL_TOL` is one of the two constants a caller owns (D64), so
 * `jaos_set_dual_tolerance` reaches it at run time and the sweep needs one
 * binary rather than one per setting. That removes the trap sweeping a
 * constant here has hit before — `make` does not track a change in
 * EXTRA_CFLAGS, so a sweep can measure one binary N times and report a flat
 * line (D154, and the sweeping-a-constant note).
 *
 * The control is the first setting: passing 0 means the built-in default,
 * and passing 1e-7 names the same number explicitly. The two must agree on
 * every figure of every instance, or the setter is not reaching what the
 * solve reads and no other row means anything.
 *
 * One line per (setting, instance). Work units are the cost, the digest says
 * whether anything moved at all, and the gap against the manifest optimum is
 * what the sweep is for.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "jaos.h"

/* FNV-1a over the raw bytes, the same shape bench/run.c uses: two solves of
 * one model must produce identical bits, so the digest is of the bytes and
 * not of anything rounded on the way. */
static uint64_t fnv1a(const void *p, size_t n, uint64_t h)
{
    const unsigned char *b = p;
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 1099511628211ULL;
    }
    return h;
}

int main(int argc, char **argv)
{
    int first = 1;
    /* 0.0 first: the built-in default, and 1e-7 after it names the same
     * number. Loosening is in the list because a sweep with one direction
     * cannot say whether the shipped value sits on a cliff. A set whose
     * instances cost minutes each gets a shorter list, named on the command
     * line — `--settings 0,1e-9`. */
    double settings[16] = { 0.0, 1e-7, 1e-6, 1e-8, 1e-9, 1e-10, 1e-11 };
    unsigned nset = 7;
    if (argc > 2 && strcmp(argv[1], "--settings") == 0) {
        nset = 0;
        for (char *t = strtok(argv[2], ","); t != NULL && nset < 16;
             t = strtok(NULL, ","))
            settings[nset++] = strtod(t, NULL);
        first = 3;
    }

    if (argc < first + 2) {
        fprintf(stderr, "usage: %s [--settings a,b,c] <manifest> "
                        "<instance.mps ...>\n", argv[0]);
        return 1;
    }
    const char *manifest = argv[first];

    printf("# dual tolerance swept through jaos_set_dual_tolerance; 0 is the\n");
    printf("# built-in default and 1e-7 names the same number, so the two\n");
    printf("# rows must agree everywhere or nothing below means anything.\n");
    printf("# Nine whitespace-separated fields on every row, whatever the\n");
    printf("# status: a column that moves with the verdict is a record only\n");
    printf("# its author can read.\n");
    printf("# dtol name status obj gap iters work digest checker\n");

    for (unsigned s = 0; s < nset; s++) {
        for (int i = first + 1; i < argc; i++) {
            char name[128];
            const char *b = strrchr(argv[i], '/');
            snprintf(name, sizeof name, "%s", b ? b + 1 : argv[i]);
            char *dot = strrchr(name, '.');
            if (dot != NULL && strcmp(dot, ".mps") == 0) *dot = '\0';

            double ref = NAN;
            FILE *f = fopen(manifest, "r");
            if (f != NULL) {
                char line[1024];
                while (fgets(line, sizeof line, f) != NULL) {
                    char n[128], sha[128], src[64];
                    long long nr, nc;
                    double v;
                    if (sscanf(line, "%127s %127s %lld %lld %lf %63s",
                               n, sha, &nr, &nc, &v, src) != 6) continue;
                    if (strcmp(n, name) == 0) { ref = v; break; }
                }
                fclose(f);
            }

            jaos_model *m = NULL;
            if (jaos_model_new(&m) != JAOS_OK) return 2;
            if (jaos_read_mps(m, argv[i]) != JAOS_OK) { jaos_model_free(m); continue; }
            if (settings[s] > 0.0 &&
                jaos_set_dual_tolerance(m, settings[s]) != JAOS_OK) return 2;

            const jaos_status rc = jaos_solve(m);
            const jaos_solve_status ss = jaos_status_of(m);
            double obj = NAN;
            (void)jaos_objective(m, &obj);

            const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
            double *x = calloc((size_t)nc, sizeof *x);
            double *y = calloc((size_t)nr, sizeof *y);
            uint64_t dig = 0;
            const char *chk = "-";
            if (x != NULL && y != NULL && ss == JAOS_SOLVE_OPTIMAL) {
                (void)jaos_solution(m, x, NULL, y, NULL);
                dig = fnv1a(y, (size_t)nr * sizeof *y,
                            fnv1a(x, (size_t)nc * sizeof *x,
                                  14695981039346656037ULL));
                jaos_check_report rep = {0};
                if (jaos_check_solution(m, x, y, 1e-6, &rep) == JAOS_OK)
                    chk = (rep.primal_feasible && rep.dual_feasible)
                              ? "ok" : "REJECTED";
            }

            /* `jaos_solve_status_str` returns "numerical error", with a
             * space in it, and a status that is two fields on the failing
             * rows and one everywhere else shifts every column after it.
             * Every reader of this file would have to know that. */
            char status[32];
            snprintf(status, sizeof status, "%s",
                     rc == JAOS_OK ? jaos_solve_status_str(ss) : "call_failed");
            for (char *p = status; *p != '\0'; p++)
                if (*p == ' ') *p = '_';

            printf("%.0e %-12s %-16s %.17g %.6g %lld %lld %016llx %s\n",
                   settings[s], name, status,
                   obj, obj - ref,
                   (long long)jaos_iterations(m),
                   (long long)jaos_work_units(m),
                   (unsigned long long)dig, chk);
            fflush(stdout);

            free(x); free(y);
            jaos_model_free(m);
        }
    }
    return 0;
}
