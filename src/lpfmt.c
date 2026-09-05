/* LP-format reader, CPLEX-style core dialect.
 *
 * Unlike MPS, the LP format is not line-oriented: expressions wrap lines
 * freely, so this is a token-stream parser. Grammar accepted:
 *
 *   file       := sense objective subject constraint* [bounds] end
 *   sense      := MIN[IMIZE|IMUM] | MAX[IMIZE|IMUM]
 *   objective  := [label ':'] expr?
 *   subject    := SUBJECT TO | SUCH THAT | ST | S.T.
 *   constraint := [label ':'] expr relop rhs
 *               | [label ':'] l relop expr relop u    (both relops alike)
 *   expr       := [+|-] term (( '+' | '-' ) term)*
 *   term       := number name | name | number      (bare number: obj only)
 *   relop      := '<=' | '<' | '=<' | '>=' | '>' | '=>' | '='
 *   bounds     := (v relop name [relop v]) | (name relop v) | (name FREE)
 *                 -- both relops of a two-sided form point the same way
 *
 * Rejected loudly: integer sections (until M3), SOS and semi-continuous,
 * constants inside constraints, bounds on unknown
 * variables. Repeated variables inside one expression sum, as algebra says
 * they should. Dialect notes live in docs/format-support.md.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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

    token tok;      /* current token */
    token pushed;   /* single-slot pushback */
    bool has_pushed;

    /* columns */
    jm_nmap cmap;
    double *cost, *cl, *cu;
    int64_t ncol, cost_cap, cl_cap, cu_cap;

    /* rows, built row-wise as constraints arrive; transposed at the end */
    int64_t *rs;                 /* [nrow+1] entry start per row */
    double *rlb, *rub;
    int64_t nrow, rs_cap, rlb_cap, rub_cap;
    /* the labels, nullptr where a constraint had none, and the objective's;
     * handed to the model at the end (D284) */
    char **rname;
    int64_t rname_cap;
    char *oname;
    int64_t *ei;                 /* entry column index */
    double *ev;                  /* entry value */
    int64_t nent, ei_cap, ev_cap;

    /* duplicate merge within one expression: stamp/slot per column */
    int64_t *stamp, *slot;
    int64_t stamp_cap, slot_cap;

    jaos_obj_sense sense;
    double offset;
} lp;

#define FAIL(...)   do { st = JAOS_ERR_INVALID_INPUT; \
    jm_set_err(p->m, __VA_ARGS__); goto done; } while (0)
#define FAIL_OOM()  do { st = JAOS_ERR_OUT_OF_MEMORY; \
    jm_set_err(p->m, "out of memory while reading LP"); goto done; } while (0)

/* --------------------------------------------------------------------- */
/* Scanner                                                               */
/* --------------------------------------------------------------------- */

/* The characters a name may hold beyond letters, digits, `_` and `.`: the
 * CPLEX LP set, which is what the files other solvers write carry. What is
 * left out is what the scanner reads as something else -- the operators,
 * `:`, whitespace, `\` -- and `[`, `]`, `*` and `^`, which CPLEX reserves
 * for the quadratic forms this dialect rejects. A name still may not start
 * with a digit or a `.`, because that is a number (D284). */
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

    /* skip whitespace and '\' comments */
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
        /* number: digits [. digits] [e|E [+|-] digits], where the exponent
         * is consumed only if digits actually follow — "2ex" is the number
         * 2 followed by the name "ex". */
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

/* Keywords that may not be used as variable names. */
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

/* What the LP writer may print as a name: exactly what this scanner reads
 * back as one T_NAME token that is not a keyword. Kept here, beside the
 * scanner, so the two cannot disagree. */
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

/* --------------------------------------------------------------------- */
/* Model assembly helpers                                                */
/* --------------------------------------------------------------------- */

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

/* Parses one linear expression. row < 0 accumulates into the objective
 * (bare constants allowed); row >= 0 appends entries for that row, merging
 * repeated variables. Afterwards p->tok is the first token past the
 * expression. Zero terms is acceptable only for the objective. */
static jaos_status parse_expr(lp *p, int64_t row, double *konst)
{
    jaos_status st = JAOS_OK;
    bool any = false;

    /* What the bare numbers in the expression added up to. The objective
     * folds them into `obj_offset` here; a constraint's caller folds them
     * into the right-hand side, which is the only place they can go (D278).
     * Summed in the order the file lists them, which is what makes two
     * readings of one file agree (D8). */
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
            break; /* no separator: the expression is over */
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
                p->ev[p->slot[j]] += coef; /* repeated variable: sum */
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
            /* A bare number. In the objective it is the offset; in a
             * constraint it moves to the other side of the relation, with
             * its sign flipped, which is what the caller does with what
             * this collects. Refused until D278, which is a refusal the
             * reader never needed: `3x + 5 <= 10` and `3x <= 5` are the
             * same constraint and nothing about the first is ambiguous. */
            if (row >= 0)
                *konst += coef;
            else
                p->offset += coef;
        } else if (have_sign) {
            FAIL("line %" PRId64 ": expected a term after the sign",
                 p->tok.line);
        } else {
            break; /* nothing consumed: empty expression */
        }
        any = true;
    }

    if (!any && row >= 0)
        FAIL("line %" PRId64 ": constraint without any term", p->tok.line);
done:
    return st;
}

/* Bound value: [+|-] (number | INF[INITY]). Consumes past the value. */
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

/* --------------------------------------------------------------------- */
/* Reader                                                                */
/* --------------------------------------------------------------------- */

