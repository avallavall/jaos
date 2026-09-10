/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "jaos_sys.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr double OSIL_INF = 1e30;
constexpr int OSIL_MAX_ATTR = 16;

enum { OX_START, OX_END, OX_SELF };
enum { OV_NONE, OV_START, OV_IDX, OV_VALUE };

typedef struct {
    char *key, *val;
} oattr;

typedef struct {
    jaos_model *m;
    char *buf;
    int64_t len, pos, line;
    char *name;
    int kind;
    oattr at[OSIL_MAX_ATTR];
    int nat;
    bool trunc;

    int64_t nvar, ncon, ccap, rcap;
    double *cost, *cl, *cu, *rl, *ru, *quad;
    int64_t *qri, *qci;
    double  *qvv;
    int64_t nqoff, qoff_cap;
    bool *cint, *csemi;
    char **cname, **rname;
    char *pname, *oname;
    double offset;
    jaos_obj_sense sense;

    int64_t want_var, want_con, want_nz, nobj;
    bool have_var_count, have_con_count, have_nz_count;

    int64_t *start, *idx;
    double *value;
    int64_t nstart, nidx, nvalue, scap, icap, vcap;
    bool by_row, oriented, seen_lcc;

    bool in_header, in_obj, in_lcc;
    int vec;
} ox;

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

static bool x_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void x_decode(char *s)
{
    char *w = s;
    for (char *r = s; *r != '\0';) {
        if (*r != '&') {
            *w++ = *r++;
            continue;
        }
        char *semi = strchr(r, ';');
        char out = '\0';
        if (semi != nullptr && semi - r <= 10) {
            const char *e = r + 1;
            const size_t n = (size_t)(semi - r - 1);
            if (n == 3 && memcmp(e, "amp", 3) == 0)
                out = '&';
            else if (n == 2 && memcmp(e, "lt", 2) == 0)
                out = '<';
            else if (n == 2 && memcmp(e, "gt", 2) == 0)
                out = '>';
            else if (n == 4 && memcmp(e, "quot", 4) == 0)
                out = '"';
            else if (n == 4 && memcmp(e, "apos", 4) == 0)
                out = '\'';
            else if (n >= 2 && e[0] == '#') {
                const int base = (e[1] == 'x' || e[1] == 'X') ? 16 : 10;
                char *end;
                const long v = strtol(e + (base == 16 ? 2 : 1), &end, base);
                if (end == semi && v > 0 && v < 128)
                    out = (char)v;
            }
        }
        if (out == '\0') {
            *w++ = *r++;
            continue;
        }
        *w++ = out;
        r = semi + 1;
    }
    *w = '\0';
}

