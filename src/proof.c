#include "jaos.h"
#include "jaos_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PROOF_LINE 4096

static char *rational_of_double(double v)
{
    jm_rational r;
    if (!jm_rational_from_double(&r, v))
        return nullptr;
    return jm_rational_decimal(&r);
}

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

    const jaos_solve_status ss = m->solve_status;
    const bool infeasible = ss == JAOS_SOLVE_INFEASIBLE && m->farkas_ok &&
        m->sol_farkas != nullptr;
    const bool unbounded = ss == JAOS_SOLVE_UNBOUNDED && m->ray_ok &&
        m->sol_ray != nullptr;
    if (infeasible && m->exact_farkas == nullptr) {
        jaos_exact_ray_report rr;
        (void)jaos_exact_certificate(m, &rr);
        m->err[0] = 0;
    }
    if (unbounded && m->exact_uray == nullptr) {
        jaos_exact_ray_report rr;
        (void)jaos_exact_unbounded_ray(m, &rr);
        m->err[0] = 0;
    }
    if (!infeasible && !unbounded) {

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

        fprintf(f, "# ray <row name> <exact multiplier>\n");
        if (m->exact_farkas != nullptr) {
            for (int64_t i = 0; i < m->num_row; i++) {
                if (jaos_row_name(m, i, nm, sizeof nm) != JAOS_OK)
                    goto io_error;
                fprintf(f, "ray %s %s\n", nm, m->exact_farkas[i]);
            }
        } else if (!write_ray(f, m, m->sol_farkas, true)) {
            goto io_error;
        }
    } else if (unbounded) {

        fprintf(f, "# ray <column name> <exact direction>\n");
        if (m->exact_uray != nullptr) {
            for (int64_t j = 0; j < m->num_col; j++) {
                if (jaos_col_name(m, j, nm, sizeof nm) != JAOS_OK)
                    goto io_error;
                fprintf(f, "ray %s %s\n", nm, m->exact_uray[j]);
            }
        } else if (!write_ray(f, m, m->sol_ray, false)) {
            goto io_error;
        }
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
    {
        jaos_proof_report pr;
        const jaos_status ck = jaos_check_proof(m, path, &pr);
        if (ck != JAOS_OK) {
            remove(path);
            return ck;
        }
        if (!pr.certified) {
            remove(path);
            jm_set_err(m, "no proof written: the numbers this answer carries "
                          "do not hold exactly, and the exact ones outgrew "
                          "the limb budget, so a file of them would say "
                          "broken to its own checker");
            return JAOS_ERR_INVALID_INPUT;
        }
    }
    return JAOS_OK;

io_error:
    fclose(f);
    remove(path);
    jm_set_err(m, "cannot write '%s'", path);
    return JAOS_ERR_IO;
}

#define RQ(expr) do { if (!(expr)) goto no_limbs; } while (0)

static int required_sign(const jm_rational *v, double lo, double hi,
                         const jm_rational *rlo, const jm_rational *rhi)
{
    const bool at_lo = lo > -INFINITY && jm_rational_cmp(v, rlo) == 0;
    const bool at_hi = hi < INFINITY && jm_rational_cmp(v, rhi) == 0;
    if (at_lo && at_hi)
        return 2;
    if (at_lo)
        return 1;
    if (at_hi)
        return -1;
    return 0;
}

static bool within(const jm_rational *v, double lo, double hi,
                   const jm_rational *rlo, const jm_rational *rhi)
{
    if (lo > -INFINITY && jm_rational_cmp(v, rlo) < 0)
        return false;
    if (hi < INFINITY && jm_rational_cmp(v, rhi) > 0)
        return false;
    return true;
}

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

    if (jm_model_ensure_rowwise(m) != JAOS_OK)
        goto done;

    const bool maximize = m->sense == JAOS_MAXIMIZE;
    jm_rational acc, term, a, b, lo, hi;
    jm_rational_set_zero(&lo);
    jm_rational_set_zero(&hi);

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
            out->certified = rate < 0;
        }
        out->terms = terms;
        rc = JAOS_OK;
        goto done;
    }

    bool primal = true;
    for (int64_t j = 0; j < nc; j++) {

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
