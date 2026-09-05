/* MPS, LP and solution writers, and the solution format's reader.
 *
 * The reader is here and not in a file of its own because it is the exact
 * inverse of the writer forty lines above it: the same generated names, the
 * same four status words, the same "format 1" line. Split across two files
 * they drift, and nothing would notice until a file written by one version
 * failed to read in another (D282).
 *
 * One contract shapes all three: a file JAOS writes is a file JAOS reads
 * back as the same model. Where a format cannot express what the model
 * holds, the writer refuses and names the row or the column. It never
 * writes a file that would read back as something else, and a failed write
 * removes the partial file rather than leaving it to be found later.
 *
 * Numbers are written under an explicit "C" locale, for the same reason the
 * readers parse under one: a host application running under a comma-decimal
 * locale would otherwise write "1,5" and produce files nothing can read.
 *
 * Rows and columns are written under the model's names (D284): the file's
 * where the model came from one, and positional -- `C<j+1>`, `R<i+1>`,
 * `COST` -- where nobody named them. That round-trips because both formats
 * list rows and columns in index order and both readers assign indices in
 * the order names appear. What can break it is two rows or two columns
 * called the same, which no reader can tell apart, so every writer here
 * refuses that by name before it opens the file; and, for LP, a name the
 * dialect's scanner would not read back as one token, refused the same way.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _POSIX_C_SOURCE 200809L

#include "jaos_internal.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* Names and numbers                                                     */
/* --------------------------------------------------------------------- */

/* A name is at most JAOS_NAME_MAX bytes, and a positional one far fewer. */
constexpr int NAME_LEN = JAOS_NAME_MAX + 1;
constexpr int NUM_LEN = 32;

/* The model's name for column j (row i), into the caller's NAME_LEN
 * buffer: its own, or its position (src/model.c). */
static void col_name(const jaos_model *m, char *buf, int64_t j)
{
    char tmp[JM_NAME_BUF];
    const char *s = jm_col_name(m, j, tmp);
    memcpy(buf, s, strlen(s) + 1);
}

static void row_name(const jaos_model *m, char *buf, int64_t i)
{
    char tmp[JM_NAME_BUF];
    const char *s = jm_row_name(m, i, tmp);
    memcpy(buf, s, strlen(s) + 1);
}

/* "%.17g" of a finite double always reads back as that double: that is the
 * IEEE-754 round-trip guarantee, and it is what makes the round trip exact
 * rather than close. Fifteen digits covers most real data and keeps the
 * file readable, so the shorter forms are tried first and kept only when
 * they read back exactly.
 *
 * The caller guarantees `v` is finite. The "C" locale must be active: both
 * the printing and the check that follows it depend on the decimal point. */
static void wr_num(char *buf, double v)
{
    for (int prec = 15; prec <= 16; prec++) {
        snprintf(buf, NUM_LEN, "%.*g", prec, v);
        if (strtod(buf, nullptr) == v)
            return;
    }
    snprintf(buf, NUM_LEN, "%.17g", v);
    /* The guarantee the round trip rests on, and the only path here that is
     * not checked by the loop above. A load-bearing invariant is an assert
     * in this project (D216, D224); this one catches a libc whose printf is
     * not correctly rounded. Measured over random bit patterns, one-ulp
     * walks from 1.0, small rationals and decimal fractions, and it has
     * never fired: bench/measurements/02-138/digits.txt owns the counts
     * (D226). */
    assert(strtod(buf, nullptr) == v);
}

/* --------------------------------------------------------------------- */
/* The writer                                                            */
/* --------------------------------------------------------------------- */

typedef struct {
    FILE *f;
    jaos_model *m;
    jaos_status st;
} wr;

/* Every refusal funnels here: the first one wins, so a later check cannot
 * overwrite the message that says what is actually wrong. */
static void wr_fail(wr *w, jaos_status st, const char *fmt, ...)
{
    if (w->st != JAOS_OK)
        return;
    w->st = st;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(w->m->err, sizeof w->m->err, fmt, ap);
    va_end(ap);
}

/* Installs the "C" locale, then opens the file. That order matters twice.
 *
 * `newlocale` allocates and can fail, and a writer that carries on without
 * the C locale is worse off than a reader that does: `wr_num` would print
 * "1,5" AND check it back with `strtod` under the same locale, so the check
 * passes and the file is written with JAOS_OK. So a locale failure is a
 * failure here, and it happens before `fopen(path, "w")` has truncated
 * whatever the caller had at that path. */
static bool wr_open(wr *w, const char *path, locale_t *prev, locale_t *cloc)
{
    *cloc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (*cloc == (locale_t)0) {
        wr_fail(w, JAOS_ERR_IO,
                "cannot install the C locale needed to write '%s'", path);
        return false;
    }
    *prev = uselocale(*cloc);
    if (*prev == (locale_t)0) {
        freelocale(*cloc);
        *cloc = (locale_t)0;
        wr_fail(w, JAOS_ERR_IO,
                "cannot switch to the C locale needed to write '%s'", path);
        return false;
    }
    w->f = fopen(path, "w");
    if (w->f == nullptr) {
        uselocale(*prev);
        freelocale(*cloc);
        *cloc = (locale_t)0;
        wr_fail(w, JAOS_ERR_IO, "cannot open '%s' for writing", path);
        return false;
    }
    return true;
}

/* Closes the stream, restores the locale, and removes the file when
 * anything went wrong. A stream error can surface only at fclose, which is
 * where a full disk usually appears, so both are checked. */
