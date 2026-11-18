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
    int64_t i, j;
    double v;
} nl_term;

typedef struct {
    double c;
    nl_term *t;
    int64_t n, cap;
} nl_form;

typedef struct {
    nl_form *q;
    double *w;
    int64_t n, qcap, wcap;
    double c;
} nl_sos;

enum { NL_NORM = 1, NL_RATIO, NL_ROOT };

typedef struct {
    int kind;
    int64_t host, line;
    double coef;
    nl_sos s;
    nl_form d, e;
    double w;
} nl_atom;

typedef struct {
    nl_form f;
    nl_atom *a;
    int64_t na, acap;
    bool sos, prod;
    nl_sos s;
    nl_form pa, pb;
    double pw;
} nl_val;

typedef struct {
    jaos_model *m;
    char *buf;
    int64_t len, pos, line;
    int64_t nvar, ncon, nobj, nbv, niv;
    int64_t nlv[3], nli[3];
    int nlv_got;
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
    nl_atom *atom;
    int64_t natom, atomcap;
    double *xlo, *xup, *xcost;
    int64_t nx, xlocap, xupcap, xccap;
    double *yrhs;
    int64_t ny, yrhscap;
    int *ktype;
    int64_t *kstart, *kcol;
    int64_t nk, ktcap, kscap, kn, kccap;
    int64_t *stamp;
    int64_t *dead;
    double *dscale;
} nl;

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

#define REFUSE(...) (jm_set_err(p->m, __VA_ARGS__), JAOS_ERR_INVALID_INPUT)

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
    *f = (nl_form){0};
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

static void form_scale(nl_form *f, double k)
{
    f->c *= k;
    for (int64_t t = 0; t < f->n; t++)
        f->t[t].v *= k;
}

static void form_div(nl_form *f, double k)
{
    f->c /= k;
    for (int64_t t = 0; t < f->n; t++)
        f->t[t].v /= k;
}

typedef struct {
    int64_t i, j, at;
    double v;
} nl_key;

static int nl_key_term(const void *a, const void *b)
{
    const nl_key *x = a, *y = b;
    if (x->i != y->i)
        return x->i < y->i ? -1 : 1;
    if (x->j != y->j)
        return x->j < y->j ? -1 : 1;
    return x->at < y->at ? -1 : x->at > y->at;
}

static int nl_key_at(const void *a, const void *b)
{
    const nl_key *x = a, *y = b;
    return x->at < y->at ? -1 : x->at > y->at;
}

static bool form_canon(nl_form *f)
{
    if (f->n == 0)
        return true;
    nl_key *k = jm_alloc_array(f->n, sizeof *k);
    if (k == nullptr)
        return false;
    for (int64_t t = 0; t < f->n; t++)
        k[t] = (nl_key){f->t[t].i, f->t[t].j, t, f->t[t].v};
    qsort(k, (size_t)f->n, sizeof *k, nl_key_term);
    int64_t u = 0;
    for (int64_t t = 0; t < f->n; t++) {
        if (u > 0 && k[u - 1].i == k[t].i && k[u - 1].j == k[t].j) {
            k[u - 1].v += k[t].v;
            continue;
        }
        k[u++] = k[t];
    }
    int64_t w = 0;
    for (int64_t t = 0; t < u; t++)
        if (k[t].v != 0.0)
            k[w++] = k[t];
    qsort(k, (size_t)w, sizeof *k, nl_key_at);
    for (int64_t t = 0; t < w; t++)
        f->t[t] = (nl_term){k[t].i, k[t].j, k[t].v};
    f->n = w;
    free(k);
    return true;
}

static int form_deg(const nl_form *f)
{
    for (int64_t t = 0; t < f->n; t++)
        if (f->t[t].j >= 0)
            return 2;
    return f->n > 0 ? 1 : 0;
}

static bool form_equal(const nl_form *a, const nl_form *b, bool *same)
{
    *same = false;
    if (a->c != b->c || a->n != b->n)
        return true;
    if (a->n == 0) {
        *same = true;
        return true;
    }
    nl_key *k = jm_alloc_array(2 * a->n, sizeof *k);
    if (k == nullptr)
        return false;
    for (int64_t t = 0; t < a->n; t++) {
        k[t] = (nl_key){a->t[t].i, a->t[t].j, 0, a->t[t].v};
        k[a->n + t] = (nl_key){b->t[t].i, b->t[t].j, 0, b->t[t].v};
    }
    qsort(k, (size_t)a->n, sizeof *k, nl_key_term);
    qsort(k + a->n, (size_t)a->n, sizeof *k, nl_key_term);
    bool eq = true;
    for (int64_t t = 0; t < a->n && eq; t++)
        eq = k[t].i == k[a->n + t].i && k[t].j == k[a->n + t].j &&
             k[t].v == k[a->n + t].v;
    free(k);
    *same = eq;
    return true;
}

static bool form_mul(nl_form *out, const nl_form *a, const nl_form *b)
{
    out->c = a->c * b->c;
    for (int64_t k = 0; k < a->n; k++)
        if (!form_put(out, a->t[k].i, a->t[k].j, a->t[k].v * b->c))
            return false;
    for (int64_t k = 0; k < b->n; k++)
        if (!form_put(out, b->t[k].i, b->t[k].j, b->t[k].v * a->c))
            return false;
    for (int64_t ka = 0; ka < a->n; ka++)
        for (int64_t kb = 0; kb < b->n; kb++) {
            const int64_t x = a->t[ka].i, y = b->t[kb].i;
            if (!form_put(out, x > y ? x : y, x > y ? y : x,
                          a->t[ka].v * b->t[kb].v))
                return false;
        }
    return true;
}

static void sos_free(nl_sos *s)
{
    for (int64_t k = 0; k < s->n; k++)
        form_free(&s->q[k]);
    free(s->q);
    free(s->w);
    *s = (nl_sos){0};
}

static bool sos_put(nl_sos *s, nl_form *q, double w)
{
    if (!JM_GROW(s->q, s->qcap, s->n + 1) || !JM_GROW(s->w, s->wcap, s->n + 1))
        return false;
    s->q[s->n] = *q;
    s->w[s->n++] = w;
    *q = (nl_form){0};
    return true;
}

static bool sos_take(nl_sos *a, nl_sos *b, double k)
{
    a->c += k * b->c;
    for (int64_t t = 0; t < b->n; t++)
        if (!sos_put(a, &b->q[t], k * b->w[t]))
            return false;
    sos_free(b);
    return true;
}

static int sos_sign(const nl_sos *s)
{
    bool pos = s->c >= 0.0, neg = s->c <= 0.0;
    for (int64_t k = 0; k < s->n; k++) {
        pos = pos && s->w[k] > 0.0;
        neg = neg && s->w[k] < 0.0;
    }
    return pos ? 1 : neg ? -1 : 0;
}

static void atom_free(nl_atom *a)
{
    sos_free(&a->s);
    form_free(&a->d);
    form_free(&a->e);
    *a = (nl_atom){0};
}

static void val_free(nl_val *v)
{
    form_free(&v->f);
    for (int64_t k = 0; k < v->na; k++)
        atom_free(&v->a[k]);
    free(v->a);
    sos_free(&v->s);
    form_free(&v->pa);
    form_free(&v->pb);
    *v = (nl_val){0};
}

