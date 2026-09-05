/* MPS reader, fixed and free layouts.
 *
 * Both layouts are read with one whitespace tokenizer: a section header is a
 * line whose first character is not blank, data lines start with blank, '*'
 * opens a comment. This reads every instance whose names contain no spaces —
 * all of Netlib and MIPLIB — and the true fixed-layout edge (names with
 * embedded blanks) is rejected rather than misread. Dialect decisions live
 * in docs/format-support.md; every rejection carries the line number.
 *
 * Numbers are parsed under an explicit "C" locale (uselocale) so a host
 * application running under a comma-decimal locale cannot corrupt instances
 * (D8 extends to parsing), and Fortran-style 'D' exponents are accepted.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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

/* --------------------------------------------------------------------- */
/* Tokens and numbers                                                    */
/* --------------------------------------------------------------------- */

constexpr int MAXTOK = 16;

/* Splits in place; strips CR/LF. Returns token count, or -1 on overflow. */
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

/* Finite double from an MPS token; accepts Fortran 'D' exponents. Assumes
 * the "C" locale is active (the reader guarantees it). */
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

/* --------------------------------------------------------------------- */
/* Reader                                                                */
/* --------------------------------------------------------------------- */

enum section { S_START, S_OBJSENSE, S_OBJNAME, S_ROWS, S_COLUMNS, S_RHS,
               S_RANGES,
               S_BOUNDS };

/* The objective row's value in the row map. Real rows are >= 0. */
#define OBJ_ROW (-1)

typedef struct {
    jaos_model *m;
    int64_t lno;

    jaos_obj_sense sense;
    double obj_offset;
    bool obj_offset_seen;

    jm_nmap rmap;                      /* row name -> index, or OBJ_ROW */
    char *rtype;                    /* per real row: L, G, E or N (free) */
    double *rhs;                    /* default 0 */
    double *range;                  /* NAN = no range */
    uint8_t *rflag;                 /* bit0: rhs seen, bit1: range seen */
    int64_t nrow;
    int64_t rtype_cap, rhs_cap, range_cap, rflag_cap;
    bool have_obj;

    /* OBJNAME names which free row is the objective. `nullptr` means the
     * section was absent and the first N row takes the job, which is the
     * rule every file that omits one is written to (D280). Owned here and
     * freed with everything else, because the token it is copied from
     * points into the line buffer and the next line overwrites it. */
    char *objname;
    /* The objective row's own name as ROWS gave it, handed to the model at
     * the end; nullptr until the row is met. */
    char *objrow;

    jm_nmap cmap;                      /* column name -> index */
    double *cost, *cl, *cu;
    uint8_t *cflag;                 /* bit0: lower set, bit1: obj coef seen */
    int64_t ncol;
    int64_t cost_cap, cl_cap, cu_cap, cflag_cap;

    /* matrix, CSC built incrementally (columns arrive contiguously) */
    int64_t *as, *ai;
    double *av;
    int64_t nnz;
    int64_t as_cap, ai_cap, av_cap;

    /* duplicate (row, col) detection: stamp = col index + 1; allocated
     * zeroed when COLUMNS starts, once nrow is final */
    int64_t *rowstamp;

    /* first-seen set names for RHS / RANGES / BOUNDS */
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
}

