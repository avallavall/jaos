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

static jaos_status nl_expr(nl *p, const char *what, int64_t which, double *c)
{
    char *s;
    if (!nl_next(p, &s))
        FAIL("line %" PRId64 ": %s %" PRId64 " has no expression", p->line,
             what, which);
    if (s[0] != 'n' || nl_nums(s + 1, c, 1) != 1)
        FAIL("line %" PRId64 ": %s %" PRId64 " has a nonlinear expression, "
             "which JAOS does not read; only a constant body is taken",
             p->line, what, which);
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
    if (!nl_next(p, &s) || nl_ints(s, v, 3) < 3)
        FAIL("line 2: expected the counts of variables, constraints and "
             "objectives");
    p->nvar = v[0];
    p->ncon = v[1];
    p->nobj = v[2];
    if (p->nvar < 0 || p->ncon < 0 || p->nobj < 0)
        FAIL("line 2: a count is negative");
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 3: expected the nonlinear constraint and objective counts");
    if (v[0] > 0 || v[1] > 0)
        FAIL("line 3: %" PRId64 " nonlinear constraints and %" PRId64
             " nonlinear objectives; JAOS reads linear models only",
             v[0], v[1]);
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 4: expected the network constraint counts");
    if (v[0] > 0 || v[1] > 0)
        FAIL("line 4: network constraints, which JAOS does not read");
    if (!nl_next(p, &s) || nl_ints(s, v, 2) < 2)
        FAIL("line 5: expected the nonlinear variable counts");
    {
        const int got = nl_ints(s, v, 3);
        for (int k = 0; k < got; k++)
            if (v[k] > 0)
                FAIL("line 5: nonlinear variables; JAOS reads linear models "
                     "only");
    }
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
    if (p->nbv < 0 || p->niv < 0 || p->nbv + p->niv > p->nvar)
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
        double c = 0.0;
        switch (s[0]) {
        case 'C':
            if (got < 1 || v[0] < 0 || v[0] >= p->ncon)
                FAIL("line %" PRId64 ": 'C' needs a row index below %" PRId64,
                     p->line, p->ncon);
            if ((st = nl_expr(p, "row", v[0], &c)) != JAOS_OK)
                return st;
            p->shift[v[0]] = c;
            break;
        case 'O':
            if (got < 2 || v[0] < 0 || v[0] >= p->nobj)
                FAIL("line %" PRId64 ": 'O' needs an objective index below %"
                     PRId64 " and a sense", p->line, p->nobj);
            if ((st = nl_expr(p, "objective", v[0], &c)) != JAOS_OK)
                return st;
            if (v[0] == 0) {
                p->offset = c;
                p->sense = v[1] == 1 ? JAOS_MAXIMIZE : JAOS_MINIMIZE;
            }
            break;
        case 'x':
        case 'd':
            if (got < 1 || v[0] < 0)
                FAIL("line %" PRId64 ": '%c' needs a count", p->line, s[0]);
            if ((st = nl_skip(p, v[0], s[0] == 'x' ? "initial guess"
                                                    : "dual guess")) != JAOS_OK)
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
        FAIL("the file has no 'b' segment, so the column bounds are unknown");
    if (!seen_r && p->ncon > 0)
        FAIL("the file has no 'r' segment, so the row bounds are unknown");
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
            char **cn = nl_names(m, base, ".col", nc);
            char **rn = nl_names(m, base, ".row", nr + 1);
            char *on = nullptr;
            if (rn != nullptr) {
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
            p->cu = malloc((size_t)nc * sizeof *p->cu);
            p->rl = malloc((size_t)nr * sizeof *p->rl);
            p->ru = malloc((size_t)nr * sizeof *p->ru);
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
        jm_locale_leave(&loc);
    }
    free(p->buf);
    free(p->cost); free(p->cl); free(p->cu); free(p->rl); free(p->ru);
    free(p->shift); free(p->ei); free(p->ej); free(p->ev);
    return st;
}