static void val_const(nl_val *v, double c)
{
    val_free(v);
    v->f.c = c;
    v->sos = true;
    v->s.c = c;
}

static void val_move(nl_val *dst, nl_val *src)
{
    val_free(dst);
    *dst = *src;
    *src = (nl_val){0};
}

static bool val_is_const(const nl_val *v)
{
    return v->na == 0 && v->f.n == 0;
}

static void val_scale(nl_val *v, double k)
{
    if (k == 0.0) {
        val_const(v, 0.0);
        return;
    }
    form_scale(&v->f, k);
    for (int64_t t = 0; t < v->na; t++)
        v->a[t].coef *= k;
    if (v->sos) {
        v->s.c *= k;
        for (int64_t t = 0; t < v->s.n; t++)
            v->s.w[t] *= k;
    }
    v->pw *= k;
}

static void val_div(nl_val *v, double k)
{
    form_div(&v->f, k);
    for (int64_t t = 0; t < v->na; t++)
        v->a[t].coef /= k;
    if (v->sos) {
        v->s.c /= k;
        for (int64_t t = 0; t < v->s.n; t++)
            v->s.w[t] /= k;
    }
    v->pw /= k;
}

static bool val_add(nl_val *a, nl_val *b, double sgn)
{
    bool ok = form_add(&a->f, &b->f, sgn);
    for (int64_t t = 0; ok && t < b->na; t++) {
        ok = JM_GROW(a->a, a->acap, a->na + 1);
        if (ok) {
            a->a[a->na] = b->a[t];
            a->a[a->na++].coef *= sgn;
            b->a[t] = (nl_atom){0};
        }
    }
    const bool sos = a->sos && b->sos;
    if (ok && sos)
        ok = sos_take(&a->s, &b->s, sgn);
    if (!sos)
        sos_free(&a->s);
    a->sos = sos;
    a->prod = false;
    form_free(&a->pa);
    form_free(&a->pb);
    val_free(b);
    return ok;
}

static jaos_status val_atom(nl_val *out, nl_atom *at)
{
    val_free(out);
    if (!JM_GROW(out->a, out->acap, 1)) {
        atom_free(at);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    out->a[out->na++] = *at;
    *at = (nl_atom){0};
    return JAOS_OK;
}

static const char *nl_op_name(int64_t op)
{
    switch (op) {
    case 4: return "rem";
    case 6: return "less";
    case 11: return "min";
    case 12: return "max";
    case 13: return "floor";
    case 14: return "ceil";
    case 20: return "or";
    case 21: return "and";
    case 22: return "lt";
    case 23: return "le";
    case 24: return "eq";
    case 28: return "ge";
    case 29: return "gt";
    case 30: return "ne";
    case 34: return "not";
    case 35: return "if";
    case 37: return "tanh";
    case 38: return "tan";
    case 40: return "sinh";
    case 41: return "sin";
    case 42: return "log10";
    case 43: return "log";
    case 44: return "exp";
    case 45: return "cosh";
    case 46: return "cos";
    case 47: return "atanh";
    case 48: return "atan2";
    case 49: return "atan";
    case 50: return "asinh";
    case 51: return "asin";
    case 52: return "acosh";
    case 53: return "acos";
    case 55: return "intdiv";
    case 56: return "precision";
    case 57: return "round";
    case 58: return "trunc";
    case 59: return "count";
    case 60: return "numberof";
    case 61: return "numberofs";
    case 64: return "piecewise-linear";
    case 65: return "ifs";
    case 70: return "and";
    case 71: return "or";
    case 72: return "implies";
    case 73: return "iff";
    case 74: return "alldiff";
    default: return "unknown";
    }
}

static const char NL_ATOMS[] = "a sqrt, an abs or a ratio";

static jaos_status nl_body(nl *p, const char *what, int64_t which, nl_val *out);

static jaos_status nl_pair(nl *p, const char *what, int64_t which,
                           nl_val *a, nl_val *b)
{
    jaos_status st = nl_body(p, what, which, a);
    if (st == JAOS_OK)
        st = nl_body(p, what, which, b);
    return st;
}

static jaos_status nl_times(nl *p, const char *what, int64_t which,
                            int64_t line, nl_val *a, nl_val *b, nl_val *out)
{
    if (!form_canon(&a->f) || !form_canon(&b->f))
        return JAOS_ERR_OUT_OF_MEMORY;
    if (val_is_const(a)) {
        val_scale(b, a->f.c);
        val_move(out, b);
        return JAOS_OK;
    }
    if (val_is_const(b)) {
        val_scale(a, b->f.c);
        val_move(out, a);
        return JAOS_OK;
    }
    if (a->na > 0 || b->na > 0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " multiplies %s by "
                      "something other than a constant", line, what, which,
                      NL_ATOMS);
    if (form_deg(&a->f) + form_deg(&b->f) > 2)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " has a product of "
                      "degree three or more, and JAOS reads a polynomial of "
                      "degree two at most", line, what, which);
    val_free(out);
    if (!form_mul(&out->f, &a->f, &b->f))
        return JAOS_ERR_OUT_OF_MEMORY;
    bool same = false;
    if (!form_equal(&a->f, &b->f, &same))
        return JAOS_ERR_OUT_OF_MEMORY;
    if (same) {
        out->sos = true;
        if (!sos_put(&out->s, &a->f, 1.0))
            return JAOS_ERR_OUT_OF_MEMORY;
    } else {
        out->prod = true;
        out->pa = a->f;
        out->pb = b->f;
        out->pw = 1.0;
        a->f = (nl_form){0};
        b->f = (nl_form){0};
    }
    return JAOS_OK;
}

static jaos_status nl_over(nl *p, const char *what, int64_t which,
                           int64_t line, nl_val *a, nl_val *b, nl_val *out)
{
    if (!form_canon(&a->f) || !form_canon(&b->f))
        return JAOS_ERR_OUT_OF_MEMORY;
    if (b->na > 0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " divides by %s", line,
                      what, which, NL_ATOMS);
    if (b->f.n == 0) {
        if (b->f.c == 0.0)
            return REFUSE("line %" PRId64 ": %s %" PRId64 " divides by zero",
                          line, what, which);
        val_div(a, b->f.c);
        val_move(out, a);
        return JAOS_OK;
    }
    if (form_deg(&b->f) > 1)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " divides by a "
                      "quadratic, and JAOS divides only by a constant or a "
                      "linear form", line, what, which);
    if (a->na > 0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " divides %s by a "
                      "linear form", line, what, which, NL_ATOMS);
    if (val_is_const(a) && a->f.c == 0.0) {
        val_const(out, 0.0);
        return JAOS_OK;
    }
    const int sign = a->sos ? sos_sign(&a->s) : 0;
    if (sign == 0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " divides by a linear "
                      "form something that is not a sum of squares, and JAOS "
                      "reads only a sum of squares over a linear form", line,
                      what, which);
    nl_atom at = {.kind = NL_RATIO, .line = line, .coef = sign};
    at.s = a->s;
    a->s = (nl_sos){0};
    a->sos = false;
    if (sign < 0) {
        at.s.c = -at.s.c;
        for (int64_t t = 0; t < at.s.n; t++)
            at.s.w[t] = -at.s.w[t];
    }
    at.d = b->f;
    b->f = (nl_form){0};
    return val_atom(out, &at);
}

