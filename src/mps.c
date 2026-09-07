/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L

#include "jaos_internal.h"

#include <ctype.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr int MAXTOK = 16;

static int split(char *line, char *tok[])
{
    int n = 0;
    for (char *p = line;;) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '\n' || *p == '\r')
            break;
        if (n == MAXTOK)
            return -1;
        tok[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            p++;
        if (*p)
            *p++ = '\0';
        else
            break;
    }
    return n;
}

static void upcase(char *s)
{
    for (; *s; s++)
        *s = (char)toupper((unsigned char)*s);
}

static bool parse_num(const char *t, double *out)
{
    char buf[64];
    size_t len = strlen(t);
    if (len == 0 || len >= sizeof buf)
        return false;
    for (size_t i = 0; i <= len; i++)
        buf[i] = (t[i] == 'D' || t[i] == 'd') ? 'E' : t[i];
    char *end;
    double v = strtod(buf, &end);
    if (end == buf || *end != '\0' || !isfinite(v))
        return false;
    *out = v;
    return true;
}

enum section { S_START, S_OBJSENSE, S_OBJNAME, S_ROWS, S_COLUMNS, S_RHS,
               S_RANGES,
               S_BOUNDS, S_SOS, S_INDICATORS };

#define OBJ_ROW (-1)

typedef struct {
    jaos_model *m;
    int64_t lno;

    jaos_obj_sense sense;
    double obj_offset;
    bool obj_offset_seen;

    jm_nmap rmap;
    char *rtype;
    double *rhs;
    double *range;
    uint8_t *rflag;
    int64_t nrow;
    int64_t rtype_cap, rhs_cap, range_cap, rflag_cap;
    bool have_obj;

    char *objname;

    char *objrow;

    char *modelname;

    bool in_intorg;
    bool *cint;
    int64_t cint_cap;
    bool any_int;
    bool *csemi;
    int64_t csemi_cap;
    bool any_semi;

    int *st_type;
    int64_t *st_start;
    int64_t *st_col;
    double *st_w;
    int64_t nsos, sos_cap, sos_start_cap, nsosm, sosm_cap, sosw_cap;

    int64_t *ind_row, *ind_col;
    int *ind_val;
    int64_t nind, indr_cap, indc_cap, indv_cap;

    jm_nmap cmap;
    double *cost, *cl, *cu;
    uint8_t *cflag;
    int64_t ncol;
    int64_t cost_cap, cl_cap, cu_cap, cflag_cap;

    int64_t *as, *ai;
    double *av;
    int64_t nnz;
    int64_t as_cap, ai_cap, av_cap;

    int64_t *rowstamp;

    char rhs_set[64], rng_set[64], bnd_set[64];
} rd;

static void rd_free(rd *r)
{
    jm_nmap_free(&r->rmap);
    jm_nmap_free(&r->cmap);
    free(r->rtype);
    free(r->rhs);
    free(r->range);
    free(r->rflag);
    free(r->cost);
    free(r->cl);
    free(r->cu);
    free(r->cflag);
    free(r->as);
    free(r->ai);
    free(r->av);
    free(r->rowstamp);
    free(r->objname);
    free(r->objrow);
    free(r->modelname);
    free(r->cint);
    free(r->csemi);
    free(r->st_type);
    free(r->st_start);
    free(r->st_col);
    free(r->st_w);
    free(r->ind_row);
    free(r->ind_col);
    free(r->ind_val);
}


static bool set_objname(rd *r, const char *name)
{
    const size_t len = strlen(name) + 1;
    char *p = malloc(len);
    if (p == nullptr)
        return false;
    memcpy(p, name, len);
    free(r->objname);
    r->objname = p;
    return true;
}

#define FAIL_OOM()  do { st = JAOS_ERR_OUT_OF_MEMORY; \
    jm_set_err(r->m, "out of memory while reading MPS"); goto done; } while (0)
#define FAIL(...)   do { st = JAOS_ERR_INVALID_INPUT; \
    jm_set_err(r->m, __VA_ARGS__); goto done; } while (0)

