/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "jaos_sys.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { CB_F, CB_LPLUS, CB_LMINUS, CB_LZERO, CB_Q, CB_QR } cb_kind;

typedef struct {
    cb_kind kind;
    int64_t dim;
} cb_block;

typedef struct {
    jaos_model *m;
    char *buf;
    int64_t len, pos, line;
    int64_t version;
    jaos_obj_sense sense;
    bool have_sense, have_var, have_con, have_int, have_obja, have_objb;
    bool have_a, have_b;
    int64_t nvar, ncon;
    cb_block *vb, *cb;
    int64_t nvb, ncb;
    bool *cint;
    double *cost;
    bool *cost_set;
    double offset;
    int64_t *ai, *aj, *aline;
    double *av;
    int64_t na;
    double *b;
    bool *b_set;
} cbf;

#define FAIL(...) \
    do { \
        jm_set_err(p->m, __VA_ARGS__); \
        return JAOS_ERR_INVALID_INPUT; \
    } while (0)

#define FAIL_OOM() \
    do { \
        jm_set_err(p->m, "out of memory"); \
        return JAOS_ERR_OUT_OF_MEMORY; \
    } while (0)

static bool cb_line(cbf *p, char **out)
{
    while (p->pos < p->len) {
        char *s = p->buf + p->pos;
        char *e = memchr(s, '\n', (size_t)(p->len - p->pos));
        if (e == nullptr) {
            p->pos = p->len;
        } else {
            *e = '\0';
            p->pos = (e - p->buf) + 1;
        }
        p->line++;
        size_t n = strlen(s);
        while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                         s[n - 1] == '\r'))
            s[--n] = '\0';
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '\0' || *s == '#')
            continue;
        *out = s;
        return true;
    }
    return false;
}

static int cb_split(char *s, char **tok, int most)
{
    int n = 0;
    while (*s != '\0') {
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '\0')
            break;
        if (n == most)
            return most + 1;
        tok[n++] = s;
        while (*s != '\0' && *s != ' ' && *s != '\t')
            s++;
        if (*s != '\0')
            *s++ = '\0';
    }
    return n;
}

static bool cb_int(const char *s, int64_t *out)
{
    char *end = nullptr;
    const long long v = strtoll(s, &end, 10);
    if (end == s || *end != '\0')
        return false;
    *out = (int64_t)v;
    return true;
}

static bool cb_real(const char *s, double *out)
{
    char *end = nullptr;
    const double v = strtod(s, &end);
    if (end == s || *end != '\0' || !isfinite(v))
        return false;
    *out = v;
    return true;
}

static jaos_status cb_want(cbf *p, const char *what, char **tok, int count)
{
    char *s;
    if (!cb_line(p, &s))
        FAIL("line %" PRId64 ": the file ends where %s was expected", p->line,
             what);
    if (cb_split(s, tok, count) != count)
        FAIL("line %" PRId64 ": %s takes %d field%s", p->line, what, count,
             count == 1 ? "" : "s");
    return JAOS_OK;
}

static jaos_status cb_count(cbf *p, const char *what, int64_t *out)
{
    char *tok[1];
    jaos_status st = cb_want(p, what, tok, 1);
    if (st != JAOS_OK)
        return st;
    if (!cb_int(tok[0], out) || *out < 0)
        FAIL("line %" PRId64 ": %s is '%s', not a count", p->line, what,
             tok[0]);
    return JAOS_OK;
}

static jaos_status cb_cone_name(cbf *p, const char *s, cb_kind *out)
{
    if (strcmp(s, "F") == 0)
        *out = CB_F;
    else if (strcmp(s, "L+") == 0)
        *out = CB_LPLUS;
    else if (strcmp(s, "L-") == 0)
        *out = CB_LMINUS;
    else if (strcmp(s, "L=") == 0)
        *out = CB_LZERO;
    else if (strcmp(s, "Q") == 0)
        *out = CB_Q;
    else if (strcmp(s, "QR") == 0)
        *out = CB_QR;
    else
        FAIL("line %" PRId64 ": cone '%s' is not one JAOS carries; it takes "
             "F, L+, L-, L=, Q and QR", p->line, s);
    return JAOS_OK;
}

