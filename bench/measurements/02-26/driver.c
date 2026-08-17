/* Runs presolve alone on each MPS named on the command line, and prints the
 * family counts beside 02-26's decline report. No simplex: the question is
 * what presolve decided, and fome13 is 48568 x 97840.
 *
 * jm_presolve_run is called directly, the way tests/test_presolve.c already
 * does -- jm_presolve is solve-local and never escapes a public call (D-08).
 *
 * One line per instance, machine-readable, so run-decline.sh can calibrate
 * against 02-10's and D106's committed figures before any new number is read.
 */
#include <stdio.h>
#include "jaos.h"
#include "jaos_internal.h"

extern char diag_name[256];

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK)
            return 2;
        if (jaos_read_mps(m, argv[a]) != JAOS_OK) {
            printf("READFAIL %s\n", argv[a]);
            jaos_model_free(m);
            continue;
        }
        const char *nm = argv[a];
        for (const char *s = argv[a]; *s; s++)
            if (*s == '/')
                nm = s + 1;
        snprintf(diag_name, sizeof diag_name, "%s", nm);

        jm_presolve p;
        jm_presolve_init(&p);
        const jaos_status st = jm_presolve_run(m, &p, nullptr);
        printf("PRESOLVE %s rows=%lld cols=%lld status=%d outcome=%d "
               "rounds=%lld fixed=%lld emptyrow=%lld emptycol=%lld "
               "singrow=%lld singcol=%lld freesing=%lld forcing=%lld "
               "redundant=%lld impfree=%lld\n",
               nm, (long long)m->num_row, (long long)m->num_col,
               (int)st, (int)p.outcome,
               (long long)p.counts.rounds,
               (long long)p.counts.fixed_col,
               (long long)p.counts.empty_row,
               (long long)p.counts.empty_col,
               (long long)p.counts.singleton_row,
               (long long)p.counts.singleton_col,
               (long long)p.counts.free_col_singleton,
               (long long)p.counts.forcing_row,
               (long long)p.counts.redundant_row,
               (long long)p.counts.implied_free_col);
        fflush(stdout);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    return 0;
}