static jaos_status rd_rows_line(rd *r, char **tok, int nt)
{
    jaos_status st = JAOS_OK;
    int64_t dummy;

    if (nt != 2)
        FAIL("line %" PRId64 ": ROWS entry needs a type and a name", r->lno);
    upcase(tok[0]);
    if (strlen(tok[0]) != 1 || strchr("NLGE", tok[0][0]) == nullptr)
        FAIL("line %" PRId64 ": unknown row type '%s'", r->lno, tok[0]);
    if (jm_nmap_get(&r->rmap, tok[1], &dummy))
        FAIL("line %" PRId64 ": duplicate row name '%s'", r->lno, tok[1]);

    if (!jm_name_ok(tok[1]))
        FAIL("line %" PRId64 ": row name '%s' is longer than %d bytes or "
             "holds a control character", r->lno, tok[1], JAOS_NAME_MAX);

    char t = tok[0][0];

    const bool is_obj = t == 'N' && !r->have_obj &&
        (r->objname == nullptr || strcmp(tok[1], r->objname) == 0);
    if (is_obj) {

        if (!jm_nmap_insert(&r->rmap, tok[1], OBJ_ROW))
            FAIL_OOM();
        r->objrow = jm_name_copy(tok[1]);
        if (r->objrow == nullptr)
            FAIL_OOM();
        r->have_obj = true;
        return JAOS_OK;
    }
    if (!JM_GROW(r->rtype, r->rtype_cap, r->nrow + 1) ||
        !JM_GROW(r->rhs, r->rhs_cap, r->nrow + 1) ||
        !JM_GROW(r->range, r->range_cap, r->nrow + 1) ||
        !JM_GROW(r->rflag, r->rflag_cap, r->nrow + 1))
        FAIL_OOM();
    if (!jm_nmap_insert(&r->rmap, tok[1], r->nrow))
        FAIL_OOM();
    r->rtype[r->nrow] = t;
    r->rhs[r->nrow] = 0.0;
    r->range[r->nrow] = NAN;
    r->rflag[r->nrow] = 0;
    r->nrow++;
done:
    return st;
}

static jaos_status rd_columns_line(rd *r, char **tok, int nt)
{
    jaos_status st = JAOS_OK;

    if (nt >= 2 && strcmp(tok[1], "'MARKER'") == 0) {
        for (int i = 2; i < nt; i++) {
            if (strcmp(tok[i], "'INTORG'") == 0) {
                r->in_intorg = true;
                return JAOS_OK;
            }
            if (strcmp(tok[i], "'INTEND'") == 0) {
                r->in_intorg = false;
                return JAOS_OK;
            }
        }
        FAIL("line %" PRId64 ": unrecognized marker", r->lno);
    }

    if (nt < 3 || nt % 2 == 0)
        FAIL("line %" PRId64 ": COLUMNS entry needs a column name and "
             "(row, value) pairs", r->lno);

    int64_t j;
    if (r->ncol > 0 &&
        strcmp(tok[0], r->cmap.pool + r->cmap.off[r->cmap.n - 1]) == 0) {
        j = r->ncol - 1;
    } else {
        int64_t dummy;
        if (jm_nmap_get(&r->cmap, tok[0], &dummy))
            FAIL("line %" PRId64 ": column '%s' reappears; column entries "
                 "must be contiguous", r->lno, tok[0]);
        if (!jm_name_ok(tok[0]))
            FAIL("line %" PRId64 ": column name '%s' is longer than %d bytes "
                 "or holds a control character", r->lno, tok[0],
                 JAOS_NAME_MAX);
        j = r->ncol;
        if (!JM_GROW(r->cost, r->cost_cap, j + 1) ||
            !JM_GROW(r->cl, r->cl_cap, j + 1) ||
            !JM_GROW(r->cu, r->cu_cap, j + 1) ||
            !JM_GROW(r->cflag, r->cflag_cap, j + 1) ||
            !JM_GROW(r->cint, r->cint_cap, j + 1) ||
            !JM_GROW(r->csemi, r->csemi_cap, j + 1) ||
            !JM_GROW(r->as, r->as_cap, j + 2))
            FAIL_OOM();
        if (!jm_nmap_insert(&r->cmap, tok[0], j))
            FAIL_OOM();
        r->cint[j] = r->in_intorg;
        r->csemi[j] = false;
        r->any_int |= r->in_intorg;
        r->cost[j] = 0.0;
        r->cl[j] = 0.0;
        r->cu[j] = INFINITY;
        r->cflag[j] = 0;
        r->as[j] = r->nnz;
        r->ncol++;
    }

    for (int i = 1; i + 1 < nt; i += 2) {
        double v;
        if (!parse_num(tok[i + 1], &v))
            FAIL("line %" PRId64 ": bad number '%s'", r->lno, tok[i + 1]);
        int64_t row;
        if (!jm_nmap_get(&r->rmap, tok[i], &row))
            FAIL("line %" PRId64 ": unknown row '%s'", r->lno, tok[i]);
        if (row == OBJ_ROW) {
            if (r->cflag[j] & 2u)
                FAIL("line %" PRId64 ": duplicate objective coefficient for "
                     "column '%s'", r->lno, tok[0]);
            r->cflag[j] |= 2u;
            r->cost[j] = v;
            continue;
        }
        if (r->rowstamp[row] == j + 1)
            FAIL("line %" PRId64 ": duplicate coefficient for row '%s' in "
                 "column '%s'", r->lno, tok[i], tok[0]);
        r->rowstamp[row] = j + 1;
        if (!JM_GROW(r->ai, r->ai_cap, r->nnz + 1) ||
            !JM_GROW(r->av, r->av_cap, r->nnz + 1))
            FAIL_OOM();
        r->ai[r->nnz] = row;
        r->av[r->nnz] = v;
        r->nnz++;
    }
done:
    return st;
}

