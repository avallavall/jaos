/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "jaos_sys.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    jaos_model *m;
    char *buf;
    int64_t len, pos, line;
    int64_t nvar, ncon, nobj, nbv, niv;
    double *cost, *cl, *cu, *rl, *ru, *shift;
    int64_t *ei, *ej;
    double *ev;
    int64_t nent, ecap, jcap, vcap;
    double offset;
    jaos_obj_sense sense;
    int nopt;
    int64_t opt[JM_NL_OPTIONS];
    double *x0;
    int64_t *qr, *qi, *qj;
    double *qv;
    int64_t nq, qrcap, qicap, qjcap, qvcap;
} nl;

typedef struct {
    int64_t i, j;
    double v;
} nl_term;

typedef struct {
    double c;
    nl_term *t;
    int64_t n, cap;
} nl_form;

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

static bool nl_next(nl *p, char **out)
{
    if (p->pos >= p->len)
        return false;
    char *s = p->buf + p->pos;
    char *e = memchr(s, '\n', (size_t)(p->len - p->pos));
    if (e == nullptr) {
        p->pos = p->len;
    } else {
        *e = '\0';
        p->pos = (e - p->buf) + 1;
    }
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\r')
        s[n - 1] = '\0';
    p->line++;
    *out = s;
    return true;
}

static int nl_ints(const char *s, int64_t *v, int n)
{
    int got = 0;
    const char *q = s;
    while (got < n) {
        char *end;
        const long long x = strtoll(q, &end, 10);
        if (end == q)
            break;
        v[got++] = x;
        q = end;
    }
    return got;
}

static int nl_nums(const char *s, double *v, int n)
{
    int got = 0;
    const char *q = s;
    while (got < n) {
        char *end;
        const double x = strtod(q, &end);
        if (end == q)
            break;
        v[got++] = x;
        q = end;
    }
    return got;
}

static void form_free(nl_form *f)
{
    free(f->t);
    f->t = nullptr;
    f->n = f->cap = 0;
    f->c = 0.0;
}

static bool form_put(nl_form *f, int64_t i, int64_t j, double v)
{
    if (v == 0.0)
        return true;
    if (!JM_GROW(f->t, f->cap, f->n + 1))
        return false;
    f->t[f->n].i = i;
    f->t[f->n].j = j;
    f->t[f->n++].v = v;
    return true;
}

static bool form_add(nl_form *a, const nl_form *b, double s)
{
    a->c += s * b->c;
    for (int64_t k = 0; k < b->n; k++)
        if (!form_put(a, b->t[k].i, b->t[k].j, s * b->t[k].v))
            return false;
    return true;
}

static int form_mul(nl_form *out, const nl_form *a, const nl_form *b)
{
    out->c = a->c * b->c;
    for (int64_t k = 0; k < a->n; k++)
        if (!form_put(out, a->t[k].i, a->t[k].j, a->t[k].v * b->c))
            return -1;
    for (int64_t k = 0; k < b->n; k++)
        if (!form_put(out, b->t[k].i, b->t[k].j, b->t[k].v * a->c))
            return -1;
    for (int64_t ka = 0; ka < a->n; ka++)
        for (int64_t kb = 0; kb < b->n; kb++) {
            if (a->t[ka].j >= 0 || b->t[kb].j >= 0)
                return 1;
            const int64_t x = a->t[ka].i, y = b->t[kb].i;
            if (!form_put(out, x > y ? x : y, x > y ? y : x,
                          a->t[ka].v * b->t[kb].v))
                return -1;
        }
    return 0;
}

static jaos_status nl_body(nl *p, const char *what, int64_t which, nl_form *f);

static jaos_status nl_pair(nl *p, const char *what, int64_t which,
                           nl_form *a, nl_form *b)
{
    jaos_status st = nl_body(p, what, which, a);
    if (st == JAOS_OK)
        st = nl_body(p, what, which, b);
    return st;
}