static bool x_next(ox *p)
{
    char *b = p->buf;
    for (;;) {
        int64_t i = p->pos;
        while (i < p->len && b[i] != '<') {
            if (b[i] == '\n')
                p->line++;
            i++;
        }
        if (i >= p->len) {
            p->pos = p->len;
            return false;
        }
        if (p->len - i >= 4 && memcmp(b + i, "<!--", 4) == 0) {
            int64_t j = i + 4;
            while (j + 3 <= p->len && memcmp(b + j, "-->", 3) != 0) {
                if (b[j] == '\n')
                    p->line++;
                j++;
            }
            if (j + 3 > p->len) {
                p->trunc = true;
                p->pos = p->len;
                return false;
            }
            p->pos = j + 3;
            continue;
        }
        if (i + 1 < p->len && (b[i + 1] == '?' || b[i + 1] == '!')) {
            int64_t j = i + 1;
            while (j < p->len && b[j] != '>') {
                if (b[j] == '\n')
                    p->line++;
                j++;
            }
            if (j >= p->len) {
                p->trunc = true;
                p->pos = p->len;
                return false;
            }
            p->pos = j + 1;
            continue;
        }
        int64_t j = i + 1;
        p->kind = OX_START;
        if (j < p->len && b[j] == '/') {
            p->kind = OX_END;
            j++;
        }
        const int64_t ns = j;
        while (j < p->len && !x_space(b[j]) && b[j] != '>' && b[j] != '/')
            j++;
        const int64_t ne = j;
        p->nat = 0;
        bool closed = false;
        for (;;) {
            while (j < p->len && x_space(b[j])) {
                if (b[j] == '\n')
                    p->line++;
                j++;
            }
            if (j >= p->len)
                break;
            if (b[j] == '/') {
                p->kind = OX_SELF;
                j++;
                continue;
            }
            if (b[j] == '>') {
                j++;
                closed = true;
                break;
            }
            const int64_t ks = j;
            while (j < p->len && b[j] != '=' && !x_space(b[j]) &&
                   b[j] != '>' && b[j] != '/')
                j++;
            const int64_t ke = j;
            while (j < p->len && x_space(b[j])) {
                if (b[j] == '\n')
                    p->line++;
                j++;
            }
            char *val = nullptr;
            if (j < p->len && b[j] == '=') {
                j++;
                while (j < p->len && x_space(b[j])) {
                    if (b[j] == '\n')
                        p->line++;
                    j++;
                }
                if (j < p->len && (b[j] == '"' || b[j] == '\'')) {
                    const char q = b[j++];
                    const int64_t vs = j;
                    while (j < p->len && b[j] != q) {
                        if (b[j] == '\n')
                            p->line++;
                        j++;
                    }
                    val = b + vs;
                    if (j < p->len)
                        b[j++] = '\0';
                }
            }
            if (val == nullptr)
                continue;
            b[ke] = '\0';
            x_decode(val);
            if (p->nat < OSIL_MAX_ATTR) {
                p->at[p->nat].key = b + ks;
                p->at[p->nat].val = val;
                p->nat++;
            }
        }
        if (!closed) {
            p->trunc = true;
            p->pos = p->len;
            return false;
        }
        b[ne] = '\0';
        p->name = b + ns;
        char *colon = strchr(p->name, ':');
        if (colon != nullptr)
            p->name = colon + 1;
        p->pos = j;
        return true;
    }
}

static const char *x_attr(const ox *p, const char *key)
{
    for (int k = 0; k < p->nat; k++)
        if (strcmp(p->at[k].key, key) == 0)
            return p->at[k].val;
    return nullptr;
}

static bool x_num(const char *s, double *out)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *end;
    double v = strtod(s, &end);
    if (end == s)
        return false;
    while (*end == ' ' || *end == '\t')
        end++;
    if (*end != '\0')
        return false;
    if (v >= OSIL_INF)
        v = INFINITY;
    else if (v <= -OSIL_INF)
        v = -INFINITY;
    *out = v;
    return true;
}

static bool x_int(const char *s, int64_t *out)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *end;
    const long long v = strtoll(s, &end, 10);
    if (end == s)
        return false;
    while (*end == ' ' || *end == '\t')
        end++;
    if (*end != '\0')
        return false;
    *out = (int64_t)v;
    return true;
}

static bool x_text_num(const ox *p, double *out)
{
    const char *s = p->buf + p->pos;
    char *end;
    double v = strtod(s, &end);
    if (end == s)
        return false;
    if (v >= OSIL_INF)
        v = INFINITY;
    else if (v <= -OSIL_INF)
        v = -INFINITY;
    *out = v;
    return true;
}

static char *x_text_dup(const ox *p)
{
    int64_t e = p->pos;
    while (e < p->len && p->buf[e] != '<')
        e++;
    int64_t s = p->pos;
    while (s < e && x_space(p->buf[s]))
        s++;
    while (e > s && x_space(p->buf[e - 1]))
        e--;
    char *t = malloc((size_t)(e - s) + 1);
    if (t == nullptr)
        return nullptr;
    memcpy(t, p->buf + s, (size_t)(e - s));
    t[e - s] = '\0';
    x_decode(t);
    return t;
}

static jaos_status o_var_room(ox *p)
{
    const int64_t need = p->nvar + 1;
    if (need <= p->ccap)
        return JAOS_OK;
    int64_t cap = p->ccap;
    if (!JM_GROW(p->cost, cap, need))
        FAIL_OOM();
    cap = p->ccap;
    if (!JM_GROW(p->cl, cap, need))
        FAIL_OOM();
    cap = p->ccap;
    if (!JM_GROW(p->cu, cap, need))
        FAIL_OOM();
    cap = p->ccap;
    if (!JM_GROW(p->quad, cap, need))
        FAIL_OOM();
    cap = p->ccap;
    if (!JM_GROW(p->cint, cap, need))
        FAIL_OOM();
    cap = p->ccap;
    if (!JM_GROW(p->csemi, cap, need))
        FAIL_OOM();
    cap = p->ccap;
    if (!JM_GROW(p->cname, cap, need))
        FAIL_OOM();
    p->ccap = cap;
    return JAOS_OK;
}