static jaos_status cb_blocks(cbf *p, const char *what, int64_t *total,
                             cb_block **out, int64_t *nout)
{
    char *tok[2];
    jaos_status st = cb_want(p, what, tok, 2);
    if (st != JAOS_OK)
        return st;
    int64_t k;
    if (!cb_int(tok[0], total) || *total < 0 || !cb_int(tok[1], &k) || k < 0)
        FAIL("line %" PRId64 ": %s's header is two counts", p->line, what);
    *out = jm_alloc_array(k > 0 ? k : 1, sizeof **out);
    if (*out == nullptr)
        FAIL_OOM();
    int64_t sum = 0;
    for (int64_t t = 0; t < k; t++) {
        if ((st = cb_want(p, what, tok, 2)) != JAOS_OK)
            return st;
        cb_kind kind;
        if ((st = cb_cone_name(p, tok[0], &kind)) != JAOS_OK)
            return st;
        int64_t d;
        if (!cb_int(tok[1], &d) || d < 1)
            FAIL("line %" PRId64 ": a cone's size is '%s', not a count of 1 "
                 "or more", p->line, tok[1]);
        if (kind == CB_QR && d < 2)
            FAIL("line %" PRId64 ": a QR cone takes 2 members or more",
                 p->line);
        (*out)[t] = (cb_block){kind, d};
        sum += d;
    }
    if (sum != *total)
        FAIL("line %" PRId64 ": the cones of %s hold %" PRId64 " entries and "
             "the header says %" PRId64, p->line, what, sum, *total);
    *nout = k;
    return JAOS_OK;
}