static jaos_status nl_body(nl *p, const char *what, int64_t which, nl_form *f)
{
    char *s;
    if (!nl_next(p, &s))
        FAIL("line %" PRId64 ": %s %" PRId64 " has no expression", p->line,
             what, which);
    if (s[0] == 'n') {
        if (nl_nums(s + 1, &f->c, 1) != 1 || !isfinite(f->c))
            FAIL("line %" PRId64 ": %s %" PRId64 " has a constant JAOS "
                 "cannot read", p->line, what, which);
        return JAOS_OK;
    }
    if (s[0] == 'v') {
        int64_t j;
        if (nl_ints(s + 1, &j, 1) != 1 || j < 0 || j >= p->nvar)
            FAIL("line %" PRId64 ": %s %" PRId64 " names a column outside "
                 "the %" PRId64 " the header declares", p->line, what, which,
                 p->nvar);
        if (!form_put(f, j, -1, 1.0))
            FAIL_OOM();
        return JAOS_OK;
    }
    if (s[0] != 'o') {
        FAIL("line %" PRId64 ": %s %" PRId64 " has a body starting '%c', "
             "which JAOS does not read", p->line, what, which, s[0]);
    }
    int64_t op;
    if (nl_ints(s + 1, &op, 1) != 1)
        FAIL("line %" PRId64 ": %s %" PRId64 " has an operator with no "
             "number", p->line, what, which);
    nl_form a = {0}, b = {0};
    jaos_status st = JAOS_OK;
    int deg = 0;
    switch (op) {
    case 0:
    case 1:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK && (!form_add(f, &a, 1.0) ||
                              !form_add(f, &b, op == 0 ? 1.0 : -1.0)))
            st = JAOS_ERR_OUT_OF_MEMORY;
        break;
    case 2:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK) {
            deg = form_mul(f, &a, &b);
            if (deg < 0)
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        break;
    case 3:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK && (b.n > 0 || b.c == 0.0)) {
            form_free(&a);
            form_free(&b);
            FAIL("line %" PRId64 ": %s %" PRId64 " divides by something "
                 "other than a nonzero constant", p->line, what, which);
        }
        if (st == JAOS_OK && !form_add(f, &a, 1.0 / b.c))
            st = JAOS_ERR_OUT_OF_MEMORY;
        break;
    case 5:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK && (b.n > 0 || (b.c != 1.0 && b.c != 2.0))) {
            form_free(&a);
            form_free(&b);
            FAIL("line %" PRId64 ": %s %" PRId64 " takes a power other than "
                 "one or two", p->line, what, which);
        }
        if (st == JAOS_OK && b.c == 1.0 && !form_add(f, &a, 1.0))
            st = JAOS_ERR_OUT_OF_MEMORY;
        if (st == JAOS_OK && b.c == 2.0) {
            deg = form_mul(f, &a, &a);
            if (deg < 0)
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        break;
    case 16:
        st = nl_body(p, what, which, &a);
        if (st == JAOS_OK && !form_add(f, &a, -1.0))
            st = JAOS_ERR_OUT_OF_MEMORY;
        break;
    case 54: {
        char *t;
        int64_t n;
        if (!nl_next(p, &t) || nl_ints(t, &n, 1) != 1 || n < 0)
            FAIL("line %" PRId64 ": %s %" PRId64 "'s sum has no count",
                 p->line, what, which);
        for (int64_t k = 0; k < n && st == JAOS_OK; k++) {
            form_free(&a);
            st = nl_body(p, what, which, &a);
            if (st == JAOS_OK && !form_add(f, &a, 1.0))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        break;
    }
    default:
        FAIL("line %" PRId64 ": %s %" PRId64 " uses operator o%" PRId64
             ", which JAOS does not read; it takes a body of degree two or "
             "less over + - * / ^ and sumlist", p->line, what, which, op);
    }
    form_free(&a);
    form_free(&b);
    if (st == JAOS_ERR_OUT_OF_MEMORY)
        FAIL_OOM();
    if (st != JAOS_OK)
        return st;
    if (deg > 0)
        FAIL("line %" PRId64 ": %s %" PRId64 " has a product of degree "
             "three or more, and JAOS reads a quadratic body at most",
             p->line, what, which);
    return JAOS_OK;
}