static jaos_status o_con_room(ox *p)
{
    const int64_t need = p->ncon + 1;
    if (need <= p->rcap)
        return JAOS_OK;
    int64_t cap = p->rcap;
    if (!JM_GROW(p->rl, cap, need))
        FAIL_OOM();
    cap = p->rcap;
    if (!JM_GROW(p->ru, cap, need))
        FAIL_OOM();
    cap = p->rcap;
    if (!JM_GROW(p->rname, cap, need))
        FAIL_OOM();
    p->rcap = cap;
    return JAOS_OK;
}

static jaos_status o_bound(ox *p, const char *key, double dflt, double *out)
{
    const char *s = x_attr(p, key);
    if (s == nullptr) {
        *out = dflt;
        return JAOS_OK;
    }
    if (!x_num(s, out))
        FAIL("line %" PRId64 ": '%s' is not a number in %s=\"%s\"", p->line,
             s, key, s);
    return JAOS_OK;
}

static jaos_status o_var(ox *p)
{
    const char *mult = x_attr(p, "mult");
    if (mult != nullptr && strcmp(mult, "1") != 0)
        FAIL("line %" PRId64 ": a <var> repeated by mult=\"%s\" is not read; "
             "write the variables out one by one", p->line, mult);
    jaos_status st = o_var_room(p);
    if (st != JAOS_OK)
        return st;
    const int64_t j = p->nvar;
    const char *type = x_attr(p, "type");
    if (type == nullptr)
        type = "C";
    bool is_int = false, is_semi = false, is_bin = false;
    if (strcmp(type, "C") == 0)
        ;
    else if (strcmp(type, "B") == 0)
        is_int = is_bin = true;
    else if (strcmp(type, "I") == 0)
        is_int = true;
    else if (strcmp(type, "S") == 0)
        is_semi = true;
    else if (strcmp(type, "D") == 0)
        is_int = is_semi = true;
    else
        FAIL("line %" PRId64 ": variable type \"%s\" is not one of C, B, I, S "
             "or D", p->line, type);
    if ((st = o_bound(p, "lb", 0.0, &p->cl[j])) != JAOS_OK)
        return st;
    if ((st = o_bound(p, "ub", is_bin ? 1.0 : INFINITY, &p->cu[j])) != JAOS_OK)
        return st;
    p->cost[j] = 0.0;
    p->quad[j] = 0.0;
    p->cint[j] = is_int;
    p->csemi[j] = is_semi;
    p->cname[j] = nullptr;
    const char *nm = x_attr(p, "name");
    if (nm != nullptr && nm[0] != '\0') {
        p->cname[j] = jm_name_copy(nm);
        if (p->cname[j] == nullptr)
            FAIL_OOM();
    }
    p->nvar++;
    return JAOS_OK;
}

static jaos_status o_con(ox *p)
{
    const char *mult = x_attr(p, "mult");
    if (mult != nullptr && strcmp(mult, "1") != 0)
        FAIL("line %" PRId64 ": a <con> repeated by mult=\"%s\" is not read; "
             "write the constraints out one by one", p->line, mult);
    const char *cst = x_attr(p, "constant");
    if (cst != nullptr) {
        double v;
        if (!x_num(cst, &v))
            FAIL("line %" PRId64 ": constant=\"%s\" is not a number", p->line,
                 cst);
        if (v != 0.0)
            FAIL("line %" PRId64 ": a <con> with constant=\"%s\" is not read; "
                 "fold the constant into lb and ub", p->line, cst);
    }
    jaos_status st = o_con_room(p);
    if (st != JAOS_OK)
        return st;
    const int64_t i = p->ncon;
    if ((st = o_bound(p, "lb", -INFINITY, &p->rl[i])) != JAOS_OK)
        return st;
    if ((st = o_bound(p, "ub", INFINITY, &p->ru[i])) != JAOS_OK)
        return st;
    p->rname[i] = nullptr;
    const char *nm = x_attr(p, "name");
    if (nm != nullptr && nm[0] != '\0') {
        p->rname[i] = jm_name_copy(nm);
        if (p->rname[i] == nullptr)
            FAIL_OOM();
    }
    p->ncon++;
    return JAOS_OK;
}

