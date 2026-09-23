/* SPDX-License-Identifier: Apache-2.0 */

#include "jaos_internal.h"
#include "jaos_sys.h"

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr int NAME_MAX_LEN = 255;

typedef enum {
    T_EOF, T_NAME, T_NUM, T_PLUS, T_MINUS, T_LE, T_GE, T_EQ, T_COLON,
    T_LBRACK, T_RBRACK, T_CARET, T_STAR, T_SLASH, T_ARROW,
} toktype;

typedef struct {
    toktype t;
    char text[NAME_MAX_LEN + 1];
    double num;
    int64_t line;
} token;

typedef struct {
    jaos_model *m;

    char *buf;
    int64_t len, pos, line;

    token tok;
    token pushed;
    bool has_pushed;
    double pre_sign;

    jm_nmap cmap;
    double *cost, *cl, *cu, *cquad;
    int64_t ncol, cost_cap, cl_cap, cu_cap, cquad_cap;
    bool any_quad;
    int64_t *qri, *qci;
    double  *qvv;
    int64_t nqoff, qoff_cap;

    int64_t *rs;
    double *rlb, *rub;
    int64_t nrow, rs_cap, rlb_cap, rub_cap;

    char **rname;
    int64_t rname_cap;
    char *oname;

    bool *cint;
    int64_t cint_cap, ncint;
    bool *csemi;
    int64_t csemi_cap, ncsemi;
    int *st_type;
    int64_t *st_start;
    int64_t *st_col;
    double *st_w;
    int64_t nsos, sos_cap, sos_start_cap, nsosm, sosm_cap, sosw_cap;
    int64_t *ind_col;
    int *ind_val;
    int64_t ind_cap, indv_cap, nind;
    int64_t *ei;
    double *ev;
    int64_t nent, ei_cap, ev_cap;

    int64_t *stamp, *slot;
    int64_t stamp_cap, slot_cap;

    char **rg_lo, **rg_hi;
    int64_t nrg, rg_lo_cap, rg_hi_cap;

    int64_t *rq_r, *rq_i, *rq_j;
    double *rq_v;
    int64_t nrq, rq_rc, rq_ic, rq_jc, rq_vc;

    jaos_obj_sense sense;
    double offset;
} lp;

#define FAIL(...)   do { st = JAOS_ERR_INVALID_INPUT; \
    jm_set_err(p->m, __VA_ARGS__); goto done; } while (0)
#define FAIL_OOM()  do { st = JAOS_ERR_OUT_OF_MEMORY; \
    jm_set_err(p->m, "out of memory while reading LP"); goto done; } while (0)

static bool name_symbol(char c)
{
    return strchr("!\"#$%&()/,;?@`'{}|~", c) != nullptr && c != '\0';
}

static bool name_start(char c)
{
    return isalpha((unsigned char)c) || c == '_' || name_symbol(c);
}

static bool name_cont(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '.' ||
           name_symbol(c);
}

static jaos_status lx_next(lp *p)
{
    jaos_status st = JAOS_OK;

    if (p->has_pushed) {
        p->tok = p->pushed;
        p->has_pushed = false;
        return JAOS_OK;
    }

    for (;;) {
        char c = p->pos < p->len ? p->buf[p->pos] : '\0';
        if (c == '\n') {
            p->line++;
            p->pos++;
        } else if (c == ' ' || c == '\t' || c == '\r') {
            p->pos++;
        } else if (c == '\\') {
            while (p->pos < p->len && p->buf[p->pos] != '\n')
                p->pos++;
        } else {
            break;
        }
    }

    p->tok.line = p->line;
    if (p->pos >= p->len) {
        p->tok.t = T_EOF;
        return JAOS_OK;
    }

    char c = p->buf[p->pos];
    switch (c) {
    case '+': p->tok.t = T_PLUS;  p->pos++; return JAOS_OK;
    case '-':
        p->pos++;
        if (p->pos < p->len && p->buf[p->pos] == '>') {
            p->pos++;
            p->tok.t = T_ARROW;
        } else {
            p->tok.t = T_MINUS;
        }
        return JAOS_OK;
    case ':': p->tok.t = T_COLON; p->pos++; return JAOS_OK;
    case '[': p->tok.t = T_LBRACK; p->pos++; return JAOS_OK;
    case ']': p->tok.t = T_RBRACK; p->pos++; return JAOS_OK;
    case '^': p->tok.t = T_CARET; p->pos++; return JAOS_OK;
    case '*': p->tok.t = T_STAR;  p->pos++; return JAOS_OK;
    case '/': p->tok.t = T_SLASH; p->pos++; return JAOS_OK;
    case '<':
        p->pos++;
        if (p->pos < p->len && p->buf[p->pos] == '=')
            p->pos++;
        p->tok.t = T_LE;
        return JAOS_OK;
    case '>':
        p->pos++;
        if (p->pos < p->len && p->buf[p->pos] == '=')
            p->pos++;
        p->tok.t = T_GE;
        return JAOS_OK;
    case '=':
        p->pos++;
        if (p->pos < p->len && p->buf[p->pos] == '<') {
            p->pos++;
            p->tok.t = T_LE;
        } else if (p->pos < p->len && p->buf[p->pos] == '>') {
            p->pos++;
            p->tok.t = T_GE;
        } else {
            p->tok.t = T_EQ;
        }
        return JAOS_OK;
    default:
        break;
    }

    if (isdigit((unsigned char)c) || c == '.') {

        int64_t s = p->pos;
        while (p->pos < p->len && isdigit((unsigned char)p->buf[p->pos]))
            p->pos++;
        if (p->pos < p->len && p->buf[p->pos] == '.') {
            p->pos++;
            while (p->pos < p->len && isdigit((unsigned char)p->buf[p->pos]))
                p->pos++;
        }
        if (p->pos < p->len &&
            (p->buf[p->pos] == 'e' || p->buf[p->pos] == 'E')) {
            int64_t q = p->pos + 1;
            if (q < p->len && (p->buf[q] == '+' || p->buf[q] == '-'))
                q++;
            if (q < p->len && isdigit((unsigned char)p->buf[q])) {
                p->pos = q;
                while (p->pos < p->len &&
                       isdigit((unsigned char)p->buf[p->pos]))
                    p->pos++;
            }
        }
        int64_t n = p->pos - s;
        if (n > NAME_MAX_LEN)
            FAIL("line %" PRId64 ": number too long", p->line);
        memcpy(p->tok.text, p->buf + s, (size_t)n);
        p->tok.text[n] = '\0';
        char *end;
        p->tok.num = strtod(p->tok.text, &end);
        if (end == p->tok.text || *end != '\0' || !isfinite(p->tok.num))
            FAIL("line %" PRId64 ": bad number '%s'", p->line, p->tok.text);
        p->tok.t = T_NUM;
        return JAOS_OK;
    }

    if (name_start(c)) {
        int64_t s = p->pos;
        while (p->pos < p->len && name_cont(p->buf[p->pos]))
            p->pos++;
        int64_t n = p->pos - s;
        if (n > NAME_MAX_LEN)
            FAIL("line %" PRId64 ": name too long", p->line);
        memcpy(p->tok.text, p->buf + s, (size_t)n);
        p->tok.text[n] = '\0';
        p->tok.t = T_NAME;
        return JAOS_OK;
    }

    FAIL("line %" PRId64 ": unexpected character '%c'", p->line, c);
done:
    return st;
}

