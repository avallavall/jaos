/* D274. How much of the gate `jaos_verify` actually proves.
 *
 * D273 said the bound admits 36 of the 110 instances that have a basis to
 * read, and said in the same breath that Hadamard is an upper bound and a
 * loose one, so the count is a floor. This runs the verifier and reports
 * what it did: proved, refused, or broken, with the bound it read, the
 * blocks it found, the memory it held and the products it formed.
 *
 * The seconds are here because nothing else can say what the elimination
 * costs -- work units belong to the solve and this is not billed to them --
 * and they are reported per instance, never summed. Each line separates the
 * solve from the proof, so the two are never confused the way they were when
 * D272's instrument looked stuck and was only solving `ken-18`.
 *
 * A caller of this file gets one line per instance and no verdict. The
 * verdict is D274's and it is written down there.
 *
 * Not a gate tool. Built and run by run-proofs.sh.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _POSIX_C_SOURCE 200809L
#include "jaos.h"
#include "jaos_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

static const char *verdict(jaos_proof p)
{
    switch (p) {
    case JAOS_PROOF_OPTIMAL: return "PROVED";
    case JAOS_PROOF_BROKEN:  return "BROKEN";
    default:                 return "refused";
    }
}

int main(int argc, char **argv)
{
    printf("# what jaos_verify does on every gate basis (D274)\n");
    printf("# bound/cap: the bits the proof needs and the bits there are,\n");
    printf("#            both read before any of the work is attempted\n");
    printf("# blocks:    strongly connected components of the basis\n");
    printf("# largest:   rows in the biggest one\n");
    printf("# held:      the biggest block table allocated, in MiB\n");
    printf("# terms:     integer products formed\n");
    printf("# solve/proof: seconds, per instance, never summed\n\n");
    printf("%-14s %8s %10s %8s %8s %9s %12s %8s %8s %s\n",
           "instance", "rows", "bound", "blocks", "largest", "held_MiB",
           "terms", "solve_s", "proof_s", "verdict");

    int64_t seen = 0, proved = 0, refused = 0, broken = 0;

    for (int a = 1; a < argc; a++) {
        const char *path = argv[a];
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;

        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) continue;
        if (jaos_read_mps(m, path) != JAOS_OK) { jaos_model_free(m); continue; }

        const double t0 = now_s();
        if (jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }
        const double t_solve = now_s() - t0;

        const double t1 = now_s();
        jaos_verify_report r;
        const jaos_status rc = jaos_verify(m, &r);
        const double t_proof = now_s() - t1;

        seen++;
        if (rc != JAOS_OK) {
            printf("%-14s %8lld %10s %8s %8s %9s %12s %8.2f %8.2f %s\n",
                   name, (long long)jaos_num_row(m), "-", "-", "-", "-", "-",
                   t_solve, t_proof, jaos_model_error(m));
            fflush(stdout);
            jaos_model_free(m);
            continue;
        }
        if (r.status == JAOS_PROOF_OPTIMAL) proved++;
        else if (r.status == JAOS_PROOF_BROKEN) broken++;
        else refused++;

        printf("%-14s %8lld %10.1f %8lld %8lld %9.1f %12lld %8.2f %8.2f %s\n",
               name, (long long)jaos_num_row(m), r.bound_bits,
               (long long)r.blocks, (long long)r.largest_block,
               (double)r.bytes_held / (1024.0 * 1024.0),
               (long long)r.terms, t_solve, t_proof, verdict(r.status));
        if (r.status == JAOS_PROOF_BROKEN) {
            const char *w = r.stage == JAOS_PROOF_STAGE_PRIMAL ? "primal"
                          : r.stage == JAOS_PROOF_STAGE_DUAL   ? "dual"
                          : "rank";
            printf("    %s broken in the %s at row %lld col %lld by %.3e\n",
                   name, w, (long long)r.at_row, (long long)r.at_col,
                   r.violation);
        }
        fflush(stdout);
        jaos_model_free(m);
    }

    printf("\n%lld bases\n", (long long)seen);
    printf("proved %lld;  refused %lld;  broken %lld\n",
           (long long)proved, (long long)refused, (long long)broken);
    return 0;
}