static jaos_status o_obj(ox *p)
{
    if (p->nobj > 0)
        FAIL("line %" PRId64 ": the file has more than one <obj>, and JAOS "
             "carries one objective", p->line);
    p->nobj++;
    const char *dir = x_attr(p, "maxOrMin");
    if (dir != nullptr) {
        if (strcmp(dir, "max") == 0)
            p->sense = JAOS_MAXIMIZE;
        else if (strcmp(dir, "min") == 0)
            p->sense = JAOS_MINIMIZE;
        else
            FAIL("line %" PRId64 ": maxOrMin=\"%s\" is neither \"max\" nor "
                 "\"min\"", p->line, dir);
    }
    const char *cst = x_attr(p, "constant");
    if (cst != nullptr && !x_num(cst, &p->offset))
        FAIL("line %" PRId64 ": constant=\"%s\" is not a number", p->line,
             cst);
    const char *nm = x_attr(p, "name");
    if (nm != nullptr && nm[0] != '\0') {
        free(p->oname);
        p->oname = jm_name_copy(nm);
        if (p->oname == nullptr)
            FAIL_OOM();
    }
    return JAOS_OK;
}

static jaos_status o_coef(ox *p)
{
    const char *s = x_attr(p, "idx");
    int64_t j;
    if (s == nullptr || !x_int(s, &j))
        FAIL("line %" PRId64 ": <coef> has no readable idx attribute",
             p->line);
    if (j < 0 || j >= p->nvar)
        FAIL("line %" PRId64 ": <coef> names variable %" PRId64 ", and the "
             "file declared %" PRId64, p->line, j, p->nvar);
    double v;
    if (!x_text_num(p, &v))
        FAIL("line %" PRId64 ": <coef> holds no number", p->line);
    p->cost[j] = v;
    return JAOS_OK;
}

static jaos_status o_qterm(ox *p)
{
    const char *s = x_attr(p, "idx");
    int64_t owner = -1;
    if (s != nullptr && !x_int(s, &owner))
        FAIL("line %" PRId64 ": <qTerm> idx=\"%s\" is not a number", p->line,
             s);
    if (owner != -1)
        FAIL("line %" PRId64 ": <qTerm> sits on constraint %" PRId64 ", and "
             "JAOS reads a quadratic objective only", p->line, owner);
    const char *s1 = x_attr(p, "idxOne"), *s2 = x_attr(p, "idxTwo");
    int64_t j1, j2;
    if (s1 == nullptr || !x_int(s1, &j1) || s2 == nullptr || !x_int(s2, &j2))
        FAIL("line %" PRId64 ": <qTerm> has no readable idxOne and idxTwo",
             p->line);
    if (j1 < 0 || j1 >= p->nvar || j2 < 0 || j2 >= p->nvar)
        FAIL("line %" PRId64 ": <qTerm> names variables %" PRId64 " and %"
             PRId64 ", and the file declared %" PRId64, p->line, j1, j2,
             p->nvar);
    const char *sc = x_attr(p, "coef");
    double c;
    if (sc == nullptr || !x_num(sc, &c))
        FAIL("line %" PRId64 ": <qTerm> has no readable coef attribute",
             p->line);
    if (j1 == j2) {
        p->quad[j1] += 2.0 * c;
        return JAOS_OK;
    }

    int64_t lo = j1, hi = j2;
    if (lo < hi) {
        const int64_t t = lo;
        lo = hi;
        hi = t;
    }
    int64_t slot = -1;
    for (int64_t k = 0; k < p->nqoff; k++)
        if (p->qri[k] == lo && p->qci[k] == hi) {
            slot = k;
            break;
        }
    if (slot < 0) {
        int64_t cap = p->qoff_cap;
        if (!JM_GROW(p->qri, cap, p->nqoff + 1))
            FAIL_OOM();
        cap = p->qoff_cap;
        if (!JM_GROW(p->qci, cap, p->nqoff + 1))
            FAIL_OOM();
        cap = p->qoff_cap;
        if (!JM_GROW(p->qvv, cap, p->nqoff + 1))
            FAIL_OOM();
        p->qoff_cap = cap;
        slot = p->nqoff++;
        p->qri[slot] = lo;
        p->qci[slot] = hi;
        p->qvv[slot] = 0.0;
    }
    p->qvv[slot] += c;
    return JAOS_OK;
}