static jaos_status cb_parse(cbf *p)
{
    jaos_status st = JAOS_OK;
    char *s, *tok[3];
    bool first = true;
    while (cb_line(p, &s)) {
        const int64_t at = p->line;
        if (first && strcmp(s, "VER") != 0)
            FAIL("line %" PRId64 ": a CBF file starts with VER", at);
        first = false;
        if (strcmp(s, "VER") == 0) {
            if (p->version > 0)
                FAIL("line %" PRId64 ": a second VER", at);
            if ((st = cb_count(p, "VER", &p->version)) != JAOS_OK)
                return st;
            if (p->version < 1 || p->version > 3)
                FAIL("line %" PRId64 ": version %" PRId64 "; JAOS reads CBF "
                     "versions 1 to 3", p->line, p->version);
        } else if (strcmp(s, "OBJSENSE") == 0) {
            if (p->have_sense)
                FAIL("line %" PRId64 ": a second OBJSENSE", at);
            if ((st = cb_want(p, "OBJSENSE", tok, 1)) != JAOS_OK)
                return st;
            if (strcmp(tok[0], "MIN") == 0)
                p->sense = JAOS_MINIMIZE;
            else if (strcmp(tok[0], "MAX") == 0)
                p->sense = JAOS_MAXIMIZE;
            else
                FAIL("line %" PRId64 ": OBJSENSE is MIN or MAX, not '%s'",
                     p->line, tok[0]);
            p->have_sense = true;
        } else if (strcmp(s, "VAR") == 0) {
            if (p->have_var)
                FAIL("line %" PRId64 ": a second VAR", at);
            if (p->have_con || p->have_obja || p->have_a)
                FAIL("line %" PRId64 ": VAR after CON or the data; the "
                     "variables come first", at);
            if ((st = cb_blocks(p, "VAR", &p->nvar, &p->vb, &p->nvb)) !=
                JAOS_OK)
                return st;
            const int64_t n = p->nvar > 0 ? p->nvar : 1;
            p->cint = jm_calloc_array(n, sizeof *p->cint);
            p->cost = jm_calloc_array(n, sizeof *p->cost);
            p->cost_set = jm_calloc_array(n, sizeof *p->cost_set);
            if (p->cint == nullptr || p->cost == nullptr ||
                p->cost_set == nullptr)
                FAIL_OOM();
            p->have_var = true;
        } else if (strcmp(s, "INT") == 0) {
            if (p->have_int)
                FAIL("line %" PRId64 ": a second INT", at);
            if (!p->have_var)
                FAIL("line %" PRId64 ": INT before VAR", at);
            int64_t k;
            if ((st = cb_count(p, "INT", &k)) != JAOS_OK)
                return st;
            for (int64_t t = 0; t < k; t++) {
                if ((st = cb_want(p, "an INT index", tok, 1)) != JAOS_OK)
                    return st;
                int64_t j;
                if (!cb_int(tok[0], &j) || j < 0 || j >= p->nvar)
                    FAIL("line %" PRId64 ": INT names variable '%s', and "
                         "there are %" PRId64, p->line, tok[0], p->nvar);
                p->cint[j] = true;
            }
            p->have_int = true;
        } else if (strcmp(s, "CON") == 0) {
            if (p->have_con)
                FAIL("line %" PRId64 ": a second CON", at);
            if (p->have_obja || p->have_a || p->have_b)
                FAIL("line %" PRId64 ": CON after the data; the structure "
                     "comes first", at);
            if ((st = cb_blocks(p, "CON", &p->ncon, &p->cb, &p->ncb)) !=
                JAOS_OK)
                return st;
            const int64_t n = p->ncon > 0 ? p->ncon : 1;
            p->b = jm_calloc_array(n, sizeof *p->b);
            p->b_set = jm_calloc_array(n, sizeof *p->b_set);
            if (p->b == nullptr || p->b_set == nullptr)
                FAIL_OOM();
            p->have_con = true;
        } else if (strcmp(s, "OBJACOORD") == 0) {
            if (p->have_obja)
                FAIL("line %" PRId64 ": a second OBJACOORD", at);
            if (!p->have_var)
                FAIL("line %" PRId64 ": OBJACOORD before VAR", at);
            int64_t k;
            if ((st = cb_count(p, "OBJACOORD", &k)) != JAOS_OK)
                return st;
            for (int64_t t = 0; t < k; t++) {
                if ((st = cb_want(p, "an OBJACOORD entry", tok, 2)) !=
                    JAOS_OK)
                    return st;
                int64_t j;
                double v;
                if (!cb_int(tok[0], &j) || j < 0 || j >= p->nvar)
                    FAIL("line %" PRId64 ": OBJACOORD names variable '%s', "
                         "and there are %" PRId64, p->line, tok[0], p->nvar);
                if (!cb_real(tok[1], &v))
                    FAIL("line %" PRId64 ": '%s' is not a finite number",
                         p->line, tok[1]);
                if (p->cost_set[j])
                    FAIL("line %" PRId64 ": OBJACOORD gives variable %" PRId64
                         " a second coefficient", p->line, j);
                p->cost_set[j] = true;
                p->cost[j] = v;
            }
            p->have_obja = true;
        } else if (strcmp(s, "OBJBCOORD") == 0) {
            if (p->have_objb)
                FAIL("line %" PRId64 ": a second OBJBCOORD", at);
            if ((st = cb_want(p, "OBJBCOORD", tok, 1)) != JAOS_OK)
                return st;
            if (!cb_real(tok[0], &p->offset))
                FAIL("line %" PRId64 ": '%s' is not a finite number", p->line,
                     tok[0]);
            p->have_objb = true;
        } else if (strcmp(s, "ACOORD") == 0) {
            if (p->have_a)
                FAIL("line %" PRId64 ": a second ACOORD", at);
            if (!p->have_var || !p->have_con)
                FAIL("line %" PRId64 ": ACOORD before VAR and CON", at);
            int64_t k;
            if ((st = cb_count(p, "ACOORD", &k)) != JAOS_OK)
                return st;
            const int64_t n = k > 0 ? k : 1;
            p->ai = jm_alloc_array(n, sizeof *p->ai);
            p->aj = jm_alloc_array(n, sizeof *p->aj);
            p->av = jm_alloc_array(n, sizeof *p->av);
            p->aline = jm_alloc_array(n, sizeof *p->aline);
            if (p->ai == nullptr || p->aj == nullptr || p->av == nullptr ||
                p->aline == nullptr)
                FAIL_OOM();
            for (int64_t t = 0; t < k; t++) {
                if ((st = cb_want(p, "an ACOORD entry", tok, 3)) != JAOS_OK)
                    return st;
                int64_t i, j;
                double v;
                if (!cb_int(tok[0], &i) || i < 0 || i >= p->ncon)
                    FAIL("line %" PRId64 ": ACOORD names constraint '%s', and "
                         "there are %" PRId64, p->line, tok[0], p->ncon);
                if (!cb_int(tok[1], &j) || j < 0 || j >= p->nvar)
                    FAIL("line %" PRId64 ": ACOORD names variable '%s', and "
                         "there are %" PRId64, p->line, tok[1], p->nvar);
                if (!cb_real(tok[2], &v))
                    FAIL("line %" PRId64 ": '%s' is not a finite number",
                         p->line, tok[2]);
                p->ai[p->na] = i;
                p->aj[p->na] = j;
                p->av[p->na] = v;
                p->aline[p->na++] = p->line;
            }
            p->have_a = true;
        } else if (strcmp(s, "BCOORD") == 0) {
            if (p->have_b)
                FAIL("line %" PRId64 ": a second BCOORD", at);
            if (!p->have_con)
                FAIL("line %" PRId64 ": BCOORD before CON", at);
            int64_t k;
            if ((st = cb_count(p, "BCOORD", &k)) != JAOS_OK)
                return st;
            for (int64_t t = 0; t < k; t++) {
                if ((st = cb_want(p, "a BCOORD entry", tok, 2)) != JAOS_OK)
                    return st;
                int64_t i;
                double v;
                if (!cb_int(tok[0], &i) || i < 0 || i >= p->ncon)
                    FAIL("line %" PRId64 ": BCOORD names constraint '%s', and "
                         "there are %" PRId64, p->line, tok[0], p->ncon);
                if (!cb_real(tok[1], &v))
                    FAIL("line %" PRId64 ": '%s' is not a finite number",
                         p->line, tok[1]);
                if (p->b_set[i])
                    FAIL("line %" PRId64 ": BCOORD gives constraint %" PRId64
                         " a second constant", p->line, i);
                p->b_set[i] = true;
                p->b[i] = v;
            }
            p->have_b = true;
        } else if (strcmp(s, "CHANGE") == 0) {
            break;
        } else if (strcmp(s, "PSDVAR") == 0 || strcmp(s, "PSDCON") == 0 ||
                   strcmp(s, "OBJFCOORD") == 0 || strcmp(s, "FCOORD") == 0 ||
                   strcmp(s, "HCOORD") == 0 || strcmp(s, "DCOORD") == 0) {
            FAIL("line %" PRId64 ": %s holds semidefinite data, which JAOS "
                 "does not carry", at, s);
        } else if (strcmp(s, "POWCONES") == 0 || strcmp(s, "POW*CONES") == 0) {
            FAIL("line %" PRId64 ": %s defines power cones, which JAOS does "
                 "not carry", at, s);
        } else {
            FAIL("line %" PRId64 ": '%s' is not a CBF keyword", at, s);
        }
    }
    if (first)
        FAIL("the file holds no CBF keyword");
    if (!p->have_sense)
        FAIL("the file has no OBJSENSE");
    return JAOS_OK;
}

