/* The exact optimality proof, written to a file and judged from the model
 * alone (D325).
 *
 * D285 gave an infeasible and an unbounded answer a certificate that can
 * leave the process that found it. An optimum had no such thing: the
 * solution file carries a point and duals judged to a tolerance, and the
 * exact rational proof `jaos_verify` computes stayed inside the model.
 * This file is that proof on disk.
 *
 * What it holds is the coordinates: every column's exact value and every
 * row's exact dual, as decimal rationals, plus the exact objective. What
 * it does NOT hold is a basis, and that is deliberate. The checker never
 * reads one. It reads the model and the file and re-derives the three
 * conditions that make a point optimal for a linear program:
 *
 *   1. the point is primal feasible -- every column inside its bounds and
 *      every row activity inside its own,
 *   2. the duals are dual feasible -- every reduced cost points into the
 *      model from the side the point rests on,
 *   3. the two are complementary -- a row or column strictly inside its
 *      bounds carries a zero multiplier.
 *
 * Together those three are sufficient, so a file that passes is proved
 * optimal and not merely consistent with a basis somebody else chose.
 * Every comparison is over the rationals, so nothing here has a tolerance
 * and there is no bar to argue about.
 *
 * The one thing that can stop it is the limb budget: a product or a sum
 * that does not fit in JM_EXACT_LIMBS ends the check as
 * JAOS_ERR_NUMERICAL, which is the honest "cannot judge" and not a
 * verdict. That is the same ceiling the proof that wrote the file ran
 * under (D273, D274).
 *
 * The file has no decimal point anywhere -- every number is an integer or
 * a ratio of two -- so unlike every other reader and writer here it needs
 * no locale handling at all. */

#include "jaos.h"
#include "jaos_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* A line of the file: the longest record is `col <name> <value> <status>`,
 * and a value is at most two magnitudes and a slash. */
#define PROOF_LINE 4096

/* ------------------------------------------------------------------ */
/* The writer                                                          */
/* ------------------------------------------------------------------ */

/* A double as the exact rational it already is, in the same decimal-ratio
 * spelling jm_rational_decimal gives an exact value (D328). The caller
 * frees it; nullptr is out of limbs or out of memory, and every finite
 * double fits, so in practice it is out of memory. */
static char *rational_of_double(double v)
{
    jm_rational r;
    if (!jm_rational_from_double(&r, v))
        return nullptr;
    return jm_rational_decimal(&r);
}

/* One `ray` record per row or per column, the vector written as exact
 * rationals. Returns false on an I/O or memory failure. */
static bool write_ray(FILE *f, const jaos_model *m, const double *v,
                      bool per_row)
{
    char nm[JAOS_NAME_MAX + 1];
    const int64_t n = per_row ? m->num_row : m->num_col;
    for (int64_t k = 0; k < n; k++) {
        const jaos_status st = per_row ? jaos_row_name(m, k, nm, sizeof nm)
                                       : jaos_col_name(m, k, nm, sizeof nm);
        if (st != JAOS_OK)
            return false;
        char *d = rational_of_double(v[k]);
        if (d == nullptr)
            return false;
        fprintf(f, "ray %s %s\n", nm, d);
        free(d);
    }
    return true;
}