static jaos_status wr_close(wr *w, const char *path, locale_t prev,
                            locale_t cloc)
{
    if (ferror(w->f))
        wr_fail(w, JAOS_ERR_IO, "writing '%s' failed", path);
    if (fclose(w->f) != 0)
        wr_fail(w, JAOS_ERR_IO, "closing '%s' failed", path);
    if (cloc) {
        uselocale(prev);
        freelocale(cloc);
    }
    if (w->st != JAOS_OK)
        remove(path);
    else
        w->m->err[0] = '\0';
    return w->st;
}

/* Two rows, the objective among them, or two columns called the same
 * would read back as one, so no writer here writes them. Checked before
 * the file is opened, in one pass per side over a map of what has been
 * seen. A positional name takes part: a column named `C2` collides with
 * an unnamed second column. */
static void names_unique(wr *w)
{
    const jaos_model *m = w->m;
    jm_nmap seen = {0};
    char nm[NAME_LEN];
    int64_t prior;

    for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
        col_name(m, nm, j);
        if (jm_nmap_get(&seen, nm, &prior))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "columns %" PRId64 " and %" PRId64 " are both named "
                    "'%s', which no file can tell apart", prior, j, nm);
        else if (!jm_nmap_insert(&seen, nm, j))
            wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
    }
    jm_nmap_free(&seen);

    if (w->st == JAOS_OK && !jm_nmap_insert(&seen, jm_obj_name(m), -1))
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
    for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++) {
        row_name(m, nm, i);
        if (jm_nmap_get(&seen, nm, &prior)) {
            if (prior < 0)
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "row %" PRId64 " and the objective are both named "
                        "'%s', which no file can tell apart", i, nm);
            else
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "rows %" PRId64 " and %" PRId64 " are both named "
                        "'%s', which no file can tell apart", prior, i, nm);
        } else if (!jm_nmap_insert(&seen, nm, i)) {
            wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
        }
    }
    jm_nmap_free(&seen);
}

/* --------------------------------------------------------------------- */
/* MPS                                                                   */
/* --------------------------------------------------------------------- */

/* A ranged row is the one thing the MPS reader rebuilds by arithmetic
 * rather than by assignment, so it is the one thing that can come back
 * different. Its `G` form recovers the lower bound exactly and computes the
 * upper; its `L` form does the opposite. Both are tried, and the one that
 * reproduces the pair exactly is used. When neither does, the row is
 * refused: writing it would produce a file that reads back as a different
 * model, which is what this file exists to prevent. */
static bool range_form(double rl, double ru, char *type, double *rhs,
                       double *rng)
{
    double d = ru - rl;
    if (!isfinite(d))
        return false;
    if (rl + fabs(d) == ru) {          /* reader: rl = b, ru = b + |r| */
        *type = 'G';
        *rhs = rl;
        *rng = d;
        return true;
    }
    if (ru - fabs(d) == rl) {          /* reader: rl = b - |r|, ru = b */
        *type = 'L';
        *rhs = ru;
        *rng = d;
        return true;
    }
    return false;
}

/* Row i as the reader's four-way row type, plus the RHS and RANGES values
 * that reconstruct its bounds. `*rng` stays NAN when the row needs no
 * RANGES entry. */
static void mps_row_kind(wr *w, int64_t i, char *type, double *rhs,
                         double *rng)
{
    const double rl = w->m->row_lower[i], ru = w->m->row_upper[i];
    char name[NAME_LEN];
    row_name(w->m, name, i);
    /* Set on every path including the two refusals, because `type` comes
     * from jm_alloc_array, which is malloc. Leaving it to the caller's
     * `w->st == JAOS_OK` guards would make three separate loop conditions
     * load-bearing for initialisation, and neither ASan nor UBSan reports a
     * read of uninitialised malloc memory. */
    *type = 'N';
    *rng = NAN;
    *rhs = 0.0;

    if (rl == -INFINITY && ru == INFINITY) {
        *type = 'N';                       /* a free row, kept as one */
        return;
    }
    if (rl == ru) {
        if (!isfinite(rl)) {
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' has both bounds at infinity, which MPS cannot "
                    "express", name);
            return;
        }
        *type = 'E';
        *rhs = rl;
        return;
    }
    if (rl == -INFINITY && isfinite(ru)) {
        *type = 'L';
        *rhs = ru;
        return;
    }
    if (ru == INFINITY && isfinite(rl)) {
        *type = 'G';
        *rhs = rl;
        return;
    }
    if (isfinite(rl) && isfinite(ru) && rl < ru) {
        if (!range_form(rl, ru, type, rhs, rng))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' has bounds no RANGES entry reproduces exactly",
                    name);
        return;
    }
    /* What is left is a lower bound above the upper one. That is a
     * legitimate model — jaos_set_row_bounds accepts it and the solve
     * reports infeasible — and MPS cannot say it: every RANGES form yields
     * an interval with its lower bound first. */
    wr_fail(w, JAOS_ERR_INVALID_INPUT,
            "row '%s' has its lower bound above its upper bound, which MPS "
            "cannot express", name);
}