typedef struct {
    int64_t i, j, line;
    double v;
} cb_ent;

static int cmp_cb_ent(const void *pa, const void *pb)
{
    const cb_ent *a = pa, *b = pb;
    if (a->j != b->j)
        return (a->j > b->j) - (a->j < b->j);
    if (a->i != b->i)
        return (a->i > b->i) - (a->i < b->i);
    return (a->line > b->line) - (a->line < b->line);
}

static jaos_status cb_build(cbf *p)
{
    jaos_model *m = p->m;
    const int64_t nv = p->nvar, nc = p->ncon;
    jaos_status st = JAOS_OK;
    int64_t *cnt = jm_calloc_array(nc > 0 ? nc : 1, sizeof *cnt);
    int64_t *one = jm_alloc_array(nc > 0 ? nc : 1, sizeof *one);
    bool *direct = jm_calloc_array(p->ncb > 0 ? p->ncb : 1, sizeof *direct);
    bool *seen = jm_calloc_array(nv > 0 ? nv : 1, sizeof *seen);
    int64_t *row_of = jm_alloc_array(nc > 0 ? nc : 1, sizeof *row_of);
    int64_t *aux_of = jm_alloc_array(nc > 0 ? nc : 1, sizeof *aux_of);
    cb_ent *ent = nullptr;
    double *cost = nullptr, *cl = nullptr, *cu = nullptr, *rl = nullptr;
    double *ru = nullptr, *av = nullptr;
    int64_t *as = nullptr, *ai = nullptr, *cols = nullptr;
    bool *cint = nullptr;
    if (cnt == nullptr || one == nullptr || direct == nullptr ||
        seen == nullptr || row_of == nullptr || aux_of == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    ent = jm_alloc_array(p->na > 0 ? p->na : 1, sizeof *ent);
    if (ent == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    for (int64_t t = 0; t < p->na; t++)
        ent[t] = (cb_ent){p->ai[t], p->aj[t], p->aline[t], p->av[t]};
    qsort(ent, (size_t)p->na, sizeof *ent, cmp_cb_ent);
    for (int64_t t = 1; t < p->na; t++)
        if (ent[t].i == ent[t - 1].i && ent[t].j == ent[t - 1].j) {
            jm_set_err(m, "line %" PRId64 ": ACOORD gives constraint %" PRId64
                          " and variable %" PRId64 " a second coefficient",
                       ent[t].line, ent[t].i, ent[t].j);
            st = JAOS_ERR_INVALID_INPUT;
            goto out;
        }
    for (int64_t t = 0; t < p->na; t++) {
        if (ent[t].v == 0.0)
            continue;
        cnt[ent[t].i]++;
        one[ent[t].i] = t;
    }

    int64_t nrow = 0, naux = 0, base = 0;
    for (int64_t k = 0; k < p->ncb; k++) {
        const cb_block *blk = &p->cb[k];
        if (blk->kind == CB_Q || blk->kind == CB_QR) {
            bool ok = true;
            for (int64_t i = base; i < base + blk->dim && ok; i++) {
                ok = cnt[i] == 1 && ent[one[i]].v == 1.0 && p->b[i] == 0.0 &&
                     !seen[ent[one[i]].j];
                if (ok)
                    seen[ent[one[i]].j] = true;
            }
            for (int64_t i = base; i < base + blk->dim; i++)
                if (cnt[i] == 1)
                    seen[ent[one[i]].j] = false;
            direct[k] = ok;
        }
        for (int64_t i = base; i < base + blk->dim; i++) {
            row_of[i] = -1;
            aux_of[i] = -1;
            if (direct[k])
                continue;
            row_of[i] = nrow++;
            if (blk->kind == CB_Q || blk->kind == CB_QR)
                aux_of[i] = nv + naux++;
        }
        base += blk->dim;
    }
    const int64_t n = nv + naux;
    cost = jm_calloc_array(n > 0 ? n : 1, sizeof *cost);
    cl = jm_alloc_array(n > 0 ? n : 1, sizeof *cl);
    cu = jm_alloc_array(n > 0 ? n : 1, sizeof *cu);
    rl = jm_alloc_array(nrow > 0 ? nrow : 1, sizeof *rl);
    ru = jm_alloc_array(nrow > 0 ? nrow : 1, sizeof *ru);
    cint = jm_calloc_array(n > 0 ? n : 1, sizeof *cint);
    as = jm_calloc_array(n + 1, sizeof *as);
    ai = jm_alloc_array(p->na + naux > 0 ? p->na + naux : 1, sizeof *ai);
    av = jm_alloc_array(p->na + naux > 0 ? p->na + naux : 1, sizeof *av);
    cols = jm_alloc_array(n > 0 ? n : 1, sizeof *cols);
    if (cost == nullptr || cl == nullptr || cu == nullptr || rl == nullptr ||
        ru == nullptr || cint == nullptr || as == nullptr || ai == nullptr ||
        av == nullptr || cols == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    base = 0;
    for (int64_t k = 0; k < p->nvb; k++) {
        const cb_block *blk = &p->vb[k];
        for (int64_t j = base; j < base + blk->dim; j++) {
            cl[j] = blk->kind == CB_LPLUS || blk->kind == CB_LZERO ? 0.0
                                                                   : -INFINITY;
            cu[j] = blk->kind == CB_LMINUS || blk->kind == CB_LZERO ? 0.0
                                                                    : INFINITY;
        }
        base += blk->dim;
    }
    for (int64_t j = nv; j < n; j++) {
        cl[j] = -INFINITY;
        cu[j] = INFINITY;
    }
    for (int64_t j = 0; j < nv; j++) {
        cost[j] = p->cost[j];
        cint[j] = p->cint[j];
    }
    base = 0;
    for (int64_t k = 0; k < p->ncb; k++) {
        const cb_block *blk = &p->cb[k];
        for (int64_t i = base; i < base + blk->dim; i++) {
            const int64_t r = row_of[i];
            if (r < 0)
                continue;
            const double c = -p->b[i];
            rl[r] = blk->kind == CB_LMINUS || blk->kind == CB_F ? -INFINITY
                                                                : c;
            ru[r] = blk->kind == CB_LPLUS || blk->kind == CB_F ? INFINITY : c;
        }
        base += blk->dim;
    }
    int64_t nz = 0;
    for (int64_t t = 0; t < p->na; t++)
        if (ent[t].v != 0.0 && row_of[ent[t].i] >= 0) {
            as[ent[t].j + 1]++;
            nz++;
        }
    for (int64_t i = 0; i < nc; i++)
        if (aux_of[i] >= 0) {
            as[aux_of[i] + 1]++;
            nz++;
        }
    for (int64_t j = 0; j < n; j++)
        as[j + 1] += as[j];
    for (int64_t j = 0; j < n; j++)
        cols[j] = as[j];
    for (int64_t t = 0; t < p->na; t++)
        if (ent[t].v != 0.0 && row_of[ent[t].i] >= 0) {
            const int64_t at = cols[ent[t].j]++;
            ai[at] = row_of[ent[t].i];
            av[at] = ent[t].v;
        }
    for (int64_t i = 0; i < nc; i++)
        if (aux_of[i] >= 0) {
            const int64_t at = cols[aux_of[i]]++;
            ai[at] = row_of[i];
            av[at] = -1.0;
        }

    st = jaos_load_lp(m, n, nrow, p->sense, p->offset, cost, cl, cu, rl, ru,
                      nz, as, ai, av);
    if (st != JAOS_OK) {
        if (m->err[0] == '\0')
            jm_set_err(m, "the CBF model failed validation");
        goto out;
    }
    bool any_int = false;
    for (int64_t j = 0; j < n; j++)
        any_int |= cint[j];
    if (any_int) {
        free(m->col_integer);
        m->col_integer = cint;
        cint = nullptr;
    }
    base = 0;
    for (int64_t k = 0; k < p->nvb && st == JAOS_OK; k++) {
        const cb_block *blk = &p->vb[k];
        if (blk->kind == CB_Q || blk->kind == CB_QR) {
            for (int64_t t = 0; t < blk->dim; t++)
                cols[t] = base + t;
            st = jaos_add_cone(m, blk->kind == CB_QR ? JAOS_CONE_ROTATED
                                                     : JAOS_CONE_QUADRATIC,
                               blk->dim, cols);
        }
        base += blk->dim;
    }
    base = 0;
    for (int64_t k = 0; k < p->ncb && st == JAOS_OK; k++) {
        const cb_block *blk = &p->cb[k];
        if (blk->kind == CB_Q || blk->kind == CB_QR) {
            for (int64_t t = 0; t < blk->dim; t++)
                cols[t] = direct[k] ? ent[one[base + t]].j : aux_of[base + t];
            st = jaos_add_cone(m, blk->kind == CB_QR ? JAOS_CONE_ROTATED
                                                     : JAOS_CONE_QUADRATIC,
                               blk->dim, cols);
        }
        base += blk->dim;
    }
    if (st == JAOS_OK)
        m->err[0] = '\0';

out:
    free(cnt); free(one); free(direct); free(seen); free(row_of);
    free(aux_of); free(ent); free(cost); free(cl); free(cu); free(rl);
    free(ru); free(cint); free(as); free(ai); free(av); free(cols);
    return st;
}

static void cb_free(cbf *p)
{
    free(p->buf);
    free(p->vb);
    free(p->cb);
    free(p->cint);
    free(p->cost);
    free(p->cost_set);
    free(p->ai);
    free(p->aj);
    free(p->av);
    free(p->aline);
    free(p->b);
    free(p->b_set);
}

jaos_status jaos_read_cbf(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    cbf pp = {0};
    cbf *p = &pp;
    p->m = m;
    p->sense = JAOS_MINIMIZE;
    jaos_status st = jm_slurp(m, path, &p->buf, &p->len);
    if (st != JAOS_OK)
        return st;
    {
        jm_locale loc;
        jm_locale_c_enter(&loc);
        st = cb_parse(p);
        if (st == JAOS_OK)
            st = cb_build(p);
        jm_locale_leave(&loc);
    }
    cb_free(p);
    return st;
}