static void lx_push(lp *p, const token *back)
{
    p->pushed = p->tok;
    p->has_pushed = true;
    p->tok = *back;
}

static bool tok_is(const lp *p, const char *kw)
{
    return p->tok.t == T_NAME && jm_strcasecmp(p->tok.text, kw) == 0;
}

static bool is_reserved(const char *s)
{
    static const char *kws[] = {
        "minimize", "minimum", "min", "maximize", "maximum", "max",
        "subject", "such", "st", "s.t.", "bounds", "bound",
        "general", "generals", "gen", "integer", "integers",
        "binary", "binaries", "bin", "semi", "semis", "sos",
        "lazy", "user",
        "end", "free", "infinity", "inf",
    };
    for (size_t i = 0; i < sizeof kws / sizeof *kws; i++)
        if (jm_strcasecmp(s, kws[i]) == 0)
            return true;
    return false;
}

static bool at_reserved(const lp *p)
{
    return p->tok.t == T_NAME && is_reserved(p->tok.text);
}

bool jm_lp_name_ok(const char *s)
{
    if (s == nullptr || !name_start(s[0]) || s[0] == '/')
        return false;
    size_t n = 0;
    for (const char *p = s; *p; p++, n++)
        if (!name_cont(*p))
            return false;
    return n <= NAME_MAX_LEN && !is_reserved(s);
}

static bool get_or_create_col(lp *p, const char *name, int64_t *out)
{
    if (jm_nmap_get(&p->cmap, name, out))
        return true;
    int64_t j = p->ncol;
    if (!JM_GROW(p->cost, p->cost_cap, j + 1) ||
        !JM_GROW(p->cl, p->cl_cap, j + 1) ||
        !JM_GROW(p->cu, p->cu_cap, j + 1) ||
        !JM_GROW(p->cquad, p->cquad_cap, j + 1) ||
        !JM_GROW(p->stamp, p->stamp_cap, j + 1) ||
        !JM_GROW(p->slot, p->slot_cap, j + 1))
        return false;
    if (!jm_nmap_insert(&p->cmap, name, j))
        return false;
    p->cost[j] = 0.0;
    p->cquad[j] = 0.0;
    p->cl[j] = 0.0;
    p->cu[j] = INFINITY;
    p->stamp[j] = 0;
    p->slot[j] = 0;
    p->ncol++;
    *out = j;
    return true;
}