jaos_status jaos_write_proof(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    /* Which of the three the last solve left. An optimum's proof is its
     * coordinates and needs a jaos_verify; a certificate is a vector the
     * solve already published, and every double in it is exact, so it
     * needs no verify at all (D328). */
    const jaos_solve_status ss = m->solve_status;
    const bool infeasible = ss == JAOS_SOLVE_INFEASIBLE && m->farkas_ok &&
        m->sol_farkas != nullptr;
    const bool unbounded = ss == JAOS_SOLVE_UNBOUNDED && m->ray_ok &&
        m->sol_ray != nullptr;
    if (!infeasible && !unbounded) {
        /* The proof is what jaos_verify left, under the same rule the
         * exact getters apply: no proof, no file. A file of zeros does not
         * read as missing, so it is refused by name instead. */
        if (m->exact_col == nullptr || m->exact_dual == nullptr) {
            jm_set_err(m, "no exact proof to write: call jaos_verify and get "
                          "JAOS_PROOF_OPTIMAL first, or solve to an "
                          "INFEASIBLE or UNBOUNDED answer with a certificate");
            return JAOS_ERR_INVALID_INPUT;
        }
        if (m->exact_obj == nullptr) {
            jm_set_err(m, "the proof has values but no objective: its sum "
                          "outgrew the limb budget, and a proof file without "
                          "one cannot be checked");
            return JAOS_ERR_INVALID_INPUT;
        }
    }

    FILE *f = fopen(path, "w");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s' for writing", path);
        return JAOS_ERR_IO;
    }
    char nm[JAOS_NAME_MAX + 1];

    fprintf(f, "# JAOS proof file, format 1\n");
    fprintf(f, "# written by JAOS %s\n", JAOS_VERSION_STRING);
    fprintf(f, "# every number is an integer or a ratio of two, exactly\n");
    fprintf(f, "proof %s\n", infeasible ? "infeasible"
                             : unbounded ? "unbounded" : "optimal");
    fprintf(f, "sense %s\n", m->sense == JAOS_MAXIMIZE ? "max" : "min");
    fprintf(f, "columns %" PRId64 "\n", m->num_col);
    fprintf(f, "rows %" PRId64 "\n", m->num_row);
    if (infeasible) {
        /* The Farkas multipliers, one per row: the same vector
         * jaos_certificate publishes, spelled exactly. */
        fprintf(f, "# ray <row name> <exact multiplier>\n");
        if (!write_ray(f, m, m->sol_farkas, true))
            goto io_error;
    } else if (unbounded) {
        fprintf(f, "# ray <column name> <exact direction>\n");
        if (!write_ray(f, m, m->sol_ray, false))
            goto io_error;
    } else {
        fprintf(f, "objective %s\n", m->exact_obj);
        fprintf(f, "# col <name> <exact value>\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            if (jaos_col_name(m, j, nm, sizeof nm) != JAOS_OK)
                goto io_error;
            fprintf(f, "col %s %s\n", nm, m->exact_col[j]);
        }
        fprintf(f, "# row <name> <exact dual>\n");
        for (int64_t i = 0; i < m->num_row; i++) {
            if (jaos_row_name(m, i, nm, sizeof nm) != JAOS_OK)
                goto io_error;
            fprintf(f, "row %s %s\n", nm, m->exact_dual[i]);
        }
    }
    fprintf(f, "end\n");

    if (ferror(f) != 0)
        goto io_error;
    if (fclose(f) != 0) {
        jm_set_err(m, "cannot finish writing '%s'", path);
        remove(path);
        return JAOS_ERR_IO;
    }
    return JAOS_OK;

io_error:
    fclose(f);
    remove(path);
    jm_set_err(m, "cannot write '%s'", path);
    return JAOS_ERR_IO;
}

/* ------------------------------------------------------------------ */
/* The checker                                                         */
/* ------------------------------------------------------------------ */

/* Every rational operation the walk makes goes through these two, so a
 * budget failure is caught once and turns the whole check into
 * JAOS_ERR_NUMERICAL instead of a verdict. */
#define RQ(expr) do { if (!(expr)) goto no_limbs; } while (0)

/* The sign a multiplier must have, given where its own quantity rests
 * between its bounds, in minimize form. Returns -1 for "must be <= 0",
 * +1 for "must be >= 0", 0 for "must be 0" and 2 for "any sign", which is
 * what a fixed pair of bounds allows. Both bounds are the model's own
 * doubles and the value is exact, so every comparison here is exact. */
static int required_sign(const jm_rational *v, double lo, double hi,
                         const jm_rational *rlo, const jm_rational *rhi)
{
    const bool at_lo = lo > -INFINITY && jm_rational_cmp(v, rlo) == 0;
    const bool at_hi = hi < INFINITY && jm_rational_cmp(v, rhi) == 0;
    if (at_lo && at_hi)
        return 2;                /* fixed: any multiplier is admissible */
    if (at_lo)
        return 1;
    if (at_hi)
        return -1;
    return 0;                    /* strictly inside, or free */
}