static jaos_status rd_vector_line(rd *r, char **tok, int nt, bool is_range)
{
    jaos_status st = JAOS_OK;
    char *set = is_range ? r->rng_set : r->rhs_set;
    const char *what = is_range ? "RANGES" : "RHS";

    if (nt < 3 || nt % 2 == 0)
        FAIL("line %" PRId64 ": %s entry needs a set name and (row, value) "
             "pairs", r->lno, what);
    if (set[0] == '\0')
        snprintf(set, 64, "%s", tok[0]);
    else if (strcmp(set, tok[0]) != 0)
        return JAOS_OK;

    for (int i = 1; i + 1 < nt; i += 2) {
        double v;
        if (!parse_num(tok[i + 1], &v))
            FAIL("line %" PRId64 ": bad number '%s'", r->lno, tok[i + 1]);
        int64_t row;
        if (!jm_nmap_get(&r->rmap, tok[i], &row))
            FAIL("line %" PRId64 ": unknown row '%s'", r->lno, tok[i]);
        if (row == OBJ_ROW) {
            if (is_range)
                FAIL("line %" PRId64 ": RANGES on the objective row", r->lno);
            if (r->obj_offset_seen)
                FAIL("line %" PRId64 ": duplicate objective RHS", r->lno);
            r->obj_offset_seen = true;

            r->obj_offset = -v;
            continue;
        }
        uint8_t bit = is_range ? 2u : 1u;
        if (r->rflag[row] & bit)
            FAIL("line %" PRId64 ": duplicate %s for row '%s'", r->lno, what,
                 tok[i]);
        r->rflag[row] |= bit;
        if (is_range) {
            if (r->rtype[row] == 'N')
                FAIL("line %" PRId64 ": RANGES on a free row", r->lno);
            r->range[row] = v;
        } else {
            r->rhs[row] = v;
        }
    }
done:
    return st;
}