static bool nl_keep_quad(nl *p, int64_t row, const nl_form *f)
{
    for (int64_t k = 0; k < f->n; k++) {
        if (f->t[k].j < 0)
            continue;
        if (!JM_GROW(p->qr, p->qrcap, p->nq + 1) ||
            !JM_GROW(p->qi, p->qicap, p->nq + 1) ||
            !JM_GROW(p->qj, p->qjcap, p->nq + 1) ||
            !JM_GROW(p->qv, p->qvcap, p->nq + 1))
            return false;
        p->qr[p->nq] = row;
        p->qi[p->nq] = f->t[k].i;
        p->qj[p->nq] = f->t[k].j;
        p->qv[p->nq++] = f->t[k].v;
    }
    return true;
}

static jaos_status nl_bounds(nl *p, const char *what, int64_t which,
                             double *lo, double *hi)
{
    char *s;
    if (!nl_next(p, &s))
        FAIL("line %" PRId64 ": bounds of %s %" PRId64 " are missing",
             p->line, what, which);
    int64_t t;
    if (nl_ints(s, &t, 1) != 1)
        FAIL("line %" PRId64 ": bounds of %s %" PRId64 " have no type",
             p->line, what, which);
    const char *q = s;
    while (*q == ' ' || *q == '\t')
        q++;
    while (*q >= '0' && *q <= '9')
        q++;
    double v[2];
    switch (t) {
    case 0:
        if (nl_nums(q, v, 2) != 2)
            FAIL("line %" PRId64 ": bounds of %s %" PRId64 " need two "
                 "numbers", p->line, what, which);
        *lo = v[0];
        *hi = v[1];
        return JAOS_OK;
    case 1:
        if (nl_nums(q, v, 1) != 1)
            FAIL("line %" PRId64 ": bounds of %s %" PRId64 " need a number",
                 p->line, what, which);
        *lo = -INFINITY;
        *hi = v[0];
        return JAOS_OK;
    case 2:
        if (nl_nums(q, v, 1) != 1)
            FAIL("line %" PRId64 ": bounds of %s %" PRId64 " need a number",
                 p->line, what, which);
        *lo = v[0];
        *hi = INFINITY;
        return JAOS_OK;
    case 3:
        *lo = -INFINITY;
        *hi = INFINITY;
        return JAOS_OK;
    case 4:
        if (nl_nums(q, v, 1) != 1)
            FAIL("line %" PRId64 ": bounds of %s %" PRId64 " need a number",
                 p->line, what, which);
        *lo = v[0];
        *hi = v[0];
        return JAOS_OK;
    case 5:
        FAIL("line %" PRId64 ": %s %" PRId64 " is a complementarity "
             "condition, which JAOS does not read", p->line, what, which);
    default:
        FAIL("line %" PRId64 ": bounds of %s %" PRId64 " have type %" PRId64
             ", which is not one of 0 to 4", p->line, what, which, t);
    }
}

static jaos_status nl_skip(nl *p, int64_t n, const char *what)
{
    for (int64_t k = 0; k < n; k++) {
        char *s;
        if (!nl_next(p, &s))
            FAIL("line %" PRId64 ": the %s segment ends early", p->line, what);
    }
    return JAOS_OK;
}

static jaos_status nl_entries(nl *p, int64_t i, int64_t n, bool objective)
{
    for (int64_t k = 0; k < n; k++) {
        char *s;
        if (!nl_next(p, &s))
            FAIL("line %" PRId64 ": the coefficients of %s %" PRId64
                 " end early", p->line, objective ? "objective" : "row", i);
        int64_t j;
        double v;
        char *end;
        j = strtoll(s, &end, 10);
        if (end == s || nl_nums(end, &v, 1) != 1)
            FAIL("line %" PRId64 ": expected 'column coefficient'", p->line);
        if (j < 0 || j >= p->nvar)
            FAIL("line %" PRId64 ": column %" PRId64 " is outside the %"
                 PRId64 " the header declares", p->line, j, p->nvar);
        if (!isfinite(v))
            FAIL("line %" PRId64 ": the coefficient is not finite", p->line);
        if (objective) {
            if (i == 0)
                p->cost[j] += v;
            continue;
        }
        if (!JM_GROW(p->ei, p->ecap, p->nent + 1) ||
            !JM_GROW(p->ej, p->jcap, p->nent + 1) ||
            !JM_GROW(p->ev, p->vcap, p->nent + 1))
            FAIL_OOM();
        p->ei[p->nent] = i;
        p->ej[p->nent] = j;
        p->ev[p->nent] = v;
        p->nent++;
    }
    return JAOS_OK;
}