static jaos_status o_el(ox *p)
{
    int64_t mult = 1, incr = 0;
    const char *s = x_attr(p, "mult");
    if (s != nullptr && (!x_int(s, &mult) || mult < 0))
        FAIL("line %" PRId64 ": <el> mult=\"%s\" is not a count", p->line, s);
    s = x_attr(p, "incr");
    if (s != nullptr && !x_int(s, &incr))
        FAIL("line %" PRId64 ": <el> incr=\"%s\" is not a number", p->line, s);
    double v;
    if (!x_text_num(p, &v))
        FAIL("line %" PRId64 ": <el> holds no number", p->line);
    for (int64_t k = 0; k < mult; k++) {
        if (p->vec == OV_VALUE) {
            if (!JM_GROW(p->value, p->vcap, p->nvalue + 1))
                FAIL_OOM();
            p->value[p->nvalue++] = v + (double)(k * incr);
        } else {
            const double d = v + (double)(k * incr);
            if (d != floor(d) || d < 0.0 || d > (double)INT64_MAX)
                FAIL("line %" PRId64 ": <el> holds %g where an index was "
                     "wanted", p->line, d);
            const int64_t iv = (int64_t)d;
            if (p->vec == OV_START) {
                if (!JM_GROW(p->start, p->scap, p->nstart + 1))
                    FAIL_OOM();
                p->start[p->nstart++] = iv;
            } else {
                if (!JM_GROW(p->idx, p->icap, p->nidx + 1))
                    FAIL_OOM();
                p->idx[p->nidx++] = iv;
            }
        }
    }
    return JAOS_OK;
}

static jaos_status o_count(ox *p, const char *key, int64_t *out, bool *have)
{
    const char *s = x_attr(p, key);
    if (s == nullptr)
        return JAOS_OK;
    if (!x_int(s, out) || *out < 0)
        FAIL("line %" PRId64 ": %s=\"%s\" is not a count", p->line, key, s);
    *have = true;
    return JAOS_OK;
}