static jaos_status nl_root(nl *p, const char *what, int64_t which,
                           int64_t line, nl_val *a, nl_val *out)
{
    if (!form_canon(&a->f))
        return JAOS_ERR_OUT_OF_MEMORY;
    if (a->na > 0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes sqrt of %s",
                      line, what, which, NL_ATOMS);
    if (val_is_const(a)) {
        if (a->f.c < 0.0)
            return REFUSE("line %" PRId64 ": %s %" PRId64 " takes sqrt of a "
                          "negative constant", line, what, which);
        val_const(out, sqrt(a->f.c));
        return JAOS_OK;
    }
    nl_atom at = {.line = line, .coef = 1.0};
    if (form_deg(&a->f) == 1) {
        at.kind = NL_ROOT;
        at.d = a->f;
        a->f = (nl_form){0};
        at.e.c = 1.0;
        at.w = 1.0;
    } else if (a->sos && sos_sign(&a->s) > 0) {
        at.kind = NL_NORM;
        at.s = a->s;
        a->s = (nl_sos){0};
        a->sos = false;
    } else if (a->prod) {
        at.kind = NL_ROOT;
        at.d = a->pa;
        at.e = a->pb;
        at.w = a->pw;
        a->pa = a->pb = (nl_form){0};
        a->prod = false;
        if (at.w < 0.0) {
            form_scale(&at.d, -1.0);
            at.w = -at.w;
        }
    } else {
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes sqrt of a "
                      "quadratic that is neither a sum of squares nor a "
                      "product of two linear forms", line, what, which);
    }
    return val_atom(out, &at);
}

static jaos_status nl_abs(nl *p, const char *what, int64_t which,
                          int64_t line, nl_val *a, nl_val *out)
{
    if (!form_canon(&a->f))
        return JAOS_ERR_OUT_OF_MEMORY;
    if (a->na > 0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes abs of %s",
                      line, what, which, NL_ATOMS);
    if (val_is_const(a)) {
        val_const(out, fabs(a->f.c));
        return JAOS_OK;
    }
    if (form_deg(&a->f) > 1)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes abs of a "
                      "quadratic, and JAOS takes abs of a linear form only",
                      line, what, which);
    nl_atom at = {.kind = NL_NORM, .line = line, .coef = 1.0};
    if (!sos_put(&at.s, &a->f, 1.0)) {
        atom_free(&at);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    return val_atom(out, &at);
}

static jaos_status nl_fold_power(nl *p, const char *what, int64_t which,
                                 int64_t line, double c, double e,
                                 double *out)
{
    if (e == 0.5) {
        if (c < 0.0)
            return REFUSE("line %" PRId64 ": %s %" PRId64 " takes sqrt of a "
                          "negative constant", line, what, which);
        *out = sqrt(c);
        return JAOS_OK;
    }
    if (e != trunc(e) || fabs(e) > 0x1p53)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " raises a constant to "
                      "the power %.17g, and JAOS computes a constant's power "
                      "only for a whole exponent or 0.5", line, what, which, e);
    double r = 1.0, b = c;
    for (uint64_t n = (uint64_t)fabs(e); n > 0; n >>= 1) {
        if (n & 1u)
            r *= b;
        if (n > 1)
            b *= b;
    }
    if (e < 0.0)
        r = 1.0 / r;
    if (!isfinite(r))
        return REFUSE("line %" PRId64 ": %s %" PRId64 " raises %.17g to the "
                      "power %.17g, which is not finite", line, what, which, c,
                      e);
    *out = r;
    return JAOS_OK;
}

static jaos_status nl_power(nl *p, const char *what, int64_t which,
                            int64_t line, nl_val *a, nl_val *b, nl_val *out)
{
    if (!form_canon(&a->f) || !form_canon(&b->f))
        return JAOS_ERR_OUT_OF_MEMORY;
    if (!val_is_const(b))
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes a power whose "
                      "exponent is not a constant", line, what, which);
    const double e = b->f.c;
    if (e == 1.0) {
        val_move(out, a);
        return JAOS_OK;
    }
    if (e == 0.0) {
        val_const(out, 1.0);
        return JAOS_OK;
    }
    if (a->na > 0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes a power of %s",
                      line, what, which, NL_ATOMS);
    if (val_is_const(a)) {
        double r = 0.0;
        const jaos_status st = nl_fold_power(p, what, which, line, a->f.c, e,
                                             &r);
        if (st == JAOS_OK)
            val_const(out, r);
        return st;
    }
    if (e == 2.0) {
        if (form_deg(&a->f) > 1)
            return REFUSE("line %" PRId64 ": %s %" PRId64 " squares a "
                          "quadratic, which has degree four, and JAOS reads a "
                          "polynomial of degree two at most", line, what, which);
        val_free(out);
        if (!form_mul(&out->f, &a->f, &a->f))
            return JAOS_ERR_OUT_OF_MEMORY;
        out->sos = true;
        return sos_put(&out->s, &a->f, 1.0) ? JAOS_OK
                                            : JAOS_ERR_OUT_OF_MEMORY;
    }
    if (e == 0.5)
        return nl_root(p, what, which, line, a, out);
    if (e == -1.0) {
        nl_val one = {0};
        val_const(&one, 1.0);
        const jaos_status st = nl_over(p, what, which, line, &one, a, out);
        val_free(&one);
        return st;
    }
    if (e == trunc(e) && e > 2.0)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes the power %.17g "
                      "of a variable, which has degree three or more, and JAOS "
                      "reads a polynomial of degree two at most", line, what,
                      which, e);
    return REFUSE("line %" PRId64 ": %s %" PRId64 " takes the power %.17g of a "
                  "variable, and JAOS reads the powers 0, 1, 2, 0.5 and -1 "
                  "only", line, what, which, e);
}