static jaos_status parse(lp *p)
{
    jaos_status st = JAOS_OK;
    int64_t j;

    if ((st = lx_next(p)) != JAOS_OK)
        goto done;

    /* objective sense */
    if (tok_is(p, "minimize") || tok_is(p, "minimum") || tok_is(p, "min"))
        p->sense = JAOS_MINIMIZE;
    else if (tok_is(p, "maximize") || tok_is(p, "maximum") ||
             tok_is(p, "max"))
        p->sense = JAOS_MAXIMIZE;
    else
        FAIL("line %" PRId64 ": expected Minimize or Maximize", p->tok.line);
    if ((st = lx_next(p)) != JAOS_OK)
        goto done;

    /* objective: optional label, optional expression */
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

    /* SUBJECT TO | SUCH THAT | ST | S.T. */
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

    /* constraints */
    for (;;) {
        if (p->tok.t == T_EOF)
            FAIL("missing End");
        if (at_reserved(p))
            break;

        /* optional label, kept as the row's name */
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

        /* The two-sided form "l <= expr <= u". A leading signed number is
         * the left bound ONLY when a relational operator follows it: in
         * "3 x + 2 y <= 5" the same number is a coefficient. So the number is
         * read, the next token decides, and where it does not decide for a
         * range the number is pushed back with its sign folded in, which is
         * what `parse_expr` would have made of the pair anyway. */
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
        /* The operator's own line: by the time a ranged mismatch is found the
         * stream has read past the right-hand side and on to the next line. */
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

        /* A constant inside the expression moves to the other side, so
         * every bound this row gets shifts by it -- BOTH ends of a ranged
         * row, because the constant sits between them (D278). Subtracted
         * once here rather than at each use, so the two ends of a range
         * cannot be shifted by different amounts. */
        rhs -= konst;
        lo_val -= konst;

        if (ranged) {
            /* Both operators must point the same way, so the pair really is
             * an interval: "3 <= x >= 8" says nothing. */
            if (rel == T_EQ || (lo_rel == T_LE) != (rel == T_LE))
                FAIL("line %" PRId64 ": the two operators of a ranged "
                     "constraint must point the same way", rel_line);
            /* "l <= expr <= u" gives [l, u]; ">=" mirrors it. An inverted
             * pair is legal input to jaos.h and is left for the solve to
             * report infeasible, exactly as MPS RANGES leaves it. */
            p->rlb[row] = lo_rel == T_LE ? lo_val : rhs;
            p->rub[row] = lo_rel == T_LE ? rhs : lo_val;
        } else {
            p->rlb[row] = rel == T_LE ? -INFINITY : rhs;
            p->rub[row] = rel == T_GE ? INFINITY : rhs;
        }
    }

    /* bounds */
    if (tok_is(p, "bounds") || tok_is(p, "bound")) {
        if ((st = lx_next(p)) != JAOS_OK)
            goto done;
        for (;;) {
            if (p->tok.t == T_EOF)
                FAIL("missing End");
            if (at_reserved(p) && !tok_is(p, "inf") && !tok_is(p, "infinity"))
                break;

            if (p->tok.t == T_NAME && !at_reserved(p)) {
                /* name relop value | name = value | name FREE */
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
                /* value relop name [relop value], and the mirror of it.
                 * `10 >= x >= 2` is `2 <= x <= 10` written the other way
                 * round and says the same thing (D281). What the first
                 * operator decides is which SIDE the leading value is, and
                 * the second must point the same way -- `3 <= x >= 8` names
                 * two lower bounds and no interval, the same fault the
                 * ranged constraint above refuses in the same words. */
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

    /* remaining sections: recognized, rejected */
    if (tok_is(p, "general") || tok_is(p, "generals") || tok_is(p, "gen") ||
        tok_is(p, "integer") || tok_is(p, "integers") ||
        tok_is(p, "binary") || tok_is(p, "binaries") || tok_is(p, "bin"))
        FAIL("line %" PRId64 ": integer variables are not supported yet "
             "(PLAN.md, M3)", p->tok.line);
    if (tok_is(p, "semi") || tok_is(p, "semis"))
        FAIL("line %" PRId64 ": semi-continuous variables are not supported",
             p->tok.line);
    if (tok_is(p, "sos"))
        FAIL("line %" PRId64 ": SOS constraints are not supported",
             p->tok.line);

    if (!tok_is(p, "end"))
        FAIL("line %" PRId64 ": expected End", p->tok.line);
    if ((st = lx_next(p)) != JAOS_OK)
        goto done;
    if (p->tok.t != T_EOF)
        FAIL("line %" PRId64 ": content after End", p->tok.line);

    /* transpose the row-wise entries into CSC and load */
    {
        /* a constraint-free file leaves rs unallocated */
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

        /* The file's names, onto the model (D284): every column has one,
         * a constraint has one where it was labelled, the objective where
         * it was. The row array may be shorter than the row count when the
         * last constraints had no label and nothing grew it; the model
         * reads it by row, so it is grown to the count. */
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

    /* The whole file at once: token scanning needs free lookahead, and this
     * is also what inflates a `.gz` instance. */
    jaos_status st = jm_slurp(m, path, &p->buf, &p->len);
    if (st != JAOS_OK)
        goto done;

    /* Numbers must parse identically whatever locale the host set. */
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
    /* On success both were handed to the model and are null here. */
    if (p->rname != nullptr)
        for (int64_t i = 0; i < p->nrow; i++)
            free(p->rname[i]);
    free(p->rname);
    free(p->oname);
    return st;
}
