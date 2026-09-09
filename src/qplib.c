/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "jaos_sys.h"
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr double QPLIB_INF = 1e20;

typedef struct {
    jaos_model *m;
    char *buf;
    int64_t len, pos, line;
    char kind[4];
    jaos_obj_sense sense;
    int64_t nvar, ncon;
    double *cost, *cl, *cu, *rl, *ru, *quad;
    bool *cint;
    int64_t *ei, *ej;
    double *ev;
    int64_t nent, ecap, jcap, vcap;
    double offset;
    char **cname, **rname;
    char *pname;
} qp;

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

static bool q_next(qp *p, char **out)
{
    for (;;) {
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
        p->line++;
        char *hash = strchr(s, '#');
        if (hash != nullptr)
            *hash = '\0';
        char *bang = strchr(s, '!');
        if (bang != nullptr)
            *bang = '\0';
        size_t n = strlen(s);
        while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
            s[--n] = '\0';
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '\0')
            continue;
        *out = s;
        return true;
    }
}

static jaos_status q_int(qp *p, const char *what, int64_t *v)
{
    char *s;
    if (!q_next(p, &s))
        FAIL("line %" PRId64 ": the file ends where %s was expected",
             p->line, what);
    char *end;
    const long long x = strtoll(s, &end, 10);
    if (end == s)
        FAIL("line %" PRId64 ": expected %s, found '%s'", p->line, what, s);
    *v = x;
    return JAOS_OK;
}

static double q_value(double v)
{
    if (v >= QPLIB_INF)
        return INFINITY;
    if (v <= -QPLIB_INF)
        return -INFINITY;
    return v;
}

static jaos_status q_num(qp *p, const char *what, double *v)
{
    char *s;
    if (!q_next(p, &s))
        FAIL("line %" PRId64 ": the file ends where %s was expected",
             p->line, what);
    char *end;
    const double x = strtod(s, &end);
    if (end == s)
        FAIL("line %" PRId64 ": expected %s, found '%s'", p->line, what, s);
    *v = x;
    return JAOS_OK;
}

static jaos_status q_index_value(qp *p, const char *what, int64_t limit,
                                 int64_t *i, double *v)
{
    char *s;
    if (!q_next(p, &s))
        FAIL("line %" PRId64 ": the file ends inside the %s entries",
             p->line, what);
    char *end;
    const long long k = strtoll(s, &end, 10);
    if (end == s)
        FAIL("line %" PRId64 ": expected an index in the %s entries",
             p->line, what);
    const double x = strtod(end, &end);
    if (end == s)
        FAIL("line %" PRId64 ": expected a value in the %s entries",
             p->line, what);
    if (k < 1 || k > limit)
        FAIL("line %" PRId64 ": index %lld in the %s entries is outside 1 to "
             "%" PRId64, p->line, k, what, limit);
    *i = k - 1;
    *v = x;
    return JAOS_OK;
}

static jaos_status q_vector(qp *p, const char *what, int64_t n, double *out,
                            bool bound)
{
    double dflt;
    jaos_status st = q_num(p, what, &dflt);
    if (st != JAOS_OK)
        return st;
    for (int64_t k = 0; k < n; k++)
        out[k] = bound ? q_value(dflt) : dflt;
    int64_t count;
    if ((st = q_int(p, what, &count)) != JAOS_OK)
        return st;
    if (count < 0 || count > n)
        FAIL("line %" PRId64 ": %" PRId64 " entries of %s against %" PRId64,
             p->line, count, what, n);
    for (int64_t k = 0; k < count; k++) {
        int64_t i;
        double v;
        if ((st = q_index_value(p, what, n, &i, &v)) != JAOS_OK)
            return st;
        out[i] = bound ? q_value(v) : v;
    }
    return JAOS_OK;
}

static jaos_status q_skip_vector(qp *p, const char *what, int64_t n)
{
    double dflt;
    jaos_status st = q_num(p, what, &dflt);
    if (st != JAOS_OK)
        return st;
    int64_t count;
    if ((st = q_int(p, what, &count)) != JAOS_OK)
        return st;
    if (count < 0 || count > n)
        FAIL("line %" PRId64 ": %" PRId64 " entries of %s against %" PRId64,
             p->line, count, what, n);
    for (int64_t k = 0; k < count; k++) {
        int64_t i;
        double v;
        if ((st = q_index_value(p, what, n, &i, &v)) != JAOS_OK)
            return st;
    }
    return JAOS_OK;
}

