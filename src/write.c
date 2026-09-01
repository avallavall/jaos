/* MPS, LP and solution writers.
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
 * The model holds no names. It is indices from the moment it is loaded, and
 * a reader's names are gone by then. So the writer generates one name per
 * index: `C1..Cn` for columns, `R1..Rm` for rows, `COST` for the objective.
 * That round-trips because both formats list rows and columns in index
 * order and both readers assign indices in the order names appear. `COST`
 * cannot collide with a generated column name, which is always `C`
 * followed by digits.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _POSIX_C_SOURCE 200809L

#include "jaos_internal.h"

#include <assert.h>
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

/* One prefix character, at most 19 digits of int64, a terminator, rounded
 * up to something a reader of this file does not have to check. */
constexpr int NAME_LEN = 24;
constexpr int NUM_LEN = 32;

static const char *OBJ_NAME = "COST";

static void col_name(char *buf, int64_t j)
{
    snprintf(buf, NAME_LEN, "C%" PRId64, j + 1);
}

static void row_name(char *buf, int64_t i)
{
    snprintf(buf, NAME_LEN, "R%" PRId64, i + 1);
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
    row_name(name, i);
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
            col_name(nm, j);
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' has a bound at an infinity MPS cannot "
                    "express", nm);
        }
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
        fprintf(w->f, " N  %s\n", OBJ_NAME);
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(rn, i);
            fprintf(w->f, " %c  %s\n", type[i], rn);
        }

        /* Column entries must be contiguous, and every column must appear
         * or the round trip loses the ones with no coefficients. A column
         * with nothing to say gets its objective entry written anyway. */
        fprintf(w->f, "COLUMNS\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(nm, j);
            const int64_t beg = m->a_start[j], end = m->a_start[j + 1];
            int pending = 0;
            if (m->col_cost[j] != 0.0 || beg == end) {
                wr_num(num, m->col_cost[j]);
                fprintf(w->f, "    %-9s %-9s %s", nm, OBJ_NAME, num);
                pending = 1;
            }
            for (int64_t k = beg; k < end; k++) {
                row_name(rn, m->a_index[k]);
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
            fprintf(w->f, "    RHS       %-9s %s\n", OBJ_NAME, num);
        }
        for (int64_t i = 0; i < m->num_row; i++) {
            if (type[i] == 'N' || rhs[i] == 0.0)
                continue;      /* a row never named in RHS defaults to 0 */
            row_name(rn, i);
            wr_num(num, rhs[i]);
            fprintf(w->f, "    RHS       %-9s %s\n", rn, num);
        }

        fprintf(w->f, "RANGES\n");
        for (int64_t i = 0; i < m->num_row; i++) {
            if (isnan(rng[i]))
                continue;
            row_name(rn, i);
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
            col_name(nm, j);
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
        row_name(rn, i);
        if (rl == -INFINITY && ru == INFINITY)
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' is free, which LP format cannot express; write "
                    "MPS instead", rn);
        else if (!isfinite(rl == -INFINITY ? ru : rl))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' has a bound at an infinity LP format cannot "
                    "express", rn);
        else if (m->ar_start[i] == m->ar_start[i + 1])
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' has no coefficients, which LP format cannot "
                    "express; write MPS instead", rn);
    }
    for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
        col_name(nm, j);
        if (m->col_lower[j] == INFINITY || m->col_upper[j] == -INFINITY)
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' has a bound at an infinity LP format cannot "
                    "express", nm);
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
        fprintf(w->f, " obj:");
        int col = 5;
        bool first = true;
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(nm, j);
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
            row_name(rn, i);
            fprintf(w->f, " %s:", rn);
            col = (int)strlen(rn) + 2;
            if (ranged) {
                wr_num(lonum, rl);
                fprintf(w->f, " %s <=", lonum);
                col += (int)strlen(lonum) + 4;
            }
            first = true;
            for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                col_name(nm, m->ar_index[k]);
                lp_term(w, &col, &first, m->ar_value[k], nm);
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
            col_name(nm, j);
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

jaos_status jaos_write_solution(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    /* The rule jaos_solution and jaos_basis apply, for their reason: a
     * solve that found no optimum has no solution to write down, and a file
     * of zeros does not read as missing. */
    if (m->solve_status != JAOS_SOLVE_OPTIMAL || m->sol_col == nullptr ||
        m->sol_col_status == nullptr || m->sol_redcost == nullptr ||
        m->sol_row == nullptr || m->sol_dual == nullptr ||
        m->sol_row_status == nullptr) {
        jm_set_err(m, "no optimal solution to write: the last solve is '%s'",
                   jaos_solve_status_str(m->solve_status));
        return JAOS_ERR_INVALID_INPUT;
    }

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;
    char nm[NAME_LEN], a[NUM_LEN], b[NUM_LEN];

    /* wr_num's caller guarantees a finite value. The two model writers
     * discharge that from the model's own invariants: every setter and
     * every loader rejects a non-finite cost, bound or coefficient. The
     * solution arrays carry no such invariant. `jm_objective_value`
     * publishes a non-finite objective deliberately when the sum overflows,
     * and a model whose bounds reach 1e300 solves to OPTIMAL with an
     * infinity or a NaN in any of the four arrays.
     *
     * Writing one would abort wr_num's assert on a build that has asserts,
     * and print a libc-dependent "nan" or "-nan" on one that does not,
     * which is a file this project's first rule says must not exist. So it
     * is refused by name, before anything is opened (D226). */
    if (!isfinite(m->objective))
        wr_fail(w, JAOS_ERR_INVALID_INPUT,
                "the objective is not finite, so there is no solution to "
                "write down");
    for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
        col_name(nm, j);
        if (!isfinite(m->sol_col[j]) || !isfinite(m->sol_redcost[j]))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' has a value or a reduced cost that is not "
                    "finite", nm);
    }
    for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++) {
        row_name(nm, i);
        if (!isfinite(m->sol_row[i]) || !isfinite(m->sol_dual[i]))
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' has an activity or a dual that is not finite",
                    nm);
    }
    if (w->st != JAOS_OK)
        return w->st;

    locale_t prev = (locale_t)0, cloc = (locale_t)0;
    if (!wr_open(w, path, &prev, &cloc))
        return w->st;

    fprintf(w->f, "# JAOS solution file, format 1\n");
    fprintf(w->f, "# written by JAOS %s\n", JAOS_VERSION_STRING);
    fprintf(w->f, "status optimal\n");
    wr_num(a, m->objective);
    fprintf(w->f, "objective %s\n", a);
    fprintf(w->f, "columns %" PRId64 "\n", m->num_col);
    fprintf(w->f, "rows %" PRId64 "\n", m->num_row);

    fprintf(w->f, "# col <name> <value> <reduced cost> <status>\n");
    for (int64_t j = 0; j < m->num_col; j++) {
        col_name(nm, j);
        wr_num(a, m->sol_col[j]);
        wr_num(b, m->sol_redcost[j]);
        fprintf(w->f, "col %s %s %s %s\n", nm, a, b,
                basis_word(m->sol_col_status[j]));
    }

    fprintf(w->f, "# row <name> <activity> <dual> <status>\n");
    for (int64_t i = 0; i < m->num_row; i++) {
        row_name(nm, i);
        wr_num(a, m->sol_row[i]);
        wr_num(b, m->sol_dual[i]);
        fprintf(w->f, "row %s %s %s %s\n", nm, a, b,
                basis_word(m->sol_row_status[i]));
    }

    fprintf(w->f, "end\n");
    return wr_close(w, path, prev, cloc);
}