static jaos_status rd_bounds_line(rd *r, char **tok, int nt)
{
    jaos_status st = JAOS_OK;
    if (nt < 3)
        FAIL("line %" PRId64 ": BOUNDS entry too short", r->lno);
    upcase(tok[0]);

    const char *type = tok[0];
    bool needs_value = strcmp(type, "UP") == 0 || strcmp(type, "LO") == 0 ||
                       strcmp(type, "FX") == 0;
    bool no_value    = strcmp(type, "FR") == 0 || strcmp(type, "MI") == 0 ||
                       strcmp(type, "PL") == 0;

    const bool int_type = strcmp(type, "BV") == 0 || strcmp(type, "LI") == 0 ||
                          strcmp(type, "UI") == 0;
    if (strcmp(type, "LI") == 0 || strcmp(type, "UI") == 0)
        needs_value = true;
    if (strcmp(type, "BV") == 0)
        no_value = true;
    const bool semi_type = strcmp(type, "SC") == 0 || strcmp(type, "SI") == 0;
    if (semi_type)
        needs_value = true;
    if (!needs_value && !no_value)
        FAIL("line %" PRId64 ": unknown bound type '%s'", r->lno, type);
    if (nt != (needs_value ? 4 : 3))
        FAIL("line %" PRId64 ": bound type '%s' takes %s", r->lno, type,
             needs_value ? "exactly one value" : "no value");

    if (r->bnd_set[0] == '\0')
        snprintf(r->bnd_set, sizeof r->bnd_set, "%s", tok[1]);
    else if (strcmp(r->bnd_set, tok[1]) != 0)
        return JAOS_OK;

    int64_t j;
    if (!jm_nmap_get(&r->cmap, tok[2], &j))
        FAIL("line %" PRId64 ": unknown column '%s'", r->lno, tok[2]);

    double v = 0.0;
    if (needs_value && !parse_num(tok[3], &v))
        FAIL("line %" PRId64 ": bad number '%s'", r->lno, tok[3]);

    if (int_type) {
        r->cint[j] = true;
        r->any_int = true;
        if (strcmp(type, "BV") == 0) {
            r->cl[j] = 0.0;
            r->cu[j] = 1.0;
            r->cflag[j] |= 1u;
        } else if (strcmp(type, "LI") == 0) {
            r->cl[j] = v;
            r->cflag[j] |= 1u;
        } else {
            r->cu[j] = v;
        }
        return JAOS_OK;
    }

    if (semi_type) {
        r->csemi[j] = true;
        r->any_semi = true;
        r->cu[j] = v;
        if (strcmp(type, "SI") == 0) {
            r->cint[j] = true;
            r->any_int = true;
        }
        return JAOS_OK;
    }

    if (strcmp(type, "UP") == 0) {

        if (v < 0.0 && (r->cflag[j] & 1u) == 0)
            r->cl[j] = -INFINITY;
        r->cu[j] = v;
    } else if (strcmp(type, "LO") == 0) {
        r->cl[j] = v;
        r->cflag[j] |= 1u;
    } else if (strcmp(type, "FX") == 0) {
        r->cl[j] = v;
        r->cu[j] = v;
        r->cflag[j] |= 1u;
    } else if (strcmp(type, "FR") == 0) {
        r->cl[j] = -INFINITY;
        r->cu[j] = INFINITY;
        r->cflag[j] |= 1u;
    } else if (strcmp(type, "MI") == 0) {
        r->cl[j] = -INFINITY;
        r->cflag[j] |= 1u;
    } else {
        r->cu[j] = INFINITY;
    }
done:
    return st;
}

static jaos_status rd_indicator_line(rd *r, char **tok, int nt)
{
    jaos_status st = JAOS_OK;
    if (nt != 4)
        FAIL("line %" PRId64 ": an indicator line is 'IF row column value'",
             r->lno);
    upcase(tok[0]);
    if (strcmp(tok[0], "IF") != 0)
        FAIL("line %" PRId64 ": an indicator line starts with IF", r->lno);
    int64_t i, j;
    if (!jm_nmap_get(&r->rmap, tok[1], &i) || i == OBJ_ROW)
        FAIL("line %" PRId64 ": unknown row '%s'", r->lno, tok[1]);
    if (!jm_nmap_get(&r->cmap, tok[2], &j))
        FAIL("line %" PRId64 ": unknown column '%s'", r->lno, tok[2]);
    double v = 0.0;
    if (!parse_num(tok[3], &v) || (v != 0.0 && v != 1.0))
        FAIL("line %" PRId64 ": an indicator fires at 0 or at 1, not '%s'",
             r->lno, tok[3]);
    if (!JM_GROW(r->ind_row, r->indr_cap, r->nind + 1) ||
        !JM_GROW(r->ind_col, r->indc_cap, r->nind + 1) ||
        !JM_GROW(r->ind_val, r->indv_cap, r->nind + 1))
        FAIL_OOM();
    r->ind_row[r->nind] = i;
    r->ind_col[r->nind] = j;
    r->ind_val[r->nind] = v == 1.0 ? 1 : 0;
    r->nind++;
done:
    return st;
}