static jaos_status nl_body(nl *p, const char *what, int64_t which, nl_val *out)
{
    char *s;
    if (!nl_next(p, &s))
        return REFUSE("line %" PRId64 ": %s %" PRId64 " has no expression",
                      p->line, what, which);
    const int64_t line = p->line;
    if (s[0] == 'n') {
        double c = 0.0;
        if (nl_nums(s + 1, &c, 1) != 1 || !isfinite(c))
            return REFUSE("line %" PRId64 ": %s %" PRId64 " has a constant "
                          "JAOS cannot read", line, what, which);
        val_const(out, c);
        return JAOS_OK;
    }
    if (s[0] == 'v') {
        int64_t j;
        if (nl_ints(s + 1, &j, 1) != 1 || j < 0 || j >= p->nvar)
            return REFUSE("line %" PRId64 ": %s %" PRId64 " names a column "
                          "outside the %" PRId64 " the header declares", line,
                          what, which, p->nvar);
        val_free(out);
        if (!form_put(&out->f, j, -1, 1.0)) {
            jm_set_err(p->m, "out of memory");
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        return JAOS_OK;
    }
    if (s[0] != 'o')
        return REFUSE("line %" PRId64 ": %s %" PRId64 " has a body starting "
                      "'%c', which JAOS does not read", line, what, which,
                      s[0]);
    int64_t op;
    if (nl_ints(s + 1, &op, 1) != 1)
        return REFUSE("line %" PRId64 ": %s %" PRId64 " has an operator with "
                      "no number", line, what, which);
    nl_val a = {0}, b = {0};
    jaos_status st = JAOS_OK;
    switch (op) {
    case 0:
    case 1:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK) {
            val_move(out, &a);
            if (!val_add(out, &b, op == 0 ? 1.0 : -1.0))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        break;
    case 2:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK)
            st = nl_times(p, what, which, line, &a, &b, out);
        break;
    case 3:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK)
            st = nl_over(p, what, which, line, &a, &b, out);
        break;
    case 5:
        st = nl_pair(p, what, which, &a, &b);
        if (st == JAOS_OK)
            st = nl_power(p, what, which, line, &a, &b, out);
        break;
    case 15:
        st = nl_body(p, what, which, &a);
        if (st == JAOS_OK)
            st = nl_abs(p, what, which, line, &a, out);
        break;
    case 16:
        st = nl_body(p, what, which, &a);
        if (st == JAOS_OK) {
            val_scale(&a, -1.0);
            val_move(out, &a);
        }
        break;
    case 39:
        st = nl_body(p, what, which, &a);
        if (st == JAOS_OK)
            st = nl_root(p, what, which, line, &a, out);
        break;
    case 54: {
        char *t;
        int64_t n;
        if (!nl_next(p, &t) || nl_ints(t, &n, 1) != 1 || n < 0) {
            st = REFUSE("line %" PRId64 ": %s %" PRId64 "'s sum has no count",
                        p->line, what, which);
            break;
        }
        val_const(out, 0.0);
        for (int64_t k = 0; k < n && st == JAOS_OK; k++) {
            st = nl_body(p, what, which, &a);
            if (st == JAOS_OK && !val_add(out, &a, 1.0))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        break;
    }
    default:
        st = REFUSE("line %" PRId64 ": %s %" PRId64 " uses operator o%" PRId64
                    " (%s), which JAOS does not read; it reads + - * / ^ sum "
                    "neg abs and sqrt, in a body that is of degree two or "
                    "that a cone holds", line, what, which, op,
                    nl_op_name(op));
    }
    val_free(&a);
    val_free(&b);
    if (st == JAOS_ERR_OUT_OF_MEMORY)
        jm_set_err(p->m, "out of memory");
    return st;
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

static bool nl_entry(nl *p, int64_t i, int64_t j, double v)
{
    if (!JM_GROW(p->ei, p->ecap, p->nent + 1) ||
        !JM_GROW(p->ej, p->jcap, p->nent + 1) ||
        !JM_GROW(p->ev, p->vcap, p->nent + 1))
        return false;
    p->ei[p->nent] = i;
    p->ej[p->nent] = j;
    p->ev[p->nent++] = v;
    return true;
}

static jaos_status nl_keep(nl *p, int64_t host, nl_val *v)
{
    if (!form_canon(&v->f))
        FAIL_OOM();
    if (host >= 0)
        p->shift[host] += v->f.c;
    else
        p->offset = v->f.c;
    if (!nl_keep_quad(p, host, &v->f))
        FAIL_OOM();
    for (int64_t k = 0; k < v->f.n; k++) {
        if (v->f.t[k].j >= 0)
            continue;
        if (host < 0)
            p->cost[v->f.t[k].i] += v->f.t[k].v;
        else if (!nl_entry(p, host, v->f.t[k].i, v->f.t[k].v))
            FAIL_OOM();
    }
    for (int64_t k = 0; k < v->na; k++) {
        if (!JM_GROW(p->atom, p->atomcap, p->natom + 1))
            FAIL_OOM();
        p->atom[p->natom] = v->a[k];
        p->atom[p->natom++].host = host;
        v->a[k] = (nl_atom){0};
    }
    return JAOS_OK;
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
        if (!nl_entry(p, i, j, v))
            FAIL_OOM();
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
    p->nlv_got = nl_ints(s, p->nlv, 3);
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
        for (int k = 2; k < 5; k++)
            p->nli[k - 2] = k < got ? v[k] : 0;
    }
    if (p->nbv < 0 || p->niv < 0 || p->nbv > p->nvar ||
        p->niv > p->nvar - p->nbv)
        FAIL("line 7: %" PRId64 " binary and %" PRId64 " integer variables "
             "against %" PRId64 " variables", p->nbv, p->niv, p->nvar);
    if (p->nli[0] != 0 || p->nli[1] != 0 || p->nli[2] != 0) {
        if (p->nlv_got < 3)
            FAIL("line 7: integer variables in nonlinear terms, and line 5 "
                 "gives no count of the variables nonlinear in both the "
                 "constraints and the objectives, so JAOS cannot place them");
        const int64_t c = p->nlv[0], o = p->nlv[1], b = p->nlv[2];
        const int64_t top = c > o ? c : o;
        if (b < 0 || b > c || b > o || p->nli[0] < 0 || p->nli[1] < 0 ||
            p->nli[2] < 0 || p->nli[0] > b || p->nli[1] > c - b ||
            p->nli[2] > (o > c ? o - c : 0) ||
            top > p->nvar - p->nbv - p->niv)
            FAIL("line 7: %" PRId64 ", %" PRId64 " and %" PRId64 " integer "
                 "variables in nonlinear terms do not fit the %" PRId64 ", %"
                 PRId64 " and %" PRId64 " nonlinear variables of line 5",
                 p->nli[0], p->nli[1], p->nli[2], c, o, b);
    }
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
            nl_val f = {0};
            st = nl_body(p, "row", v[0], &f);
            if (st == JAOS_OK)
                st = nl_keep(p, v[0], &f);
            val_free(&f);
            if (st != JAOS_OK)
                return st;
            break;
        }
        case 'O': {
            if (got < 2 || v[0] < 0 || v[0] >= p->nobj)
                FAIL("line %" PRId64 ": 'O' needs an objective index below %"
                     PRId64 " and a sense", p->line, p->nobj);
            nl_val f = {0};
            st = nl_body(p, "objective", v[0], &f);
            if (st == JAOS_OK && v[0] == 0) {
                p->sense = v[1] == 1 ? JAOS_MAXIMIZE : JAOS_MINIMIZE;
                st = nl_keep(p, -1, &f);
            }
            val_free(&f);
            if (st != JAOS_OK)
                return st;
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
                       int64_t want, int64_t room)
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
    char **names = jm_calloc_array(room > 0 ? room : 1, sizeof *names);
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

static int64_t nl_new_col(nl *p, double lo, double up)
{
    if (!JM_GROW(p->xlo, p->xlocap, p->nx + 1) ||
        !JM_GROW(p->xup, p->xupcap, p->nx + 1) ||
        !JM_GROW(p->xcost, p->xccap, p->nx + 1))
        return -1;
    p->xcost[p->nx] = 0.0;
    p->xlo[p->nx] = lo;
    p->xup[p->nx] = up;
    return p->nvar + p->nx++;
}