jaos_status jaos_write_mps(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    /* Nothing is opened until every check below has passed. `fopen(path,
     * "w")` truncates, and `wr_close` removes the file on failure, so
     * opening first would make a refused write destroy whatever the caller
     * already had at that path. */
    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;
    const char *obj = jm_obj_name(m);

    char *type = jm_alloc_array(m->num_row, sizeof(char));
    double *rhs = jm_alloc_array(m->num_row, sizeof(double));
    double *rng = jm_alloc_array(m->num_row, sizeof(double));
    if (type == nullptr || rhs == nullptr || rng == nullptr)
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");

    /* Every row and every column is classified before the file is opened,
     * so a refusal touches nothing on disk at all. */
    for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++)
        mps_row_kind(w, i, &type[i], &rhs[i], &rng[i]);

    char nm[NAME_LEN], rn[NAME_LEN], num[NUM_LEN];

    for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
        if (m->col_lower[j] == INFINITY || m->col_upper[j] == -INFINITY) {
            col_name(m, nm, j);
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' has a bound at an infinity MPS cannot "
                    "express", nm);
        }
    }

    /* The names. Two the same are refused for every format; MPS has one
     * more, because the reader takes a second field of 'MARKER' in COLUMNS
     * as an integer marker, so a row called that would not read back as a
     * row. */
    if (w->st == JAOS_OK)
        names_unique(w);
    for (int64_t i = -1; w->st == JAOS_OK && i < m->num_row; i++) {
        if (i >= 0)
            row_name(m, rn, i);
        if (strcmp(i < 0 ? obj : rn, "'MARKER'") == 0)
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "%s is named 'MARKER', which the MPS reader takes for "
                    "an integer marker; rename it", i < 0 ? "the objective"
                                                          : "a row");
    }

    locale_t prev = (locale_t)0, cloc = (locale_t)0;
    if (w->st != JAOS_OK || !wr_open(w, path, &prev, &cloc)) {
        free(type);
        free(rhs);
        free(rng);
        return w->st;
    }

    {
        fprintf(w->f, "* written by JAOS %s\n", JAOS_VERSION_STRING);
        fprintf(w->f, "NAME          JAOS\n");
        if (m->sense == JAOS_MAXIMIZE)
            fprintf(w->f, "OBJSENSE      MAX\n");

        fprintf(w->f, "ROWS\n");
        fprintf(w->f, " N  %s\n", obj);
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(m, rn, i);
            fprintf(w->f, " %c  %s\n", type[i], rn);
        }

        /* Column entries must be contiguous, and every column must appear
         * or the round trip loses the ones with no coefficients. A column
         * with nothing to say gets its objective entry written anyway. */
        fprintf(w->f, "COLUMNS\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            const int64_t beg = m->a_start[j], end = m->a_start[j + 1];
            int pending = 0;
            if (m->col_cost[j] != 0.0 || beg == end) {
                wr_num(num, m->col_cost[j]);
                fprintf(w->f, "    %-9s %-9s %s", nm, obj, num);
                pending = 1;
            }
            for (int64_t k = beg; k < end; k++) {
                row_name(m, rn, m->a_index[k]);
                wr_num(num, m->a_value[k]);
                if (pending == 0)
                    fprintf(w->f, "    %-9s %-9s %s", nm, rn, num);
                else
                    fprintf(w->f, "   %-9s %s", rn, num);
                if (++pending == 2) {
                    fprintf(w->f, "\n");
                    pending = 0;
                }
            }
            if (pending != 0)
                fprintf(w->f, "\n");
        }

        /* The objective constant travels as an RHS entry on the objective
         * row, negated: the reader stores `-v` (docs/format-support.md). */
        fprintf(w->f, "RHS\n");
        if (m->obj_offset != 0.0) {
            wr_num(num, -m->obj_offset);
            fprintf(w->f, "    RHS       %-9s %s\n", obj, num);
        }
        for (int64_t i = 0; i < m->num_row; i++) {
            if (type[i] == 'N' || rhs[i] == 0.0)
                continue;      /* a row never named in RHS defaults to 0 */
            row_name(m, rn, i);
            wr_num(num, rhs[i]);
            fprintf(w->f, "    RHS       %-9s %s\n", rn, num);
        }

        fprintf(w->f, "RANGES\n");
        for (int64_t i = 0; i < m->num_row; i++) {
            if (isnan(rng[i]))
                continue;
            row_name(m, rn, i);
            wr_num(num, rng[i]);
            fprintf(w->f, "    RNG       %-9s %s\n", rn, num);
        }

        /* Bound forms are chosen so the reader's negative-UP wart never
         * fires. It drops a lower bound that was never set explicitly, so
         * every UP written here is preceded by an LO or an MI: by an LO
         * even at the default zero, when the upper bound is negative. */
        fprintf(w->f, "BOUNDS\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            const double cl = m->col_lower[j], cu = m->col_upper[j];
            col_name(m, nm, j);
            if (cl == 0.0 && cu == INFINITY)
                continue;                          /* the reader's default */
            if (cl == -INFINITY && cu == INFINITY) {
                fprintf(w->f, " FR BND       %s\n", nm);
                continue;
            }
            if (cl == cu) {
                wr_num(num, cl);
                fprintf(w->f, " FX BND       %-9s %s\n", nm, num);
                continue;
            }
            if (cl == -INFINITY) {
                fprintf(w->f, " MI BND       %s\n", nm);
            } else if (cl != 0.0 || cu < 0.0) {
                wr_num(num, cl);
                fprintf(w->f, " LO BND       %-9s %s\n", nm, num);
            }
            if (cu != INFINITY) {
                wr_num(num, cu);
                fprintf(w->f, " UP BND       %-9s %s\n", nm, num);
            }
        }

        fprintf(w->f, "ENDATA\n");
    }

    free(type);
    free(rhs);
    free(rng);
    return wr_close(w, path, prev, cloc);
}

/* --------------------------------------------------------------------- */
/* LP                                                                    */
/* --------------------------------------------------------------------- */

/* The reader wraps expressions freely, so the width is chosen for whoever
 * opens the file rather than for the parser. */
constexpr int LP_WRAP = 72;