static jaos_status nl_header(nl *p)
{
    char *s;
    int64_t v[6];
    if (!nl_next(p, &s) || s[0] == '\0')
        FAIL("line 1: an .nl file starts with 'g' or 'b'; this one is empty");
    if (s[0] == 'b')
        FAIL("line 1: a binary .nl file; JAOS reads the text form only, "
             "which AMPL writes with 'option nl_comments 0; write g...' or "
             "the -og flag");
    if (s[0] != 'g')
        FAIL("line 1: an .nl file starts with 'g'; this line starts with "
             "'%c'", s[0]);
    {
        int64_t o[JM_NL_OPTIONS + 1];
        const int got = nl_ints(s + 1, o, JM_NL_OPTIONS + 1);
        if (got > 0 && o[0] > 0 && o[0] <= JM_NL_OPTIONS && got > o[0]) {
            p->nopt = (int)o[0];
            memcpy(p->opt, o + 1, (size_t)p->nopt * sizeof *p->opt);
        }
    }
    if (!nl_next(p, &s) || nl_ints(s, v, 3) < 3)
        FAIL("line 2: expected the counts of variables, constraints and "
             "objectives");
    p->nvar = v[0];
    p->ncon = v[1];
    p->nobj = v[2];
    if (p->nvar < 0 || p->ncon < 0 || p->nobj < 0)
        FAIL("line 2: a count is negative");
    if (!jm_declared_fits(p->nvar, p->len) ||
        !jm_declared_fits(p->ncon, p->len))
        FAIL("line 2: %" PRId64 " variables and %" PRId64 " constraints in "
             "a file of %" PRId64 " bytes; JAOS reads a count past %" PRId64
             " only from a file at least that many bytes long",
             p->nvar, p->ncon, p->len, JM_READ_DECLARED_FLOOR);
    p->m->nl_rows = p->ncon;
    p->m->nl_cols = p->nvar;
    p->m->nl_nopt = p->nopt;
    memcpy(p->m->nl_opt, p->opt, sizeof p->m->nl_opt);
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 3: expected the nonlinear constraint and objective counts");
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 4: expected the network constraint counts");
    if (v[0] > 0 || v[1] > 0)
        FAIL("line 4: network constraints, which JAOS does not read");
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 5: expected the nonlinear variable counts");
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 6: expected the linear network variable and function "
             "counts");
    if (v[1] > 0)
        FAIL("line 6: %" PRId64 " user functions, which JAOS does not read",
             v[1]);
    if (!nl_next(p, &s) || nl_ints(s, v, 5) < 2)
        FAIL("line 7: expected the binary and integer variable counts");
    p->nbv = v[0];
    p->niv = v[1];
    {
        const int got = nl_ints(s, v, 5);
        for (int k = 2; k < got; k++)
            if (v[k] > 0)
                FAIL("line 7: integer variables in nonlinear terms; JAOS "
                     "reads linear models only");
    }
    if (p->nbv < 0 || p->niv < 0 || p->nbv > p->nvar ||
        p->niv > p->nvar - p->nbv)
        FAIL("line 7: %" PRId64 " binary and %" PRId64 " integer variables "
             "against %" PRId64 " variables", p->nbv, p->niv, p->nvar);
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 8: expected the nonzero counts");
    if (!nl_next(p, &s))
        FAIL("line 9: expected the name lengths");
    if (!nl_next(p, &s))
        FAIL("line 10: expected the common expression counts");
    {
        const int got = nl_ints(s, v, 5);
        for (int k = 0; k < got; k++)
            if (v[k] > 0)
                FAIL("line 10: defined variables (common expressions), "
                     "which JAOS does not read");
    }
    return JAOS_OK;
}