static int64_t nl_new_row(nl *p, double rhs)
{
    if (!JM_GROW(p->yrhs, p->yrhscap, p->ny + 1))
        return -1;
    p->yrhs[p->ny] = rhs;
    return p->ncon + p->ny++;
}

static bool nl_cone_col(nl *p, int64_t col)
{
    if (!JM_GROW(p->kcol, p->kccap, p->kn + 1))
        return false;
    p->kcol[p->kn++] = col;
    if (col < p->nvar)
        p->stamp[col] = p->nk + 1;
    return true;
}

static bool nl_cone_has(const nl *p, int64_t col)
{
    return col < p->nvar && p->stamp[col] == p->nk + 1;
}

static bool nl_cone_open(nl *p, int type)
{
    if (!JM_GROW(p->ktype, p->ktcap, p->nk + 1) ||
        !JM_GROW(p->kstart, p->kscap, p->nk + 2))
        return false;
    p->ktype[p->nk] = type;
    p->kstart[p->nk] = p->kn;
    return true;
}

static void nl_cone_close(nl *p)
{
    p->nk++;
    p->kstart[p->nk] = p->kn;
}

static jaos_status nl_member(nl *p, const nl_form *a, double k)
{
    if (a->n == 0) {
        const int64_t c = nl_new_col(p, k * a->c, k * a->c);
        return c >= 0 && nl_cone_col(p, c) ? JAOS_OK : JAOS_ERR_OUT_OF_MEMORY;
    }
    if (a->n == 1 && a->c == 0.0 && a->t[0].j < 0 && k * a->t[0].v == 1.0 &&
        !nl_cone_has(p, a->t[0].i))
        return nl_cone_col(p, a->t[0].i) ? JAOS_OK : JAOS_ERR_OUT_OF_MEMORY;
    const int64_t w = nl_new_col(p, -INFINITY, INFINITY);
    const int64_t r = nl_new_row(p, k * a->c);
    if (w < 0 || r < 0 || !nl_entry(p, r, w, 1.0))
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t t = 0; t < a->n; t++)
        if (!nl_entry(p, r, a->t[t].i, -k * a->t[t].v))
            return JAOS_ERR_OUT_OF_MEMORY;
    return nl_cone_col(p, w) ? JAOS_OK : JAOS_ERR_OUT_OF_MEMORY;
}

static jaos_status nl_factor(nl *p, const nl_form *a, double *w)
{
    if (a->n == 1 && a->c == 0.0 && a->t[0].v > 0.0 &&
        !nl_cone_has(p, a->t[0].i)) {
        *w *= a->t[0].v;
        return nl_cone_col(p, a->t[0].i) ? JAOS_OK : JAOS_ERR_OUT_OF_MEMORY;
    }
    if (a->n == 0 && a->c > 0.0) {
        *w *= a->c;
        const int64_t c = nl_new_col(p, 1.0, 1.0);
        return c >= 0 && nl_cone_col(p, c) ? JAOS_OK : JAOS_ERR_OUT_OF_MEMORY;
    }
    return nl_member(p, a, 1.0);
}

static jaos_status nl_members(nl *p, const nl_sos *s)
{
    jaos_status st = JAOS_OK;
    for (int64_t k = 0; k < s->n && st == JAOS_OK; k++)
        st = nl_member(p, &s->q[k], sqrt(s->w[k]));
    if (st == JAOS_OK && s->c > 0.0) {
        nl_form one = {.c = 1.0};
        st = nl_member(p, &one, sqrt(s->c));
    }
    return st;
}

static double nl_low(const nl_form *f, const double *lb, const double *ub)
{
    double s = f->c;
    for (int64_t t = 0; t < f->n; t++) {
        const double v = f->t[t].v;
        s += v > 0.0 ? v * lb[f->t[t].i] : v * ub[f->t[t].i];
    }
    return s;
}

static const char *nl_kind_name(int kind)
{
    switch (kind) {
    case NL_NORM: return "a norm (sqrt of a sum of squares, or abs)";
    case NL_RATIO: return "a sum of squares over a linear form";
    default: return "sqrt of a linear form or of a product of two";
    }
}

static jaos_status nl_host(nl *p, int64_t host, double coef, int64_t col)
{
    if (host < 0) {
        if (col < p->nvar)
            p->cost[col] += coef;
        else
            p->xcost[col - p->nvar] += coef;
        return JAOS_OK;
    }
    return nl_entry(p, host, col, coef) ? JAOS_OK : JAOS_ERR_OUT_OF_MEMORY;
}

static jaos_status nl_atom_build(nl *p, nl_atom *a, const double *lb,
                                 const double *ub)
{
    const char *what = a->host < 0 ? "objective" : "row";
    const int64_t which = a->host < 0 ? 0 : a->host;
    if (!form_canon(&a->d) || !form_canon(&a->e))
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t k = 0; k < a->s.n; k++)
        if (!form_canon(&a->s.q[k]))
            return JAOS_ERR_OUT_OF_MEMORY;
    int side;
    if (a->host < 0) {
        side = p->sense == JAOS_MAXIMIZE ? -1 : 1;
    } else {
        const double lo = p->rl[a->host], up = p->ru[a->host];
        if (isfinite(lo) && isfinite(up))
            return REFUSE("line %" PRId64 ": row %" PRId64 " has %s and two "
                          "finite bounds, which is not a convex set; JAOS "
                          "takes such a row with one side", a->line, a->host,
                          nl_kind_name(a->kind));
        if (!isfinite(lo) && !isfinite(up))
            return JAOS_OK;
        side = isfinite(up) ? 1 : -1;
    }
    if (a->coef == 0.0)
        return JAOS_OK;
    const bool convex = a->kind != NL_ROOT;
    if ((a->coef > 0.0) != (convex == (side > 0))) {
        if (a->host < 0)
            return REFUSE("line %" PRId64 ": the objective has %s, which is %s, "
                          "with a %s coefficient in a %s, so the objective is "
                          "not convex", a->line, nl_kind_name(a->kind),
                          convex ? "convex" : "concave",
                          a->coef > 0.0 ? "positive" : "negative",
                          side > 0 ? "minimization" : "maximization");
        return REFUSE("line %" PRId64 ": row %" PRId64 " has %s, which is %s, "
                      "with a %s coefficient on its %s side, so the row is not "
                      "a convex set", a->line, a->host, nl_kind_name(a->kind),
                      convex ? "convex" : "concave",
                      a->coef > 0.0 ? "positive" : "negative",
                      side > 0 ? "upper" : "lower");
    }
    if (a->kind == NL_RATIO && !(nl_low(&a->d, lb, ub) >= 0.0))
        return REFUSE("line %" PRId64 ": %s %" PRId64 " divides by a linear "
                      "form that the column bounds do not hold at or above 0, "
                      "and a sum of squares over it is convex only where they "
                      "do", a->line, what, which);
    if (a->kind == NL_ROOT && a->e.n > 0 &&
        !(nl_low(&a->d, lb, ub) >= 0.0 && nl_low(&a->e, lb, ub) >= 0.0))
        return REFUSE("line %" PRId64 ": %s %" PRId64 " takes sqrt of a "
                      "product whose factors the column bounds do not hold at "
                      "or above 0, and it is concave only where they do",
                      a->line, what, which);
    jaos_status st = JAOS_OK;
    if (a->kind == NL_NORM) {
        if (!nl_cone_open(p, JAOS_CONE_QUADRATIC))
            return JAOS_ERR_OUT_OF_MEMORY;
        const int64_t s = nl_new_col(p, 0.0, INFINITY);
        if (s < 0 || !nl_cone_col(p, s))
            return JAOS_ERR_OUT_OF_MEMORY;
        st = nl_members(p, &a->s);
        if (st == JAOS_OK)
            st = nl_host(p, a->host, a->coef, s);
    } else if (a->kind == NL_RATIO) {
        if (!nl_cone_open(p, JAOS_CONE_ROTATED))
            return JAOS_ERR_OUT_OF_MEMORY;
        double scale = 2.0;
        if (a->d.n == 1 && a->d.c == 0.0 && a->d.t[0].v > 0.0) {
            scale = 2.0 / a->d.t[0].v;
            if (!nl_cone_col(p, a->d.t[0].i))
                return JAOS_ERR_OUT_OF_MEMORY;
        } else {
            st = nl_member(p, &a->d, 1.0);
        }
        const int64_t u = nl_new_col(p, 0.0, INFINITY);
        if (u < 0 || !nl_cone_col(p, u))
            return JAOS_ERR_OUT_OF_MEMORY;
        if (st == JAOS_OK)
            st = nl_members(p, &a->s);
        if (st == JAOS_OK)
            st = nl_host(p, a->host, a->coef * scale, u);
    } else {
        if (!nl_cone_open(p, JAOS_CONE_ROTATED))
            return JAOS_ERR_OUT_OF_MEMORY;
        double w = a->w;
        st = nl_factor(p, &a->d, &w);
        if (st == JAOS_OK)
            st = nl_factor(p, &a->e, &w);
        const int64_t h = nl_new_col(p, -INFINITY, INFINITY);
        if (h < 0 || !nl_cone_col(p, h))
            return JAOS_ERR_OUT_OF_MEMORY;
        if (st == JAOS_OK)
            st = nl_host(p, a->host, a->coef * sqrt(w / 2.0), h);
    }
    if (st == JAOS_OK)
        nl_cone_close(p);
    return st;
}