static void lp_term(wr *w, int *col, bool *first, double coef,
                    const char *name)
{
    char num[NUM_LEN];
    wr_num(num, fabs(coef));
    int n;
    if (*first) {
        n = fprintf(w->f, " %s%s %s", coef < 0.0 ? "-" : "", num, name);
        *first = false;
    } else {
        n = fprintf(w->f, " %s %s %s", coef < 0.0 ? "-" : "+", num, name);
    }
    *col += n > 0 ? n : 0;
    if (*col >= LP_WRAP) {
        fprintf(w->f, "\n   ");
        *col = 3;
    }
}

jaos_status jaos_write_lp(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    /* The dialect is written row by row and the model is held column by
     * column, so the row-wise mirror is what makes this possible at all. */
    jaos_status rs = jm_model_ensure_rowwise(m);
    if (rs != JAOS_OK) {
        jm_set_err(m, "out of memory building the row-wise copy");
        return rs;
    }

    /* Nothing is opened until every check below has passed, for the reason
     * jaos_write_mps gives. */
    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;

    char nm[NAME_LEN], rn[NAME_LEN], num[NUM_LEN];

    /* Three things this dialect cannot say, all checked before the file is
     * opened. docs/format-support.md lists them; jaos_write_mps has none of
     * them, which is why every message here points at it. */
    for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++) {
        const double rl = m->row_lower[i], ru = m->row_upper[i];
        row_name(m, rn, i);
        if (rl == -INFINITY && ru == INFINITY)
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' is free, which LP format cannot express; write "
                    "MPS instead", rn);
        else if (!isfinite(rl == -INFINITY ? ru : rl))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' has a bound at an infinity LP format cannot "
                    "express", rn);
        else if (m->ar_start[i] == m->ar_start[i + 1] && m->num_col == 0)
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' has no coefficients and the model has no "
                    "columns to write a zero term against; write MPS "
                    "instead", rn);
    }
    for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
        col_name(m, nm, j);
        if (m->col_lower[j] == INFINITY || m->col_upper[j] == -INFINITY)
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' has a bound at an infinity LP format cannot "
                    "express", nm);
    }

    /* The names: two the same, as every writer refuses, and one the LP
     * scanner would not read back as one token -- a name starting with a
     * digit, holding a `-` or a `:`, or spelling a keyword. MPS takes
     * every name this library holds, which is why the message points
     * there. The objective's name is a label like any other. */
    if (w->st == JAOS_OK)
        names_unique(w);
    for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
        col_name(m, nm, j);
        if (!jm_lp_name_ok(nm))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' has a name LP format cannot spell; write "
                    "MPS instead", nm);
    }
    for (int64_t i = -1; w->st == JAOS_OK && i < m->num_row; i++) {
        if (i >= 0)
            row_name(m, rn, i);
        if (!jm_lp_name_ok(i < 0 ? jm_obj_name(m) : rn))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "%s '%s' has a name LP format cannot spell; write MPS "
                    "instead", i < 0 ? "the objective" : "row",
                    i < 0 ? jm_obj_name(m) : rn);
    }

    locale_t prev = (locale_t)0, cloc = (locale_t)0;
    if (w->st != JAOS_OK || !wr_open(w, path, &prev, &cloc))
        return w->st;

    {
        fprintf(w->f, "\\ written by JAOS %s\n", JAOS_VERSION_STRING);
        fprintf(w->f, "%s\n",
                m->sense == JAOS_MAXIMIZE ? "Maximize" : "Minimize");

        /* Every column, including the ones costing nothing, and in index
         * order. LP format has no COLUMNS section: the reader numbers a
         * column where its name FIRST appears in the token stream, so
         * listing only the costed ones here would renumber every other
         * column by where its first coefficient happens to sit. That is
         * silent: the file is valid and describes a different model. Most of
         * the gate did exactly that before this loop stopped skipping, and
         * bench/measurements/02-138/lpcover.txt owns the count (D226). A
         * zero term is also what lets LP name a column that appears in no
         * row at all. */
        fprintf(w->f, " %s:", jm_obj_name(m));
        int col = (int)strlen(jm_obj_name(m)) + 2;
        bool first = true;
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            lp_term(w, &col, &first, m->col_cost[j], nm);
        }
        if (m->obj_offset != 0.0) {
            wr_num(num, fabs(m->obj_offset));
            const char *sign = m->obj_offset < 0.0 ? "-" : (first ? "" : "+");
            fprintf(w->f, first ? " %s%s" : " %s %s", sign, num);
        }
        fprintf(w->f, "\n");

        fprintf(w->f, "Subject To\n");
        for (int64_t i = 0; i < m->num_row; i++) {
            const double rl = m->row_lower[i], ru = m->row_upper[i];
            /* Ranged: two finite ends that differ. Written as the two-sided
             * form, whose left bound sits between the label and the terms. */
            const bool ranged = rl != ru && rl != -INFINITY && ru != INFINITY;
            char lonum[NUM_LEN];
            row_name(m, rn, i);
            fprintf(w->f, " %s:", rn);
            col = (int)strlen(rn) + 2;
            if (ranged) {
                wr_num(lonum, rl);
                fprintf(w->f, " %s <=", lonum);
                col += (int)strlen(lonum) + 4;
            }
            first = true;
            for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                col_name(m, nm, m->ar_index[k]);
                lp_term(w, &col, &first, m->ar_value[k], nm);
            }
            if (m->ar_start[i] == m->ar_start[i + 1]) {
                /* A row with no coefficients. LP has no form for a
                 * constraint with an empty body, but a term whose
                 * coefficient is zero is an ordinary term, and the reader
                 * drops explicit zeros on the way back in (`model.c` keeps
                 * the matrix free of them), so `0 x1 >= 5` round-trips to
                 * the empty row it came from. Column 0 every time: a fixed
                 * rule, not a choice the data can influence.
                 *
                 * This was refused as unwritable until D276 measured the
                 * round trip. It is 34 of the 35 gate instances the LP
                 * writer used to turn away. */
                col_name(m, nm, 0);
                lp_term(w, &col, &first, 0.0, nm);
            }
            if (ranged) {
                wr_num(num, ru);
                fprintf(w->f, " <= %s\n", num);
            } else {
                const char *rel =
                    rl == ru ? "=" : (rl == -INFINITY ? "<=" : ">=");
                wr_num(num, rl == -INFINITY ? ru : rl);
                fprintf(w->f, " %s %s\n", rel, num);
            }
        }

        /* One statement per column is all the reader needs: later
         * statements override earlier ones component-wise. */
        fprintf(w->f, "Bounds\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            const double cl = m->col_lower[j], cu = m->col_upper[j];
            col_name(m, nm, j);
            if (cl == 0.0 && cu == INFINITY)
                continue;
            if (cl == -INFINITY && cu == INFINITY) {
                fprintf(w->f, " %s free\n", nm);
            } else if (cl == cu) {
                wr_num(num, cl);
                fprintf(w->f, " %s = %s\n", nm, num);
            } else if (cl == -INFINITY) {
                wr_num(num, cu);
                fprintf(w->f, " -inf <= %s <= %s\n", nm, num);
            } else if (cu == INFINITY) {
                wr_num(num, cl);
                fprintf(w->f, " %s >= %s\n", nm, num);
            } else {
                char lo[NUM_LEN];
                wr_num(lo, cl);
                wr_num(num, cu);
                fprintf(w->f, " %s <= %s <= %s\n", lo, nm, num);
            }
        }

        fprintf(w->f, "End\n");
    }

    return wr_close(w, path, prev, cloc);
}