static jaos_status o_parse(ox *p)
{
    jaos_status st;
    bool seen_root = false;
    while (x_next(p)) {
        const char *n = p->name;
        if (p->kind == OX_END) {
            if (strcmp(n, "instanceHeader") == 0)
                p->in_header = false;
            else if (strcmp(n, "obj") == 0)
                p->in_obj = false;
            else if (strcmp(n, "linearConstraintCoefficients") == 0)
                p->in_lcc = false;
            else if (strcmp(n, "start") == 0 || strcmp(n, "rowIdx") == 0 ||
                     strcmp(n, "colIdx") == 0 || strcmp(n, "value") == 0)
                p->vec = OV_NONE;
            continue;
        }
        if (strcmp(n, "osil") == 0) {
            seen_root = true;
        } else if (strcmp(n, "instanceHeader") == 0) {
            p->in_header = p->kind == OX_START;
        } else if (strcmp(n, "name") == 0 && p->in_header) {
            if (p->kind == OX_START) {
                free(p->pname);
                p->pname = x_text_dup(p);
                if (p->pname == nullptr)
                    FAIL_OOM();
            }
        } else if (strcmp(n, "variables") == 0) {
            if ((st = o_count(p, "numberOfVariables", &p->want_var,
                              &p->have_var_count)) != JAOS_OK)
                return st;
        } else if (strcmp(n, "var") == 0) {
            if ((st = o_var(p)) != JAOS_OK)
                return st;
        } else if (strcmp(n, "constraints") == 0) {
            if ((st = o_count(p, "numberOfConstraints", &p->want_con,
                              &p->have_con_count)) != JAOS_OK)
                return st;
        } else if (strcmp(n, "con") == 0) {
            if ((st = o_con(p)) != JAOS_OK)
                return st;
        } else if (strcmp(n, "obj") == 0) {
            if ((st = o_obj(p)) != JAOS_OK)
                return st;
            p->in_obj = p->kind == OX_START;
        } else if (strcmp(n, "coef") == 0 && p->in_obj) {
            if ((st = o_coef(p)) != JAOS_OK)
                return st;
        } else if (strcmp(n, "qTerm") == 0) {
            if ((st = o_qterm(p)) != JAOS_OK)
                return st;
        } else if (strcmp(n, "linearConstraintCoefficients") == 0) {
            if (p->seen_lcc)
                FAIL("line %" PRId64 ": a second "
                     "<linearConstraintCoefficients> block", p->line);
            p->seen_lcc = true;
            p->in_lcc = p->kind == OX_START;
            if ((st = o_count(p, "numberOfValues", &p->want_nz,
                              &p->have_nz_count)) != JAOS_OK)
                return st;
        } else if (p->in_lcc && strcmp(n, "start") == 0) {
            p->vec = p->kind == OX_START ? OV_START : OV_NONE;
        } else if (p->in_lcc && strcmp(n, "rowIdx") == 0) {
            if (p->oriented && p->by_row)
                FAIL("line %" PRId64 ": the matrix carries both a <colIdx> "
                     "and a <rowIdx> block", p->line);
            p->oriented = true;
            p->by_row = false;
            p->vec = p->kind == OX_START ? OV_IDX : OV_NONE;
        } else if (p->in_lcc && strcmp(n, "colIdx") == 0) {
            if (p->oriented && !p->by_row)
                FAIL("line %" PRId64 ": the matrix carries both a <colIdx> "
                     "and a <rowIdx> block", p->line);
            p->oriented = true;
            p->by_row = true;
            p->vec = p->kind == OX_START ? OV_IDX : OV_NONE;
        } else if (p->in_lcc && strcmp(n, "value") == 0) {
            p->vec = p->kind == OX_START ? OV_VALUE : OV_NONE;
        } else if (strcmp(n, "el") == 0 && p->vec != OV_NONE) {
            if ((st = o_el(p)) != JAOS_OK)
                return st;
        } else if (strcmp(n, "nonlinearExpressions") == 0) {
            FAIL("line %" PRId64 ": the model has a <nonlinearExpressions> "
                 "block, and JAOS reads linear and separable quadratic "
                 "models only", p->line);
        } else if (strcmp(n, "quadraticConstraints") == 0) {
            FAIL("line %" PRId64 ": the model has a <quadraticConstraints> "
                 "block, which JAOS does not carry", p->line);
        } else if (strcmp(n, "sos") == 0 || strcmp(n, "SOS") == 0) {
            FAIL("line %" PRId64 ": the model has an SOS block, which JAOS "
                 "does not read from OSiL; read MPS instead", p->line);
        }
    }
    if (p->trunc)
        FAIL("line %" PRId64 ": the file ends inside a tag", p->line);
    if (!seen_root)
        FAIL("the file has no <osil> element, so it is not an OSiL model");
    if (p->have_var_count && p->want_var != p->nvar)
        FAIL("the file declares %" PRId64 " variables and carries %" PRId64,
             p->want_var, p->nvar);
    if (p->have_con_count && p->want_con != p->ncon)
        FAIL("the file declares %" PRId64 " constraints and carries %" PRId64,
             p->want_con, p->ncon);
    return JAOS_OK;
}