static jaos_status parse_quadratic(lp *p, double sign, int64_t row)
{
    jaos_status st = JAOS_OK;
    const int64_t open_line = p->tok.line;
    int64_t *qcol = nullptr, *qrow = nullptr;
    double *qval = nullptr;
    int64_t nq = 0, qcol_cap = 0, qval_cap = 0, qrow_cap = 0;
    if ((st = lx_next(p)) != JAOS_OK)
        goto done;
    while (p->tok.t != T_RBRACK) {
        double coef = 1.0;
        if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
            coef = p->tok.t == T_MINUS ? -1.0 : 1.0;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        } else if (nq > 0) {
            FAIL("line %" PRId64 ": expected + or - between quadratic terms",
                 p->tok.line);
        }
        if (p->tok.t == T_NUM) {
            coef *= p->tok.num;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }
        if (p->tok.t != T_NAME || is_reserved(p->tok.text))
            FAIL("line %" PRId64 ": expected a variable in the quadratic "
                 "term", p->tok.line);
        int64_t j;
        if (!get_or_create_col(p, p->tok.text, &j))
            FAIL_OOM();
        int64_t other = j;
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        if (p->tok.t == T_CARET) {
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            if (p->tok.t != T_NUM || p->tok.num != 2.0)
                FAIL("line %" PRId64 ": only a square, '^ 2', is read in a "
                     "quadratic term", p->tok.line);
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        } else if (p->tok.t == T_STAR) {
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            if (p->tok.t != T_NAME || is_reserved(p->tok.text))
                FAIL("line %" PRId64 ": expected a variable after '*'",
                     p->tok.line);
            if (!get_or_create_col(p, p->tok.text, &other))
                FAIL_OOM();
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        } else {
            FAIL("line %" PRId64 ": a term inside [ ] needs '^ 2' or '* "
                 "variable'", p->tok.line);
        }
        if (!JM_GROW(qcol, qcol_cap, nq + 1) ||
            !JM_GROW(qrow, qrow_cap, nq + 1) ||
            !JM_GROW(qval, qval_cap, nq + 1))
            FAIL_OOM();
        qcol[nq] = j;
        qrow[nq] = other;
        qval[nq] = coef;
        nq++;
        if (p->tok.t == T_EOF)
            FAIL("line %" PRId64 ": the [ opened here is never closed",
                 open_line);
    }
    if ((st = lx_next(p)) != JAOS_OK)
        goto done;
    double factor = 2.0;
    if (p->tok.t == T_SLASH) {
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        if (p->tok.t != T_NUM || p->tok.num != 2.0)
            FAIL("line %" PRId64 ": a quadratic block is divided by 2 or "
                 "not divided at all", p->tok.line);
        factor = 1.0;
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
    }

    for (int64_t k = 0; k < nq; k++) {
        const double v = factor * sign * qval[k];
        if (row >= 0) {
            const int64_t i = qrow[k] > qcol[k] ? qrow[k] : qcol[k];
            const int64_t j = qrow[k] > qcol[k] ? qcol[k] : qrow[k];
            if (!JM_GROW(p->rq_r, p->rq_rc, p->nrq + 1) ||
                !JM_GROW(p->rq_i, p->rq_ic, p->nrq + 1) ||
                !JM_GROW(p->rq_j, p->rq_jc, p->nrq + 1) ||
                !JM_GROW(p->rq_v, p->rq_vc, p->nrq + 1))
                FAIL_OOM();
            p->rq_r[p->nrq] = row;
            p->rq_i[p->nrq] = i;
            p->rq_j[p->nrq] = j;
            p->rq_v[p->nrq] = i == j ? v : 0.5 * v;
            p->nrq++;
            continue;
        }
        if (qrow[k] == qcol[k]) {
            p->cquad[qcol[k]] += v;
            if (!isfinite(p->cquad[qcol[k]]))
                FAIL("line %" PRId64 ": the squared terms on one variable add "
                     "up past what a double holds", p->line);
            p->any_quad = true;
            continue;
        }
        int64_t i = qrow[k], j = qcol[k];
        if (i < j) {
            const int64_t t = i;
            i = j;
            j = t;
        }
        int64_t at = -1;
        for (int64_t q = 0; q < p->nqoff; q++)
            if (p->qri[q] == i && p->qci[q] == j) {
                at = q;
                break;
            }
        if (at < 0) {
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
            at = p->nqoff++;
            p->qri[at] = i;
            p->qci[at] = j;
            p->qvv[at] = 0.0;
        }
        p->qvv[at] += 0.5 * v;
        p->any_quad = true;
    }
done:
    free(qcol);
    free(qrow);
    free(qval);
    return st;
}

static jaos_status parse_expr(lp *p, int64_t row, double *konst)
{
    jaos_status st = JAOS_OK;
    bool any = false;

    if (konst != nullptr)
        *konst = 0.0;

    for (;;) {
        double sign = 1.0;
        bool have_sign = false;

        if (p->pre_sign != 0.0) {
            sign = p->pre_sign;
            have_sign = true;
            p->pre_sign = 0.0;
        } else if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
            sign = p->tok.t == T_MINUS ? -1.0 : 1.0;
            have_sign = true;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        } else if (any) {
            break;
        }

        if (p->tok.t == T_LBRACK) {
            if ((st = parse_quadratic(p, sign, row)) != JAOS_OK)
                goto done;
            any = true;
            continue;
        }

        double coef = sign;
        bool have_num = false;
        if (p->tok.t == T_NUM) {
            coef = sign * p->tok.num;
            have_num = true;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }
        if (p->tok.t == T_STAR || p->tok.t == T_CARET ||
            p->tok.t == T_SLASH || p->tok.t == T_RBRACK)
            FAIL("line %" PRId64 ": unexpected character '%c'; a product or "
                 "a square belongs inside a [ ] block",
                 p->tok.line,
                 p->tok.t == T_STAR ? '*' : p->tok.t == T_CARET ? '^'
                                          : p->tok.t == T_SLASH ? '/' : ']');

        if (p->tok.t == T_NAME && !is_reserved(p->tok.text)) {
            int64_t j;
            if (!get_or_create_col(p, p->tok.text, &j))
                FAIL_OOM();
            if (row < 0) {
                p->cost[j] += coef;
            } else if (p->stamp[j] == row + 1) {
                p->ev[p->slot[j]] += coef;
            } else {
                if (!JM_GROW(p->ei, p->ei_cap, p->nent + 1) ||
                    !JM_GROW(p->ev, p->ev_cap, p->nent + 1))
                    FAIL_OOM();
                p->stamp[j] = row + 1;
                p->slot[j] = p->nent;
                p->ei[p->nent] = j;
                p->ev[p->nent] = coef;
                p->nent++;
            }
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        } else if (have_num) {

            if (row >= 0)
                *konst += coef;
            else
                p->offset += coef;
        } else if (have_sign) {
            FAIL("line %" PRId64 ": expected a term after the sign",
                 p->tok.line);
        } else {
            break;
        }
        any = true;
    }
done:
    return st;
}

static jaos_status parse_bound_value(lp *p, double *out)
{
    jaos_status st = JAOS_OK;
    double sign = 1.0;

    if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
        sign = p->tok.t == T_MINUS ? -1.0 : 1.0;
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
    }
    if (p->tok.t == T_NUM)
        *out = sign * p->tok.num;
    else if (tok_is(p, "inf") || tok_is(p, "infinity"))
        *out = sign * INFINITY;
    else
        FAIL("line %" PRId64 ": expected a bound value", p->tok.line);
    st = lx_next(p);
done:
    return st;
}