/* Whether a quantity sits inside its bounds. Exact. */
static bool within(const jm_rational *v, double lo, double hi,
                   const jm_rational *rlo, const jm_rational *rhi)
{
    if (lo > -INFINITY && jm_rational_cmp(v, rlo) < 0)
        return false;
    if (hi < INFINITY && jm_rational_cmp(v, rhi) > 0)
        return false;
    return true;
}

/* One whitespace-delimited token, advancing `*p`. Returns nullptr at the
 * end of the line. */
static char *tok(char **p)
{
    char *s = *p;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '\0' || *s == '\n' || *s == '\r')
        return nullptr;
    char *e = s;
    while (*e != '\0' && *e != ' ' && *e != '\t' && *e != '\n' && *e != '\r')
        e++;
    if (*e != '\0') {
        *e = '\0';
        e++;
    }
    *p = e;
    return s;
}

jaos_status jaos_check_proof(jaos_model *m, const char *path,
                             jaos_proof_report *out)
{
    if (m == nullptr || path == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    const int64_t nc = m->num_col, nr = m->num_row;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    FILE *f = nullptr;
    jm_rational *x = nullptr, *y = nullptr;
    bool *seen_col = nullptr, *seen_row = nullptr;
    jm_rational claimed;
    bool have_obj = false;
    int64_t terms = 0;

    *out = (jaos_proof_report){ .primal = false, .dual = false,
                                .objective = false, .bad_row = -1,
                                .bad_col = -1, .terms = 0,
                                .kind = JAOS_PROOF_FILE_OPTIMAL,
                                .certified = false };
    jaos_proof_kind kind = JAOS_PROOF_FILE_OPTIMAL;

    x = calloc((size_t)(nc > 0 ? nc : 1), sizeof *x);
    y = calloc((size_t)(nr > 0 ? nr : 1), sizeof *y);
    seen_col = calloc((size_t)(nc > 0 ? nc : 1), sizeof *seen_col);
    seen_row = calloc((size_t)(nr > 0 ? nr : 1), sizeof *seen_row);
    if (x == nullptr || y == nullptr || seen_col == nullptr ||
        seen_row == nullptr)
        goto done;

    f = fopen(path, "r");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s'", path);
        rc = JAOS_ERR_IO;
        goto done;
    }

    char line[PROOF_LINE];
    int64_t saw_cols = -1, saw_rows = -1, lineno = 0;
    bool saw_proof = false, saw_end = false;
    while (fgets(line, sizeof line, f) != nullptr) {
        lineno++;
        char *p = line;
        char *k = tok(&p);
        if (k == nullptr || k[0] == '#')
            continue;
        if (strcmp(k, "proof") == 0) {
            const char *w = tok(&p);
            if (w != nullptr && strcmp(w, "optimal") == 0) {
                kind = JAOS_PROOF_FILE_OPTIMAL;
            } else if (w != nullptr && strcmp(w, "infeasible") == 0) {
                kind = JAOS_PROOF_FILE_INFEASIBLE;
            } else if (w != nullptr && strcmp(w, "unbounded") == 0) {
                kind = JAOS_PROOF_FILE_UNBOUNDED;
            } else {
                jm_set_err(m, "%s:%lld: a proof file proves an optimum, an "
                              "infeasibility or an unboundedness, and nothing "
                              "else", path, (long long)lineno);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            out->kind = kind;
            saw_proof = true;
        } else if (strcmp(k, "sense") == 0) {
            const char *w = tok(&p);
            const bool want_max = m->sense == JAOS_MAXIMIZE;
            if (w == nullptr ||
                (strcmp(w, "max") != 0 && strcmp(w, "min") != 0) ||
                (strcmp(w, "max") == 0) != want_max) {
                jm_set_err(m, "%s:%lld: the file's objective sense is not "
                              "this model's", path, (long long)lineno);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
        } else if (strcmp(k, "columns") == 0 || strcmp(k, "rows") == 0) {
            const char *w = tok(&p);
            char *e = nullptr;
            const long long v = w ? strtoll(w, &e, 10) : -1;
            if (w == nullptr || *e != '\0' || v < 0) {
                jm_set_err(m, "%s:%lld: '%s' needs a count", path,
                           (long long)lineno, k);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            if (k[0] == 'c')
                saw_cols = (int64_t)v;
            else
                saw_rows = (int64_t)v;
        } else if (strcmp(k, "objective") == 0) {
            const char *w = tok(&p);
            if (w == nullptr || !jm_rational_from_decimal(&claimed, w)) {
                jm_set_err(m, "%s:%lld: the objective is not an exact "
                              "rational", path, (long long)lineno);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            have_obj = true;
        } else if (strcmp(k, "col") == 0 || strcmp(k, "row") == 0) {
            if (kind != JAOS_PROOF_FILE_OPTIMAL) {
                jm_set_err(m, "%s:%lld: a '%s' record belongs to an optimum's "
                              "proof, and this file claims something else",
                           path, (long long)lineno, k);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            const bool is_col = k[0] == 'c';
            char *nm = tok(&p);
            char *val = tok(&p);
            int64_t at = -1;
            if (nm == nullptr || val == nullptr ||
                (is_col ? jaos_col_index(m, nm, &at)
                        : jaos_row_index(m, nm, &at)) != JAOS_OK) {
                jm_set_err(m, "%s:%lld: no %s of this model is named '%s'",
                           path, (long long)lineno, is_col ? "column" : "row",
                           nm ? nm : "");
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            bool *seen = is_col ? &seen_col[at] : &seen_row[at];
            if (*seen) {
                jm_set_err(m, "%s:%lld: '%s' appears twice", path,
                           (long long)lineno, nm);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            if (!jm_rational_from_decimal(is_col ? &x[at] : &y[at], val)) {
                jm_set_err(m, "%s:%lld: '%s' is not an exact rational this "
                              "build can hold", path, (long long)lineno, val);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            *seen = true;
        } else if (strcmp(k, "ray") == 0) {
            /* A certificate's own record (D328): one per row for an
             * infeasibility, one per column for an unboundedness. It goes
             * into the same two arrays -- `y` carries the Farkas
             * multipliers, `x` the ray's direction -- so the parser needs
             * no third one. */
            if (!saw_proof || kind == JAOS_PROOF_FILE_OPTIMAL) {
                jm_set_err(m, "%s:%lld: a 'ray' record needs a file that "
                              "claims an infeasibility or an unboundedness",
                           path, (long long)lineno);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            const bool per_row = kind == JAOS_PROOF_FILE_INFEASIBLE;
            char *nm = tok(&p);
            char *val = tok(&p);
            int64_t at = -1;
            if (nm == nullptr || val == nullptr ||
                (per_row ? jaos_row_index(m, nm, &at)
                         : jaos_col_index(m, nm, &at)) != JAOS_OK) {
                jm_set_err(m, "%s:%lld: no %s of this model is named '%s'",
                           path, (long long)lineno, per_row ? "row" : "column",
                           nm ? nm : "");
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            bool *seen = per_row ? &seen_row[at] : &seen_col[at];
            if (*seen) {
                jm_set_err(m, "%s:%lld: '%s' appears twice", path,
                           (long long)lineno, nm);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            if (!jm_rational_from_decimal(per_row ? &y[at] : &x[at], val)) {
                jm_set_err(m, "%s:%lld: '%s' is not an exact rational this "
                              "build can hold", path, (long long)lineno, val);
                rc = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            *seen = true;
        } else if (strcmp(k, "end") == 0) {
            saw_end = true;
        } else {
            jm_set_err(m, "%s:%lld: '%s' is not a proof record", path,
                       (long long)lineno, k);
            rc = JAOS_ERR_INVALID_INPUT;
            goto done;
        }
    }
    if (ferror(f) != 0) {
        jm_set_err(m, "cannot read '%s'", path);
        rc = JAOS_ERR_IO;
        goto done;
    }
    if (!saw_proof || !saw_end ||
        (kind == JAOS_PROOF_FILE_OPTIMAL && !have_obj)) {
        jm_set_err(m, "'%s' is not a complete proof file", path);
        rc = JAOS_ERR_INVALID_INPUT;
        goto done;
    }
    if (kind != JAOS_PROOF_FILE_OPTIMAL && have_obj) {
        jm_set_err(m, "'%s' claims a certificate and carries an objective; a "
                      "certificate proves that no answer exists, not what one "
                      "is", path);
        rc = JAOS_ERR_INVALID_INPUT;
        goto done;
    }
    if (saw_cols != nc || saw_rows != nr) {
        jm_set_err(m, "'%s' is a proof for %lld columns and %lld rows, and "
                      "this model has %lld and %lld", path,
                   (long long)saw_cols, (long long)saw_rows, (long long)nc,
                   (long long)nr);
        rc = JAOS_ERR_INVALID_INPUT;
        goto done;
    }
    /* Which half must be complete depends on the claim: an optimum needs
     * both, a Farkas certificate the rows alone, a ray the columns alone.
     * The half a claim does not use stays at zero, which is what the
     * checks below read for a multiplier that is not there. */
    const bool need_col = kind != JAOS_PROOF_FILE_INFEASIBLE;
    const bool need_row = kind != JAOS_PROOF_FILE_UNBOUNDED;
    for (int64_t j = 0; need_col && j < nc; j++)
        if (!seen_col[j]) {
            jm_set_err(m, "'%s' names nothing for column %lld", path,
                       (long long)j);
            rc = JAOS_ERR_INVALID_INPUT;
            goto done;
        }
    for (int64_t i = 0; need_row && i < nr; i++)
        if (!seen_row[i]) {
            jm_set_err(m, "'%s' names nothing for row %lld", path,
                       (long long)i);
            rc = JAOS_ERR_INVALID_INPUT;
            goto done;
        }

    /* Everything parsed. From here the model decides, and nothing the file
     * said about a basis or a status is read, because none was written. */
    if (jm_model_ensure_rowwise(m) != JAOS_OK)
        goto done;

    const bool maximize = m->sense == JAOS_MAXIMIZE;
    jm_rational acc, term, a, b, lo, hi;
    jm_rational_set_zero(&lo);
    jm_rational_set_zero(&hi);

    /* A Farkas certificate, exactly (D328). y is admissible when every
     * column's (A'y)_j has a finite bound on the side it points at and
     * every row's y_i has one on its own side; then the supremum of y'Ax
     * over the box and the infimum of y'(row activity) over the row
     * bounds are both finite, and y proves the model infeasible exactly
     * when the second is STRICTLY above the first.
     *
     * There is no tolerance here and so no near miss.
     * jaos_check_certificate skips a term below its own traffic, because
     * a sum of doubles cannot place a zero more finely; this walk cannot,
     * and a multiplier that is a rounding away from zero on a column with
     * no bound on that side makes the supremum infinite and refuses the
     * file. The two checkers can disagree, and this one is the strict
     * one. */
    if (kind == JAOS_PROOF_FILE_INFEASIBLE) {
        jm_rational sup, inf;
        jm_rational_set_zero(&sup);
        jm_rational_set_zero(&inf);
        bool bounded = true;
        for (int64_t j = 0; bounded && j < nc; j++) {
            jm_rational_set_zero(&acc);
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                const int64_t i = m->a_index[k];
                if (jm_rational_is_zero(&y[i]))
                    continue;
                RQ(jm_rational_from_double(&a, m->a_value[k]));
                RQ(jm_rational_mul(&term, &a, &y[i]));
                RQ(jm_rational_add(&acc, &acc, &term));
                terms++;
            }
            const int32_t sg = jm_rational_sign(&acc);
            if (sg == 0)
                continue;
            const double bnd = sg > 0 ? m->col_upper[j] : m->col_lower[j];
            if (!isfinite(bnd)) {
                bounded = false;
                out->bad_col = j;
                break;
            }
            RQ(jm_rational_from_double(&a, bnd));
            RQ(jm_rational_mul(&term, &acc, &a));
            RQ(jm_rational_add(&sup, &sup, &term));
        }
        for (int64_t i = 0; bounded && i < nr; i++) {
            const int32_t sg = jm_rational_sign(&y[i]);
            if (sg == 0)
                continue;
            const double bnd = sg > 0 ? m->row_lower[i] : m->row_upper[i];
            if (!isfinite(bnd)) {
                bounded = false;
                out->bad_row = i;
                break;
            }
            RQ(jm_rational_from_double(&a, bnd));
            RQ(jm_rational_mul(&term, &y[i], &a));
            RQ(jm_rational_add(&inf, &inf, &term));
        }
        if (bounded) {
            RQ(jm_rational_sub(&b, &inf, &sup));
            out->certified = jm_rational_sign(&b) > 0;
        }
        out->terms = terms;
        rc = JAOS_OK;
        goto done;
    }

    /* An unbounded ray, exactly (D328). d is admissible when no column
     * moves toward a finite bound and no row activity does either, and it
     * proves the model unbounded exactly when c'd improves the objective
     * in the model's own sense -- strictly, since a rate of zero is a
     * direction that goes nowhere. */
    if (kind == JAOS_PROOF_FILE_UNBOUNDED) {
        bool escapes = false;
        for (int64_t j = 0; j < nc; j++) {
            const int32_t sg = jm_rational_sign(&x[j]);
            if (sg == 0)
                continue;
            const double bnd = sg > 0 ? m->col_upper[j] : m->col_lower[j];
            if (isfinite(bnd)) {
                escapes = true;
                out->bad_col = j;
                break;
            }
        }
        for (int64_t i = 0; !escapes && i < nr; i++) {
            jm_rational_set_zero(&acc);
            for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                const int64_t j = m->ar_index[k];
                if (jm_rational_is_zero(&x[j]))
                    continue;
                RQ(jm_rational_from_double(&a, m->ar_value[k]));
                RQ(jm_rational_mul(&term, &a, &x[j]));
                RQ(jm_rational_add(&acc, &acc, &term));
                terms++;
            }
            const int32_t sg = jm_rational_sign(&acc);
            if (sg == 0)
                continue;
            const double bnd = sg > 0 ? m->row_upper[i] : m->row_lower[i];
            if (isfinite(bnd)) {
                escapes = true;
                out->bad_row = i;
                break;
            }
        }
        if (!escapes) {
            jm_rational_set_zero(&acc);
            for (int64_t j = 0; j < nc; j++) {
                if (m->col_cost[j] == 0.0 || jm_rational_is_zero(&x[j]))
                    continue;
                RQ(jm_rational_from_double(&a, m->col_cost[j]));
                RQ(jm_rational_mul(&term, &a, &x[j]));
                RQ(jm_rational_add(&acc, &acc, &term));
                terms++;
            }
            int32_t rate = jm_rational_sign(&acc);
            if (maximize)
                rate = -rate;
            out->certified = rate < 0;   /* the minimize form improves */
        }
        out->terms = terms;
        rc = JAOS_OK;
        goto done;
    }

    /* 1. The columns, inside their own bounds. */
    bool primal = true;
    for (int64_t j = 0; j < nc; j++) {
        /* An infinite bound is never converted: jm_rational holds no
         * infinity, and `within` and `required_sign` read the double
         * first and the rational only where it is finite. */
        if (isfinite(m->col_lower[j]))
            RQ(jm_rational_from_double(&lo, m->col_lower[j]));
        if (isfinite(m->col_upper[j]))
            RQ(jm_rational_from_double(&hi, m->col_upper[j]));
        if (!within(&x[j], m->col_lower[j], m->col_upper[j], &lo, &hi)) {
            primal = false;
            if (out->bad_col < 0)
                out->bad_col = j;
            break;
        }
    }

    /* 2. The rows: the activity is exact, and so is the comparison. */
    for (int64_t i = 0; primal && i < nr; i++) {
        jm_rational_set_zero(&acc);
        for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
            const int64_t j = m->ar_index[k];
            if (jm_rational_is_zero(&x[j]))
                continue;
            RQ(jm_rational_from_double(&a, m->ar_value[k]));
            RQ(jm_rational_mul(&term, &a, &x[j]));
            RQ(jm_rational_add(&acc, &acc, &term));
            terms++;
        }
        if (isfinite(m->row_lower[i]))
            RQ(jm_rational_from_double(&lo, m->row_lower[i]));
        if (isfinite(m->row_upper[i]))
            RQ(jm_rational_from_double(&hi, m->row_upper[i]));
        if (!within(&acc, m->row_lower[i], m->row_upper[i], &lo, &hi)) {
            primal = false;
            out->bad_row = i;
            break;
        }
        /* 3. The row's own multiplier, judged by where the activity
         * actually rests: at the lower bound it may only push up, at the
         * upper only down, and strictly inside it must be zero. That is
         * dual feasibility and complementary slackness in one test, and it
         * needs no basis status to make it. */
        const int want = required_sign(&acc, m->row_lower[i],
                                       m->row_upper[i], &lo, &hi);
        if (want != 2) {
            int32_t sg = jm_rational_sign(&y[i]);
            if (maximize)
                sg = -sg;
            if ((want == 0 && sg != 0) || (want == 1 && sg < 0) ||
                (want == -1 && sg > 0)) {
                out->bad_row = i;
                goto dual_failed;
            }
        }
    }

    /* 4. The reduced costs: d_j = c_j - sum_i a_ij y_i, over the column's
     * own entries, and the same test against where x_j rests. */
    for (int64_t j = 0; j < nc; j++) {
        RQ(jm_rational_from_double(&acc, m->col_cost[j]));
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (jm_rational_is_zero(&y[i]))
                continue;
            RQ(jm_rational_from_double(&a, m->a_value[k]));
            RQ(jm_rational_mul(&term, &a, &y[i]));
            RQ(jm_rational_sub(&acc, &acc, &term));
            terms++;
        }
        if (isfinite(m->col_lower[j]))
            RQ(jm_rational_from_double(&lo, m->col_lower[j]));
        if (isfinite(m->col_upper[j]))
            RQ(jm_rational_from_double(&hi, m->col_upper[j]));
        const int want = required_sign(&x[j], m->col_lower[j],
                                       m->col_upper[j], &lo, &hi);
        if (want == 2)
            continue;
        int32_t sg = jm_rational_sign(&acc);
        if (maximize)
            sg = -sg;
        if ((want == 0 && sg != 0) || (want == 1 && sg < 0) ||
            (want == -1 && sg > 0)) {
            if (out->bad_col < 0)
                out->bad_col = j;
            goto dual_failed;
        }
    }
    out->dual = true;
dual_failed:
    out->primal = primal;

    /* 5. The objective the file claims is the one the point has:
     * c'x + c0, over the model as loaded, in the model's own sense --
     * the same value jaos_objective reports. */
    RQ(jm_rational_from_double(&acc, m->obj_offset));
    for (int64_t j = 0; j < nc; j++) {
        if (m->col_cost[j] == 0.0 || jm_rational_is_zero(&x[j]))
            continue;
        RQ(jm_rational_from_double(&a, m->col_cost[j]));
        RQ(jm_rational_mul(&term, &a, &x[j]));
        RQ(jm_rational_add(&acc, &acc, &term));
        terms++;
    }
    RQ(jm_rational_sub(&b, &acc, &claimed));
    out->objective = jm_rational_is_zero(&b);
    out->certified = out->primal && out->dual && out->objective;
    out->terms = terms;
    rc = JAOS_OK;
    goto done;

no_limbs:
    out->terms = terms;
    jm_set_err(m, "the check ran out of exact arithmetic after %lld "
                  "products; rebuild with a larger -DJM_EXACT_LIMBS to "
                  "judge this file", (long long)terms);
    rc = JAOS_ERR_NUMERICAL;

done:
    if (f != nullptr)
        fclose(f);
    free(x);
    free(y);
    free(seen_col);
    free(seen_row);
    return rc;
}