static jaos_status o_matrix(ox *p, int64_t **out_start, int64_t **out_index,
                            double **out_value, int64_t *out_nz)
{
    const int64_t nc = p->nvar, nr = p->ncon;
    *out_start = nullptr;
    *out_index = nullptr;
    *out_value = nullptr;
    *out_nz = 0;
    if (!p->seen_lcc || (p->nstart == 0 && p->nidx == 0 && p->nvalue == 0))
        return JAOS_OK;
    if (p->nidx != p->nvalue)
        FAIL("the matrix carries %" PRId64 " indices and %" PRId64 " values",
             p->nidx, p->nvalue);
    if (p->have_nz_count && p->want_nz != p->nidx)
        FAIL("the matrix declares %" PRId64 " values and carries %" PRId64,
             p->want_nz, p->nidx);
    if (!p->oriented)
        FAIL("the matrix has neither a <rowIdx> nor a <colIdx> block");
    const int64_t outer = p->by_row ? nr : nc;
    if (p->nstart != outer + 1)
        FAIL("the matrix <start> block holds %" PRId64 " entries, and %"
             PRId64 " were wanted", p->nstart, outer + 1);
    if (p->start[0] != 0 || p->start[outer] != p->nidx)
        FAIL("the matrix <start> block runs from %" PRId64 " to %" PRId64
             ", and 0 to %" PRId64 " were wanted", p->start[0],
             p->start[outer], p->nidx);
    for (int64_t k = 0; k < outer; k++)
        if (p->start[k] > p->start[k + 1])
            FAIL("the matrix <start> block falls from %" PRId64 " to %" PRId64
                 " at entry %" PRId64, p->start[k], p->start[k + 1], k);
    const int64_t inner = p->by_row ? nc : nr;
    for (int64_t k = 0; k < p->nidx; k++)
        if (p->idx[k] >= inner)
            FAIL("the matrix names %s %" PRId64 ", and the file declared %"
                 PRId64, p->by_row ? "variable" : "constraint", p->idx[k],
                 inner);
    if (!p->by_row) {
        *out_start = p->start;
        *out_index = p->idx;
        *out_value = p->value;
        *out_nz = p->nidx;
        p->start = nullptr;
        p->idx = nullptr;
        p->value = nullptr;
        return JAOS_OK;
    }
    int64_t *as = jm_calloc_array(nc + 1, sizeof *as);
    int64_t *fill = jm_calloc_array(nc > 0 ? nc : 1, sizeof *fill);
    int64_t *ai = jm_alloc_array(p->nidx > 0 ? p->nidx : 1, sizeof *ai);
    double *av = jm_alloc_array(p->nidx > 0 ? p->nidx : 1, sizeof *av);
    if (as == nullptr || fill == nullptr || ai == nullptr || av == nullptr) {
        free(as);
        free(fill);
        free(ai);
        free(av);
        FAIL_OOM();
    }
    for (int64_t k = 0; k < p->nidx; k++)
        as[p->idx[k] + 1]++;
    for (int64_t j = 0; j < nc; j++)
        as[j + 1] += as[j];
    for (int64_t i = 0; i < nr; i++)
        for (int64_t k = p->start[i]; k < p->start[i + 1]; k++) {
            const int64_t j = p->idx[k];
            const int64_t pos = as[j] + fill[j]++;
            ai[pos] = i;
            av[pos] = p->value[k];
        }
    free(fill);
    *out_start = as;
    *out_index = ai;
    *out_value = av;
    *out_nz = p->nidx;
    return JAOS_OK;
}

static jaos_status o_names(ox *p)
{
    jaos_model *m = p->m;
    const int64_t nc = p->nvar, nr = p->ncon;
    bool any = p->oname != nullptr;
    for (int64_t j = 0; !any && j < nc; j++)
        any = p->cname[j] != nullptr;
    for (int64_t i = 0; !any && i < nr; i++)
        any = p->rname[i] != nullptr;
    if (!any)
        return JAOS_OK;
    char **cn = jm_calloc_array(nc > 0 ? nc : 1, sizeof *cn);
    char **rn = jm_calloc_array(nr > 0 ? nr : 1, sizeof *rn);
    if (cn == nullptr || rn == nullptr) {
        free(cn);
        free(rn);
        FAIL_OOM();
    }
    char tmp[JM_NAME_BUF];
    for (int64_t j = 0; j < nc; j++) {
        if (p->cname[j] != nullptr) {
            cn[j] = p->cname[j];
            p->cname[j] = nullptr;
            continue;
        }
        snprintf(tmp, sizeof tmp, "C%" PRId64, j + 1);
        cn[j] = jm_name_copy(tmp);
        if (cn[j] == nullptr)
            goto oom;
    }
    for (int64_t i = 0; i < nr; i++) {
        if (p->rname[i] != nullptr) {
            rn[i] = p->rname[i];
            p->rname[i] = nullptr;
            continue;
        }
        snprintf(tmp, sizeof tmp, "R%" PRId64, i + 1);
        rn[i] = jm_name_copy(tmp);
        if (rn[i] == nullptr)
            goto oom;
    }
    jm_model_take_names(m, cn, rn, p->oname);
    p->oname = nullptr;
    return JAOS_OK;
oom:
    for (int64_t j = 0; j < nc; j++)
        free(cn[j]);
    for (int64_t i = 0; i < nr; i++)
        free(rn[i]);
    free(cn);
    free(rn);
    FAIL_OOM();
}