static jaos_status q_names(qp *p, const char *what, int64_t n, char ***out)
{
    int64_t count;
    jaos_status st = q_int(p, what, &count);
    if (st != JAOS_OK)
        return st;
    if (count < 0 || count > n)
        FAIL("line %" PRId64 ": %" PRId64 " %s against %" PRId64, p->line,
             count, what, n);
    if (count == 0)
        return JAOS_OK;
    char **names = jm_calloc_array(n > 0 ? n : 1, sizeof *names);
    if (names == nullptr)
        FAIL_OOM();
    *out = names;
    for (int64_t k = 0; k < count; k++) {
        char *s;
        if (!q_next(p, &s))
            FAIL("line %" PRId64 ": the file ends inside the %s", p->line,
                 what);
        char *end;
        const long long i = strtoll(s, &end, 10);
        if (end == s || i < 1 || i > n)
            FAIL("line %" PRId64 ": expected an index in the %s", p->line,
                 what);
        while (*end == ' ' || *end == '\t')
            end++;
        if (*end == '\0' || !jm_name_ok(end))
            FAIL("line %" PRId64 ": '%s' is not a name JAOS accepts", p->line,
                 end);
        free(names[i - 1]);
        names[i - 1] = jm_name_copy(end);
        if (names[i - 1] == nullptr)
            FAIL_OOM();
    }
    return JAOS_OK;
}

