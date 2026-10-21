/* The reading behind 02-405: a model's structures under random edits,
 * against a mirror the harness keeps by the rules of docs/api.md.
 *
 * Each model starts as a small LP and takes EDITS edits, each one of:
 * delete one or two columns, delete a row, add a column, add a row, add an
 * SOS set, delete a cone, add a cone, set or clear a row's indicator, set a
 * row's quadratic part, replace the objective's Q, set one diagonal entry
 * of Q, mark a column integer or semi-continuous, set a coefficient, a
 * cost or a box. The mirror applies the same edit: a deleted column leaves
 * its SOS sets (an empty set goes), and a column that switches an
 * indicator or belongs to a cone cannot be deleted. Properties:
 *   P1 each call succeeds or fails as the mirror says it should
 *   P2 after each edit every getter reads what the mirror holds: sizes,
 *      costs, boxes, marks, coefficients, sides, indicators, the rows'
 *      quadratic parts, Q, the SOS sets and the cones
 *   P3 every tenth edit, jaos_model_copy's model reads the same
 *   P4 every tenth edit, the model written in each of the six formats
 *      (MPS, LP, .nl, QPLIB, CBF, OSiL) and read back reads the same,
 *      unless that format's writer refuses it; for .nl, in the order its
 *      documented rule gives the columns (continuous, binary, general
 *      integer); for CBF, which turns bounds into rows by its documented
 *      rule, the columns, their costs and marks, and the cones
 *
 * Usage: structedit RUNS SEED EDITS TMPDIR
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng_state;

static uint64_t rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static int64_t ri(int64_t lo, int64_t hi)
{
    return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1));
}

#define N 20
#define S 6

typedef struct {
    int64_t nc, nr;
    double cost[N], lo[N], up[N];
    bool isint[N], semi[N];
    double a[N][N], rl[N], ru[N];
    double qo[N][N], rq[N][N][N];
    int64_t indc[N];
    int indv[N];
    int nsos, sost[S];
    int64_t sosn[S], sosc[S][N];
    double sosw[S][N];
    int ncone;
    jaos_cone_type cot[S];
    int64_t con[S], coc[S][N];
} mirror;

static mirror M;
static int64_t refused_writes, refused_sets;

static jaos_status load(jaos_model *m)
{
    int64_t ap[N + 1], ai[N * N];
    double av[N * N];
    int64_t nz = 0;
    for (int64_t j = 0; j < M.nc; j++) {
        ap[j] = nz;
        for (int64_t i = 0; i < M.nr; i++)
            if (M.a[i][j] != 0.0) {
                ai[nz] = i;
                av[nz++] = M.a[i][j];
            }
    }
    ap[M.nc] = nz;
    return jaos_load_lp(m, M.nc, M.nr, JAOS_MINIMIZE, 0.0, M.cost, M.lo,
                        M.up, M.rl, M.ru, nz, ap, ai, av);
}

static void start(void)
{
    memset(&M, 0, sizeof M);
    M.nc = ri(4, 10);
    M.nr = ri(3, 8);
    for (int64_t j = 0; j < M.nc; j++) {
        M.cost[j] = (double)ri(-9, 9);
        M.lo[j] = 0.0;
        M.up[j] = (double)ri(1, 10);
    }
    for (int64_t i = 0; i < M.nr; i++) {
        M.indc[i] = -1;
        M.rl[i] = -INFINITY;
        M.ru[i] = (double)ri(0, 20);
        for (int64_t j = 0; j < M.nc; j++)
            if (ri(0, 9) < 4)
                M.a[i][j] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
    }
}

static bool protected_col(int64_t j)
{
    for (int k = 0; k < M.ncone; k++)
        for (int64_t t = 0; t < M.con[k]; t++)
            if (M.coc[k][t] == j)
                return true;
    for (int64_t i = 0; i < M.nr; i++)
        if (M.indc[i] == j)
            return true;
    return false;
}

static void mirror_delete_col(int64_t j)
{
    for (int64_t c = j; c + 1 < M.nc; c++) {
        M.cost[c] = M.cost[c + 1];
        M.lo[c] = M.lo[c + 1];
        M.up[c] = M.up[c + 1];
        M.isint[c] = M.isint[c + 1];
        M.semi[c] = M.semi[c + 1];
        for (int64_t i = 0; i < M.nr; i++)
            M.a[i][c] = M.a[i][c + 1];
    }
    for (int64_t p = 0; p < M.nc; p++)
        for (int64_t c = j; c + 1 < M.nc; c++)
            M.qo[p][c] = M.qo[p][c + 1];
    for (int64_t c = j; c + 1 < M.nc; c++)
        for (int64_t p = 0; p < M.nc; p++)
            M.qo[c][p] = M.qo[c + 1][p];
    for (int64_t i = 0; i < M.nr; i++) {
        for (int64_t p = 0; p < M.nc; p++)
            for (int64_t c = j; c + 1 < M.nc; c++)
                M.rq[i][p][c] = M.rq[i][p][c + 1];
        for (int64_t c = j; c + 1 < M.nc; c++)
            for (int64_t p = 0; p < M.nc; p++)
                M.rq[i][c][p] = M.rq[i][c + 1][p];
    }
    M.nc--;
    for (int64_t p = 0; p <= M.nc; p++) {
        M.qo[p][M.nc] = M.qo[M.nc][p] = 0.0;
        for (int64_t i = 0; i < M.nr; i++)
            M.rq[i][p][M.nc] = M.rq[i][M.nc][p] = 0.0;
    }
    for (int64_t i = 0; i < M.nr; i++) {
        M.a[i][M.nc] = 0.0;
        if (M.indc[i] > j)
            M.indc[i]--;
    }
    for (int k = 0; k < M.nsos;) {
        int64_t at = 0;
        for (int64_t t = 0; t < M.sosn[k]; t++) {
            if (M.sosc[k][t] == j)
                continue;
            M.sosc[k][at] = M.sosc[k][t] > j ? M.sosc[k][t] - 1 : M.sosc[k][t];
            M.sosw[k][at++] = M.sosw[k][t];
        }
        M.sosn[k] = at;
        if (at > 0) {
            k++;
            continue;
        }
        for (int q = k; q + 1 < M.nsos; q++) {
            M.sost[q] = M.sost[q + 1];
            M.sosn[q] = M.sosn[q + 1];
            memcpy(M.sosc[q], M.sosc[q + 1], sizeof M.sosc[q]);
            memcpy(M.sosw[q], M.sosw[q + 1], sizeof M.sosw[q]);
        }
        M.nsos--;
    }
    for (int k = 0; k < M.ncone; k++)
        for (int64_t t = 0; t < M.con[k]; t++)
            if (M.coc[k][t] > j)
                M.coc[k][t]--;
}

static void mirror_delete_row(int64_t i)
{
    for (int64_t r = i; r + 1 < M.nr; r++) {
        M.rl[r] = M.rl[r + 1];
        M.ru[r] = M.ru[r + 1];
        M.indc[r] = M.indc[r + 1];
        M.indv[r] = M.indv[r + 1];
        memcpy(M.a[r], M.a[r + 1], sizeof M.a[r]);
        memcpy(M.rq[r], M.rq[r + 1], sizeof M.rq[r]);
    }
    M.nr--;
    memset(M.a[M.nr], 0, sizeof M.a[M.nr]);
    memset(M.rq[M.nr], 0, sizeof M.rq[M.nr]);
    M.indc[M.nr] = -1;
}

static int pick_distinct(int64_t *out, int64_t n)
{
    if (n > M.nc)
        return 0;
    for (int64_t k = 0; k < n; k++) {
        for (;;) {
            const int64_t j = ri(0, M.nc - 1);
            bool dup = false;
            for (int64_t q = 0; q < k; q++)
                dup = dup || out[q] == j;
            if (!dup) {
                out[k] = j;
                break;
            }
        }
    }
    return 1;
}

static int64_t broke[5];

static void fail(int p, int64_t r, int64_t s, const char *how, const char *what)
{
    broke[p]++;
    if (broke[p] <= 10)
        printf("P%d m%" PRId64 " edit %" PRId64 " (%s): %s\n", p, r, s, how,
               what);
}

static bool same(double a, double b)
{
    return a == b || (isnan(a) && isnan(b));
}

static bool compare(const jaos_model *m, char *what, size_t len)
{
    if (jaos_num_col(m) != M.nc || jaos_num_row(m) != M.nr) {
        snprintf(what, len, "size %" PRId64 "x%" PRId64 ", mirror %" PRId64
                 "x%" PRId64, jaos_num_row(m), jaos_num_col(m), M.nr, M.nc);
        return false;
    }
    for (int64_t j = 0; j < M.nc; j++) {
        double c, lo, up;
        bool in, se;
        if (jaos_col_cost(m, j, &c) != JAOS_OK ||
            jaos_col_bounds(m, j, &lo, &up) != JAOS_OK ||
            jaos_col_integer(m, j, &in) != JAOS_OK ||
            jaos_col_semicontinuous(m, j, &se) != JAOS_OK) {
            snprintf(what, len, "column %" PRId64 " getters fail", j);
            return false;
        }
        if (!same(c, M.cost[j]) || !same(lo, M.lo[j]) || !same(up, M.up[j]) ||
            in != M.isint[j] || se != M.semi[j]) {
            snprintf(what, len, "column %" PRId64 ": cost %g/%g box [%g,%g]/"
                     "[%g,%g] int %d/%d semi %d/%d", j, c, M.cost[j], lo, up,
                     M.lo[j], M.up[j], in, M.isint[j], se, M.semi[j]);
            return false;
        }
    }
    for (int64_t i = 0; i < M.nr; i++) {
        double lo, up;
        int64_t col;
        int val;
        if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK ||
            jaos_row_indicator(m, i, &col, &val) != JAOS_OK) {
            snprintf(what, len, "row %" PRId64 " getters fail", i);
            return false;
        }
        if (!same(lo, M.rl[i]) || !same(up, M.ru[i])) {
            snprintf(what, len, "row %" PRId64 " sides [%g,%g]/[%g,%g]", i,
                     lo, up, M.rl[i], M.ru[i]);
            return false;
        }
        if (col != M.indc[i] || (col >= 0 && val != M.indv[i])) {
            snprintf(what, len, "row %" PRId64 " indicator %" PRId64 "=%d/%"
                     PRId64 "=%d", i, col, val, M.indc[i], M.indv[i]);
            return false;
        }
        for (int64_t j = 0; j < M.nc; j++) {
            double v;
            if (jaos_coefficient(m, i, j, &v) != JAOS_OK || !same(v, M.a[i][j])) {
                snprintf(what, len, "a[%" PRId64 "][%" PRId64 "] %g/%g", i, j,
                         v, M.a[i][j]);
                return false;
            }
        }
        const int64_t nz = jaos_row_quadratic_nz(m, i);
        int64_t rr[N * N], cc[N * N];
        double vv[N * N];
        int64_t want = 0;
        for (int64_t p = 0; p < M.nc; p++)
            for (int64_t q = 0; q <= p; q++)
                if (M.rq[i][p][q] != 0.0)
                    want++;
        if (nz != want || (nz > 0 && jaos_row_quadratic(m, i, rr, cc, vv) !=
                                         JAOS_OK)) {
            snprintf(what, len, "row %" PRId64 " quadratic nz %" PRId64 "/%"
                     PRId64, i, nz, want);
            return false;
        }
        for (int64_t k = 0; k < nz; k++)
            if (rr[k] < cc[k] || rr[k] >= M.nc || !same(vv[k], M.rq[i][rr[k]][cc[k]])) {
                snprintf(what, len, "row %" PRId64 " quadratic (%" PRId64 ",%"
                         PRId64 ") %g", i, rr[k], cc[k], vv[k]);
                return false;
            }
    }
    {
        const int64_t nz = jaos_quadratic_nz(m);
        int64_t want = 0;
        for (int64_t p = 0; p < M.nc; p++)
            for (int64_t q = 0; q <= p; q++)
                if (M.qo[p][q] != 0.0)
                    want++;
        int64_t rr[N * N + 1], cc[N * N + 1];
        double vv[N * N + 1];
        if (nz != want || jaos_quadratic(m, rr, cc, vv) != JAOS_OK) {
            snprintf(what, len, "Q nz %" PRId64 "/%" PRId64, nz, want);
            return false;
        }
        for (int64_t k = 0; k < nz; k++)
            if (rr[k] < cc[k] || rr[k] >= M.nc || !same(vv[k], M.qo[rr[k]][cc[k]])) {
                snprintf(what, len, "Q (%" PRId64 ",%" PRId64 ") %g", rr[k],
                         cc[k], vv[k]);
                return false;
            }
    }
    if (jaos_num_sos(m) != M.nsos) {
        snprintf(what, len, "sos sets %" PRId64 "/%d", jaos_num_sos(m), M.nsos);
        return false;
    }
    for (int k = 0; k < M.nsos; k++) {
        int type;
        int64_t n, cols[N];
        double w[N];
        if (jaos_sos(m, k, &type, &n, nullptr, nullptr) != JAOS_OK ||
            n != M.sosn[k] || type != M.sost[k] ||
            jaos_sos(m, k, &type, &n, cols, w) != JAOS_OK) {
            snprintf(what, len, "sos %d header", k);
            return false;
        }
        for (int64_t t = 0; t < n; t++)
            if (cols[t] != M.sosc[k][t] || !same(w[t], M.sosw[k][t])) {
                snprintf(what, len, "sos %d member %" PRId64 ": %" PRId64 "@%g/%"
                         PRId64 "@%g", k, t, cols[t], w[t], M.sosc[k][t],
                         M.sosw[k][t]);
                return false;
            }
    }
    if (jaos_num_cones(m) != M.ncone) {
        snprintf(what, len, "cones %" PRId64 "/%d", jaos_num_cones(m), M.ncone);
        return false;
    }
    for (int k = 0; k < M.ncone; k++) {
        jaos_cone_type type;
        int64_t n, cols[N];
        if (jaos_cone(m, k, &type, &n, cols) != JAOS_OK || type != M.cot[k] ||
            n != M.con[k]) {
            snprintf(what, len, "cone %d header", k);
            return false;
        }
        for (int64_t t = 0; t < n; t++)
            if (cols[t] != M.coc[k][t]) {
                snprintf(what, len, "cone %d member %" PRId64 ": %" PRId64 "/%"
                         PRId64, k, t, cols[t], M.coc[k][t]);
                return false;
            }
    }
    return true;
}

static bool compare_columns_and_cones(const jaos_model *m, char *what,
                                      size_t len)
{
    if (jaos_num_col(m) != M.nc) {
        snprintf(what, len, "columns %" PRId64 "/%" PRId64, jaos_num_col(m),
                 M.nc);
        return false;
    }
    for (int64_t j = 0; j < M.nc; j++) {
        double c;
        bool in;
        if (jaos_col_cost(m, j, &c) != JAOS_OK ||
            jaos_col_integer(m, j, &in) != JAOS_OK || !same(c, M.cost[j]) ||
            in != M.isint[j]) {
            snprintf(what, len, "column %" PRId64 ": cost %g/%g int %d/%d", j,
                     c, M.cost[j], in, M.isint[j]);
            return false;
        }
    }
    if (jaos_num_cones(m) != M.ncone) {
        snprintf(what, len, "cones %" PRId64 "/%d", jaos_num_cones(m), M.ncone);
        return false;
    }
    for (int k = 0; k < M.ncone; k++) {
        jaos_cone_type type;
        int64_t n, cols[N];
        if (jaos_cone(m, k, &type, &n, cols) != JAOS_OK || type != M.cot[k] ||
            n != M.con[k] || memcmp(cols, M.coc[k], (size_t)n * sizeof *cols)) {
            snprintf(what, len, "cone %d", k);
            return false;
        }
    }
    return true;
}

static mirror SAVED;

static void permute_for_nl(void)
{
    int64_t order[N], to[N], n = 0;
    for (int pass = 0; pass < 3; pass++)
        for (int64_t j = 0; j < M.nc; j++) {
            const bool binary = M.isint[j] && M.lo[j] == 0.0 && M.up[j] == 1.0;
            const int kind = !M.isint[j] ? 0 : binary ? 1 : 2;
            if (kind == pass)
                order[n++] = j;
        }
    for (int64_t k = 0; k < M.nc; k++)
        to[order[k]] = k;
    SAVED = M;
    for (int64_t k = 0; k < M.nc; k++) {
        const int64_t j = order[k];
        M.cost[k] = SAVED.cost[j];
        M.lo[k] = SAVED.lo[j];
        M.up[k] = SAVED.up[j];
        M.isint[k] = SAVED.isint[j];
        M.semi[k] = SAVED.semi[j];
        for (int64_t i = 0; i < M.nr; i++)
            M.a[i][k] = SAVED.a[i][j];
        for (int64_t q = 0; q < M.nc; q++) {
            M.qo[k][q] = SAVED.qo[j][order[q]];
            for (int64_t i = 0; i < M.nr; i++)
                M.rq[i][k][q] = SAVED.rq[i][j][order[q]];
        }
    }
    for (int64_t i = 0; i < M.nr; i++)
        if (M.indc[i] >= 0)
            M.indc[i] = to[M.indc[i]];
    for (int k = 0; k < M.nsos; k++)
        for (int64_t t = 0; t < M.sosn[k]; t++)
            M.sosc[k][t] = to[M.sosc[k][t]];
    for (int k = 0; k < M.ncone; k++)
        for (int64_t t = 0; t < M.con[k]; t++)
            M.coc[k][t] = to[M.coc[k][t]];
}

typedef struct {
    const char *name, *ext;
    jaos_status (*write)(jaos_model *, const char *);
    jaos_status (*read)(jaos_model *, const char *);
    int64_t trips, refused, broken;
} format;

static format FMT[6] = {
    {"mps", "mps", jaos_write_mps, jaos_read_mps, 0, 0, 0},
    {"lp", "lp", jaos_write_lp, jaos_read_lp, 0, 0, 0},
    {"nl", "nl", jaos_write_nl, jaos_read_nl, 0, 0, 0},
    {"qplib", "qplib", jaos_write_qplib, jaos_read_qplib, 0, 0, 0},
    {"cbf", "cbf", jaos_write_cbf, jaos_read_cbf, 0, 0, 0},
    {"osil", "osil", jaos_write_osil, jaos_read_osil, 0, 0, 0},
};

static const char *const EDIT[16] = {
    "delete-col", "delete-two-cols", "delete-row", "add-col", "add-row",
    "add-sos", "delete-cone", "add-cone", "indicator", "row-quadratic",
    "objective-q", "diagonal-q", "integer", "semi", "coefficient", "box"};

static int edit(jaos_model *m, char *how, size_t len, int *expect_ok,
                jaos_status *got)
{
    for (;;) {
        const int e = (int)ri(0, 15);
        int64_t js[4];
        jaos_status st = JAOS_OK;
        *expect_ok = 1;
        if (e == 0) {
            if (M.nc < 2)
                continue;
            const int64_t j = ri(0, M.nc - 1);
            *expect_ok = !protected_col(j);
            st = jaos_delete_cols(m, 1, &j);
            if (*expect_ok && st == JAOS_OK)
                mirror_delete_col(j);
            snprintf(how, len, "%s %" PRId64, EDIT[e], j);
        } else if (e == 1) {
            if (M.nc < 3 || !pick_distinct(js, 2))
                continue;
            *expect_ok = !protected_col(js[0]) && !protected_col(js[1]);
            st = jaos_delete_cols(m, 2, js);
            if (*expect_ok && st == JAOS_OK) {
                const int64_t hi = js[0] > js[1] ? js[0] : js[1];
                const int64_t lo = js[0] > js[1] ? js[1] : js[0];
                mirror_delete_col(hi);
                mirror_delete_col(lo);
            }
            snprintf(how, len, "%s %" PRId64 " %" PRId64, EDIT[e], js[0], js[1]);
        } else if (e == 2) {
            if (M.nr < 2)
                continue;
            const int64_t i = ri(0, M.nr - 1);
            st = jaos_delete_rows(m, 1, &i);
            if (st == JAOS_OK)
                mirror_delete_row(i);
            snprintf(how, len, "%s %" PRId64, EDIT[e], i);
        } else if (e == 3) {
            if (M.nc >= N)
                continue;
            int64_t idx[N];
            double val[N];
            int64_t n = 0;
            for (int64_t i = 0; i < M.nr; i++)
                if (ri(0, 9) < 4) {
                    idx[n] = i;
                    val[n++] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
                }
            const int64_t start2[2] = {0, n};
            const double c = (double)ri(-9, 9), lo = 0.0,
                         up = (double)ri(1, 10);
            st = jaos_add_cols(m, 1, &c, &lo, &up, n, start2, idx, val);
            if (st == JAOS_OK) {
                const int64_t j = M.nc++;
                M.cost[j] = c;
                M.lo[j] = lo;
                M.up[j] = up;
                M.isint[j] = M.semi[j] = false;
                for (int64_t k = 0; k < n; k++)
                    M.a[idx[k]][j] = val[k];
            }
            snprintf(how, len, "%s with %" PRId64 " entries", EDIT[e], n);
        } else if (e == 4) {
            if (M.nr >= N)
                continue;
            int64_t idx[N];
            double val[N];
            int64_t n = 0;
            for (int64_t j = 0; j < M.nc; j++)
                if (ri(0, 9) < 4) {
                    idx[n] = j;
                    val[n++] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
                }
            const int64_t start2[2] = {0, n};
            const double lo = -INFINITY, up = (double)ri(0, 20);
            st = jaos_add_rows(m, 1, &lo, &up, n, start2, idx, val);
            if (st == JAOS_OK) {
                const int64_t i = M.nr++;
                M.rl[i] = lo;
                M.ru[i] = up;
                M.indc[i] = -1;
                for (int64_t k = 0; k < n; k++)
                    M.a[i][idx[k]] = val[k];
            }
            snprintf(how, len, "%s with %" PRId64 " entries", EDIT[e], n);
        } else if (e == 5) {
            const int64_t n = ri(1, 4);
            if (M.nsos >= S || !pick_distinct(js, n))
                continue;
            const int type = (int)ri(1, 2);
            double w[4];
            for (int64_t k = 0; k < n; k++)
                w[k] = (double)(k + 1) + 0.5 * (double)ri(0, 1);
            for (int64_t k = n - 1; k > 0; k--) {
                const int64_t q = ri(0, k);
                const double t = w[k];
                w[k] = w[q];
                w[q] = t;
            }
            st = jaos_add_sos(m, type, n, js, w);
            if (st == JAOS_OK) {
                const int k = M.nsos++;
                M.sost[k] = type;
                M.sosn[k] = n;
                for (int64_t t = 0; t < n; t++) {
                    M.sosc[k][t] = js[t];
                    M.sosw[k][t] = w[t];
                }
                for (int64_t a = 1; a < n; a++)
                    for (int64_t b = a; b > 0 && M.sosw[k][b - 1] > M.sosw[k][b]; b--) {
                        const double tw = M.sosw[k][b];
                        M.sosw[k][b] = M.sosw[k][b - 1];
                        M.sosw[k][b - 1] = tw;
                        const int64_t tc = M.sosc[k][b];
                        M.sosc[k][b] = M.sosc[k][b - 1];
                        M.sosc[k][b - 1] = tc;
                    }
            }
            snprintf(how, len, "%s type %d over %" PRId64, EDIT[e], type, n);
        } else if (e == 6) {
            if (M.ncone == 0)
                continue;
            const int64_t k = ri(0, M.ncone - 1);
            st = jaos_delete_cones(m, 1, &k);
            if (st == JAOS_OK) {
                for (int q = (int)k; q + 1 < M.ncone; q++) {
                    M.cot[q] = M.cot[q + 1];
                    M.con[q] = M.con[q + 1];
                    memcpy(M.coc[q], M.coc[q + 1], sizeof M.coc[q]);
                }
                M.ncone--;
            }
            snprintf(how, len, "%s %" PRId64, EDIT[e], k);
        } else if (e == 7) {
            const jaos_cone_type type = ri(0, 1) ? JAOS_CONE_QUADRATIC
                                                 : JAOS_CONE_ROTATED;
            const int64_t n = type == JAOS_CONE_QUADRATIC ? ri(2, 4) : ri(3, 4);
            if (M.ncone >= S || !pick_distinct(js, n))
                continue;
            st = jaos_add_cone(m, type, n, js);
            if (st == JAOS_OK) {
                const int k = M.ncone++;
                M.cot[k] = type;
                M.con[k] = n;
                memcpy(M.coc[k], js, (size_t)n * sizeof *js);
            } else {
                *expect_ok = 0;
                refused_sets++;
            }
            snprintf(how, len, "%s type %d over %" PRId64, EDIT[e], (int)type, n);
        } else if (e == 8) {
            const int64_t i = ri(0, M.nr - 1);
            if (ri(0, 3) == 0) {
                st = jaos_set_row_indicator(m, i, -1, 0);
                if (st == JAOS_OK)
                    M.indc[i] = -1;
                snprintf(how, len, "%s clear %" PRId64, EDIT[e], i);
            } else {
                const int64_t j = ri(0, M.nc - 1);
                const int v = (int)ri(0, 1);
                if (!M.isint[j]) {
                    if (jaos_set_col_integer(m, j, true) != JAOS_OK)
                        return -1;
                    M.isint[j] = true;
                }
                st = jaos_set_row_indicator(m, i, j, v);
                if (st == JAOS_OK) {
                    M.indc[i] = j;
                    M.indv[i] = v;
                } else {
                    *expect_ok = 0;
                    refused_sets++;
                }
                snprintf(how, len, "%s %" PRId64 " on %" PRId64 "=%d", EDIT[e],
                         i, j, v);
            }
        } else if (e == 9) {
            const int64_t i = ri(0, M.nr - 1);
            const int64_t k = ri(0, 3);
            int64_t rr[4], cc[4];
            double vv[4];
            int64_t n = 0;
            for (int64_t t = 0; t < k; t++) {
                int64_t p = ri(0, M.nc - 1), q = ri(0, M.nc - 1);
                if (p < q) {
                    const int64_t s2 = p;
                    p = q;
                    q = s2;
                }
                bool dup = false;
                for (int64_t u = 0; u < n; u++)
                    dup = dup || (rr[u] == p && cc[u] == q);
                if (dup)
                    continue;
                rr[n] = p;
                cc[n] = q;
                vv[n++] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
            }
            st = jaos_set_row_quadratic(m, i, n, rr, cc, vv);
            if (st == JAOS_OK) {
                memset(M.rq[i], 0, sizeof M.rq[i]);
                for (int64_t u = 0; u < n; u++)
                    M.rq[i][rr[u]][cc[u]] = M.rq[i][cc[u]][rr[u]] = vv[u];
            } else {
                *expect_ok = 0;
                refused_sets++;
            }
            snprintf(how, len, "%s %" PRId64 " with %" PRId64, EDIT[e], i, n);
        } else if (e == 10) {
            const int64_t k = ri(0, 4);
            int64_t rr[4], cc[4];
            double vv[4];
            int64_t n = 0;
            for (int64_t t = 0; t < k; t++) {
                int64_t p = ri(0, M.nc - 1), q = ri(0, M.nc - 1);
                if (p < q) {
                    const int64_t s2 = p;
                    p = q;
                    q = s2;
                }
                bool dup = false;
                for (int64_t u = 0; u < n; u++)
                    dup = dup || (rr[u] == p && cc[u] == q);
                if (dup || n >= 4)
                    continue;
                rr[n] = p;
                cc[n] = q;
                vv[n++] = (double)(ri(0, 1) ? ri(1, 3) : -ri(1, 3));
            }
            st = jaos_set_quadratic(m, n, rr, cc, vv);
            if (st == JAOS_OK) {
                memset(M.qo, 0, sizeof M.qo);
                for (int64_t u = 0; u < n; u++)
                    M.qo[rr[u]][cc[u]] = M.qo[cc[u]][rr[u]] = vv[u];
            }
            snprintf(how, len, "%s with %" PRId64, EDIT[e], n);
        } else if (e == 11) {
            const int64_t j = ri(0, M.nc - 1);
            const double q = (double)ri(0, 3);
            st = jaos_set_col_quadratic(m, j, q);
            if (st == JAOS_OK)
                M.qo[j][j] = q;
            snprintf(how, len, "%s %" PRId64 " %g", EDIT[e], j, q);
        } else if (e == 12) {
            const int64_t j = ri(0, M.nc - 1);
            bool used = false;
            for (int64_t i = 0; i < M.nr; i++)
                used = used || M.indc[i] == j;
            const bool b = used || ri(0, 1) == 1;
            st = jaos_set_col_integer(m, j, b);
            if (st == JAOS_OK)
                M.isint[j] = b;
            snprintf(how, len, "%s %" PRId64 " %d", EDIT[e], j, b);
        } else if (e == 13) {
            const int64_t j = ri(0, M.nc - 1);
            const bool b = ri(0, 1) == 1;
            st = jaos_set_col_semicontinuous(m, j, b);
            if (st == JAOS_OK)
                M.semi[j] = b;
            else {
                *expect_ok = 0;
                refused_sets++;
            }
            snprintf(how, len, "%s %" PRId64 " %d", EDIT[e], j, b);
        } else if (e == 14) {
            const int64_t i = ri(0, M.nr - 1), j = ri(0, M.nc - 1);
            const double v = (double)ri(-3, 3);
            st = jaos_set_coefficient(m, i, j, v);
            if (st == JAOS_OK)
                M.a[i][j] = v;
            snprintf(how, len, "%s %" PRId64 " %" PRId64 " %g", EDIT[e], i, j, v);
        } else {
            const int64_t j = ri(0, M.nc - 1);
            const double c = (double)ri(-9, 9), lo = (double)ri(0, 2),
                         up = lo + (double)ri(0, 8);
            st = jaos_set_col_cost(m, j, c);
            if (st == JAOS_OK)
                st = jaos_set_col_bounds(m, j, lo, up);
            if (st == JAOS_OK) {
                M.cost[j] = c;
                M.lo[j] = lo;
                M.up[j] = up;
            }
            snprintf(how, len, "%s %" PRId64 " %g [%g,%g]", EDIT[e], j, c, lo, up);
        }
        *got = st;
        return e;
    }
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: structedit RUNS SEED EDITS TMPDIR\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    const int64_t edits = atoll(argv[3]);
    char path[512];
    snprintf(path, sizeof path, "%s/structedit.mps", argv[4]);
    int64_t kinds[16] = {0}, checks = 0, copies = 0, trips = 0;
    for (int64_t r = 0; r < runs; r++) {
        start();
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK || load(m) != JAOS_OK)
            return 1;
        for (int64_t s = 1; s <= edits; s++) {
            char how[128], what[256];
            int expect_ok = 1;
            jaos_status got = JAOS_OK;
            const int e = edit(m, how, sizeof how, &expect_ok, &got);
            if (e < 0)
                return 1;
            kinds[e]++;
            if (e <= 1 && (got == JAOS_OK) != (expect_ok != 0)) {
                snprintf(what, sizeof what, "returned %d, the mirror expects %s",
                         (int)got, expect_ok ? "success" : "a refusal");
                fail(1, r, s, how, what);
                break;
            }
            checks++;
            if (!compare(m, what, sizeof what)) {
                fail(2, r, s, how, what);
                break;
            }
            if (s % 10 != 0)
                continue;
            jaos_model *c = nullptr;
            if (jaos_model_copy(m, &c) != JAOS_OK)
                return 1;
            copies++;
            if (!compare(c, what, sizeof what))
                fail(3, r, s, how, what);
            jaos_model_free(c);
            for (int f = 0; f < 6; f++) {
                format *ft = &FMT[f];
                snprintf(path, sizeof path, "%s/structedit.%s", argv[4], ft->ext);
                if (ft->write(m, path) != JAOS_OK) {
                    ft->refused++;
                    refused_writes++;
                    continue;
                }
                jaos_model *b = nullptr;
                if (jaos_model_new(&b) != JAOS_OK)
                    return 1;
                char tag[200];
                if (ft->read(b, path) != JAOS_OK) {
                    snprintf(what, sizeof what, "%s: what the writer wrote does "
                             "not read: %s", ft->name, jaos_model_error(b));
                    ft->broken++;
                    fail(4, r, s, how, what);
                } else {
                    trips++;
                    ft->trips++;
                    if (f == 2)
                        permute_for_nl();
                    const bool ok = f == 4 ? compare_columns_and_cones(b, tag,
                                                                       sizeof tag)
                                           : compare(b, tag, sizeof tag);
                    if (f == 2)
                        M = SAVED;
                    if (!ok) {
                        snprintf(what, sizeof what, "%s: %s", ft->name, tag);
                        ft->broken++;
                        fail(4, r, s, how, what);
                    }
                }
                jaos_model_free(b);
            }
        }
        jaos_model_free(m);
    }
    printf("models %" PRId64 " edits %" PRId64 " checks %" PRId64 " copies %"
           PRId64 " MPS trips %" PRId64 " writes refused %" PRId64
           " calls refused %" PRId64 "\nkinds", runs, runs * edits, checks,
           copies, trips, refused_writes, refused_sets);
    for (int e = 0; e < 16; e++)
        printf(" %s=%" PRId64, EDIT[e], kinds[e]);
    printf("\n");
    for (int f = 0; f < 6; f++)
        printf("%-6s trips %" PRId64 " refused %" PRId64 " broken %" PRId64
               "\n", FMT[f].name, FMT[f].trips, FMT[f].refused, FMT[f].broken);
    printf("P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " broken total %" PRId64 "\n", broke[1], broke[2], broke[3],
           broke[4], broke[1] + broke[2] + broke[3] + broke[4]);
    return 0;
}
