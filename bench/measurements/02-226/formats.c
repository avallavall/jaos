/* What JAOS writes, JAOS reads back as the same model. 02-138 and 3bf0585
 * read that field by field. This reads it by the answer: the status, the
 * objective and the node count have to survive the round trip.
 *
 * The node count is the sharp one. A copy that lost a semi-continuous mark
 * or an SOS set still compares equal field by field where the getter reads
 * the mark, and can still reach the same objective, but it reaches it
 * without the tree.
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rs;
static uint64_t nx(void)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return rs;
}
static int64_t pick(int64_t n) { return (int64_t)(nx() % (uint64_t)n); }

#define MAXC 8
#define MAXR 6

typedef struct {
    int64_t nc, nr, nz;
    jaos_obj_sense sense;
    double cost[MAXC], cl[MAXC], cu[MAXC];
    double rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool integer[MAXC], semi[MAXC];
    double quad[MAXC];
    int64_t sos_n, sos_col[MAXC];
    double sos_w[MAXC];
    int sos_type;
    int64_t ind_row, ind_col;
    int ind_val;
} model;

static void gen(model *g)
{
    memset(g, 0, sizeof *g);
    g->nc = 2 + pick(MAXC - 1);
    g->nr = 1 + pick(MAXR);
    g->sense = pick(2) ? JAOS_MINIMIZE : JAOS_MAXIMIZE;
    g->sos_n = 0;
    g->ind_row = -1;
    g->ind_col = -1;

    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)(pick(11) - 5);
        switch (pick(6)) {
        case 0: g->cl[j] = 0.0;  g->cu[j] = 1.0; break;
        case 1: g->cl[j] = 0.0;  g->cu[j] = (double)(1 + pick(6)); break;
        case 2: g->cl[j] = -2.0; g->cu[j] = 3.0; break;
        case 3: g->cl[j] = 0.0;  g->cu[j] = INFINITY; break;
        case 4: g->cl[j] = -INFINITY; g->cu[j] = INFINITY; break;
        default: g->cl[j] = (double)pick(3); g->cu[j] = g->cl[j] + (double)pick(4);
        }
        g->integer[j] = pick(2) != 0;
    }

    int64_t nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->ap[j] = nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (pick(10) < 4)
                continue;
            const double v = (double)(pick(7) - 3);
            if (v == 0.0)
                continue;
            g->ai[nz] = i; g->av[nz] = v; nz++;
        }
    }
    g->ap[g->nc] = nz;
    g->nz = nz;

    for (int64_t i = 0; i < g->nr; i++) {
        const double b = (double)(pick(13) - 4);
        switch (pick(4)) {
        case 0: g->rl[i] = -INFINITY; g->ru[i] = b; break;
        case 1: g->rl[i] = b; g->ru[i] = INFINITY; break;
        case 2: g->rl[i] = b; g->ru[i] = b; break;
        default: g->rl[i] = b; g->ru[i] = b + (double)pick(5);
        }
    }

    const int64_t shape = pick(5);
    if (shape == 1 && g->nc >= 2) {
        g->sos_type = 1 + (int)pick(2);
        g->sos_n = 2 + pick(g->nc - 1);
        for (int64_t k = 0; k < g->sos_n; k++) {
            g->sos_col[k] = k;
            g->sos_w[k] = (double)(k + 1);
            if (g->cl[k] == -INFINITY) g->cl[k] = 0.0;
            if (g->cu[k] == INFINITY) g->cu[k] = 4.0;
        }
    } else if (shape == 2) {
        const int64_t j = pick(g->nc);
        g->semi[j] = true;
        g->integer[j] = false;
        if (g->cl[j] <= 0.0) g->cl[j] = 1.0;
        if (g->cu[j] == INFINITY || g->cu[j] < g->cl[j]) g->cu[j] = g->cl[j] + 3.0;
    } else if (shape == 3) {
        const int64_t j = pick(g->nc);
        g->cl[j] = 0.0; g->cu[j] = 1.0; g->integer[j] = true;
        g->ind_col = j;
        g->ind_row = pick(g->nr);
        g->ind_val = (int)pick(2);
    } else if (shape == 4) {
        const double sign = g->sense == JAOS_MINIMIZE ? 1.0 : -1.0;
        for (int64_t j = 0; j < g->nc; j++)
            if (pick(2))
                g->quad[j] = sign * (double)(1 + pick(4));
    }

    /* A model with no integer column and a semi-continuous one is the shape
     * that found the defect of 02-225, so build it twice as often. */
    if (pick(4) == 0) {
        for (int64_t j = 0; j < g->nc; j++)
            g->integer[j] = false;
        const int64_t j = pick(g->nc);
        g->semi[j] = true;
        if (g->cl[j] <= 0.0) g->cl[j] = 1.0;
        if (g->cu[j] == INFINITY || g->cu[j] < g->cl[j]) g->cu[j] = g->cl[j] + 3.0;
        g->sos_n = 0;
        g->ind_row = -1;
        g->ind_col = -1;
    }
}