static jaos_status nl_segments(nl *p)
{
    char *s;
    bool seen_r = false, seen_b = false;
    while (nl_next(p, &s)) {
        if (s[0] == '\0' || s[0] == '#')
            continue;
        int64_t v[3];
        const int got = nl_ints(s + 1, v, 3);
        jaos_status st = JAOS_OK;
        switch (s[0]) {
        case 'C': {
            if (got < 1 || v[0] < 0 || v[0] >= p->ncon)
                FAIL("line %" PRId64 ": 'C' needs a row index below %" PRId64,
                     p->line, p->ncon);
            nl_form f = {0};
            if ((st = nl_body(p, "row", v[0], &f)) != JAOS_OK) {
                form_free(&f);
                return st;
            }
            p->shift[v[0]] = f.c;
            bool ok = nl_keep_quad(p, v[0], &f);
            for (int64_t k = 0; ok && k < f.n; k++) {
                if (f.t[k].j >= 0)
                    continue;
                ok = JM_GROW(p->ei, p->ecap, p->nent + 1) &&
                     JM_GROW(p->ej, p->jcap, p->nent + 1) &&
                     JM_GROW(p->ev, p->vcap, p->nent + 1);
                if (!ok)
                    break;
                p->ei[p->nent] = v[0];
                p->ej[p->nent] = f.t[k].i;
                p->ev[p->nent++] = f.t[k].v;
            }
            form_free(&f);
            if (!ok)
                FAIL_OOM();
            break;
        }
        case 'O': {
            if (got < 2 || v[0] < 0 || v[0] >= p->nobj)
                FAIL("line %" PRId64 ": 'O' needs an objective index below %"
                     PRId64 " and a sense", p->line, p->nobj);
            nl_form f = {0};
            if ((st = nl_body(p, "objective", v[0], &f)) != JAOS_OK) {
                form_free(&f);
                return st;
            }
            bool ok = true;
            if (v[0] == 0) {
                p->offset = f.c;
                p->sense = v[1] == 1 ? JAOS_MAXIMIZE : JAOS_MINIMIZE;
                ok = nl_keep_quad(p, -1, &f);
                for (int64_t k = 0; ok && k < f.n; k++)
                    if (f.t[k].j < 0)
                        p->cost[f.t[k].i] += f.t[k].v;
            }
            form_free(&f);
            if (!ok)
                FAIL_OOM();
            break;
        }
        case 'x':
            if (got < 1 || v[0] < 0)
                FAIL("line %" PRId64 ": 'x' needs a count", p->line);
            if (v[0] > 0 && p->x0 == nullptr &&
                (p->x0 = jm_calloc_array(p->nvar > 0 ? p->nvar : 1,
                                         sizeof *p->x0)) == nullptr)
                FAIL_OOM();
            for (int64_t k = 0; k < v[0]; k++) {
                char *t;
                int64_t j;
                double val;
                if (!nl_next(p, &t))
                    FAIL("line %" PRId64 ": the initial guess segment ends "
                         "early", p->line);
                char *end;
                j = strtoll(t, &end, 10);
                if (end == t || nl_nums(end, &val, 1) != 1)
                    FAIL("line %" PRId64 ": an initial guess needs a column "
                         "and a value", p->line);
                if (j < 0 || j >= p->nvar)
                    FAIL("line %" PRId64 ": an initial guess names column "
                         "%" PRId64 " of %" PRId64, p->line, j, p->nvar);
                p->x0[j] = val;
            }
            break;
        case 'd':
            if (got < 1 || v[0] < 0)
                FAIL("line %" PRId64 ": 'd' needs a count", p->line);
            if ((st = nl_skip(p, v[0], "dual guess")) != JAOS_OK)
                return st;
            break;
        case 'r':
            for (int64_t i = 0; i < p->ncon; i++)
                if ((st = nl_bounds(p, "row", i, &p->rl[i], &p->ru[i]))
                    != JAOS_OK)
                    return st;
            seen_r = true;
            break;
        case 'b':
            for (int64_t j = 0; j < p->nvar; j++)
                if ((st = nl_bounds(p, "column", j, &p->cl[j], &p->cu[j]))
                    != JAOS_OK)
                    return st;
            seen_b = true;
            break;
        case 'k':
            if (got < 1 || v[0] < 0)
                FAIL("line %" PRId64 ": 'k' needs a count", p->line);
            if ((st = nl_skip(p, v[0], "column count")) != JAOS_OK)
                return st;
            break;
        case 'J':
            if (got < 2 || v[0] < 0 || v[0] >= p->ncon || v[1] < 0)
                FAIL("line %" PRId64 ": 'J' needs a row index below %" PRId64
                     " and a count", p->line, p->ncon);
            if ((st = nl_entries(p, v[0], v[1], false)) != JAOS_OK)
                return st;
            break;
        case 'G':
            if (got < 2 || v[0] < 0 || v[0] >= p->nobj || v[1] < 0)
                FAIL("line %" PRId64 ": 'G' needs an objective index below %"
                     PRId64 " and a count", p->line, p->nobj);
            if ((st = nl_entries(p, v[0], v[1], true)) != JAOS_OK)
                return st;
            break;
        case 'S': {
            int64_t w[2];
            if (nl_ints(s + 1, w, 2) < 2 || w[1] < 0)
                FAIL("line %" PRId64 ": 'S' needs a kind and a count", p->line);
            if ((st = nl_skip(p, w[1], "suffix")) != JAOS_OK)
                return st;
            break;
        }
        case 'V':
            FAIL("line %" PRId64 ": a defined variable, which JAOS does not "
                 "read", p->line);
        case 'F':
            FAIL("line %" PRId64 ": a user function, which JAOS does not read",
                 p->line);
        case 'L':
            FAIL("line %" PRId64 ": a logical constraint, which JAOS does not "
                 "read", p->line);
        default:
            FAIL("line %" PRId64 ": unknown segment '%c'", p->line, s[0]);
        }
    }
    if (!seen_b && p->nvar > 0)
        FAIL("line %" PRId64 ", the end of the file: there is no 'b' segment, "
             "so the column bounds are unknown", p->line);
    if (!seen_r && p->ncon > 0)
        FAIL("line %" PRId64 ", the end of the file: there is no 'r' segment, "
             "so the row bounds are unknown", p->line);
    return JAOS_OK;
}

