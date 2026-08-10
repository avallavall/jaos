/* JAOS as one competitor among several: solve one instance, report what it
 * cost in the units a comparison needs.
 *
 * Separate from bench/run.c on purpose. That tool judges JAOS against
 * external ground truth and writes the record the gate lives on, and no
 * wall-clock number belongs anywhere near it (D17). This one exists to be
 * put beside HiGHS and SoPlex, where seconds are the whole question, and
 * everything it prints goes into a different file that is never a baseline.
 *
 * Times the solve and nothing else. Reading the model is excluded for the
 * same reason the work counter excludes it: a caller who loads once and
 * solves repeatedly should not see the load in every figure, and the
 * competitors' own reported solve times exclude it too.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* `-std=c23` is strict ISO, which hides clock_gettime; the library's own
 * sources ask for it the same way. */
#define _POSIX_C_SOURCE 200809L

#include "jaos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_seconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* One line, tab separated, for a harness to collect. `status` is the word
 * bench/run.c uses, so the two records can be read side by side. */
static void emit(const char *name, const char *status, double secs,
                 double obj, long long iters, long long work)
{
    printf("jaos\t%s\t%s\t%.6f\t%.17g\t%lld\t%lld\n",
           name, status, secs, obj, iters, work);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: jaos_time <name> <path.mps> [repeats]\n");
        return 2;
    }
    const char *name = argv[1];
    const char *path = argv[2];
    int repeats = argc > 3 ? atoi(argv[3]) : 1;
    if (repeats < 1)
        repeats = 1;

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) {
        emit(name, "ALLOC-FAILED", 0.0, 0.0, 0, 0);
        return 2;
    }
    if (jaos_read_mps(m, path) != JAOS_OK) {
        emit(name, "READ-FAILED", 0.0, 0.0, 0, 0);
        jaos_model_free(m);
        return 2;
    }

    /* The minimum of several runs, not the mean. The mean measures how busy
     * the machine was; the minimum is the closest this can get to the run
     * with no interference in it, which is the quantity a comparison wants.
     *
     * **Every run clears the basis first, and without that this program
     * measures the wrong thing entirely.** Since D68 a solve that reaches an
     * optimum leaves its basis on the model and the next solve resumes from
     * it, so the second repeat here finished in one iteration and the
     * minimum-of-N picked it: JAOS appeared to solve `25fv47` in 0.0015s and
     * 0 iterations against a true 0.49s and 9459. Nothing failed and no
     * number looked impossible on its own — only the ratio did.
     *
     * The gate already learned this and clears the basis between its two
     * determinism solves for the same reason (SPECS.md §8). This program did
     * not, so warm re-solve silently redefined what it was timing on the day
     * it landed. Clearing is two frees and sits outside the timed region. */
    double best = -1.0;
    jaos_solve_status ss = JAOS_SOLVE_NOT_RUN;
    double obj = 0.0;
    long long iters = 0, work = 0;

    for (int i = 0; i < repeats; i++) {
        jaos_clear_basis(m);
        double t0 = now_seconds();
        jaos_status st = jaos_solve(m);
        double dt = now_seconds() - t0;
        if (st != JAOS_OK) {
            emit(name, "SOLVE-ERROR", dt, 0.0, 0, 0);
            jaos_model_free(m);
            return 2;
        }
        if (best < 0.0 || dt < best)
            best = dt;
        ss = jaos_status_of(m);
        (void)jaos_objective(m, &obj);

        /* The instrument checks itself, because the failure above was silent.
         * Two cold solves of one model are bit-identical by D8, so the
         * repeats must agree on iterations and work to the digit. If they do
         * not, something is being carried between them and every second in
         * this record is timing a different thing from the one it names —
         * which is exactly what happened, and what nothing here noticed. */
        const long long it = (long long)jaos_iterations(m);
        const long long wk = (long long)jaos_work_units(m);
        if (i > 0 && (it != iters || wk != work)) {
            emit(name, "REPEATS-DISAGREE", dt, obj, it, wk);
            jaos_model_free(m);
            return 2;
        }
        iters = it;
        work = wk;
    }

    emit(name, jaos_solve_status_str(ss), best, obj, iters, work);
    jaos_model_free(m);
    return 0;
}