static jaos_status o_build(ox *p)
{
    jaos_model *m = p->m;
    const int64_t nc = p->nvar, nr = p->ncon;
    int64_t *as = nullptr, *ai = nullptr, nz = 0;
    double *av = nullptr;
    jaos_status st = o_matrix(p, &as, &ai, &av, &nz);
    if (st != JAOS_OK)
        return st;
    st = jaos_load_lp(m, nc, nr, p->sense, p->offset, p->cost, p->cl, p->cu,
                      p->rl, p->ru, nz, nc > 0 ? as : nullptr, ai, av);
    free(as);
    free(ai);
    free(av);
    if (st != JAOS_OK) {
        if (m->err[0] == '\0')
            jm_set_err(m, "the OSiL model failed validation");
        return st;
    }
    bool any_int = false, any_semi = false, any_quad = false;
    for (int64_t j = 0; j < nc; j++) {
        any_int |= p->cint[j];
        any_semi |= p->csemi[j];
        any_quad |= p->quad[j] != 0.0;
    }
    if (any_int) {
        free(m->col_integer);
        m->col_integer = p->cint;
        p->cint = nullptr;
    }
    if (any_semi) {
        free(m->col_semi);
        m->col_semi = p->csemi;
        p->csemi = nullptr;
    }
    if (any_quad && p->nqoff == 0) {
        free(m->col_quad);
        m->col_quad = p->quad;
        p->quad = nullptr;
    } else if (any_quad || p->nqoff > 0) {

        int64_t n = p->nqoff;
        for (int64_t j = 0; j < nc; j++)
            n += p->quad[j] != 0.0;
        int64_t *qr = jm_alloc_array(n > 0 ? n : 1, sizeof *qr);
        int64_t *qc = jm_alloc_array(n > 0 ? n : 1, sizeof *qc);
        double *qv = jm_alloc_array(n > 0 ? n : 1, sizeof *qv);
        if (qr == nullptr || qc == nullptr || qv == nullptr) {
            free(qr); free(qc); free(qv);
            FAIL_OOM();
        }
        int64_t at = 0;
        for (int64_t j = 0; j < nc; j++)
            if (p->quad[j] != 0.0) {
                qr[at] = j; qc[at] = j; qv[at] = p->quad[j]; at++;
            }
        for (int64_t k = 0; k < p->nqoff; k++) {
            qr[at] = p->qri[k]; qc[at] = p->qci[k]; qv[at] = p->qvv[k];
            at++;
        }
        const jaos_status qst = jaos_set_quadratic(m, at, qr, qc, qv);
        free(qr); free(qc); free(qv);
        if (qst != JAOS_OK)
            return qst;
    }
    if ((st = o_names(p)) != JAOS_OK)
        return st;
    if (p->pname != nullptr && p->pname[0] != '\0') {
        free(m->model_name);
        m->model_name = p->pname;
        p->pname = nullptr;
    }
    return JAOS_OK;
}

static void o_free(ox *p)
{
    free(p->buf);
    free(p->cost);
    free(p->cl);
    free(p->cu);
    free(p->rl);
    free(p->ru);
    free(p->quad);
    free(p->qri);
    free(p->qci);
    free(p->qvv);
    free(p->cint);
    free(p->csemi);
    if (p->cname != nullptr)
        for (int64_t j = 0; j < p->nvar; j++)
            free(p->cname[j]);
    free(p->cname);
    if (p->rname != nullptr)
        for (int64_t i = 0; i < p->ncon; i++)
            free(p->rname[i]);
    free(p->rname);
    free(p->pname);
    free(p->oname);
    free(p->start);
    free(p->idx);
    free(p->value);
}

jaos_status jaos_read_osil(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    ox pp = {0};
    ox *p = &pp;
    p->m = m;
    p->line = 1;
    p->sense = JAOS_MINIMIZE;
    jaos_status st = jm_slurp(m, path, &p->buf, &p->len);
    if (st != JAOS_OK)
        return st;
    {
        jm_locale loc;
        jm_locale_c_enter(&loc);
        st = o_parse(p);
        if (st == JAOS_OK)
            st = o_build(p);
        jm_locale_leave(&loc);
    }
    o_free(p);
    return st;
}