/* --------------------------------------------------------------------- */
/* Solution                                                              */
/* --------------------------------------------------------------------- */

static const char *basis_word(jaos_basis_status s)
{
    switch (s) {
    case JAOS_BASIS_BASIC:    return "basic";
    case JAOS_BASIS_AT_LOWER: return "lower";
    case JAOS_BASIS_AT_UPPER: return "upper";
    case JAOS_BASIS_FREE:     return "free";
    }
    return "unknown";
}

/* The three words a file's status line may carry, and the three the reader
 * accepts: the outcomes that carry an answer, `optimal` with a point and a
 * basis and the other two with a certificate (D285). */
static const char *status_word(jaos_solve_status s)
{
    switch (s) {
    case JAOS_SOLVE_OPTIMAL:    return "optimal";
    case JAOS_SOLVE_INFEASIBLE: return "infeasible";
    case JAOS_SOLVE_UNBOUNDED:  return "unbounded";
    default:                    return nullptr;
    }
}

jaos_status jaos_write_solution(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    /* What the last solve left to write down: an optimum with its point
     * and basis, or a certificate. The rule jaos_solution, jaos_certificate
     * and jaos_unbounded_ray apply, for their reason: a solve that left
     * none of the three has nothing to write, and a file of zeros does not
     * read as missing. */
    const jaos_solve_status ss = m->solve_status;
    const bool optimal = ss == JAOS_SOLVE_OPTIMAL && m->sol_col != nullptr &&
        m->sol_col_status != nullptr && m->sol_redcost != nullptr &&
        m->sol_row != nullptr && m->sol_dual != nullptr &&
        m->sol_row_status != nullptr;
    const bool infeasible = ss == JAOS_SOLVE_INFEASIBLE && m->farkas_ok &&
        m->sol_farkas != nullptr;
    const bool unbounded = ss == JAOS_SOLVE_UNBOUNDED && m->ray_ok &&
        m->sol_ray != nullptr;
    if (!optimal && !infeasible && !unbounded) {
        if (ss == JAOS_SOLVE_INFEASIBLE || ss == JAOS_SOLVE_UNBOUNDED)
            jm_set_err(m, "the last solve is '%s' and left no certificate "
                       "to write", jaos_solve_status_str(ss));
        else
            jm_set_err(m, "nothing to write: the last solve is '%s'",
                       jaos_solve_status_str(ss));
        return JAOS_ERR_INVALID_INPUT;
    }

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;
    char nm[NAME_LEN], a[NUM_LEN], b[NUM_LEN];

    /* wr_num's caller guarantees a finite value. The two model writers
     * discharge that from the model's own invariants: every setter and
     * every loader rejects a non-finite cost, bound or coefficient. The
     * answer arrays carry no such invariant. `jm_objective_value`
     * publishes a non-finite objective deliberately when the sum overflows,
     * and a model whose bounds reach 1e300 solves to OPTIMAL with an
     * infinity or a NaN in any of the four arrays; a certificate is a
     * vector the solve computed and is checked the same way.
     *
     * Writing one would abort wr_num's assert on a build that has asserts,
     * and print a libc-dependent "nan" or "-nan" on one that does not,
     * which is a file this project's first rule says must not exist. So it
     * is refused by name, before anything is opened (D226). */
    if (optimal) {
        if (!isfinite(m->objective))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "the objective is not finite, so there is no solution to "
                    "write down");
        for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
            col_name(m, nm, j);
            if (!isfinite(m->sol_col[j]) || !isfinite(m->sol_redcost[j]))
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "column '%s' has a value or a reduced cost that is "
                        "not finite", nm);
        }
        for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++) {
            row_name(m, nm, i);
            if (!isfinite(m->sol_row[i]) || !isfinite(m->sol_dual[i]))
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "row '%s' has an activity or a dual that is not "
                        "finite", nm);
        }
    } else if (infeasible) {
        for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++) {
            row_name(m, nm, i);
            if (!isfinite(m->sol_farkas[i]))
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "row '%s' has a certificate entry that is not "
                        "finite", nm);
        }
    } else {
        for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
            col_name(m, nm, j);
            if (!isfinite(m->sol_ray[j]))
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "column '%s' has a ray entry that is not finite",
                        nm);
        }
    }
    /* Records are positional and the reader would take a repeated name in
     * its stride, so this refusal is for the person reading the file, and
     * for one rule across the three writers. */
    if (w->st == JAOS_OK)
        names_unique(w);
    if (w->st != JAOS_OK)
        return w->st;

    locale_t prev = (locale_t)0, cloc = (locale_t)0;
    if (!wr_open(w, path, &prev, &cloc))
        return w->st;

    fprintf(w->f, "# JAOS solution file, format 1\n");
    fprintf(w->f, "# written by JAOS %s\n", JAOS_VERSION_STRING);
    fprintf(w->f, "status %s\n", status_word(ss));
    if (optimal) {
        wr_num(a, m->objective);
        fprintf(w->f, "objective %s\n", a);
    }
    fprintf(w->f, "columns %" PRId64 "\n", m->num_col);
    fprintf(w->f, "rows %" PRId64 "\n", m->num_row);

    if (optimal) {
        fprintf(w->f, "# col <name> <value> <reduced cost> <status>\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            wr_num(a, m->sol_col[j]);
            wr_num(b, m->sol_redcost[j]);
            fprintf(w->f, "col %s %s %s %s\n", nm, a, b,
                    basis_word(m->sol_col_status[j]));
        }
        fprintf(w->f, "# row <name> <activity> <dual> <status>\n");
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(m, nm, i);
            wr_num(a, m->sol_row[i]);
            wr_num(b, m->sol_dual[i]);
            fprintf(w->f, "row %s %s %s %s\n", nm, a, b,
                    basis_word(m->sol_row_status[i]));
        }
    } else if (infeasible) {
        /* The Farkas certificate, one entry per row: jaos_certificate's
         * vector, which jaos_check_certificate judges from the model
         * alone. */
        fprintf(w->f, "# ray <row name> <multiplier>\n");
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(m, nm, i);
            wr_num(a, m->sol_farkas[i]);
            fprintf(w->f, "ray %s %s\n", nm, a);
        }
    } else {
        /* The unbounded ray, one entry per column: jaos_unbounded_ray's
         * vector, which jaos_check_ray judges from the model alone. */
        fprintf(w->f, "# ray <column name> <direction>\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            wr_num(a, m->sol_ray[j]);
            fprintf(w->f, "ray %s %s\n", nm, a);
        }
    }

    fprintf(w->f, "end\n");
    return wr_close(w, path, prev, cloc);
}