static void show(const model *g)
{
    printf("  nc=%" PRId64 " nr=%" PRId64 " sense=%s\n", g->nc, g->nr,
           g->sense == JAOS_MINIMIZE ? "min" : "max");
    for (int64_t j = 0; j < g->nc; j++)
        printf("  col %" PRId64 " cost=%g lo=%g up=%g int=%d semi=%d q=%g\n", j,
               g->cost[j], g->cl[j], g->cu[j], (int)g->integer[j],
               (int)g->semi[j], g->quad[j]);
    for (int64_t i = 0; i < g->nr; i++)
        printf("  row %" PRId64 " lo=%g up=%g\n", i, g->rl[i], g->ru[i]);
    for (int64_t j = 0; j < g->nc; j++)
        for (int64_t k = g->ap[j]; k < g->ap[j + 1]; k++)
            printf("  a[%" PRId64 ",%" PRId64 "]=%g\n", g->ai[k], j, g->av[k]);
    if (g->sos_n > 0) {
        printf("  sos type=%d n=%" PRId64 "\n", g->sos_type, g->sos_n);
    }
    if (g->ind_row >= 0)
        printf("  ind row=%" PRId64 " col=%" PRId64 " val=%d\n", g->ind_row,
               g->ind_col, g->ind_val);
}

static jaos_status build(const model *g, jaos_model **out)
{
    jaos_model *m = nullptr;
    jaos_status rc = jaos_model_new(&m);
    if (rc != JAOS_OK)
        return rc;
    rc = jaos_load_lp(m, g->nc, g->nr, g->sense, 0.0, g->cost, g->cl, g->cu,
                      g->rl, g->ru, g->nz, g->ap, g->ai, g->av);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }

    for (int64_t j = 0; j < g->nc && rc == JAOS_OK; j++) {
        if (g->integer[j])
            rc = jaos_set_col_integer(m, j, true);
        if (rc == JAOS_OK && g->semi[j])
            rc = jaos_set_col_semicontinuous(m, j, true);
        if (rc == JAOS_OK && g->quad[j] != 0.0)
            rc = jaos_set_col_quadratic(m, j, g->quad[j]);
    }
    if (rc == JAOS_OK && g->sos_n > 0)
        rc = jaos_add_sos(m, g->sos_type, g->sos_n, g->sos_col, g->sos_w);
    if (rc == JAOS_OK && g->ind_row >= 0)
        rc = jaos_set_row_indicator(m, g->ind_row, g->ind_col, g->ind_val);
    if (rc == JAOS_OK)
        rc = jaos_set_work_limit(m, 20000000);
    if (rc == JAOS_OK)
        rc = jaos_set_mip_node_limit(m, 200000);
    if (rc != JAOS_OK) { jaos_model_free(m); return rc; }
    *out = m;
    return JAOS_OK;
}

typedef struct {
    const char *name;
    const char *path;
    jaos_status (*write)(jaos_model *, const char *);
    jaos_status (*read)(jaos_model *, const char *);
    /* The nl format wants the continuous columns first, then the binary
     * ones, then the general integer ones, so its writer reorders the
     * columns (`order` in `jaos_write_nl`). A reordered model is the same
     * model and reaches the same answer, which 02-224 reads, but it does
     * not walk the same tree. The node count is compared only for the
     * formats that write the columns in the order the model holds them. */
    bool keeps_order;
} fmt;

typedef struct {
    jaos_solve_status status;
    double obj;
    int64_t nodes;
} answer;