static jaos_status rd_sos_line(rd *r, char **tok, int nt)
{
    jaos_status st = JAOS_OK;
    char head[3] = {0, 0, 0};
    if (nt >= 1 && strlen(tok[0]) == 2) {
        head[0] = (char)toupper((unsigned char)tok[0][0]);
        head[1] = tok[0][1];
    }
    if (head[0] == 'S' && (head[1] == '1' || head[1] == '2') &&
        (nt == 1 || nt == 2 || nt == 3)) {
        if (!JM_GROW(r->st_type, r->sos_cap, r->nsos + 1) ||
            !JM_GROW(r->st_start, r->sos_start_cap, r->nsos + 2))
            FAIL_OOM();
        r->st_type[r->nsos] = head[1] - '0';
        r->st_start[r->nsos] = r->nsosm;
        r->nsos++;
        r->st_start[r->nsos] = r->nsosm;
        return JAOS_OK;
    }
    if (r->nsos == 0)
        FAIL("line %" PRId64 ": an SOS member before any S1 or S2 header",
             r->lno);
    const char *name = tok[0];
    const char *wtxt = nullptr;
    char buf[JAOS_NAME_MAX + 32];
    if (nt == 2) {
        wtxt = tok[1];
    } else if (nt == 1) {
        const char *colon = strrchr(tok[0], ':');
        if (colon == nullptr || (size_t)(colon - tok[0]) >= sizeof buf)
            FAIL("line %" PRId64 ": an SOS member is 'column weight'", r->lno);
        memcpy(buf, tok[0], (size_t)(colon - tok[0]));
        buf[colon - tok[0]] = '\0';
        name = buf;
        wtxt = colon + 1;
    } else {
        FAIL("line %" PRId64 ": an SOS member is 'column weight'", r->lno);
    }
    int64_t j;
    if (!jm_nmap_get(&r->cmap, name, &j))
        FAIL("line %" PRId64 ": unknown column '%s'", r->lno, name);
    double w = 0.0;
    if (!parse_num(wtxt, &w))
        FAIL("line %" PRId64 ": bad number '%s'", r->lno, wtxt);
    if (!JM_GROW(r->st_col, r->sosm_cap, r->nsosm + 1) ||
        !JM_GROW(r->st_w, r->sosw_cap, r->nsosm + 1))
        FAIL_OOM();
    r->st_col[r->nsosm] = j;
    r->st_w[r->nsosm] = w;
    r->nsosm++;
    r->st_start[r->nsos] = r->nsosm;
done:
    return st;
}