/* --------------------------------------------------------------------- */
/* The solution reader                                                    */
/* --------------------------------------------------------------------- */

/* The inverse of `basis_word`. Returns false on a word that is not one of
 * the four. */
static bool basis_of_word(const char *w, jaos_basis_status *out)
{
    if (strcmp(w, "basic") == 0)      { *out = JAOS_BASIS_BASIC;    return true; }
    if (strcmp(w, "lower") == 0)      { *out = JAOS_BASIS_AT_LOWER; return true; }
    if (strcmp(w, "upper") == 0)      { *out = JAOS_BASIS_AT_UPPER; return true; }
    if (strcmp(w, "free") == 0)       { *out = JAOS_BASIS_FREE;     return true; }
    return false;
}

/* The inverse of `status_word`. */
static bool status_of_word(const char *w, jaos_solve_status *out)
{
    if (strcmp(w, "optimal") == 0)    { *out = JAOS_SOLVE_OPTIMAL;    return true; }
    if (strcmp(w, "infeasible") == 0) { *out = JAOS_SOLVE_INFEASIBLE; return true; }
    if (strcmp(w, "unbounded") == 0)  { *out = JAOS_SOLVE_UNBOUNDED;  return true; }
    return false;
}

/* One finite number, parsed under the caller's already-installed "C"
 * locale. Rejects what `strtod` leaves behind, so `1.5x` is an error and
 * not 1.5, and rejects an infinity or a NaN: the writer refuses to write
 * one (D226), so a file holding one was not written by this library. */
static bool rd_num(const char *tok, double *out)
{
    char *end = nullptr;
    errno = 0;
    const double v = strtod(tok, &end);
    if (end == tok || *end != '\0' || !isfinite(v))
        return false;
    *out = v;
    return true;
}

/* Where a read puts what it finds. Every pointer is optional. */
typedef struct {
    jaos_solve_status status;
    double objective;
    double *col_value, *col_dual;
    jaos_basis_status *col_status;
    double *row_activity, *row_dual;
    jaos_basis_status *row_status;
    double *row_ray;         /* on infeasible, num_row entries */
    double *col_ray;         /* on unbounded, num_col entries  */
} sol_read;

/* The one reader behind the three public calls: the whole file, every
 * record checked against the model and against the status the file
 * declared, so a `col` record in an infeasible file or a `ray` record in
 * an optimal one is refused rather than taken. `want_optimal` is +1 when
 * the caller can only take an optimum, -1 when only a certificate, and 0
 * for either; the refusal names the call that reads the other kind. */
