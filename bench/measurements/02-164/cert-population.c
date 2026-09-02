/* D254's population arm: every instance named on the command line must
 * answer INFEASIBLE, publish a certificate, and have the model's own
 * checker certify it. A path prefixed "feasible:" is the control arm —
 * it must answer OPTIMAL and refuse a certificate, which is what proves
 * this driver can tell the two apart at all. Exit 0 only when both arms
 * hold in full. Built against the reference library (-DJAOS_NO_PRESOLVE)
 * so the simplex, not presolve, proves every infeasibility. */
#include "jaos.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int bad = 0, ncert = 0, ncontrol = 0;

    for (int k = 1; k < argc; k++) {
        const char *path = argv[k];
        int control = 0;
        if (strncmp(path, "feasible:", 9) == 0) {
            path += 9;
            control = 1;
        }

        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK)
            return 9;
        if (jaos_read_mps(m, path) != JAOS_OK) {
            printf("%-14s READ FAIL: %s\n", path, jaos_model_error(m));
            return 9;
        }
        if (jaos_solve(m) != JAOS_OK) {
            printf("%-14s SOLVE FAIL\n", path);
            return 9;
        }

        double *y = malloc((size_t)(jaos_num_row(m) > 0 ? jaos_num_row(m)
                                                        : 1) * sizeof *y);
        if (y == nullptr)
            return 9;

        if (control) {
            if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
                printf("%-14s CONTROL BROKE: not optimal\n", path);
                bad++;
            } else if (jaos_certificate(m, y) == JAOS_OK) {
                printf("%-14s CONTROL BROKE: a feasible solve handed "
                       "out a certificate\n", path);
                bad++;
            } else {
                printf("%-14s control: optimal, no certificate\n", path);
                ncontrol++;
            }
        } else if (jaos_status_of(m) != JAOS_SOLVE_INFEASIBLE) {
            printf("%-14s NOT INFEASIBLE (status %d)\n", path,
                   (int)jaos_status_of(m));
            bad++;
        } else if (jaos_certificate(m, y) != JAOS_OK) {
            printf("%-14s NO CERTIFICATE\n", path);
            bad++;
        } else {
            jaos_certificate_report rep;
            if (jaos_check_certificate(m, y, 1e-7, &rep) != JAOS_OK) {
                printf("%-14s CHECK CALL FAILED\n", path);
                bad++;
            } else if (!rep.certified) {
                printf("%-14s NOT CERTIFIED gap=%.6g sup=%.6g inf=%.6g\n",
                       path, rep.gap, rep.sup_columns, rep.inf_rows);
                /* Which side died: scan the row terms this driver can
                 * reach through the public API. y_i > 0 needs a finite
                 * row lower side, y_i < 0 a finite upper. Column deaths
                 * are invisible from here and print nothing. */
                for (int64_t i = 0; i < jaos_num_row(m); i++) {
                    if (y[i] == 0.0)
                        continue;
                    double lo, up;
                    if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK)
                        break;
                    const double side = y[i] > 0.0 ? lo : up;
                    if (!isfinite(side))
                        printf("    row %lld: y=%.6g needs the %s side, "
                               "absent\n", (long long)i, y[i],
                               y[i] > 0.0 ? "lower" : "upper");
                }
                bad++;
            } else {
                printf("%-14s certified gap=%.6g sup=%.6g inf=%.6g\n",
                       path, rep.gap, rep.sup_columns, rep.inf_rows);
                ncert++;
            }
        }
        free(y);
        jaos_model_free(m);
    }

    printf("certified %d, controls %d, bad %d\n", ncert, ncontrol, bad);
    if (ncert == 0 || ncontrol == 0) {
        printf("an arm never fired: this run measured nothing\n");
        return 2;
    }
    return bad != 0;
}