static jaos_status nl_cone_row(nl *p, int64_t i, const nl_form *q,
                               const double *lb, bool *done)
{
    *done = false;
    const double lo = p->rl[i], up = p->ru[i];
    double sgn;
    if (isfinite(up) && !isfinite(lo) && up == 0.0)
        sgn = 1.0;
    else if (isfinite(lo) && !isfinite(up) && lo == 0.0)
        sgn = -1.0;
    else
        return JAOS_OK;
    int64_t neg = 0, off = 0, h = -1, u = -1, v = -1;
    double a0 = 0.0;
    for (int64_t t = 0; t < q->n; t++) {
        const double c = sgn * q->t[t].v;
        if (q->t[t].i == q->t[t].j) {
            if (c < 0.0) {
                neg++;
                h = q->t[t].i;
                a0 = -c;
            }
        } else {
            off++;
            u = q->t[t].i;
            v = q->t[t].j;
            a0 = -c;
        }
    }
    bool soc = off == 0 && neg == 1 && lb[h] >= 0.0;
    bool rot = off == 1 && neg == 0 && a0 > 0.0 && lb[u] >= 0.0 &&
               lb[v] >= 0.0;
    for (int64_t t = 0; rot && t < q->n; t++)
        if (q->t[t].i == q->t[t].j && (q->t[t].i == u || q->t[t].i == v))
            rot = false;
    if (!soc && !rot)
        return JAOS_OK;
    if (!nl_cone_open(p, soc ? JAOS_CONE_QUADRATIC : JAOS_CONE_ROTATED))
        return JAOS_ERR_OUT_OF_MEMORY;
    if (soc ? !nl_cone_col(p, h)
            : !nl_cone_col(p, v < u ? v : u) || !nl_cone_col(p, v < u ? u : v))
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t t = 0; t < q->n; t++) {
        if (q->t[t].i != q->t[t].j || q->t[t].i == h)
            continue;
        const double ratio = (soc ? 1.0 : 2.0) * sgn * q->t[t].v / a0;
        const nl_form x = {.t = (nl_term[]){{q->t[t].i, -1, 1.0}}, .n = 1};
        const jaos_status st = nl_member(p, &x, ratio == 1.0 ? 1.0
                                                             : sqrt(ratio));
        if (st != JAOS_OK)
            return st;
    }
    p->dead[i] = p->nk;
    p->dscale[i] = sgn / (soc ? 2.0 * a0 : a0);
    nl_cone_close(p);
    *done = true;
    return JAOS_OK;
}

typedef struct {
    int64_t r, c, at;
    double v;
} nl_ent;

static int nl_ent_cmp(const void *a, const void *b)
{
    const nl_ent *x = a, *y = b;
    if (x->r != y->r)
        return x->r < y->r ? -1 : 1;
    if (x->c != y->c)
        return x->c < y->c ? -1 : 1;
    return x->at < y->at ? -1 : x->at > y->at;
}

typedef struct {
    int64_t r, at;
} nl_qkey;

static int nl_qkey_cmp(const void *a, const void *b)
{
    const nl_qkey *x = a, *y = b;
    if (x->r != y->r)
        return x->r < y->r ? -1 : 1;
    return x->at < y->at ? -1 : x->at > y->at;
}

static jaos_status nl_merge(nl *p)
{
    if (p->nent == 0)
        return JAOS_OK;
    nl_ent *e = jm_alloc_array(p->nent, sizeof *e);
    if (e == nullptr)
        FAIL_OOM();
    for (int64_t k = 0; k < p->nent; k++)
        e[k] = (nl_ent){p->ei[k], p->ej[k], k, p->ev[k]};
    qsort(e, (size_t)p->nent, sizeof *e, nl_ent_cmp);
    int64_t u = 0;
    for (int64_t k = 0; k < p->nent; k++) {
        if (u > 0 && e[u - 1].r == e[k].r && e[u - 1].c == e[k].c) {
            e[u - 1].v += e[k].v;
            continue;
        }
        e[u++] = e[k];
    }
    int64_t w = 0;
    for (int64_t k = 0; k < u; k++) {
        if (e[k].v == 0.0)
            continue;
        p->ei[w] = e[k].r;
        p->ej[w] = e[k].c;
        p->ev[w++] = e[k].v;
    }
    p->nent = w;
    free(e);
    return JAOS_OK;
}