static jaos_status read_solution_file(jaos_model *m, const char *path,
                                      int want_optimal, sol_read *o)
{
    FILE *f = fopen(path, "r");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s' for reading", path);
        return JAOS_ERR_IO;
    }

    /* The same locale rule the model readers follow, and for the same
     * reason: a host application under a comma-decimal locale would read
     * "1.5" as 1. */
    locale_t cloc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    locale_t prev = cloc ? uselocale(cloc) : (locale_t)0;

    jaos_status st = JAOS_OK;
    char *line = nullptr;
    size_t lsz = 0;
    int64_t lno = 0, ncol = -1, nrow = -1, seen_col = 0, seen_row = 0,
            seen_ray = 0;
    bool have_status = false, have_obj = false, ended = false;
    jaos_solve_status ss = JAOS_SOLVE_NOT_RUN;
    double obj = 0.0;
    char nm[NAME_LEN];

#define RD_FAIL(...)  do { st = JAOS_ERR_INVALID_INPUT; \
    jm_set_err(m, __VA_ARGS__); goto done; } while (0)

    while (getline(&line, &lsz, f) >= 0) {
        lno++;
        if (ended)
            RD_FAIL("line %" PRId64 ": content after 'end'", lno);

        char *tok[8];
        int nt = 0;
        for (char *p = strtok(line, " \t\r\n");
             p != nullptr && nt < 8; p = strtok(nullptr, " \t\r\n"))
            tok[nt++] = p;
        if (nt == 0 || tok[0][0] == '#')
            continue;   /* blank, or one of the writer's comment lines */

        if (strcmp(tok[0], "status") == 0) {
            if (nt != 2)
                RD_FAIL("line %" PRId64 ": 'status' takes one word", lno);
            if (have_status)
                RD_FAIL("line %" PRId64 ": a second 'status' line", lno);
            /* Only the three outcomes with something to write are ever
             * written (D226, D285), so only those are read. A file saying
             * anything else was not written by this library. */
            if (!status_of_word(tok[1], &ss))
                RD_FAIL("line %" PRId64 ": status is '%s'; only 'optimal', "
                        "'infeasible' and 'unbounded' are written and read",
                        lno, tok[1]);
            if (want_optimal > 0 && ss != JAOS_SOLVE_OPTIMAL)
                RD_FAIL("line %" PRId64 ": status is '%s', and this call "
                        "reads an optimum; jaos_read_certificate reads a "
                        "certificate", lno, tok[1]);
            if (want_optimal < 0 && ss == JAOS_SOLVE_OPTIMAL)
                RD_FAIL("line %" PRId64 ": status is 'optimal', and this "
                        "call reads a certificate; jaos_read_solution reads "
                        "an optimum", lno);
            have_status = true;
        } else if (strcmp(tok[0], "objective") == 0) {
            if (!have_status)
                RD_FAIL("line %" PRId64 ": 'objective' before 'status'", lno);
            if (ss != JAOS_SOLVE_OPTIMAL)
                RD_FAIL("line %" PRId64 ": an 'objective' in a file whose "
                        "status is '%s'", lno, status_word(ss));
            if (nt != 2 || !rd_num(tok[1], &obj))
                RD_FAIL("line %" PRId64 ": 'objective' takes one finite "
                        "number", lno);
            have_obj = true;
        } else if (strcmp(tok[0], "columns") == 0 ||
                   strcmp(tok[0], "rows") == 0) {
            const bool is_col = tok[0][0] == 'c';
            if (nt != 2)
                RD_FAIL("line %" PRId64 ": '%s' takes one count", lno, tok[0]);
            char *end = nullptr;
            errno = 0;
            const long long v = strtoll(tok[1], &end, 10);
            if (end == tok[1] || *end != '\0' || errno != 0 || v < 0)
                RD_FAIL("line %" PRId64 ": '%s' is not a count", lno, tok[1]);
            /* The model decides the shape. A file from another model is
             * refused here rather than read into the wrong arrays. */
            const int64_t want = is_col ? m->num_col : m->num_row;
            if ((int64_t)v != want)
                RD_FAIL("line %" PRId64 ": the file has %lld %s and this "
                        "model has %" PRId64, lno, v, tok[0], want);
            if (is_col)
                ncol = (int64_t)v;
            else
                nrow = (int64_t)v;
        } else if (strcmp(tok[0], "col") == 0 ||
                   strcmp(tok[0], "row") == 0) {
            const bool is_col = tok[0][0] == 'c';
            if (!have_status)
                RD_FAIL("line %" PRId64 ": a record before 'status'", lno);
            if (ss != JAOS_SOLVE_OPTIMAL)
                RD_FAIL("line %" PRId64 ": a '%s' record in a file whose "
                        "status is '%s'", lno, tok[0], status_word(ss));
            if (ncol < 0 || nrow < 0)
                RD_FAIL("line %" PRId64 ": a record before both counts", lno);
            if (nt != 5)
                RD_FAIL("line %" PRId64 ": a '%s' record takes a name, two "
                        "numbers and a status", lno, tok[0]);
            const int64_t k = is_col ? seen_col : seen_row;
            const int64_t lim = is_col ? ncol : nrow;
            if (k >= lim)
                RD_FAIL("line %" PRId64 ": more '%s' records than the count "
                        "says", lno, tok[0]);
            /* Records are positional; the name is checked, not searched.
             * It must be the name this model gives that index -- its own
             * or the positional one (D284) -- so a mismatch means the file
             * describes a different model, or this one renamed since. */
            if (is_col)
                col_name(m, nm, k);
            else
                row_name(m, nm, k);
            if (strcmp(tok[1], nm) != 0)
                RD_FAIL("line %" PRId64 ": expected '%s' here and the file "
                        "says '%s'; records are in index order and named "
                        "as the model names them", lno, nm, tok[1]);
            double v1, v2;
            jaos_basis_status bs;
            if (!rd_num(tok[2], &v1))
                RD_FAIL("line %" PRId64 ": '%s' is not a finite number", lno,
                        tok[2]);
            if (!rd_num(tok[3], &v2))
                RD_FAIL("line %" PRId64 ": '%s' is not a finite number", lno,
                        tok[3]);
            if (!basis_of_word(tok[4], &bs))
                RD_FAIL("line %" PRId64 ": '%s' is not a basis status", lno,
                        tok[4]);
            if (is_col) {
                if (o->col_value != nullptr)  o->col_value[k] = v1;
                if (o->col_dual != nullptr)   o->col_dual[k] = v2;
                if (o->col_status != nullptr) o->col_status[k] = bs;
                seen_col++;
            } else {
                if (o->row_activity != nullptr) o->row_activity[k] = v1;
                if (o->row_dual != nullptr)     o->row_dual[k] = v2;
                if (o->row_status != nullptr)   o->row_status[k] = bs;
                seen_row++;
            }
        } else if (strcmp(tok[0], "ray") == 0) {
            /* A certificate entry: over the rows of an infeasible file,
             * over the columns of an unbounded one, in index order and
             * under the model's names, the same rule as the records
             * above. */
            if (!have_status)
                RD_FAIL("line %" PRId64 ": a record before 'status'", lno);
            if (ss == JAOS_SOLVE_OPTIMAL)
                RD_FAIL("line %" PRId64 ": a 'ray' record in a file whose "
                        "status is 'optimal'", lno);
            if (ncol < 0 || nrow < 0)
                RD_FAIL("line %" PRId64 ": a record before both counts", lno);
            if (nt != 3)
                RD_FAIL("line %" PRId64 ": a 'ray' record takes a name and "
                        "a number", lno);
            const bool over_rows = ss == JAOS_SOLVE_INFEASIBLE;
            const int64_t lim = over_rows ? nrow : ncol;
            if (seen_ray >= lim)
                RD_FAIL("line %" PRId64 ": more 'ray' records than the count "
                        "says", lno);
            if (over_rows)
                row_name(m, nm, seen_ray);
            else
                col_name(m, nm, seen_ray);
            if (strcmp(tok[1], nm) != 0)
                RD_FAIL("line %" PRId64 ": expected '%s' here and the file "
                        "says '%s'; records are in index order and named "
                        "as the model names them", lno, nm, tok[1]);
            double v;
            if (!rd_num(tok[2], &v))
                RD_FAIL("line %" PRId64 ": '%s' is not a finite number", lno,
                        tok[2]);
            if (over_rows) {
                if (o->row_ray != nullptr) o->row_ray[seen_ray] = v;
            } else {
                if (o->col_ray != nullptr) o->col_ray[seen_ray] = v;
            }
            seen_ray++;
        } else if (strcmp(tok[0], "end") == 0) {
            if (nt != 1)
                RD_FAIL("line %" PRId64 ": 'end' takes nothing", lno);
            ended = true;
        } else {
            RD_FAIL("line %" PRId64 ": unknown record '%s'", lno, tok[0]);
        }
    }

    if (!ended)
        RD_FAIL("the file ends without 'end'");
    if (!have_status)
        RD_FAIL("no 'status' line");
    if (ncol < 0 || nrow < 0)
        RD_FAIL("the file gives no column or row count");
    if (ss == JAOS_SOLVE_OPTIMAL) {
        if (!have_obj)
            RD_FAIL("no 'objective' line");
        if (seen_col != ncol)
            RD_FAIL("the file says %" PRId64 " columns and carries %" PRId64,
                    ncol, seen_col);
        if (seen_row != nrow)
            RD_FAIL("the file says %" PRId64 " rows and carries %" PRId64,
                    nrow, seen_row);
    } else {
        const int64_t lim = ss == JAOS_SOLVE_INFEASIBLE ? nrow : ncol;
        if (seen_ray != lim)
            RD_FAIL("the file says %" PRId64 " %s and its certificate "
                    "carries %" PRId64, lim,
                    ss == JAOS_SOLVE_INFEASIBLE ? "rows" : "columns",
                    seen_ray);
    }

    o->status = ss;
    o->objective = obj;
    jm_set_err(m, "%s", "");