/* OBJNAME's row name, copied. Returns false only out of memory. */
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
    /* The name is kept on the model (D284), so it has to be one the model
     * accepts: a token has no whitespace, so this is the length and the
     * control characters. */
    if (!jm_name_ok(tok[1]))
        FAIL("line %" PRId64 ": row name '%s' is longer than %d bytes or "
             "holds a control character", r->lno, tok[1], JAOS_NAME_MAX);

    char t = tok[0][0];
    /* Which N row is the objective. Without an OBJNAME section it is the
     * first, which is the rule every file that omits one is written to.
     * With one it is the row that section named, and every other N row is
     * an ordinary free row -- including the ones that come BEFORE it, which
     * is why this cannot be decided by position (D280). */
    const bool is_obj = t == 'N' && !r->have_obj &&
        (r->objname == nullptr || strcmp(tok[1], r->objname) == 0);
    if (is_obj) {
        /* The objective is not a matrix row. Its name goes to the model
         * as the objective's, so a file written back carries it. */
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

    /* Integer markers: recognized, rejected until M3. */
    if (nt >= 2 && strcmp(tok[1], "'MARKER'") == 0) {
        for (int i = 2; i < nt; i++)
            if (strcmp(tok[i], "'INTORG'") == 0 ||
                strcmp(tok[i], "'INTEND'") == 0)
                FAIL("line %" PRId64 ": integer variables are not supported "
                     "yet (PLAN.md, M3)", r->lno);
        FAIL("line %" PRId64 ": unrecognized marker", r->lno);
    }

    if (nt < 3 || nt % 2 == 0)
        FAIL("line %" PRId64 ": COLUMNS entry needs a column name and "
             "(row, value) pairs", r->lno);

    /* Continuation of the current column, or a new one. */
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
            !JM_GROW(r->as, r->as_cap, j + 2))
            FAIL_OOM();
        if (!jm_nmap_insert(&r->cmap, tok[0], j))
            FAIL_OOM();
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

/* Shared by RHS and RANGES: set name, then (row, value) pairs. */
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
        return JAOS_OK; /* alternate set: first-seen wins, rest skipped */

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
            /* RHS on the objective row is the negated constant. */
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
    if (strcmp(type, "BV") == 0 || strcmp(type, "LI") == 0 ||
        strcmp(type, "UI") == 0)
        FAIL("line %" PRId64 ": bound type '%s' needs integer support "
             "(PLAN.md, M3)", r->lno, type);
    if (strcmp(type, "SC") == 0 || strcmp(type, "SI") == 0)
        FAIL("line %" PRId64 ": semi-continuous bound type '%s' is not "
             "supported", r->lno, type);
    if (!needs_value && !no_value)
        FAIL("line %" PRId64 ": unknown bound type '%s'", r->lno, type);
    if (nt != (needs_value ? 4 : 3))
        FAIL("line %" PRId64 ": bound type '%s' takes %s", r->lno, type,
             needs_value ? "exactly one value" : "no value");

    if (r->bnd_set[0] == '\0')
        snprintf(r->bnd_set, sizeof r->bnd_set, "%s", tok[1]);
    else if (strcmp(r->bnd_set, tok[1]) != 0)
        return JAOS_OK; /* alternate set skipped */

    int64_t j;
    if (!jm_nmap_get(&r->cmap, tok[2], &j))
        FAIL("line %" PRId64 ": unknown column '%s'", r->lno, tok[2]);

    double v = 0.0;
    if (needs_value && !parse_num(tok[3], &v))
        FAIL("line %" PRId64 ": bad number '%s'", r->lno, tok[3]);

    if (strcmp(type, "UP") == 0) {
        /* Classic MPS wart: a negative upper bound on a column whose lower
         * bound was never set explicitly drops that lower bound to -inf.
         * Documented in docs/format-support.md. */
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
    } else { /* PL */
        r->cu[j] = INFINITY;
    }
done:
    return st;
}

jaos_status jaos_read_mps(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    /* Read through jm_slurp so a `.gz` instance costs this reader nothing
     * but the call, then hand the bytes back to getline through fmemopen:
     * the line loop below is unchanged by compression. */
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

    /* Numbers must parse identically whatever locale the host application
     * set: switch this thread to "C" for the duration of the read. */
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
                /* the instance name is not retained */
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
                /* The same two spellings the OBJSENSE section has: the name
                 * on the header line, or on the line after it. */
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
                /* Every row is known by here, so this is the first moment a
                 * name that matches nothing can be reported (D280). */
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
        default:
            FAIL("line %" PRId64 ": data outside any section", r->lno);
        }
    }

    if (!ended)
        FAIL("missing ENDATA");

    /* Assemble row bounds from type + rhs + range, then hand everything to
     * the one validating loader. Range semantics per docs/format-support.md. */
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
            default: /* additional N rows stay free */
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

        /* The file's names, onto the model (D284). The row map's objective
         * entry carries OBJ_ROW and is skipped as out of range; the
         * objective's own name travels separately. */
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