static jaos_status nl_rowquads(nl *p, nl_form *rq, nl_form *oq)
{
    if (p->nq == 0)
        return JAOS_OK;
    nl_qkey *k = jm_alloc_array(p->nq, sizeof *k);
    if (k == nullptr)
        FAIL_OOM();
    for (int64_t t = 0; t < p->nq; t++)
        k[t] = (nl_qkey){p->qr[t], t};
    qsort(k, (size_t)p->nq, sizeof *k, nl_qkey_cmp);
    bool ok = true;
    for (int64_t t = 0; ok && t < p->nq; t++) {
        const int64_t at = k[t].at, r = p->qr[at];
        ok = form_put(r < 0 ? oq : &rq[r], p->qi[at], p->qj[at], p->qv[at]);
    }
    free(k);
    for (int64_t i = 0; ok && i < p->ncon; i++)
        ok = form_canon(&rq[i]);
    if (ok)
        ok = form_canon(oq);
    if (!ok)
        FAIL_OOM();
    return JAOS_OK;
}

static jaos_status nl_set_quad(jaos_model *m, int64_t row, const nl_form *q)
{
    if (q->n == 0)
        return JAOS_OK;
    int64_t *qi = jm_alloc_array(q->n, sizeof *qi);
    int64_t *qj = jm_alloc_array(q->n, sizeof *qj);
    double *qv = jm_alloc_array(q->n, sizeof *qv);
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (qi != nullptr && qj != nullptr && qv != nullptr) {
        for (int64_t t = 0; t < q->n; t++) {
            qi[t] = q->t[t].i;
            qj[t] = q->t[t].j;
            qv[t] = qi[t] == qj[t] ? 2.0 * q->t[t].v : q->t[t].v;
        }
        st = row < 0 ? jaos_set_quadratic(m, q->n, qi, qj, qv)
                     : jaos_set_row_quadratic(m, row, q->n, qi, qj, qv);
        if (st != JAOS_OK && m->err[0] == '\0')
            jm_set_err(m, "the .nl model's quadratic part failed validation");
    } else {
        jm_set_err(m, "out of memory");
    }
    free(qi);
    free(qj);
    free(qv);
    return st;
}

static void nl_implied(const nl *p, const int64_t *rs, const bool *nonlin,
                       double *lb, double *ub)
{
    for (int64_t j = 0; j < p->nvar; j++) {
        lb[j] = p->cl[j];
        ub[j] = p->cu[j];
    }
    for (int64_t i = 0; i < p->ncon; i++) {
        if (nonlin[i] || rs[i + 1] - rs[i] != 1)
            continue;
        const int64_t j = p->ej[rs[i]];
        const double a = p->ev[rs[i]], lo = p->rl[i], up = p->ru[i];
        const double l = a > 0.0 ? lo / a : up / a;
        const double u = a > 0.0 ? up / a : lo / a;
        if (l > lb[j])
            lb[j] = l;
        if (u < ub[j])
            ub[j] = u;
    }
}

static void nl_names_in(nl *p, const char *path, int64_t ncol, int64_t nrow,
                        const int64_t *rowmap)
{
    jaos_model *m = p->m;
    size_t n = strlen(path);
    char *base = malloc(n + 1);
    if (base == nullptr)
        return;
    memcpy(base, path, n + 1);
    if (n > 3 && strcmp(base + n - 3, ".gz") == 0) {
        base[n - 3] = '\0';
        n -= 3;
    }
    if (n > 3 && strcmp(base + n - 3, ".nl") == 0)
        base[n - 3] = '\0';
    const int64_t extra = p->nobj > 0 ? 1 : 0;
    char **cn = nl_names(m, base, ".col", p->nvar, ncol);
    char **raw = nl_names(m, base, ".row", p->ncon + extra, p->ncon + extra);
    char *on = nullptr;
    char **rn = nullptr;
    if (raw != nullptr) {
        if (extra == 1) {
            on = raw[p->ncon];
            raw[p->ncon] = nullptr;
        }
        rn = jm_calloc_array(nrow > 0 ? nrow : 1, sizeof *rn);
        for (int64_t i = 0; i < p->ncon; i++) {
            if (rn != nullptr && rowmap[i] >= 0) {
                rn[rowmap[i]] = raw[i];
                raw[i] = nullptr;
            }
            free(raw[i]);
        }
        free(raw);
    }
    if (cn != nullptr || rn != nullptr)
        jm_model_take_names(m, cn, rn, on);
    else
        free(on);
    free(base);
}

static jaos_status nl_integers(nl *p, int64_t ncol)
{
    jaos_model *m = p->m;
    const int64_t c = p->nlv[0], o = p->nlv[1], b = p->nlv[2];
    const bool nonlin = p->nli[0] + p->nli[1] + p->nli[2] > 0;
    if (p->nbv + p->niv == 0 && !nonlin)
        return JAOS_OK;
    bool *ci = jm_calloc_array(ncol, sizeof(bool));
    if (ci == nullptr) {
        jm_set_err(m, "out of memory");
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = p->nvar - p->nbv - p->niv; j < p->nvar; j++)
        ci[j] = true;
    if (nonlin) {
        for (int64_t j = b - p->nli[0]; j < b; j++)
            ci[j] = true;
        for (int64_t j = c - p->nli[1]; j < c; j++)
            ci[j] = true;
        for (int64_t j = o - p->nli[2]; o > c && j < o; j++)
            ci[j] = true;
    }
    free(m->col_integer);
    m->col_integer = ci;
    return JAOS_OK;
}