#undef RD_FAIL
done:
    free(line);
    fclose(f);
    if (cloc) {
        uselocale(prev ? prev : LC_GLOBAL_LOCALE);
        freelocale(cloc);
    }
    return st;
}

jaos_status jaos_read_solution(jaos_model *m, const char *path,
    double *objective,
    double *col_value, double *col_dual, jaos_basis_status *col_status,
    double *row_activity, double *row_dual, jaos_basis_status *row_status)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    sol_read o = {.col_value = col_value, .col_dual = col_dual,
                  .col_status = col_status, .row_activity = row_activity,
                  .row_dual = row_dual, .row_status = row_status};
    const jaos_status st = read_solution_file(m, path, +1, &o);
    if (st == JAOS_OK && objective != nullptr)
        *objective = o.objective;
    return st;
}

jaos_status jaos_read_certificate(jaos_model *m, const char *path,
                                  jaos_solve_status *status,
                                  double *row_ray, double *col_ray)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    sol_read o = {.row_ray = row_ray, .col_ray = col_ray};
    const jaos_status st = read_solution_file(m, path, -1, &o);
    if (st == JAOS_OK && status != nullptr)
        *status = o.status;
    return st;
}

jaos_status jaos_solution_file_status(jaos_model *m, const char *path,
                                      jaos_solve_status *status)
{
    if (m == nullptr || path == nullptr || status == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    sol_read o = {0};
    const jaos_status st = read_solution_file(m, path, 0, &o);
    if (st == JAOS_OK)
        *status = o.status;
    return st;
}
