/* D264's population arm: every instance named on the command line must
 * answer INFEASIBLE, hand out an IIS, and have that IIS pass the oracle
 * tests/test_iis.c uses -- the named sides kept alone re-solve
 * INFEASIBLE, and dropping any one of them re-solves OPTIMAL. A second
 * call must return the same sides and the same counts. One line per
 * instance: the size, where the candidates came from and how many, the
 * members split into row sides and column sides, the re-solves the
 * filter ran and their work, and the oracle's word. Exit 0 only when
 * every instance holds. Built with -Isrc for the matrix, which the public
 * API does not read back. */
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *base(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/* A copy of m with only the named sides kept, no objective, and one side
 * less when drop_row/drop_col and drop_side name one. */
static jaos_model *subsystem(const jaos_model *m, const jaos_iis_side *rs,
                             const jaos_iis_side *cs, int64_t drop_row,
                             int64_t drop_col, jaos_iis_side drop_side)
{
    const int64_t nr = m->num_row, nc = m->num_col;
    double *zero = calloc((size_t)(nc + 1), sizeof *zero);
    double *rl = malloc((size_t)(nr + 1) * sizeof *rl);
    double *ru = malloc((size_t)(nr + 1) * sizeof *ru);
    double *cl = malloc((size_t)(nc + 1) * sizeof *cl);
    double *cu = malloc((size_t)(nc + 1) * sizeof *cu);
    if (!zero || !rl || !ru || !cl || !cu)
        exit(9);
    for (int64_t i = 0; i < nr; i++) {
        int s = rs[i];
        if (i == drop_row)
            s &= ~drop_side;
        rl[i] = s & JAOS_IIS_LOWER ? m->row_lower[i] : -INFINITY;
        ru[i] = s & JAOS_IIS_UPPER ? m->row_upper[i] : INFINITY;
    }
    for (int64_t j = 0; j < nc; j++) {
        int s = cs[j];
        if (j == drop_col)
            s &= ~drop_side;
        cl[j] = s & JAOS_IIS_LOWER ? m->col_lower[j] : -INFINITY;
        cu[j] = s & JAOS_IIS_UPPER ? m->col_upper[j] : INFINITY;
    }
    jaos_model *s = nullptr;
    if (jaos_model_new(&s) != JAOS_OK ||
        jaos_load_lp(s, nc, nr, JAOS_MINIMIZE, 0.0, zero, cl, cu, rl, ru,
                     m->num_nz, m->a_start, m->a_index, m->a_value) != JAOS_OK)
        exit(9);
    free(zero);
    free(rl);
    free(ru);
    free(cl);
    free(cu);
    return s;
}

static jaos_solve_status verdict(jaos_model *s)
{
    if (jaos_solve(s) != JAOS_OK)
        exit(9);
    const jaos_solve_status st = jaos_status_of(s);
    jaos_model_free(s);
    return st;
}

/* 0 when the oracle holds; otherwise a small code naming what failed. */
static int oracle(const jaos_model *m, const jaos_iis_side *rs,
                  const jaos_iis_side *cs, int64_t *oracle_solves,
                  int64_t *at, jaos_iis_side *at_side)
{
    static const jaos_iis_side sides[2] = {JAOS_IIS_LOWER, JAOS_IIS_UPPER};
    *oracle_solves = 1;
    if (verdict(subsystem(m, rs, cs, -1, -1, JAOS_IIS_NONE)) !=
        JAOS_SOLVE_INFEASIBLE)
        return 1;
    for (int64_t i = 0; i < m->num_row; i++)
        for (int k = 0; k < 2; k++) {
            if (!(rs[i] & sides[k]))
                continue;
            (*oracle_solves)++;
            if (verdict(subsystem(m, rs, cs, i, -1, sides[k])) !=
                JAOS_SOLVE_OPTIMAL) {
                *at = i;
                *at_side = sides[k];
                return 2;
            }
        }
    for (int64_t j = 0; j < m->num_col; j++)
        for (int k = 0; k < 2; k++) {
            if (!(cs[j] & sides[k]))
                continue;
            (*oracle_solves)++;
            if (verdict(subsystem(m, rs, cs, -1, j, sides[k])) !=
                JAOS_SOLVE_OPTIMAL) {
                *at = j;
                *at_side = sides[k];
                return 3;
            }
        }
    return 0;
}

int main(int argc, char **argv)
{
    int bad = 0;
    for (int k = 1; k < argc; k++) {
        const char *path = argv[k];
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK)
            return 9;
        if (jaos_read_mps(m, path) != JAOS_OK) {
            printf("%-10s READ FAIL: %s\n", base(path), jaos_model_error(m));
            return 9;
        }
        if (jaos_solve(m) != JAOS_OK) {
            printf("%-10s SOLVE FAIL\n", base(path));
            return 9;
        }
        if (jaos_status_of(m) != JAOS_SOLVE_INFEASIBLE) {
            printf("%-10s NOT INFEASIBLE (%s)\n", base(path),
                   jaos_solve_status_str(jaos_status_of(m)));
            bad++;
            jaos_model_free(m);
            continue;
        }
        const int64_t nr = m->num_row, nc = m->num_col;
        jaos_iis_side *rs = calloc((size_t)(nr + 1), sizeof *rs);
        jaos_iis_side *cs = calloc((size_t)(nc + 1), sizeof *cs);
        jaos_iis_side *rs2 = calloc((size_t)(nr + 1), sizeof *rs2);
        jaos_iis_side *cs2 = calloc((size_t)(nc + 1), sizeof *cs2);
        if (!rs || !cs || !rs2 || !cs2)
            return 9;
        jaos_iis_report rep, rep2;
        const jaos_status rc = jaos_iis(m, rs, cs, &rep);
        if (rc != JAOS_OK) {
            printf("%-10s %5lld x %-5lld IIS FAILED (%s): %s\n", base(path),
                   (long long)nr, (long long)nc, jaos_status_str(rc),
                   jaos_model_error(m));
            bad++;
            jaos_model_free(m);
            free(rs); free(cs); free(rs2); free(cs2);
            continue;
        }
        int64_t rows = 0, cols = 0;
        for (int64_t i = 0; i < nr; i++)
            rows += (rs[i] & 1) + ((rs[i] >> 1) & 1);
        for (int64_t j = 0; j < nc; j++)
            cols += (cs[j] & 1) + ((cs[j] >> 1) & 1);

        const bool same =
            jaos_iis(m, rs2, cs2, &rep2) == JAOS_OK &&
            memcmp(rs, rs2, (size_t)nr * sizeof *rs) == 0 &&
            memcmp(cs, cs2, (size_t)nc * sizeof *cs) == 0 &&
            rep.solves == rep2.solves && rep.work_units == rep2.work_units;

        int64_t osolves = 0, at = -1;
        jaos_iis_side at_side = JAOS_IIS_NONE;
        const int o = oracle(m, rs, cs, &osolves, &at, &at_side);
        char word[96];
        if (o == 0)
            snprintf(word, sizeof word, "irreducible");
        else if (o == 1)
            snprintf(word, sizeof word, "NOT INFEASIBLE ALONE");
        else
            snprintf(word, sizeof word,
                     "%s %lld %s NOT NEEDED by a cold re-solve",
                     o == 2 ? "row" : "column", (long long)at,
                     at_side == JAOS_IIS_LOWER ? "lower" : "upper");
        printf("%-10s %5lld x %-5lld cand=%-5lld %-11s members=%-4lld "
               "rows=%-4lld cols=%-4lld solves=%-5lld work=%-12lld %s%s\n",
               base(path), (long long)nr, (long long)nc,
               (long long)rep.candidates,
               rep.from_certificate ? "certificate" : "every-side",
               (long long)rep.members, (long long)rows, (long long)cols,
               (long long)rep.solves, (long long)rep.work_units, word,
               same ? "" : "  NOT REPRODUCIBLE");
        if (o != 0 || !same || rep.members != rows + cols)
            bad++;
        jaos_model_free(m);
        free(rs); free(cs); free(rs2); free(cs2);
    }
    return bad == 0 ? 0 : 1;
}