static char **nl_names(jaos_model *m, const char *base, const char *ext,
                       int64_t want)
{
    char *path = malloc(strlen(base) + strlen(ext) + 1);
    if (path == nullptr)
        return nullptr;
    strcpy(path, base);
    strcat(path, ext);
    char *buf = nullptr;
    int64_t len = 0;
    const jaos_status st = jm_slurp(m, path, &buf, &len);
    free(path);
    m->err[0] = '\0';
    if (st != JAOS_OK)
        return nullptr;
    char **names = jm_calloc_array(want > 0 ? want : 1, sizeof *names);
    if (names == nullptr) {
        free(buf);
        return nullptr;
    }
    int64_t n = 0, pos = 0;
    bool ok = true;
    while (n < want && pos < len) {
        char *s = buf + pos;
        char *e = memchr(s, '\n', (size_t)(len - pos));
        if (e != nullptr) {
            *e = '\0';
            pos = (e - buf) + 1;
        } else {
            pos = len;
        }
        size_t k = strlen(s);
        if (k > 0 && s[k - 1] == '\r')
            s[k - 1] = '\0';
        if (!jm_name_ok(s)) {
            ok = false;
            break;
        }
        names[n] = jm_name_copy(s);
        if (names[n] == nullptr) {
            ok = false;
            break;
        }
        n++;
    }
    free(buf);
    if (!ok || n < want) {
        for (int64_t k = 0; k < want; k++)
            free(names[k]);
        free(names);
        return nullptr;
    }
    return names;
}