static jaos_status q_parse(qp *p)
{
    char *s;
    if (!q_next(p, &s))
        FAIL("line 1: an empty file");
    p->pname = jm_name_ok(s) ? jm_name_copy(s) : nullptr;
    if (!q_next(p, &s) || strlen(s) != 3)
        FAIL("line %" PRId64 ": expected the three-letter problem type",
             p->line);
    for (int k = 0; k < 3; k++)
        p->kind[k] = (char)toupper((unsigned char)s[k]);
    p->kind[3] = '\0';
    const char obj = p->kind[0], var = p->kind[1], con = p->kind[2];
    if (obj != 'L' && obj != 'D' && obj != 'C' && obj != 'Q')
        FAIL("line %" PRId64 ": objective type '%c' is not one of L, D, C, Q",
             p->line, obj);
    if (var != 'C' && var != 'B' && var != 'M' && var != 'I' && var != 'G')
        FAIL("line %" PRId64 ": variable type '%c' is not one of C, B, M, I, "
             "G", p->line, var);
    if (con != 'N' && con != 'B' && con != 'L')
        FAIL("line %" PRId64 ": constraint type '%c'; JAOS reads N, B and L, "
             "not quadratic constraints", p->line, con);
    if (!q_next(p, &s))
        FAIL("line %" PRId64 ": expected minimize or maximize", p->line);
    for (char *c = s; *c; c++)
        *c = (char)tolower((unsigned char)*c);
    if (strncmp(s, "min", 3) == 0)
        p->sense = JAOS_MINIMIZE;
    else if (strncmp(s, "max", 3) == 0)
        p->sense = JAOS_MAXIMIZE;
    else
        FAIL("line %" PRId64 ": expected minimize or maximize, found '%s'",
             p->line, s);

    jaos_status st;
    if ((st = q_int(p, "the number of variables", &p->nvar)) != JAOS_OK)
        return st;
    if (p->nvar < 0)
        FAIL("line %" PRId64 ": a negative number of variables", p->line);
    if (con == 'L') {
        if ((st = q_int(p, "the number of constraints", &p->ncon)) != JAOS_OK)
            return st;
        if (p->ncon < 0)
            FAIL("line %" PRId64 ": a negative number of constraints", p->line);
    }
    const int64_t nv = p->nvar > 0 ? p->nvar : 1;
    const int64_t nr = p->ncon > 0 ? p->ncon : 1;
    p->cost = jm_calloc_array(nv, sizeof(double));
    p->cl = jm_calloc_array(nv, sizeof(double));
    p->cu = jm_calloc_array(nv, sizeof(double));
    p->quad = jm_calloc_array(nv, sizeof(double));
    p->cint = jm_calloc_array(nv, sizeof(bool));
    p->rl = jm_calloc_array(nr, sizeof(double));
    p->ru = jm_calloc_array(nr, sizeof(double));
    if (!p->cost || !p->cl || !p->cu || !p->quad || !p->cint || !p->rl ||
        !p->ru)
        FAIL_OOM();

    if (obj != 'L') {
        int64_t nq;
        if ((st = q_int(p, "the number of objective Q entries", &nq)) != JAOS_OK)
            return st;
        for (int64_t k = 0; k < nq; k++) {
            if (!q_next(p, &s))
                FAIL("line %" PRId64 ": the file ends inside the Q entries",
                     p->line);
            char *end;
            const long long i = strtoll(s, &end, 10);
            const long long j = strtoll(end, &end, 10);
            const double v = strtod(end, &end);
            if (i < 1 || i > p->nvar || j < 1 || j > p->nvar)
                FAIL("line %" PRId64 ": a Q entry names variable %lld or %lld "
                     "outside 1 to %" PRId64, p->line, i, j, p->nvar);
            if (i != j)
                FAIL("line %" PRId64 ": the Q entry (%lld, %lld) is off the "
                     "diagonal; JAOS reads a separable quadratic objective "
                     "only", p->line, i, j);
            if (!isfinite(v))
                FAIL("line %" PRId64 ": a Q entry is not finite", p->line);
            p->quad[i - 1] += v;
        }
    }
    if ((st = q_vector(p, "the objective coefficients", p->nvar, p->cost,
                       false)) != JAOS_OK)
        return st;
    if ((st = q_num(p, "the objective constant", &p->offset)) != JAOS_OK)
        return st;

    if (con == 'L') {
        int64_t na;
        if ((st = q_int(p, "the number of constraint entries", &na)) != JAOS_OK)
            return st;
        for (int64_t k = 0; k < na; k++) {
            if (!q_next(p, &s))
                FAIL("line %" PRId64 ": the file ends inside the constraint "
                     "entries", p->line);
            char *end;
            const long long i = strtoll(s, &end, 10);
            const long long j = strtoll(end, &end, 10);
            const double v = strtod(end, &end);
            if (i < 1 || i > p->ncon)
                FAIL("line %" PRId64 ": constraint %lld is outside 1 to %"
                     PRId64, p->line, i, p->ncon);
            if (j < 1 || j > p->nvar)
                FAIL("line %" PRId64 ": variable %lld is outside 1 to %" PRId64,
                     p->line, j, p->nvar);
            if (!isfinite(v))
                FAIL("line %" PRId64 ": a constraint entry is not finite",
                     p->line);
            if (!JM_GROW(p->ei, p->ecap, p->nent + 1) ||
                !JM_GROW(p->ej, p->jcap, p->nent + 1) ||
                !JM_GROW(p->ev, p->vcap, p->nent + 1))
                FAIL_OOM();
            p->ei[p->nent] = i - 1;
            p->ej[p->nent] = j - 1;
            p->ev[p->nent] = v;
            p->nent++;
        }
        if ((st = q_vector(p, "the constraint lower bounds", p->ncon, p->rl,
                           true)) != JAOS_OK)
            return st;
        if ((st = q_vector(p, "the constraint upper bounds", p->ncon, p->ru,
                           true)) != JAOS_OK)
            return st;
    }
    if (var == 'B') {
        for (int64_t j = 0; j < p->nvar; j++) {
            p->cl[j] = 0.0;
            p->cu[j] = 1.0;
            p->cint[j] = true;
        }
    } else if (con != 'N') {
        if ((st = q_vector(p, "the variable lower bounds", p->nvar, p->cl,
                           true)) != JAOS_OK)
            return st;
        if ((st = q_vector(p, "the variable upper bounds", p->nvar, p->cu,
                           true)) != JAOS_OK)
            return st;
    } else {
        for (int64_t j = 0; j < p->nvar; j++) {
            p->cl[j] = -INFINITY;
            p->cu[j] = INFINITY;
        }
    }
    if (var == 'I') {
        for (int64_t j = 0; j < p->nvar; j++)
            p->cint[j] = true;
    } else if (var == 'M' || var == 'G') {
        double *types = jm_calloc_array(nv, sizeof(double));
        if (types == nullptr)
            FAIL_OOM();
        st = q_vector(p, "the variable types", p->nvar, types, false);
        if (st != JAOS_OK) {
            free(types);
            return st;
        }
        for (int64_t j = 0; j < p->nvar; j++) {
            if (types[j] == 0.0)
                continue;
            if (types[j] != 1.0 && types[j] != 2.0) {
                const double bad = types[j];
                free(types);
                FAIL("variable %" PRId64 " has type %g, which is not 0, 1 or "
                     "2", j + 1, bad);
            }
            p->cint[j] = true;
            if (types[j] == 2.0) {
                p->cl[j] = 0.0;
                p->cu[j] = 1.0;
            }
        }
        free(types);
    }
    if ((st = q_skip_vector(p, "the initial primal values", p->nvar)) != JAOS_OK)
        return st;
    if (con == 'L' &&
        (st = q_skip_vector(p, "the initial dual values", p->ncon)) != JAOS_OK)
        return st;
    if ((st = q_skip_vector(p, "the initial reduced costs", p->nvar)) != JAOS_OK)
        return st;
    if ((st = q_names(p, "variable names", p->nvar, &p->cname)) != JAOS_OK)
        return st;
    if (con == 'L' &&
        (st = q_names(p, "constraint names", p->ncon, &p->rname)) != JAOS_OK)
        return st;
    return JAOS_OK;
}