static bool answer_of(jaos_model *m, answer *a)
{
    if (jaos_solve(m) != JAOS_OK)
        return false;
    a->status = jaos_status_of(m);
    a->obj = 0.0;
    (void)jaos_objective(m, &a->obj);
    jaos_mip_report r;
    memset(&r, 0, sizeof r);
    (void)jaos_mip_result(m, &r);
    a->nodes = r.nodes;
    return true;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? strtoll(argv[1], nullptr, 10) : 20000;
    rs = argc > 2 ? strtoull(argv[2], nullptr, 10) : 88172645463325252ULL;
    const char *dump = argc > 3 && argv[3][0] != '\0' ? argv[3] : nullptr;

    const fmt formats[] = {
        {"mps",   "build/fmt_tmp.mps",   jaos_write_mps,   jaos_read_mps,  true},
        {"lp",    "build/fmt_tmp.lp",    jaos_write_lp,    jaos_read_lp,   true},
        {"osil",  "build/fmt_tmp.osil",  jaos_write_osil,  jaos_read_osil, true},
        {"qplib", "build/fmt_tmp.qplib", jaos_write_qplib, jaos_read_qplib, true},
        {"nl",    "build/fmt_tmp.nl",    jaos_write_nl,    jaos_read_nl,   false},
    };
    const int64_t nf = (int64_t)(sizeof formats / sizeof formats[0]);

    int64_t built = 0, tried = 0, refused = 0, skipped = 0, bad = 0;
    int64_t node_only = 0, node_checked = 0, node_tree = 0;
    for (int64_t t = 0; t < runs; t++) {
        model g;
        gen(&g);

        jaos_model *a = nullptr;
        if (build(&g, &a) != JAOS_OK) { skipped++; continue; }
        answer ra;
        if (!answer_of(a, &ra)) { jaos_model_free(a); skipped++; continue; }
        if (ra.status == JAOS_SOLVE_WORK_LIMIT ||
            ra.status == JAOS_SOLVE_NODE_LIMIT) {
            jaos_model_free(a); skipped++; continue;
        }
        built++;

        for (int64_t k = 0; k < nf; k++) {
            jaos_model *w = nullptr;
            if (build(&g, &w) != JAOS_OK) { skipped++; continue; }
            if (formats[k].write(w, formats[k].path) != JAOS_OK) {
                refused++;
                jaos_model_free(w);
                continue;
            }
            jaos_model_free(w);

            jaos_model *b = nullptr;
            if (jaos_model_new(&b) != JAOS_OK) { skipped++; continue; }
            const jaos_status rr = formats[k].read(b, formats[k].path);
            if (rr != JAOS_OK) {
                printf("READ t=%" PRId64 " fmt=%s rc=%s err=[%s]\n", t,
                       formats[k].name, jaos_status_str(rr),
                       jaos_model_error(b));
                bad++;
                if (dump != nullptr) show(&g);
                jaos_model_free(b);
                continue;
            }
            if (jaos_set_work_limit(b, 20000000) != JAOS_OK ||
                jaos_set_mip_node_limit(b, 200000) != JAOS_OK) {
                jaos_model_free(b); skipped++; continue;
            }
            answer rb;
            if (!answer_of(b, &rb)) { jaos_model_free(b); skipped++; continue; }
            tried++;

            if (ra.status != rb.status) {
                printf("STATUS t=%" PRId64 " fmt=%s whole=%s back=%s\n", t,
                       formats[k].name, jaos_solve_status_str(ra.status),
                       jaos_solve_status_str(rb.status));
                bad++;
                if (dump != nullptr) show(&g);
            } else if (ra.status == JAOS_SOLVE_OPTIMAL) {
                const double s = fabs(ra.obj) > 1.0 ? fabs(ra.obj) : 1.0;
                if (fabs(ra.obj - rb.obj) > 1e-6 * s) {
                    printf("OBJ t=%" PRId64 " fmt=%s whole=%.17g back=%.17g\n",
                           t, formats[k].name, ra.obj, rb.obj);
                    bad++;
                    if (dump != nullptr) show(&g);
                } else if (formats[k].keeps_order) {
                    node_checked++;
                    if (ra.nodes != rb.nodes) {
                        printf("NODES t=%" PRId64 " fmt=%s whole=%" PRId64
                               " back=%" PRId64 " obj=%.17g\n", t,
                               formats[k].name, ra.nodes, rb.nodes, ra.obj);
                        bad++;
                        node_only++;
                        if (dump != nullptr) show(&g);
                    }
                    if (ra.nodes > 0)
                        node_tree++;
                }
            }
            jaos_model_free(b);
        }
        jaos_model_free(a);
    }
    printf("built=%" PRId64 " round_trips=%" PRId64 " refused=%" PRId64
           " skipped=%" PRId64 " bad=%" PRId64 " (nodes only %" PRId64 ")"
           " node_checked=%" PRId64 " of_those_used_the_tree=%" PRId64 "\n",
           built, tried, refused, skipped, bad, node_only, node_checked,
           node_tree);
    return bad != 0 ? 1 : 0;
}