static bool scan_ranges(lp *p)
{
    static const char mark[] = "\\ range ";
    const int64_t mlen = (int64_t)sizeof mark - 1;
    for (int64_t at = 0; at < p->len;) {
        int64_t end = at;
        while (end < p->len && p->buf[end] != '\n')
            end++;
        if (end - at > mlen && memcmp(p->buf + at, mark, (size_t)mlen) == 0) {
            char a[NAME_MAX_LEN + 1], b[NAME_MAX_LEN + 1];
            int64_t q = at + mlen, n = 0;
            while (q < end && p->buf[q] == ' ')
                q++;
            while (q < end && p->buf[q] != ' ' && p->buf[q] != '\r' &&
                   n < NAME_MAX_LEN)
                a[n++] = p->buf[q++];
            a[n] = '\0';
            n = 0;
            while (q < end && p->buf[q] == ' ')
                q++;
            while (q < end && p->buf[q] != ' ' && p->buf[q] != '\r' &&
                   n < NAME_MAX_LEN)
                b[n++] = p->buf[q++];
            b[n] = '\0';
            if (a[0] != '\0' && b[0] != '\0') {
                if (!JM_GROW(p->rg_lo, p->rg_lo_cap, p->nrg + 1) ||
                    !JM_GROW(p->rg_hi, p->rg_hi_cap, p->nrg + 1))
                    return false;
                p->rg_lo[p->nrg] = jm_name_copy(a);
                p->rg_hi[p->nrg] = jm_name_copy(b);
                if (p->rg_lo[p->nrg] == nullptr || p->rg_hi[p->nrg] == nullptr) {
                    free(p->rg_lo[p->nrg]);
                    free(p->rg_hi[p->nrg]);
                    return false;
                }
                p->nrg++;
            }
        }
        at = end + 1;
    }
    return true;
}

static bool same_row(const lp *p, int64_t a, int64_t b)
{
    const int64_t na = p->rs[a + 1] - p->rs[a], nb = p->rs[b + 1] - p->rs[b];
    if (na != nb || p->ind_col[a] != p->ind_col[b] ||
        p->ind_val[a] != p->ind_val[b])
        return false;
    for (int64_t k = 0; k < na; k++)
        if (p->ei[p->rs[a] + k] != p->ei[p->rs[b] + k] ||
            p->ev[p->rs[a] + k] != p->ev[p->rs[b] + k])
            return false;
    return true;
}

static bool fold_ranges(lp *p)
{
    if (p->nrg == 0 || p->nrow == 0)
        return true;
    jm_nmap rows = {0};
    bool *gone = jm_calloc_array(p->nrow, sizeof *gone);
    bool *curved = jm_calloc_array(p->nrow, sizeof *curved);
    int64_t *renum = jm_alloc_array(p->nrow, sizeof *renum);
    bool ok = gone != nullptr && curved != nullptr && renum != nullptr;
    for (int64_t t = 0; ok && t < p->nrq; t++)
        curved[p->rq_r[t]] = true;
    for (int64_t i = 0; ok && i < p->nrow; i++)
        if (p->rname[i] != nullptr)
            ok = jm_nmap_insert(&rows, p->rname[i], i);
    int64_t folded = 0;
    for (int64_t k = 0; ok && k < p->nrg; k++) {
        int64_t a, b;
        if (!jm_nmap_get(&rows, p->rg_lo[k], &a) ||
            !jm_nmap_get(&rows, p->rg_hi[k], &b) || a == b || gone[a] ||
            gone[b] || curved[a] || curved[b])
            continue;
        if (!(isfinite(p->rlb[a]) && p->rub[a] == INFINITY &&
              p->rlb[b] == -INFINITY && isfinite(p->rub[b]) &&
              p->rlb[a] < p->rub[b] && same_row(p, a, b)))
            continue;
        p->rub[a] = p->rub[b];
        gone[b] = true;
        folded++;
    }
    jm_nmap_free(&rows);
    if (ok && folded > 0) {
        int64_t r = 0, e = 0;
        for (int64_t i = 0; i < p->nrow; i++) {
            const int64_t b0 = p->rs[i], n = p->rs[i + 1] - b0;
            renum[i] = gone[i] ? -1 : r;
            if (gone[i]) {
                free(p->rname[i]);
                continue;
            }
            memmove(p->ei + e, p->ei + b0, (size_t)n * sizeof *p->ei);
            memmove(p->ev + e, p->ev + b0, (size_t)n * sizeof *p->ev);
            p->rs[r] = e;
            p->rlb[r] = p->rlb[i];
            p->rub[r] = p->rub[i];
            p->rname[r] = p->rname[i];
            p->ind_col[r] = p->ind_col[i];
            p->ind_val[r] = p->ind_val[i];
            e += n;
            r++;
        }
        p->rs[r] = e;
        p->nrow = r;
        p->nind = r;
        p->nent = e;
        for (int64_t t = 0; t < p->nrq; t++)
            p->rq_r[t] = renum[p->rq_r[t]];
    }
    free(gone);
    free(curved);
    free(renum);
    return ok;
}

typedef struct {
    int64_t r, i, j, at;
    double v;
} lq_entry;

static int cmp_lq(const void *pa, const void *pb)
{
    const lq_entry *a = pa, *b = pb;
    if (a->r != b->r)
        return a->r < b->r ? -1 : 1;
    if (a->j != b->j)
        return a->j < b->j ? -1 : 1;
    if (a->i != b->i)
        return a->i < b->i ? -1 : 1;
    return (a->at > b->at) - (a->at < b->at);
}