typedef struct {
    int64_t r, i, j;
    double v;
} nl_qent;

static int nl_qcmp(const void *a, const void *b)
{
    const nl_qent *x = a, *y = b;
    if (x->r != y->r)
        return x->r < y->r ? -1 : 1;
    if (x->i != y->i)
        return x->i < y->i ? -1 : 1;
    return x->j < y->j ? -1 : x->j > y->j;
}

static jaos_status nl_quadratics(nl *p)
{
    jaos_model *m = p->m;
    if (p->nq == 0)
        return JAOS_OK;
    nl_qent *q = jm_alloc_array(p->nq, sizeof *q);
    int64_t *qi = jm_alloc_array(p->nq, sizeof *qi);
    int64_t *qj = jm_alloc_array(p->nq, sizeof *qj);
    double *qv = jm_alloc_array(p->nq, sizeof *qv);
    if (q == nullptr || qi == nullptr || qj == nullptr || qv == nullptr) {
        free(q); free(qi); free(qj); free(qv);
        jm_set_err(m, "out of memory");
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t k = 0; k < p->nq; k++) {
        q[k].r = p->qr[k];
        q[k].i = p->qi[k];
        q[k].j = p->qj[k];
        q[k].v = p->qv[k];
    }
    qsort(q, (size_t)p->nq, sizeof *q, nl_qcmp);
    int64_t u = 0;
    for (int64_t k = 0; k < p->nq; k++) {
        if (u > 0 && q[u - 1].r == q[k].r && q[u - 1].i == q[k].i &&
            q[u - 1].j == q[k].j) {
            q[u - 1].v += q[k].v;
            continue;
        }
        q[u++] = q[k];
    }
    jaos_status st = JAOS_OK;
    int64_t at = 0;
    while (at < u && st == JAOS_OK) {
        const int64_t row = q[at].r;
        int64_t n = 0;
        while (at < u && q[at].r == row) {
            if (q[at].v != 0.0) {
                qi[n] = q[at].i;
                qj[n] = q[at].j;
                qv[n++] = q[at].i == q[at].j ? 2.0 * q[at].v : q[at].v;
            }
            at++;
        }
        if (n == 0)
            continue;
        st = row < 0 ? jaos_set_quadratic(m, n, qi, qj, qv)
                     : jaos_set_row_quadratic(m, row, n, qi, qj, qv);
        if (st != JAOS_OK && m->err[0] == '\0')
            jm_set_err(m, "the .nl model's quadratic part failed validation");
    }
    free(q); free(qi); free(qj); free(qv);
    return st;
}

