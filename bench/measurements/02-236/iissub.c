/* The reading behind 02-236: the infeasible subsystem written out as a
 * model, over generated models.
 *
 * The generator is 02-235's, with the rows pulled off the planted point on
 * six models in ten, so most models are infeasible. Every row and column
 * is named, so a member of the written model can be matched to the row or
 * column it came from.
 *
 * Seven properties:
 *   P1 the written model solves infeasible
 *   P2 dropping any one member side, and writing the model again, gives
 *      a model that solves feasible: the subsystem is irreducible
 *   P3 the written model is the member sides and nothing else: its rows
 *      are the rows with a member side, in index order, each member side
 *      at the value it had and each other side infinite, its columns are
 *      the columns those rows touch plus the columns with a member bound,
 *      each column's member sides at their values and the others
 *      infinite, and every cost zero
 *   P4 the report's member count is the number of sides marked
 *   P5 a second search on a fresh copy marks the same sides
 *   P6 the subsystem of the written model is the written model: every
 *      finite side of it is a member
 *   P7 a model with integer marks gets the subsystem of its relaxation:
 *      the same sides as the model without marks
 *
 * Usage: iissub RUNS SEED [DUMP_INDEX DUMP_PATH]
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

#define MAXC 24
#define MAXR 16

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool isint[MAXC];
    bool maximise, lp_only;
    double offset;
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->maximise = ri(0, 1) == 1;
    g->lp_only = ri(0, 2) == 0;
    g->offset = (double)ri(-3, 3);
    const bool broken = ri(0, 9) < 6;
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = !g->lp_only && ri(0, 9) < 6;
        g->cost[j] = (double)ri(-9, 9);
        g->cl[j] = 0.0;
        g->cu[j] = broken && ri(0, 3) == 0 ? INFINITY : (double)ri(1, 10);
        z[j] = ri(0, isfinite(g->cu[j]) ? (int64_t)g->cu[j] : 10);
        g->ap[j] = g->nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (ri(0, 9) >= 5)
                continue;
            int64_t a = ri(-3, 3);
            if (a == 0)
                a = 1;
            g->ai[g->nz] = i;
            g->av[g->nz] = (double)a;
            g->nz++;
            act[i] += (double)a * (double)z[j];
        }
    }
    g->ap[g->nc] = g->nz;
    for (int64_t i = 0; i < g->nr; i++) {
        const int64_t kind = ri(0, 9);
        const double shift = broken ? (double)ri(-40, 40) : 0.0;
        if (kind < 3) {
            g->rl[i] = g->ru[i] = act[i] + shift;
        } else if (kind < 6) {
            g->rl[i] = -INFINITY;
            g->ru[i] = act[i] + (double)ri(0, 3) + shift;
        } else if (kind < 9) {
            g->rl[i] = act[i] - (double)ri(0, 3) + shift;
            g->ru[i] = INFINITY;
        } else {
            g->rl[i] = act[i] - (double)ri(0, 2) + shift;
            g->ru[i] = act[i] + (double)ri(0, 2) + shift;
        }
    }
}

static jaos_model *load(const gen *g, bool with_marks)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    if (jaos_load_lp(m, g->nc, g->nr,
                     g->maximise ? JAOS_MAXIMIZE : JAOS_MINIMIZE, g->offset,
                     g->cost, g->cl, g->cu, g->rl, g->ru, g->nz, g->ap,
                     g->ai, g->av) != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    char name[32];
    for (int64_t i = 0; i < g->nr; i++) {
        snprintf(name, sizeof name, "r%" PRId64, i);
        if (jaos_set_row_name(m, i, name) != JAOS_OK) abort();
    }
    for (int64_t j = 0; j < g->nc; j++) {
        snprintf(name, sizeof name, "c%" PRId64, j);
        if (jaos_set_col_name(m, j, name) != JAOS_OK) abort();
    }
    if (with_marks)
        for (int64_t j = 0; j < g->nc; j++)
            if (g->isint[j] && jaos_set_col_integer(m, j, true) != JAOS_OK)
                abort();
    return m;
}

static int64_t broke[9];

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

static int64_t index_of(const char *name)
{
    return atoll(name + 1);
}

static bool has_lo(jaos_iis_side s) { return s == JAOS_IIS_LOWER || s == JAOS_IIS_BOTH; }
static bool has_hi(jaos_iis_side s) { return s == JAOS_IIS_UPPER || s == JAOS_IIS_BOTH; }

static jaos_solve_status solve_status(jaos_model *m)
{
    if (jaos_solve(m) != JAOS_OK) abort();
    return jaos_status_of(m);
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 1000;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    const int64_t dump = argc > 4 ? atoll(argv[3]) : -1;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t infeasible = 0, subsystems = 0, members_total = 0, drops = 0;
    int64_t from_cert = 0, mips_read = 0, mips_refused = 0;
    static jaos_iis_side rs[MAXR], cs[MAXC], rs2[MAXR], cs2[MAXC];
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        if (idx == dump) {
            jaos_model *d = load(&g, true);
            if (d == nullptr || jaos_write_mps(d, argv[4]) != JAOS_OK)
                abort();
            jaos_model_free(d);
        }
        jaos_model *m = load(&g, false);
        if (m == nullptr) abort();
        if (solve_status(m) != JAOS_SOLVE_INFEASIBLE) {
            jaos_model_free(m);
            continue;
        }
        infeasible++;
        jaos_iis_report rep;
        if (jaos_iis(m, rs, cs, &rep) != JAOS_OK) {
            fail(1, idx, "no-subsystem", 0.0, 0.0);
            jaos_model_free(m);
            continue;
        }
        subsystems++;
        members_total += rep.members;
        if (rep.from_certificate) from_cert++;

        int64_t marked = 0;
        for (int64_t i = 0; i < g.nr; i++)
            marked += has_lo(rs[i]) + has_hi(rs[i]);
        for (int64_t j = 0; j < g.nc; j++)
            marked += has_lo(cs[j]) + has_hi(cs[j]);
        if (marked != rep.members)
            fail(4, idx, "members-vs-marked", (double)rep.members,
                 (double)marked);

        jaos_model *sub = nullptr;
        if (jaos_iis_model(m, rs, cs, &sub) != JAOS_OK || sub == nullptr) {
            fail(1, idx, "no-written-model", 0.0, 0.0);
            jaos_model_free(m);
            continue;
        }
        if (solve_status(sub) != JAOS_SOLVE_INFEASIBLE)
            fail(1, idx, "written-model-feasible", (double)jaos_status_of(sub),
                 0.0);

        /* P3: structure against the original, by name. */
        {
            const int64_t snr = jaos_num_row(sub), snc = jaos_num_col(sub);
            int64_t want_rows = 0;
            for (int64_t i = 0; i < g.nr; i++)
                if (rs[i] != JAOS_IIS_NONE) want_rows++;
            if (snr != want_rows)
                fail(3, idx, "row-count", (double)snr, (double)want_rows);
            char name[JAOS_NAME_MAX + 1];
            int64_t last = -1;
            static bool col_present[MAXC];
            memset(col_present, 0, sizeof col_present);
            for (int64_t si = 0; si < snr; si++) {
                if (jaos_row_name(sub, si, name, sizeof name) != JAOS_OK) abort();
                const int64_t i = index_of(name);
                if (i <= last) fail(3, idx, "row-order", (double)i, (double)last);
                last = i;
                if (i < 0 || i >= g.nr || rs[i] == JAOS_IIS_NONE) {
                    fail(3, idx, "row-not-a-member", (double)i, 0.0);
                    continue;
                }
                double lo, hi;
                if (jaos_row_bounds(sub, si, &lo, &hi) != JAOS_OK) abort();
                if (has_lo(rs[i]) ? lo != g.rl[i] : lo != -INFINITY)
                    fail(3, idx, "row-lower", lo, g.rl[i]);
                if (has_hi(rs[i]) ? hi != g.ru[i] : hi != INFINITY)
                    fail(3, idx, "row-upper", hi, g.ru[i]);
            }
            last = -1;
            for (int64_t sj = 0; sj < snc; sj++) {
                if (jaos_col_name(sub, sj, name, sizeof name) != JAOS_OK) abort();
                const int64_t j = index_of(name);
                if (j <= last) fail(3, idx, "col-order", (double)j, (double)last);
                last = j;
                if (j < 0 || j >= g.nc) { fail(3, idx, "col-unknown", (double)j, 0.0); continue; }
                col_present[j] = true;
                double lo, hi, c;
                if (jaos_col_bounds(sub, sj, &lo, &hi) != JAOS_OK) abort();
                if (jaos_col_cost(sub, sj, &c) != JAOS_OK) abort();
                if (c != 0.0) fail(3, idx, "col-cost", c, 0.0);
                if (has_lo(cs[j]) ? lo != g.cl[j] : lo != -INFINITY)
                    fail(3, idx, "col-lower", lo, g.cl[j]);
                if (has_hi(cs[j]) ? hi != g.cu[j] : hi != INFINITY)
                    fail(3, idx, "col-upper", hi, g.cu[j]);
            }
            for (int64_t j = 0; j < g.nc; j++) {
                bool touched = cs[j] != JAOS_IIS_NONE;
                for (int64_t p = g.ap[j]; p < g.ap[j + 1] && !touched; p++)
                    if (rs[g.ai[p]] != JAOS_IIS_NONE) touched = true;
                if (touched != col_present[j])
                    fail(3, idx, "col-presence", (double)j, touched ? 1.0 : 0.0);
            }
        }

        /* P6: the subsystem of the written model is itself. */
        {
            jaos_model *again = nullptr;
            if (jaos_model_copy(sub, &again) != JAOS_OK) abort();
            static jaos_iis_side srs[MAXR], scs[MAXC];
            jaos_iis_report r2;
            if (solve_status(again) != JAOS_SOLVE_INFEASIBLE ||
                jaos_iis(again, srs, scs, &r2) != JAOS_OK) {
                fail(6, idx, "subsystem-of-subsystem-missing", 0.0, 0.0);
            } else {
                if (r2.members != rep.members)
                    fail(6, idx, "subsystem-of-subsystem-members",
                         (double)r2.members, (double)rep.members);
                const int64_t snr = jaos_num_row(again), snc = jaos_num_col(again);
                for (int64_t si = 0; si < snr; si++) {
                    double lo, hi;
                    if (jaos_row_bounds(again, si, &lo, &hi) != JAOS_OK) abort();
                    if (has_lo(srs[si]) != isfinite(lo) || has_hi(srs[si]) != isfinite(hi))
                        fail(6, idx, "subsystem-of-subsystem-row", (double)si, 0.0);
                }
                for (int64_t sj = 0; sj < snc; sj++) {
                    double lo, hi;
                    if (jaos_col_bounds(again, sj, &lo, &hi) != JAOS_OK) abort();
                    if (has_lo(scs[sj]) != isfinite(lo) || has_hi(scs[sj]) != isfinite(hi))
                        fail(6, idx, "subsystem-of-subsystem-col", (double)sj, 0.0);
                }
            }
            jaos_model_free(again);
        }
        jaos_model_free(sub);

        /* P2: irreducibility, one member dropped at a time. */
        for (int64_t i = 0; i < g.nr; i++) {
            for (int side = 0; side < 2; side++) {
                const bool have = side == 0 ? has_lo(rs[i]) : has_hi(rs[i]);
                if (!have) continue;
                memcpy(rs2, rs, sizeof rs); memcpy(cs2, cs, sizeof cs);
                rs2[i] = side == 0 ? (has_hi(rs[i]) ? JAOS_IIS_UPPER : JAOS_IIS_NONE)
                                   : (has_lo(rs[i]) ? JAOS_IIS_LOWER : JAOS_IIS_NONE);
                jaos_model *less = nullptr;
                if (jaos_iis_model(m, rs2, cs2, &less) != JAOS_OK) abort();
                drops++;
                if (solve_status(less) == JAOS_SOLVE_INFEASIBLE)
                    fail(2, idx, "row-side-not-needed", (double)i, (double)side);
                jaos_model_free(less);
            }
        }
        for (int64_t j = 0; j < g.nc; j++) {
            for (int side = 0; side < 2; side++) {
                const bool have = side == 0 ? has_lo(cs[j]) : has_hi(cs[j]);
                if (!have) continue;
                memcpy(rs2, rs, sizeof rs); memcpy(cs2, cs, sizeof cs);
                cs2[j] = side == 0 ? (has_hi(cs[j]) ? JAOS_IIS_UPPER : JAOS_IIS_NONE)
                                   : (has_lo(cs[j]) ? JAOS_IIS_LOWER : JAOS_IIS_NONE);
                jaos_model *less = nullptr;
                if (jaos_iis_model(m, rs2, cs2, &less) != JAOS_OK) abort();
                drops++;
                if (solve_status(less) == JAOS_SOLVE_INFEASIBLE)
                    fail(2, idx, "col-side-not-needed", (double)j, (double)side);
                jaos_model_free(less);
            }
        }

        /* P5: a second search on a fresh copy. */
        {
            jaos_model *fresh = load(&g, false);
            if (fresh == nullptr) abort();
            jaos_iis_report r3;
            if (solve_status(fresh) != JAOS_SOLVE_INFEASIBLE ||
                jaos_iis(fresh, rs2, cs2, &r3) != JAOS_OK)
                fail(5, idx, "second-search-missing", 0.0, 0.0);
            else if (memcmp(rs, rs2, (size_t)g.nr * sizeof *rs) != 0 ||
                     memcmp(cs, cs2, (size_t)g.nc * sizeof *cs) != 0 ||
                     r3.members != rep.members)
                fail(5, idx, "second-search-differs", (double)r3.members,
                     (double)rep.members);
            jaos_model_free(fresh);
        }

        /* P7: the marks change nothing. */
        if (!g.lp_only) {
            jaos_model *mip = load(&g, true);
            if (mip == nullptr) abort();
            if (jaos_set_work_limit(mip, 20000000) != JAOS_OK) abort();
            jaos_iis_report r4;
            const jaos_solve_status ms = solve_status(mip);
            if (ms != JAOS_SOLVE_INFEASIBLE) {
                fail(7, idx, "mip-not-infeasible", (double)ms, 0.0);
            } else if (jaos_iis(mip, rs2, cs2, &r4) != JAOS_OK) {
                mips_refused++;
                fail(7, idx, "mip-subsystem-refused", 0.0, 0.0);
            } else {
                mips_read++;
                if (memcmp(rs, rs2, (size_t)g.nr * sizeof *rs) != 0 ||
                    memcmp(cs, cs2, (size_t)g.nc * sizeof *cs) != 0)
                    fail(7, idx, "mip-subsystem-differs", (double)r4.members,
                         (double)rep.members);
            }
            jaos_model_free(mip);
        }
        jaos_model_free(m);
    }

    int64_t total = 0;
    for (int p = 1; p <= 7; p++) total += broke[p];
    printf("models %" PRId64 " infeasible %" PRId64 " subsystems %" PRId64
           " members %" PRId64 " drops %" PRId64 " from-certificate %" PRId64
           " mips-read %" PRId64 " mips-refused %" PRId64 "\n", runs,
           infeasible, subsystems, members_total, drops, from_cert,
           mips_read, mips_refused);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " P7=%" PRId64 " total=%" PRId64
           "\n", broke[1], broke[2], broke[3], broke[4], broke[5], broke[6],
           broke[7], total);
    return total == 0 ? 0 : 1;
}