jaos_status jaos_read_mps(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    char *src = nullptr;
    int64_t srclen = 0;
    jaos_status open_st = jm_slurp(m, path, &src, &srclen);
    if (open_st != JAOS_OK)
        return open_st;
    FILE *f = fmemopen(src, (size_t)srclen, "r");
    if (f == nullptr) {
        free(src);
        jm_set_err(m, "out of memory reading '%s'", path);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    locale_t cloc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    locale_t prev = cloc ? uselocale(cloc) : (locale_t)0;

    rd rr = {0};
    rd *r = &rr;
    r->m = m;
    r->sense = JAOS_MINIMIZE;

    jaos_status st = JAOS_OK;
    enum section sec = S_START;
    bool ended = false;
    bool objsense_pending = false;
    bool objname_pending = false;

    char *line = nullptr;
    size_t lsz = 0;
    char *tok[MAXTOK];

    while (!ended && getline(&line, &lsz, f) >= 0) {
        r->lno++;
        if (line[0] == '*')
            continue;
        bool header = line[0] != ' ' && line[0] != '\t' &&
                      line[0] != '\n' && line[0] != '\r' && line[0] != '\0';
        int nt = split(line, tok);
        if (nt < 0)
            FAIL("line %" PRId64 ": too many fields", r->lno);
        if (nt == 0)
            continue;

        if (header) {
            char kw[16];
            snprintf(kw, sizeof kw, "%s", tok[0]);
            upcase(kw);
            objsense_pending = false;
            objname_pending = false;
            if (strcmp(kw, "NAME") == 0) {

                free(r->modelname);
                r->modelname = nullptr;
                if (nt >= 2 && jm_name_ok(tok[1])) {
                    r->modelname = jm_name_copy(tok[1]);
                    if (r->modelname == nullptr)
                        FAIL_OOM();
                }
            } else if (strcmp(kw, "OBJSENSE") == 0) {
                if (nt >= 2) {
                    upcase(tok[1]);
                    if (strcmp(tok[1], "MAX") == 0 ||
                        strcmp(tok[1], "MAXIMIZE") == 0)
                        r->sense = JAOS_MAXIMIZE;
                    else if (strcmp(tok[1], "MIN") == 0 ||
                             strcmp(tok[1], "MINIMIZE") == 0)
                        r->sense = JAOS_MINIMIZE;
                    else
                        FAIL("line %" PRId64 ": unknown objective sense '%s'",
                             r->lno, tok[1]);
                } else {
                    objsense_pending = true;
                }
                sec = S_OBJSENSE;
            } else if (strcmp(kw, "OBJNAME") == 0) {

                if (sec >= S_ROWS)
                    FAIL("line %" PRId64 ": OBJNAME after ROWS; it says which "
                         "free row to read as the objective, so it has to "
                         "come first", r->lno);
                if (r->objname != nullptr)
                    FAIL("line %" PRId64 ": a second OBJNAME section", r->lno);
                if (nt >= 2) {
                    if (!set_objname(r, tok[1]))
                        FAIL_OOM();
                } else {
                    objname_pending = true;
                }
                sec = S_OBJNAME;
            } else if (strcmp(kw, "ROWS") == 0) {
                if (objname_pending)
                    FAIL("line %" PRId64 ": OBJNAME with no row name",
                         r->lno);
                sec = S_ROWS;
            } else if (strcmp(kw, "COLUMNS") == 0) {
                if (sec != S_ROWS)
                    FAIL("line %" PRId64 ": COLUMNS must follow ROWS", r->lno);

                if (r->objname != nullptr && !r->have_obj)
                    FAIL("line %" PRId64 ": OBJNAME names '%s', and ROWS has "
                         "no free row by that name", r->lno, r->objname);
                r->rowstamp = jm_calloc_array(r->nrow, sizeof(int64_t));
                if (r->rowstamp == nullptr)
                    FAIL_OOM();
                sec = S_COLUMNS;
            } else if (strcmp(kw, "RHS") == 0) {
                if (sec < S_COLUMNS)
                    FAIL("line %" PRId64 ": RHS before COLUMNS", r->lno);
                sec = S_RHS;
            } else if (strcmp(kw, "RANGES") == 0) {
                if (sec < S_COLUMNS)
                    FAIL("line %" PRId64 ": RANGES before COLUMNS", r->lno);
                sec = S_RANGES;
            } else if (strcmp(kw, "BOUNDS") == 0) {
                if (sec < S_COLUMNS)
                    FAIL("line %" PRId64 ": BOUNDS before COLUMNS", r->lno);
                sec = S_BOUNDS;
            } else if (strcmp(kw, "SOS") == 0) {
                if (sec < S_COLUMNS)
                    FAIL("line %" PRId64 ": SOS before COLUMNS", r->lno);
                sec = S_SOS;
            } else if (strcmp(kw, "INDICATORS") == 0) {
                if (sec < S_COLUMNS)
                    FAIL("line %" PRId64 ": INDICATORS before COLUMNS", r->lno);
                sec = S_INDICATORS;
            } else if (strcmp(kw, "ENDATA") == 0) {
                ended = true;
            } else {
                FAIL("line %" PRId64 ": unsupported section '%s'", r->lno,
                     tok[0]);
            }
            continue;
        }

        switch (sec) {
        case S_OBJNAME:
            if (!objname_pending)
                FAIL("line %" PRId64 ": unexpected data in OBJNAME", r->lno);
            if (nt != 1)
                FAIL("line %" PRId64 ": OBJNAME takes one row name", r->lno);
            if (!set_objname(r, tok[0]))
                FAIL_OOM();
            objname_pending = false;
            break;
        case S_OBJSENSE:
            if (!objsense_pending)
                FAIL("line %" PRId64 ": unexpected data in OBJSENSE", r->lno);
            upcase(tok[0]);
            if (strcmp(tok[0], "MAX") == 0 || strcmp(tok[0], "MAXIMIZE") == 0)
                r->sense = JAOS_MAXIMIZE;
            else if (strcmp(tok[0], "MIN") == 0 ||
                     strcmp(tok[0], "MINIMIZE") == 0)
                r->sense = JAOS_MINIMIZE;
            else
                FAIL("line %" PRId64 ": unknown objective sense '%s'", r->lno,
                     tok[0]);
            objsense_pending = false;
            break;
        case S_ROWS:
            if ((st = rd_rows_line(r, tok, nt)) != JAOS_OK)
                goto done;
            break;
        case S_COLUMNS:
            if ((st = rd_columns_line(r, tok, nt)) != JAOS_OK)
                goto done;
            break;
        case S_RHS:
            if ((st = rd_vector_line(r, tok, nt, false)) != JAOS_OK)
                goto done;
            break;
        case S_RANGES:
            if ((st = rd_vector_line(r, tok, nt, true)) != JAOS_OK)
                goto done;
            break;
        case S_BOUNDS:
            if ((st = rd_bounds_line(r, tok, nt)) != JAOS_OK)
                goto done;
            break;
        case S_SOS:
            if ((st = rd_sos_line(r, tok, nt)) != JAOS_OK)
                goto done;
            break;
        case S_INDICATORS:
            if ((st = rd_indicator_line(r, tok, nt)) != JAOS_OK)
                goto done;
            break;
        default:
            FAIL("line %" PRId64 ": data outside any section", r->lno);
        }
    }

    if (!ended)
        FAIL("missing ENDATA");
    for (int64_t k = 0; k < r->nind; k++)
        if (!r->cint[r->ind_col[k]])
            FAIL("indicator %lld names a column that is not integer",
                 (long long)(k + 1));
    for (int64_t k = 0; k < r->nsos; k++) {
        const int64_t b = r->st_start[k], e = r->st_start[k + 1];
        if (e == b)
            FAIL("SOS set %lld has no members", (long long)(k + 1));
        for (int64_t t = b; t < e; t++)
            for (int64_t u = b; u < t; u++)
                if (r->st_col[t] == r->st_col[u] || r->st_w[t] == r->st_w[u])
                    FAIL("SOS set %lld repeats a member or a weight",
                         (long long)(k + 1));
    }

    {
        double *rl = jm_alloc_array(r->nrow, sizeof(double));
        double *ru = jm_alloc_array(r->nrow, sizeof(double));
        if (rl == nullptr || ru == nullptr) {
            free(rl);
            free(ru);
            FAIL_OOM();
        }
        for (int64_t i = 0; i < r->nrow; i++) {
            double b = r->rhs[i], rg = r->range[i];
            switch (r->rtype[i]) {
            case 'L':
                rl[i] = isnan(rg) ? -INFINITY : b - fabs(rg);
                ru[i] = b;
                break;
            case 'G':
                rl[i] = b;
                ru[i] = isnan(rg) ? INFINITY : b + fabs(rg);
                break;
            case 'E':
                if (isnan(rg)) {
                    rl[i] = b;
                    ru[i] = b;
                } else if (rg >= 0.0) {
                    rl[i] = b;
                    ru[i] = b + rg;
                } else {
                    rl[i] = b + rg;
                    ru[i] = b;
                }
                break;
            default:
                rl[i] = -INFINITY;
                ru[i] = INFINITY;
                break;
            }
        }

        if (r->ncol > 0)
            r->as[r->ncol] = r->nnz;
        st = jaos_load_lp(m, r->ncol, r->nrow, r->sense, r->obj_offset,
                          r->cost, r->cl, r->cu, rl, ru,
                          r->nnz, r->ncol > 0 ? r->as : nullptr,
                          r->ai, r->av);
        free(rl);
        free(ru);
        if (st != JAOS_OK) {
            jm_set_err(m, "internal: assembled model failed validation");
            goto done;
        }

        char **cn = jm_nmap_to_names(&r->cmap, r->ncol);
        char **rn = jm_nmap_to_names(&r->rmap, r->nrow);
        if (cn == nullptr || rn == nullptr) {
            free(cn);
            free(rn);
            jm_set_err(m, "out of memory keeping the names");
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        jm_model_take_names(m, cn, rn, r->objrow);
        r->objrow = nullptr;
        free(m->model_name);
        m->model_name = r->modelname;
        r->modelname = nullptr;

        free(m->col_integer);
        m->col_integer = nullptr;
        free(m->col_semi);
        m->col_semi = nullptr;
        if ((r->any_int || r->any_semi) && r->ncol > 0) {
            m->col_integer = r->cint;
            r->cint = nullptr;
        }
        if (r->any_semi && r->ncol > 0) {
            m->col_semi = r->csemi;
            r->csemi = nullptr;
        }
        for (int64_t k = 0; k < r->nsos; k++) {
            const int64_t b = r->st_start[k], e = r->st_start[k + 1];
            if (e == b)
                continue;
            if ((st = jaos_add_sos(m, r->st_type[k], e - b, r->st_col + b,
                                   r->st_w + b)) != JAOS_OK)
                goto done;
        }
        for (int64_t k = 0; k < r->nind; k++)
            if ((st = jaos_set_row_indicator(m, r->ind_row[k], r->ind_col[k],
                                             r->ind_val[k])) != JAOS_OK)
                goto done;
    }

done:
    free(line);
    rd_free(r);
    if (cloc) {
        uselocale(prev);
        freelocale(cloc);
    }
    fclose(f);
    free(src);
    return st;
}