static jaos_status q_build(qp *p)
{
    jaos_model *m = p->m;
    const int64_t nc = p->nvar, nr = p->ncon;
    int64_t *as = jm_calloc_array(nc + 1, sizeof *as);
    int64_t *fill = jm_calloc_array(nc > 0 ? nc : 1, sizeof *fill);
    int64_t *ai = malloc((size_t)(p->nent > 0 ? p->nent : 1) * sizeof *ai);
    double *av = malloc((size_t)(p->nent > 0 ? p->nent : 1) * sizeof *av);
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
    if (st != JAOS_OK) {
        if (m->err[0] == '\0')
            jm_set_err(m, "the QPLIB model failed validation");
        goto out;
    }
    bool any_int = false, any_quad = false;
    for (int64_t j = 0; j < nc; j++) {
        any_int |= p->cint[j];
        any_quad |= p->quad[j] != 0.0;
    }
    if (any_int) {
        free(m->col_integer);
        m->col_integer = p->cint;
        p->cint = nullptr;
    }
    if (any_quad) {
        free(m->col_quad);
        m->col_quad = p->quad;
        p->quad = nullptr;
    }
    if (p->cname != nullptr || p->rname != nullptr) {
        if (p->cname == nullptr) {
            p->cname = jm_calloc_array(nc > 0 ? nc : 1, sizeof *p->cname);
            if (p->cname == nullptr) {
                jm_set_err(m, "out of memory");
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto out;
            }
        }
        if (p->rname == nullptr) {
            p->rname = jm_calloc_array(nr > 0 ? nr : 1, sizeof *p->rname);
            if (p->rname == nullptr) {
                jm_set_err(m, "out of memory");
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto out;
            }
        }
        for (int64_t j = 0; j < nc; j++)
            if (p->cname[j] == nullptr) {
                char tmp[JM_NAME_BUF];
                snprintf(tmp, sizeof tmp, "C%" PRId64, j + 1);
                p->cname[j] = jm_name_copy(tmp);
                if (p->cname[j] == nullptr) {
                    jm_set_err(m, "out of memory");
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto out;
                }
            }
        for (int64_t i = 0; i < nr; i++)
            if (p->rname[i] == nullptr) {
                char tmp[JM_NAME_BUF];
                snprintf(tmp, sizeof tmp, "R%" PRId64, i + 1);
                p->rname[i] = jm_name_copy(tmp);
                if (p->rname[i] == nullptr) {
                    jm_set_err(m, "out of memory");
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto out;
                }
            }
        jm_model_take_names(m, p->cname, p->rname, nullptr);
        p->cname = nullptr;
        p->rname = nullptr;
    }
    if (p->pname != nullptr) {
        free(m->model_name);
        m->model_name = p->pname;
        p->pname = nullptr;
    }
    st = JAOS_OK;
out:
    free(as);
    free(fill);
    free(ai);
    free(av);
    return st;
}

static void q_free(qp *p)
{
    free(p->buf);
    free(p->cost); free(p->cl); free(p->cu); free(p->rl); free(p->ru);
    free(p->quad); free(p->cint);
    free(p->ei); free(p->ej); free(p->ev);
    if (p->cname != nullptr)
        for (int64_t j = 0; j < p->nvar; j++)
            free(p->cname[j]);
    free(p->cname);
    if (p->rname != nullptr)
        for (int64_t i = 0; i < p->ncon; i++)
            free(p->rname[i]);
    free(p->rname);
    free(p->pname);
}

jaos_status jaos_read_qplib(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    qp pp = {0};
    qp *p = &pp;
    p->m = m;
    p->sense = JAOS_MINIMIZE;
    jaos_status st = jm_slurp(m, path, &p->buf, &p->len);
    if (st != JAOS_OK)
        return st;
    {
        jm_locale loc;
        jm_locale_c_enter(&loc);
        st = q_parse(p);
        if (st == JAOS_OK)
            st = q_build(p);
        jm_locale_leave(&loc);
    }
    q_free(p);
    return st;
}