static jaos_status set_row_quads(lp *p, bool check)
{
    if (p->nrq == 0)
        return JAOS_OK;
    lq_entry *e = jm_alloc_array(p->nrq, sizeof *e);
    int64_t *qi = jm_alloc_array(p->nrq, sizeof *qi);
    int64_t *qj = jm_alloc_array(p->nrq, sizeof *qj);
    double *qv = jm_alloc_array(p->nrq, sizeof *qv);
    jaos_status st = JAOS_OK;
    if (e == nullptr || qi == nullptr || qj == nullptr || qv == nullptr) {
        jm_set_err(p->m, "out of memory while reading LP");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    for (int64_t t = 0; t < p->nrq; t++)
        e[t] = (lq_entry){p->rq_r[t], p->rq_i[t], p->rq_j[t], t, p->rq_v[t]};
    qsort(e, (size_t)p->nrq, sizeof *e, cmp_lq);
    for (int64_t t = 0; t < p->nrq && st == JAOS_OK;) {
        const int64_t row = e[t].r;
        int64_t n = 0;
        for (; t < p->nrq && e[t].r == row; t++) {
            if (n > 0 && qi[n - 1] == e[t].i && qj[n - 1] == e[t].j) {
                qv[n - 1] += e[t].v;
                continue;
            }
            qi[n] = e[t].i;
            qj[n] = e[t].j;
            qv[n] = e[t].v;
            n++;
        }
        for (int64_t k = 0; k < n && st == JAOS_OK; k++)
            if (!isfinite(qv[k])) {
                jm_set_err(p->m, "row %lld has a quadratic term that is not "
                                 "finite", (long long)row);
                st = JAOS_ERR_INVALID_INPUT;
            }
        if (st == JAOS_OK && !check)
            st = jaos_set_row_quadratic(p->m, row, n, qi, qj, qv);
    }
out:
    free(e);
    free(qi);
    free(qj);
    free(qv);
    return st;
}

static jaos_status parse(lp *p)
{
    jaos_status st = JAOS_OK;
    int64_t j;

    if ((st = lx_next(p)) != JAOS_OK)
        goto done;

    if (tok_is(p, "minimize") || tok_is(p, "minimum") || tok_is(p, "min"))
        p->sense = JAOS_MINIMIZE;
    else if (tok_is(p, "maximize") || tok_is(p, "maximum") ||
             tok_is(p, "max"))
        p->sense = JAOS_MAXIMIZE;
    else
        FAIL("line %" PRId64 ": expected Minimize or Maximize", p->tok.line);
    if ((st = lx_next(p)) != JAOS_OK)
        goto done;

    if (p->tok.t == T_NAME && !at_reserved(p)) {
        token saved = p->tok;
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        if (p->tok.t == T_COLON) {
            p->oname = jm_name_copy(saved.text);
            if (p->oname == nullptr)
                FAIL_OOM();
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        } else {
            lx_push(p, &saved);
        }
    }
    if ((st = parse_expr(p, -1, nullptr)) != JAOS_OK)
        goto done;

    bool have_rows = true;
    if (tok_is(p, "subject") || tok_is(p, "such")) {
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        if (!tok_is(p, "to") && !tok_is(p, "that"))
            FAIL("line %" PRId64 ": expected 'Subject To'", p->tok.line);
    } else if (tok_is(p, "st") || tok_is(p, "s.t.")) {
        ;
    } else if (at_reserved(p)) {
        have_rows = false;
    } else {
        FAIL("line %" PRId64 ": expected 'Subject To'", p->tok.line);
    }
    if (have_rows && (st = lx_next(p)) != JAOS_OK)
        goto done;

    for (;;) {
        if (p->tok.t == T_EOF)
            FAIL("missing End");
        if (at_reserved(p)) {
            token saved = p->tok;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            const bool labelled = p->tok.t == T_COLON;
            lx_push(p, &saved);
            if (labelled)
                goto labelled_row;
            if (tok_is(p, "lazy") || tok_is(p, "user")) {
                const bool lazy = tok_is(p, "lazy");
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (!(lazy ? tok_is(p, "constraints") : tok_is(p, "cuts")))
                    FAIL("line %" PRId64 ": expected '%s'", p->tok.line,
                         lazy ? "Lazy Constraints" : "User Cuts");
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                continue;
            }
            break;
        }

    labelled_row:;
        char *label = nullptr;
        if (p->tok.t == T_NAME) {
            token saved = p->tok;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            if (p->tok.t == T_COLON) {
                label = jm_name_copy(saved.text);
                if (label == nullptr)
                    FAIL_OOM();
                if ((st = lx_next(p)) != JAOS_OK) {
                    free(label);
                    goto done;
                }
            } else {
                lx_push(p, &saved);
            }
        }

        int64_t row = p->nrow;
        if (!JM_GROW(p->rs, p->rs_cap, row + 2) ||
            !JM_GROW(p->rlb, p->rlb_cap, row + 1) ||
            !JM_GROW(p->rub, p->rub_cap, row + 1) ||
            !JM_GROW(p->rname, p->rname_cap, row + 1)) {
            free(label);
            FAIL_OOM();
        }
        p->rs[row] = p->nent;
        p->rname[row] = label;
        p->nrow++;
        if (!JM_GROW(p->ind_col, p->ind_cap, row + 1) ||
            !JM_GROW(p->ind_val, p->indv_cap, row + 1))
            FAIL_OOM();
        for (int64_t r = p->nind; r <= row; r++) {
            p->ind_col[r] = -1;
            p->ind_val[r] = 0;
        }
        p->nind = row + 1;

    body:
        bool ranged = false;
        toktype lo_rel = T_EOF;
        double lo_val = 0.0;
        if (p->tok.t == T_NUM || p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
            double lsign = 1.0;
            if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
                lsign = p->tok.t == T_MINUS ? -1.0 : 1.0;
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
            }
            if (p->tok.t == T_NUM) {
                token num_tok = p->tok;
                num_tok.num = lsign * p->tok.num;
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (p->tok.t == T_LE || p->tok.t == T_GE) {
                    ranged = true;
                    lo_rel = p->tok.t;
                    lo_val = num_tok.num;
                    if ((st = lx_next(p)) != JAOS_OK)
                        goto done;
                } else {
                    lx_push(p, &num_tok);
                }
            } else {
                p->pre_sign = lsign;
            }
        }

        double konst = 0.0;
        if ((st = parse_expr(p, row, &konst)) != JAOS_OK)
            goto done;

        toktype rel = p->tok.t;

        const int64_t rel_line = p->tok.line;
        if (rel != T_LE && rel != T_GE && rel != T_EQ)
            FAIL("line %" PRId64 ": expected <=, >= or = after the "
                 "constraint expression", p->tok.line);
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;

        double sign = 1.0;
        if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
            sign = p->tok.t == T_MINUS ? -1.0 : 1.0;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }
        double rhs;
        if (p->tok.t == T_NUM)
            rhs = sign * p->tok.num;
        else if (tok_is(p, "inf") || tok_is(p, "infinity"))
            rhs = sign * INFINITY;
        else
            FAIL("line %" PRId64 ": expected a number on the right-hand "
                 "side", p->tok.line);
        if (isinf(rhs) && (rel == T_EQ || (rel == T_GE) == (rhs > 0.0)))
            FAIL("line %" PRId64 ": no finite activity meets a constraint "
                 "that is %s %s; only '>= -inf' and '<= inf' are read, as a "
                 "free row", p->tok.line,
                 rel == T_EQ ? "=" : rel == T_LE ? "<=" : ">=",
                 rhs > 0.0 ? "inf" : "-inf");
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;

        if (p->tok.t == T_ARROW) {
            if (p->ind_col[row] >= 0)
                FAIL("line %" PRId64 ": a second '->' on one constraint",
                     p->tok.line);
            if (ranged || rel != T_EQ || konst != 0.0 ||
                p->nent != p->rs[row] + 1 || p->ev[p->rs[row]] != 1.0 ||
                (rhs != 0.0 && rhs != 1.0))
                FAIL("line %" PRId64 ": an indicator is 'variable = 0 ->' or "
                     "'variable = 1 ->'", rel_line);
            const int64_t z = p->ei[p->rs[row]];
            p->ind_col[row] = z;
            p->ind_val[row] = rhs == 1.0 ? 1 : 0;
            p->nent = p->rs[row];
            p->stamp[z] = 0;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            goto body;
        }
        if (p->tok.t == T_LE || p->tok.t == T_GE)
            FAIL("line %" PRId64 ": a third bound on one constraint",
                 p->tok.line);

        rhs -= konst;
        lo_val -= konst;

        if (ranged) {

            if (rel == T_EQ || (lo_rel == T_LE) != (rel == T_LE))
                FAIL("line %" PRId64 ": the two operators of a ranged "
                     "constraint must point the same way", rel_line);

            p->rlb[row] = lo_rel == T_LE ? lo_val : rhs;
            p->rub[row] = lo_rel == T_LE ? rhs : lo_val;
        } else {
            p->rlb[row] = rel == T_LE ? -INFINITY : rhs;
            p->rub[row] = rel == T_GE ? INFINITY : rhs;
        }
    }

    if (tok_is(p, "bounds") || tok_is(p, "bound")) {
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        for (;;) {
            if (p->tok.t == T_EOF)
                FAIL("missing End");
            if (at_reserved(p) && !tok_is(p, "inf") && !tok_is(p, "infinity"))
                break;

            if (p->tok.t == T_NAME && !at_reserved(p)) {

                if (!get_or_create_col(p, p->tok.text, &j))
                    FAIL_OOM();
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (tok_is(p, "free")) {
                    p->cl[j] = -INFINITY;
                    p->cu[j] = INFINITY;
                    if ((st = lx_next(p)) != JAOS_OK)
                        goto done;
                } else if (p->tok.t == T_LE || p->tok.t == T_GE ||
                           p->tok.t == T_EQ) {
                    toktype rel = p->tok.t;
                    double v;
                    if ((st = lx_next(p)) != JAOS_OK)
                        goto done;
                    if ((st = parse_bound_value(p, &v)) != JAOS_OK)
                        goto done;
                    if (rel == T_LE)
                        p->cu[j] = v;
                    else if (rel == T_GE)
                        p->cl[j] = v;
                    else {
                        p->cl[j] = v;
                        p->cu[j] = v;
                    }
                } else {
                    FAIL("line %" PRId64 ": malformed bound", p->tok.line);
                }
            } else {

                double first;
                if ((st = parse_bound_value(p, &first)) != JAOS_OK)
                    goto done;
                const toktype rel1 = p->tok.t;
                const int64_t rel1_line = p->tok.line;
                if (rel1 != T_LE && rel1 != T_GE)
                    FAIL("line %" PRId64 ": expected <= or >= after the bound "
                         "value", p->tok.line);
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (p->tok.t != T_NAME || at_reserved(p))
                    FAIL("line %" PRId64 ": expected a variable name",
                         p->tok.line);
                if (!get_or_create_col(p, p->tok.text, &j))
                    FAIL_OOM();
                if (rel1 == T_LE)
                    p->cl[j] = first;
                else
                    p->cu[j] = first;
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (p->tok.t == T_LE || p->tok.t == T_GE) {
                    if (p->tok.t != rel1)
                        FAIL("line %" PRId64 ": the two operators of a "
                             "two-sided bound must point the same way",
                             rel1_line);
                    double second;
                    if ((st = lx_next(p)) != JAOS_OK)
                        goto done;
                    if ((st = parse_bound_value(p, &second)) != JAOS_OK)
                        goto done;
                    if (rel1 == T_LE)
                        p->cu[j] = second;
                    else
                        p->cl[j] = second;
                }
            }
        }
    }

    for (;;) {
    if (tok_is(p, "general") || tok_is(p, "generals") ||
           tok_is(p, "gen") || tok_is(p, "integer") ||
           tok_is(p, "integers") || tok_is(p, "binary") ||
           tok_is(p, "binaries") || tok_is(p, "bin")) {
        const bool binary = tok_is(p, "binary") || tok_is(p, "binaries") ||
                            tok_is(p, "bin");
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        while (p->tok.t == T_NAME && !at_reserved(p)) {
            if (!get_or_create_col(p, p->tok.text, &j))
                FAIL_OOM();
            if (!JM_GROW(p->cint, p->cint_cap, p->ncol))
                FAIL_OOM();
            for (int64_t k = p->ncint; k < p->ncol; k++)
                p->cint[k] = false;
            p->ncint = p->ncol;
            p->cint[j] = true;
            if (binary) {
                p->cl[j] = 0.0;
                p->cu[j] = 1.0;
            }
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }
        continue;
    }

    if (tok_is(p, "semi") || tok_is(p, "semis")) {
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        if (p->tok.t == T_MINUS) {
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            if (!tok_is(p, "continuous"))
                FAIL("line %" PRId64 ": expected 'continuous' after 'semi-'",
                     p->tok.line);
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }
        while (p->tok.t == T_NAME && !at_reserved(p)) {
            if (!get_or_create_col(p, p->tok.text, &j))
                FAIL_OOM();
            if (!JM_GROW(p->csemi, p->csemi_cap, p->ncol))
                FAIL_OOM();
            for (int64_t k = p->ncsemi; k < p->ncol; k++)
                p->csemi[k] = false;
            p->ncsemi = p->ncol;
            p->csemi[j] = true;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }
        continue;
    }
    if (tok_is(p, "sos")) {
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        while (p->tok.t == T_NAME && !at_reserved(p)) {
            char first[NAME_MAX_LEN + 1];
            snprintf(first, sizeof first, "%s", p->tok.text);
            const int64_t fline = p->tok.line;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            if (p->tok.t != T_COLON)
                FAIL("line %" PRId64 ": expected ':' after '%s' in the SOS "
                     "section", fline, first);
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            double sign = 1.0;
            if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
                sign = p->tok.t == T_MINUS ? -1.0 : 1.0;
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (p->tok.t != T_NUM)
                    FAIL("line %" PRId64 ": an SOS member is 'variable:weight'",
                         fline);
            }
            if (p->tok.t == T_NUM) {
                if (p->nsos == 0)
                    FAIL("line %" PRId64 ": an SOS member before any set",
                         fline);
                if (!get_or_create_col(p, first, &j))
                    FAIL_OOM();
                if (!JM_GROW(p->st_col, p->sosm_cap, p->nsosm + 1) ||
                    !JM_GROW(p->st_w, p->sosw_cap, p->nsosm + 1))
                    FAIL_OOM();
                p->st_col[p->nsosm] = j;
                p->st_w[p->nsosm] = sign * p->tok.num;
                p->nsosm++;
                p->st_start[p->nsos] = p->nsosm;
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                continue;
            }
            char tybuf[NAME_MAX_LEN + 1];
            snprintf(tybuf, sizeof tybuf, "%s", first);
            if (p->tok.t == T_NAME) {
                snprintf(tybuf, sizeof tybuf, "%s", p->tok.text);
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (p->tok.t != T_COLON)
                    FAIL("line %" PRId64 ": expected '::' after the SOS type",
                         fline);
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
            }
            if (p->tok.t != T_COLON)
                FAIL("line %" PRId64 ": expected '::' after the SOS type",
                     fline);
            int type = 0;
            if (jm_strcasecmp(tybuf, "S1") == 0)
                type = 1;
            else if (jm_strcasecmp(tybuf, "S2") == 0)
                type = 2;
            else
                FAIL("line %" PRId64 ": an SOS set is S1 or S2, not '%s'",
                     fline, tybuf);
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
            if (!JM_GROW(p->st_type, p->sos_cap, p->nsos + 1) ||
                !JM_GROW(p->st_start, p->sos_start_cap, p->nsos + 2))
                FAIL_OOM();
            p->st_type[p->nsos] = type;
            p->st_start[p->nsos] = p->nsosm;
            p->nsos++;
            p->st_start[p->nsos] = p->nsosm;
        }
        continue;
    }
    break;
    }
    for (int64_t k = 0; k < p->nsos; k++) {
        const int64_t b = p->st_start[k], e = p->st_start[k + 1];
        if (e == b)
            FAIL("line %" PRId64 ": SOS set %lld has no members",
                 p->tok.line, (long long)(k + 1));
        for (int64_t t = b; t < e; t++)
            for (int64_t u = b; u < t; u++)
                if (p->st_col[t] == p->st_col[u] || p->st_w[t] == p->st_w[u])
                    FAIL("line %" PRId64 ": SOS set %lld repeats a member or "
                         "a weight", p->tok.line, (long long)(k + 1));
    }

    for (int64_t i = 0; i < p->nind && i < p->nrow; i++) {
        const int64_t z = p->ind_col[i];
        if (z >= 0 && !(z < p->ncint && p->cint[z]))
            FAIL("line %" PRId64 ": the indicator variable of constraint %lld "
                 "must be declared General or Binary", p->tok.line,
                 (long long)(i + 1));
    }
    if (!tok_is(p, "end"))
        FAIL("line %" PRId64 ": expected End", p->tok.line);
    if ((st = lx_next(p)) != JAOS_OK)
        goto done;
    if (p->tok.t != T_EOF)
        FAIL("line %" PRId64 ": content after End", p->tok.line);

    {

        if (!JM_GROW(p->rs, p->rs_cap, p->nrow + 1))
            FAIL_OOM();
        p->rs[p->nrow] = p->nent;
        if (!fold_ranges(p))
            FAIL_OOM();

        int64_t *as = jm_calloc_array(p->ncol + 1, sizeof(int64_t));
        int64_t *ai = jm_alloc_array(p->nent, sizeof(int64_t));
        double *av = jm_alloc_array(p->nent, sizeof(double));
        int64_t *fill = jm_calloc_array(p->ncol, sizeof(int64_t));
        if (!as || !ai || !av || !fill) {
            free(as); free(ai); free(av); free(fill);
            FAIL_OOM();
        }
        for (int64_t k = 0; k < p->nent; k++)
            as[p->ei[k] + 1]++;
        for (int64_t c = 0; c < p->ncol; c++)
            as[c + 1] += as[c];
        for (int64_t i = 0; i < p->nrow; i++) {
            for (int64_t k = p->rs[i]; k < p->rs[i + 1]; k++) {
                int64_t c = p->ei[k];
                int64_t pos = as[c] + fill[c]++;
                ai[pos] = i;
                av[pos] = p->ev[k];
            }
        }
        free(fill);

        if ((st = set_row_quads(p, true)) != JAOS_OK) {
            free(as);
            free(ai);
            free(av);
            goto done;
        }
        st = jaos_load_lp(p->m, p->ncol, p->nrow, p->sense, p->offset,
                          p->cost, p->cl, p->cu, p->rlb, p->rub,
                          p->nent, p->ncol > 0 ? as : nullptr, ai, av);
        free(as);
        free(ai);
        free(av);
        if (st != JAOS_OK) {
            jm_set_err(p->m, "internal: assembled model failed validation");
            goto done;
        }

        if (!JM_GROW(p->rname, p->rname_cap, p->nrow))
            FAIL_OOM();
        char **cn = jm_nmap_to_names(&p->cmap, p->ncol);
        if (cn == nullptr)
            FAIL_OOM();
        jm_model_take_names(p->m, cn, p->nrow > 0 ? p->rname : nullptr,
                            p->oname);
        if (p->nrow == 0)
            free(p->rname);
        p->rname = nullptr;
        p->oname = nullptr;

        free(p->m->col_integer);
        p->m->col_integer = nullptr;
        free(p->m->col_semi);
        p->m->col_semi = nullptr;
        free(p->m->col_quad);
        p->m->col_quad = nullptr;
        if (p->any_quad && p->ncol > 0 && p->nqoff == 0) {
            p->m->col_quad = p->cquad;
            p->cquad = nullptr;
        } else if (p->any_quad && p->ncol > 0) {

            int64_t n = p->nqoff;
            for (int64_t j = 0; j < p->ncol; j++)
                n += p->cquad[j] != 0.0;
            int64_t *qr = jm_alloc_array(n > 0 ? n : 1, sizeof *qr);
            int64_t *qc = jm_alloc_array(n > 0 ? n : 1, sizeof *qc);
            double *qv = jm_alloc_array(n > 0 ? n : 1, sizeof *qv);
            if (qr == nullptr || qc == nullptr || qv == nullptr) {
                free(qr); free(qc); free(qv);
                FAIL_OOM();
            }
            int64_t at = 0;
            for (int64_t j = 0; j < p->ncol; j++)
                if (p->cquad[j] != 0.0) {
                    qr[at] = j; qc[at] = j; qv[at] = p->cquad[j]; at++;
                }
            for (int64_t q = 0; q < p->nqoff; q++) {
                qr[at] = p->qri[q]; qc[at] = p->qci[q]; qv[at] = p->qvv[q];
                at++;
            }
            st = jaos_set_quadratic(p->m, at, qr, qc, qv);
            free(qr); free(qc); free(qv);
            if (st != JAOS_OK)
                goto done;
        }
        if (p->ncint > 0 || p->ncsemi > 0) {
            bool *ci = jm_calloc_array(p->ncol, sizeof(bool));
            if (ci == nullptr)
                FAIL_OOM();
            if (p->ncint > 0)
                memcpy(ci, p->cint, (size_t)p->ncint * sizeof *ci);
            p->m->col_integer = ci;
        }
        if (p->ncsemi > 0) {
            bool *cs = jm_calloc_array(p->ncol, sizeof(bool));
            if (cs == nullptr)
                FAIL_OOM();
            memcpy(cs, p->csemi, (size_t)p->ncsemi * sizeof *cs);
            p->m->col_semi = cs;
        }
        for (int64_t k = 0; k < p->nsos; k++) {
            const int64_t b = p->st_start[k], e = p->st_start[k + 1];
            if (e == b)
                continue;
            if ((st = jaos_add_sos(p->m, p->st_type[k], e - b, p->st_col + b,
                                   p->st_w + b)) != JAOS_OK)
                goto done;
        }
        for (int64_t i = 0; i < p->nind && i < p->nrow; i++)
            if (p->ind_col[i] >= 0 &&
                (st = jaos_set_row_indicator(p->m, i, p->ind_col[i],
                                             p->ind_val[i])) != JAOS_OK)
                goto done;
        if ((st = set_row_quads(p, false)) != JAOS_OK)
            goto done;
    }