static jaos_status nl_build(nl *p, const char *path)
{
    jaos_model *m = p->m;
    const int64_t nc = p->nvar, nr = p->ncon;
    for (int64_t i = 0; i < nr; i++) {
        if (p->shift[i] != 0.0) {
            p->rl[i] -= p->shift[i];
            p->ru[i] -= p->shift[i];
        }
    }
    int64_t *as = jm_calloc_array(nc + 1, sizeof *as);
    int64_t *fill = jm_calloc_array(nc > 0 ? nc : 1, sizeof *fill);
    int64_t *ai = jm_alloc_array(p->nent > 0 ? p->nent : 1, sizeof *ai);
    double *av = jm_alloc_array(p->nent > 0 ? p->nent : 1, sizeof *av);
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (as == nullptr || fill == nullptr || ai == nullptr || av == nullptr) {
        jm_set_err(m, "out of memory");
        goto out;
    }
    for (int64_t k = 0; k < p->nent; k++)
        as[p->ej[k] + 1]++;
    for (int64_t j = 0; j < nc; j++)
        as[j + 1] += as[j];
    for (int64_t k = 0; k < p->nent; k++) {
        const int64_t j = p->ej[k];
        const int64_t pos = as[j] + fill[j]++;
        ai[pos] = p->ei[k];
        av[pos] = p->ev[k];
    }
    st = jaos_load_lp(m, nc, nr, p->sense, p->offset, p->cost, p->cl, p->cu,
                      p->rl, p->ru, p->nent, nc > 0 ? as : nullptr, ai, av);
    if (st == JAOS_OK)
        st = nl_quadratics(p);
    if (st != JAOS_OK) {
        if (m->err[0] == '\0')
            jm_set_err(m, "the .nl model failed validation");
        goto out;
    }
    if (p->nbv + p->niv > 0) {
        bool *ci = jm_calloc_array(nc, sizeof(bool));
        if (ci == nullptr) {
            jm_set_err(m, "out of memory");
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto out;
        }
        for (int64_t j = nc - p->nbv - p->niv; j < nc; j++)
            ci[j] = true;
        free(m->col_integer);
        m->col_integer = ci;
    }
    {
        size_t n = strlen(path);
        char *base = malloc(n + 1);
        if (base != nullptr) {
            memcpy(base, path, n + 1);
            if (n > 3 && strcmp(base + n - 3, ".gz") == 0) {
                base[n - 3] = '\0';
                n -= 3;
            }
            if (n > 3 && strcmp(base + n - 3, ".nl") == 0)
                base[n - 3] = '\0';
            const int64_t extra = p->nobj > 0 ? 1 : 0;
            char **cn = nl_names(m, base, ".col", nc);
            char **rn = nl_names(m, base, ".row", nr + extra);
            char *on = nullptr;
            if (rn != nullptr && extra == 1) {
                on = rn[nr];
                rn[nr] = nullptr;
            }
            if (cn != nullptr || rn != nullptr)
                jm_model_take_names(m, cn, rn, on);
            free(base);
        }
    }
    st = JAOS_OK;
out:
    free(as);
    free(fill);
    free(ai);
    free(av);
    return st;
}

jaos_status jaos_read_nl(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    nl pp = {0};
    nl *p = &pp;
    p->m = m;
    p->sense = JAOS_MINIMIZE;
    jaos_status st = jm_slurp(m, path, &p->buf, &p->len);
    if (st != JAOS_OK)
        return st;
    {
        jm_locale loc;
        jm_locale_c_enter(&loc);
        st = nl_header(p);
        if (st == JAOS_OK) {
            const int64_t nc = p->nvar > 0 ? p->nvar : 1;
            const int64_t nr = p->ncon > 0 ? p->ncon : 1;
            p->cost = jm_calloc_array(nc, sizeof *p->cost);
            p->cl = jm_calloc_array(nc, sizeof *p->cl);
            p->cu = jm_alloc_array(nc, sizeof *p->cu);
            p->rl = jm_alloc_array(nr, sizeof *p->rl);
            p->ru = jm_alloc_array(nr, sizeof *p->ru);
            p->shift = jm_calloc_array(nr, sizeof *p->shift);
            if (p->cost == nullptr || p->cl == nullptr || p->cu == nullptr ||
                p->rl == nullptr || p->ru == nullptr || p->shift == nullptr) {
                jm_set_err(m, "out of memory");
                st = JAOS_ERR_OUT_OF_MEMORY;
            } else {
                for (int64_t j = 0; j < nc; j++)
                    p->cu[j] = INFINITY;
                for (int64_t i = 0; i < nr; i++) {
                    p->rl[i] = -INFINITY;
                    p->ru[i] = INFINITY;
                }
                st = nl_segments(p);
            }
        }
        if (st == JAOS_OK)
            st = nl_build(p, path);
        if (st == JAOS_OK) {
            m->nl_nopt = p->nopt;
            memcpy(m->nl_opt, p->opt, sizeof m->nl_opt);
            m->nl_rows = p->ncon;
            m->nl_cols = p->nvar;
        }
        if (st == JAOS_OK && p->x0 != nullptr && jm_model_has_integer(m))
            st = jaos_set_mip_start(m, p->x0);
        jm_locale_leave(&loc);
    }
    free(p->x0);
    free(p->buf);
    free(p->cost); free(p->cl); free(p->cu); free(p->rl); free(p->ru);
    free(p->shift); free(p->ei); free(p->ej); free(p->ev);
    free(p->qr); free(p->qi); free(p->qj); free(p->qv);
    return st;
}
