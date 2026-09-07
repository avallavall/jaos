/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L

#include "jaos_internal.h"

#include <ctype.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

constexpr int NAME_MAX_LEN = 255;

typedef enum {
    T_EOF, T_NAME, T_NUM, T_PLUS, T_MINUS, T_LE, T_GE, T_EQ, T_COLON,
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

    jm_nmap cmap;
    double *cost, *cl, *cu;
    int64_t ncol, cost_cap, cl_cap, cu_cap;

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
    int64_t *ei;
    double *ev;
    int64_t nent, ei_cap, ev_cap;

    int64_t *stamp, *slot;
    int64_t stamp_cap, slot_cap;

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
    case '-': p->tok.t = T_MINUS; p->pos++; return JAOS_OK;
    case ':': p->tok.t = T_COLON; p->pos++; return JAOS_OK;
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
    return p->tok.t == T_NAME && strcasecmp(p->tok.text, kw) == 0;
}

static bool is_reserved(const char *s)
{
    static const char *kws[] = {
        "minimize", "minimum", "min", "maximize", "maximum", "max",
        "subject", "such", "st", "s.t.", "bounds", "bound",
        "general", "generals", "gen", "integer", "integers",
        "binary", "binaries", "bin", "semi", "semis", "sos",
        "end", "free", "infinity", "inf",
    };
    for (size_t i = 0; i < sizeof kws / sizeof *kws; i++)
        if (strcasecmp(s, kws[i]) == 0)
            return true;
    return false;
}

static bool at_reserved(const lp *p)
{
    return p->tok.t == T_NAME && is_reserved(p->tok.text);
}

bool jm_lp_name_ok(const char *s)
{
    if (s == nullptr || !name_start(s[0]))
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
        !JM_GROW(p->stamp, p->stamp_cap, j + 1) ||
        !JM_GROW(p->slot, p->slot_cap, j + 1))
        return false;
    if (!jm_nmap_insert(&p->cmap, name, j))
        return false;
    p->cost[j] = 0.0;
    p->cl[j] = 0.0;
    p->cu[j] = INFINITY;
    p->stamp[j] = 0;
    p->slot[j] = 0;
    p->ncol++;
    *out = j;
    return true;
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

        if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
            sign = p->tok.t == T_MINUS ? -1.0 : 1.0;
            have_sign = true;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        } else if (any) {
            break;
        }

        double coef = sign;
        bool have_num = false;
        if (p->tok.t == T_NUM) {
            coef = sign * p->tok.num;
            have_num = true;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }

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

    if (!any && row >= 0)
        FAIL("line %" PRId64 ": constraint without any term", p->tok.line);
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

    if (tok_is(p, "subject") || tok_is(p, "such")) {
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        if (!tok_is(p, "to") && !tok_is(p, "that"))
            FAIL("line %" PRId64 ": expected 'Subject To'", p->tok.line);
    } else if (!tok_is(p, "st") && !tok_is(p, "s.t.")) {
        FAIL("line %" PRId64 ": expected 'Subject To'", p->tok.line);
    }
    if ((st = lx_next(p)) != JAOS_OK)
        goto done;

    for (;;) {
        if (p->tok.t == T_EOF)
            FAIL("missing End");
        if (at_reserved(p))
            break;

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

        bool ranged = false;
        toktype lo_rel = T_EOF;
        double lo_val = 0.0;
        if (p->tok.t == T_NUM || p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
            double lsign = 1.0;
            if (p->tok.t == T_PLUS || p->tok.t == T_MINUS) {
                lsign = p->tok.t == T_MINUS ? -1.0 : 1.0;
                if ((st = lx_next(p)) != JAOS_OK)
                    goto done;
                if (p->tok.t != T_NUM)
                    FAIL("line %" PRId64 ": expected a number after the sign",
                         p->tok.line);
            }
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
        if (p->tok.t != T_NUM)
            FAIL("line %" PRId64 ": expected a number on the right-hand "
                 "side", p->tok.line);
        double rhs = sign * p->tok.num;
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;

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

                if (!jm_nmap_get(&p->cmap, p->tok.text, &j))
                    FAIL("line %" PRId64 ": bound on unknown variable '%s'",
                         p->tok.line, p->tok.text);
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
                if (!jm_nmap_get(&p->cmap, p->tok.text, &j))
                    FAIL("line %" PRId64 ": bound on unknown variable '%s'",
                         p->tok.line, p->tok.text);
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

    while (tok_is(p, "general") || tok_is(p, "generals") ||
           tok_is(p, "gen") || tok_is(p, "integer") ||
           tok_is(p, "integers") || tok_is(p, "binary") ||
           tok_is(p, "binaries") || tok_is(p, "bin")) {
        const bool binary = tok_is(p, "binary") || tok_is(p, "binaries") ||
                            tok_is(p, "bin");
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        while (p->tok.t == T_NAME && !at_reserved(p)) {
            if (!jm_nmap_get(&p->cmap, p->tok.text, &j))
                FAIL("line %" PRId64 ": '%s' in an integer section is not a "
                     "variable of the model", p->tok.line, p->tok.text);
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
            if (!jm_nmap_get(&p->cmap, p->tok.text, &j))
                FAIL("line %" PRId64 ": '%s' in a semi-continuous section is "
                     "not a variable of the model", p->tok.line, p->tok.text);
            if (!JM_GROW(p->csemi, p->csemi_cap, p->ncol))
                FAIL_OOM();
            for (int64_t k = p->ncsemi; k < p->ncol; k++)
                p->csemi[k] = false;
            p->ncsemi = p->ncol;
            p->csemi[j] = true;
            if ((st = lx_next(p)) != JAOS_OK)
                goto done;
        }
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
                if (!jm_nmap_get(&p->cmap, first, &j))
                    FAIL("line %" PRId64 ": '%s' in an SOS set is not a "
                         "variable of the model", fline, first);
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
            if (strcasecmp(tybuf, "S1") == 0)
                type = 1;
            else if (strcasecmp(tybuf, "S2") == 0)
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

    {
        locale_t cloc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
        locale_t prev = cloc ? uselocale(cloc) : (locale_t)0;
        st = parse(p);
        if (cloc) {
            uselocale(prev);
            freelocale(cloc);
        }
    }

done:
    free(p->buf);
    jm_nmap_free(&p->cmap);
    free(p->cost);
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
    return st;
}