done:
    return st;
}

jaos_status jaos_read_lp(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    lp pp = {0};
    lp *p = &pp;
    p->m = m;
    p->line = 1;
    p->sense = JAOS_MINIMIZE;

    jaos_status st = jm_slurp(m, path, &p->buf, &p->len);
    if (st != JAOS_OK)
        goto done;
    if (!scan_ranges(p)) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        jm_set_err(m, "out of memory while reading LP");
        goto done;
    }

    {
        jm_locale loc;
        jm_locale_c_enter(&loc);
        st = parse(p);
        jm_locale_leave(&loc);
    }

done:
    free(p->buf);
    jm_nmap_free(&p->cmap);
    free(p->cost);
    free(p->cquad);
    free(p->qri);
    free(p->qci);
    free(p->qvv);
    free(p->cl);
    free(p->cu);
    free(p->rs);
    free(p->rlb);
    free(p->rub);
    free(p->ei);
    free(p->ev);
    free(p->stamp);
    free(p->slot);

    if (p->rname != nullptr)
        for (int64_t i = 0; i < p->nrow; i++)
            free(p->rname[i]);
    free(p->rname);
    free(p->oname);
    free(p->cint);
    free(p->csemi);
    free(p->st_type);
    free(p->st_start);
    free(p->st_col);
    free(p->st_w);
    free(p->ind_col);
    free(p->ind_val);
    for (int64_t k = 0; k < p->nrg; k++) {
        free(p->rg_lo[k]);
        free(p->rg_hi[k]);
    }
    free(p->rg_lo);
    free(p->rg_hi);
    free(p->rq_r);
    free(p->rq_i);
    free(p->rq_j);
    free(p->rq_v);
    return st;
}
