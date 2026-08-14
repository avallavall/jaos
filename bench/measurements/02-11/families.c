/* Which family fires on an instance, and how many times.
 *
 * The record's `presolve=` field says how much came off; it does not say what
 * took it. On the three grow models exactly 20 columns and 20 nonzeros come
 * off each, so every removed column has degree 1 -- but four families can
 * remove such a column and they do different things to the row it leaves
 * behind. JM_PS_SINGLETON_COL relaxes that row and freezes it;
 * JM_PS_FIXED_COL does not. Which one it is decides what the simplex then
 * sees, so it is the first thing to establish.
 *
 * Calls jm_presolve_run directly rather than jaos_solve, so nothing here is
 * affected by scaling or by the solve itself. It reads; it changes nothing.
 */
#include <stdio.h>
#include <stdlib.h>
#include "jaos.h"
#include "jaos_internal.h"

int main(int argc, char **argv)
{
    printf("%-10s %6s %6s %6s | %5s %5s %5s %5s %5s %5s %5s %5s\n",
           "instance", "rows", "cols", "nz",
           "emptR", "emptC", "sglR", "sglC", "freeC", "fixC", "forcR", "redR");
    for (int a = 1; a < argc; a++) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[a]) != JAOS_OK) {
            printf("%s: read failed\n", argv[a]);
            jaos_model_free(m);
            continue;
        }
        jm_presolve p;
        jm_presolve_init(&p);
        if (jm_presolve_run(m, &p, NULL) != JAOS_OK) {
            printf("%s: presolve failed\n", argv[a]);
            jm_presolve_free(&p);
            jaos_model_free(m);
            continue;
        }
        const char *name = argv[a];
        const char *slash = name;
        for (const char *s = name; *s; s++) if (*s == '/') slash = s + 1;
        name = slash;

        const jaos_model *red = (p.outcome == JM_PRESOLVE_REDUCED ||
                                 p.outcome == JM_PRESOLVE_SOLVED)
                                ? &p.reduced : m;
        printf("%-10s %6lld %6lld %6lld | %5lld %5lld %5lld %5lld %5lld %5lld %5lld %5lld\n",
               name,
               (long long)red->num_row, (long long)red->num_col,
               (long long)(red->num_col > 0 ? red->a_start[red->num_col] : 0),
               (long long)p.counts.empty_row, (long long)p.counts.empty_col,
               (long long)p.counts.singleton_row, (long long)p.counts.singleton_col,
               (long long)p.counts.free_col_singleton, (long long)p.counts.fixed_col,
               (long long)p.counts.forcing_row, (long long)p.counts.redundant_row);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    return 0;
}