static jaos_status nl_build(nl *p, const char *path)
{
    jaos_model *m = p->m;
    const int64_t nv = p->nvar, nr0 = p->ncon;
    for (int64_t i = 0; i < nr0; i++) {
        if (p->shift[i] != 0.0) {
            p->rl[i] -= p->shift[i];
            p->ru[i] -= p->shift[i];
        }
    }
    jaos_status st = nl_merge(p);
    if (st != JAOS_OK)
        return st;
    const int64_t nlin = p->nent;
    nl_form *rq = jm_calloc_array(nr0 > 0 ? nr0 : 1, sizeof *rq);
    nl_form oq = {0};
    int64_t *rs = jm_calloc_array(nr0 + 1, sizeof *rs);
    bool *nonlin = jm_calloc_array(nr0 > 0 ? nr0 : 1, sizeof *nonlin);
    double *lb = jm_alloc_array(nv > 0 ? nv : 1, sizeof *lb);
    double *ub = jm_alloc_array(nv > 0 ? nv : 1, sizeof *ub);
    p->dead = jm_alloc_array(nr0 > 0 ? nr0 : 1, sizeof *p->dead);
    p->dscale = jm_calloc_array(nr0 > 0 ? nr0 : 1, sizeof *p->dscale);
    p->stamp = jm_calloc_array(nv > 0 ? nv : 1, sizeof *p->stamp);
    int64_t *rowmap = nullptr, *as = nullptr, *fill = nullptr, *ai = nullptr;
    double *av = nullptr, *cost = nullptr, *cl = nullptr, *cu = nullptr;
    double *rl = nullptr, *ru = nullptr;
    if (rq == nullptr || rs == nullptr || nonlin == nullptr || lb == nullptr ||
        ub == nullptr || p->dead == nullptr || p->dscale == nullptr ||
        p->stamp == nullptr || !nl_cone_open(p, 0)) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    for (int64_t i = 0; i < nr0; i++)
        p->dead[i] = -1;
    if ((st = nl_rowquads(p, rq, &oq)) != JAOS_OK)
        goto out;
    for (int64_t k = 0; k < nlin; k++)
        rs[p->ei[k] + 1]++;
    for (int64_t i = 0; i < nr0; i++) {
        rs[i + 1] += rs[i];
        nonlin[i] = rq[i].n > 0;
    }
    for (int64_t k = 0; k < p->natom; k++)
        if (p->atom[k].host >= 0)
            nonlin[p->atom[k].host] = true;
    nl_implied(p, rs, nonlin, lb, ub);
    for (int64_t i = 0; i < nr0; i++) {
        if (rq[i].n == 0 || rs[i + 1] > rs[i])
            continue;
        bool hosted = false;
        for (int64_t k = 0; k < p->natom && !hosted; k++)
            hosted = p->atom[k].host == i;
        bool done = false;
        if (!hosted && (st = nl_cone_row(p, i, &rq[i], lb, &done)) != JAOS_OK) {
            if (st == JAOS_ERR_OUT_OF_MEMORY)
                jm_set_err(m, "out of memory");
            goto out;
        }
    }
    for (int64_t k = 0; k < p->natom; k++) {
        if (!nl_cone_open(p, 0)) {
            st = JAOS_ERR_OUT_OF_MEMORY;
        } else {
            st = nl_atom_build(p, &p->atom[k], lb, ub);
        }
        if (st != JAOS_OK) {
            if (st == JAOS_ERR_OUT_OF_MEMORY)
                jm_set_err(m, "out of memory");
            goto out;
        }
    }
    const int64_t nc = nv + p->nx;
    rowmap = jm_alloc_array(nr0 + p->ny > 0 ? nr0 + p->ny : 1, sizeof *rowmap);
    if (rowmap == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    int64_t nr = 0;
    for (int64_t i = 0; i < nr0; i++)
        rowmap[i] = p->dead[i] >= 0 ? -1 : nr++;
    for (int64_t k = 0; k < p->ny; k++)
        rowmap[nr0 + k] = nr++;
    as = jm_calloc_array(nc + 1, sizeof *as);
    fill = jm_calloc_array(nc > 0 ? nc : 1, sizeof *fill);
    ai = jm_alloc_array(p->nent > 0 ? p->nent : 1, sizeof *ai);
    av = jm_alloc_array(p->nent > 0 ? p->nent : 1, sizeof *av);
    cost = jm_calloc_array(nc > 0 ? nc : 1, sizeof *cost);
    cl = jm_alloc_array(nc > 0 ? nc : 1, sizeof *cl);
    cu = jm_alloc_array(nc > 0 ? nc : 1, sizeof *cu);
    rl = jm_alloc_array(nr > 0 ? nr : 1, sizeof *rl);
    ru = jm_alloc_array(nr > 0 ? nr : 1, sizeof *ru);
    if (as == nullptr || fill == nullptr || ai == nullptr || av == nullptr ||
        cost == nullptr || cl == nullptr || cu == nullptr || rl == nullptr ||
        ru == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    for (int64_t j = 0; j < nc; j++) {
        cost[j] = j < nv ? p->cost[j] : p->xcost[j - nv];
        cl[j] = j < nv ? p->cl[j] : p->xlo[j - nv];
        cu[j] = j < nv ? p->cu[j] : p->xup[j - nv];
    }
    for (int64_t i = 0; i < nr0; i++)
        if (rowmap[i] >= 0) {
            rl[rowmap[i]] = p->rl[i];
            ru[rowmap[i]] = p->ru[i];
        }
    for (int64_t k = 0; k < p->ny; k++)
        rl[rowmap[nr0 + k]] = ru[rowmap[nr0 + k]] = p->yrhs[k];
    int64_t nent = 0;
    for (int64_t k = 0; k < p->nent; k++)
        if (rowmap[p->ei[k]] >= 0)
            as[p->ej[k] + 1]++;
    for (int64_t j = 0; j < nc; j++)
        as[j + 1] += as[j];
    for (int64_t k = 0; k < p->nent; k++) {
        const int64_t r = rowmap[p->ei[k]], j = p->ej[k];
        if (r < 0)
            continue;
        const int64_t at = as[j] + fill[j]++;
        ai[at] = r;
        av[at] = p->ev[k];
        nent++;
    }
    st = jaos_load_lp(m, nc, nr, p->sense, p->offset, cost, cl, cu, rl, ru,
                      nent, nc > 0 ? as : nullptr, ai, av);
    if (st == JAOS_OK)
        st = nl_set_quad(m, -1, &oq);
    for (int64_t i = 0; i < nr0 && st == JAOS_OK; i++)
        if (rowmap[i] >= 0)
            st = nl_set_quad(m, rowmap[i], &rq[i]);
    for (int64_t k = 0; k < p->nk && st == JAOS_OK; k++) {
        const int64_t b = p->kstart[k], e = p->kstart[k + 1];
        if (e > b)
            st = jaos_add_cone(m, (jaos_cone_type)p->ktype[k], e - b,
                               p->kcol + b);
    }
    if (st != JAOS_OK) {
        if (m->err[0] == '\0')
            jm_set_err(m, "the .nl model failed validation");
        goto out;
    }
    if ((st = nl_integers(p, nc)) != JAOS_OK)
        goto out;
    nl_names_in(p, path, nc, nr, rowmap);
    if (p->x0 != nullptr && nc > nv) {
        double *x = jm_alloc_array(nc, sizeof *x);
        if (x == nullptr) {
            jm_set_err(m, "out of memory");
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto out;
        }
        for (int64_t j = 0; j < nc; j++)
            x[j] = j < nv ? p->x0[j] : NAN;
        free(p->x0);
        p->x0 = x;
    }
    free(m->nl_row_at);
    free(m->nl_row_scale);
    m->nl_row_at = jm_alloc_array(nr0 > 0 ? nr0 : 1, sizeof *m->nl_row_at);
    m->nl_row_scale = jm_alloc_array(nr0 > 0 ? nr0 : 1,
                                     sizeof *m->nl_row_scale);
    if (m->nl_row_at == nullptr || m->nl_row_scale == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    for (int64_t i = 0; i < nr0; i++) {
        m->nl_row_at[i] = p->dead[i] >= 0 ? -1 - p->dead[i] : rowmap[i];
        m->nl_row_scale[i] = p->dscale[i];
    }
    m->nl_kept_rows = nr;
    m->nl_kept_cols = nc;
    m->nl_kept_cones = m->num_cone;
    st = JAOS_OK;
out:
    if (rq != nullptr)
        for (int64_t i = 0; i < nr0; i++)
            form_free(&rq[i]);
    free(rq);
    form_free(&oq);
    free(rs);
    free(nonlin);
    free(lb);
    free(ub);
    free(rowmap);
    free(as);
    free(fill);
    free(ai);
    free(av);
    free(cost);
    free(cl);
    free(cu);
    free(rl);
    free(ru);
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
    for (int64_t k = 0; k < p->natom; k++)
        atom_free(&p->atom[k]);
    free(p->atom);
    free(p->xlo); free(p->xup); free(p->xcost); free(p->yrhs);
    free(p->ktype); free(p->kstart); free(p->kcol);
    free(p->dead); free(p->dscale); free(p->stamp);
    return st;
}
