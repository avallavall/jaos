/* SPDX-License-Identifier: Apache-2.0 */

#include "jaos_internal.h"
#include "jaos_sys.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr int NAME_LEN = JAOS_NAME_MAX + 1;
constexpr int NUM_LEN = 32;

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

static void wr_num(char *buf, double v)
{
    for (int prec = 15; prec <= 16; prec++) {
        snprintf(buf, NUM_LEN, "%.*g", prec, v);
        if (strtod(buf, nullptr) == v)
            return;
    }
    snprintf(buf, NUM_LEN, "%.17g", v);

    assert(strtod(buf, nullptr) == v);
}

typedef struct {
    FILE *f;
    jaos_model *m;
    jaos_status st;

    bool gz;
    jm_memstream ms;
} wr;

static bool path_is_gz(const char *path)
{
    const size_t n = strlen(path);
    return n >= 3 && strcmp(path + n - 3, ".gz") == 0;
}

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

static bool wr_open(wr *w, const char *path, jm_locale *loc)
{
    if (!jm_locale_c_enter(loc)) {
        wr_fail(w, JAOS_ERR_IO,
                "cannot install the C locale needed to write '%s'", path);
        return false;
    }

    w->gz = path_is_gz(path);
    if (w->gz)
        w->f = jm_memstream_open(&w->ms) ? w->ms.f : nullptr;
    else
        w->f = fopen(path, "w");
    if (w->f == nullptr) {
        jm_locale_leave(loc);
        wr_fail(w, JAOS_ERR_IO, "cannot open '%s' for writing", path);
        return false;
    }
    return true;
}

static jaos_status wr_close(wr *w, const char *path, jm_locale *loc)
{
    if (ferror(w->f))
        wr_fail(w, JAOS_ERR_IO, "writing '%s' failed", path);
    if (w->gz ? !jm_memstream_close(&w->ms) : fclose(w->f) != 0)
        wr_fail(w, JAOS_ERR_IO, "closing '%s' failed", path);
    jm_locale_leave(loc);

    if (w->gz && w->st == JAOS_OK) {
        char *packed = nullptr;
        int64_t packed_n = 0;
        if (!jm_gzip(w->ms.buf != nullptr ? w->ms.buf : "",
                     (int64_t)w->ms.len, &packed, &packed_n)) {
            wr_fail(w, JAOS_ERR_OUT_OF_MEMORY,
                    "out of memory compressing '%s'", path);
        } else {
            FILE *out = fopen(path, "wb");
            if (out == nullptr) {
                wr_fail(w, JAOS_ERR_IO, "cannot open '%s' for writing", path);
            } else {
                const size_t wrote = packed_n > 0
                    ? fwrite(packed, 1, (size_t)packed_n, out) : 0;
                if (wrote != (size_t)packed_n)
                    wr_fail(w, JAOS_ERR_IO, "writing '%s' failed", path);
                if (fclose(out) != 0)
                    wr_fail(w, JAOS_ERR_IO, "closing '%s' failed", path);
            }
            free(packed);
        }
    }
    free(w->ms.buf);
    w->ms.buf = nullptr;

    if (w->st != JAOS_OK)
        remove(path);
    else
        w->m->err[0] = '\0';
    return w->st;
}

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

static bool range_form(double rl, double ru, char *type, double *rhs,
                       double *rng)
{
    double d = ru - rl;
    if (!isfinite(d))
        return false;
    if (rl + fabs(d) == ru) {
        *type = 'G';
        *rhs = rl;
        *rng = d;
        return true;
    }
    if (ru - fabs(d) == rl) {
        *type = 'L';
        *rhs = ru;
        *rng = d;
        return true;
    }
    return false;
}

static void mps_row_kind(wr *w, int64_t i, char *type, double *rhs,
                         double *rng)
{
    const double rl = w->m->row_lower[i], ru = w->m->row_upper[i];
    char name[NAME_LEN];
    row_name(w->m, name, i);

    *type = 'N';
    *rng = NAN;
    *rhs = 0.0;

    if (rl == -INFINITY && ru == INFINITY) {
        *type = 'N';
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

    wr_fail(w, JAOS_ERR_INVALID_INPUT,
            "row '%s' has its lower bound above its upper bound, which MPS "
            "cannot express", name);
}

jaos_status jaos_write_mps(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;
    const char *obj = jm_obj_name(m);

    char *type = jm_alloc_array(m->num_row, sizeof(char));
    double *rhs = jm_alloc_array(m->num_row, sizeof(double));
    double *rng = jm_alloc_array(m->num_row, sizeof(double));
    if (type == nullptr || rhs == nullptr || rng == nullptr)
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");

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

    jm_locale loc = {0};
    if (w->st != JAOS_OK || !wr_open(w, path, &loc)) {
        free(type);
        free(rhs);
        free(rng);
        return w->st;
    }

    {
        fprintf(w->f, "* written by JAOS %s\n", JAOS_VERSION_STRING);
        fprintf(w->f, "NAME          %s\n",
                m->model_name != nullptr ? m->model_name : "JAOS");
        if (m->sense == JAOS_MAXIMIZE)
            fprintf(w->f, "OBJSENSE      MAX\n");

        fprintf(w->f, "ROWS\n");
        fprintf(w->f, " N  %s\n", obj);
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(m, rn, i);
            fprintf(w->f, " %c  %s\n", type[i], rn);
        }

        fprintf(w->f, "COLUMNS\n");
        bool in_int = false;
        for (int64_t j = 0; j < m->num_col; j++) {

            const bool is_int = m->col_integer != nullptr && m->col_integer[j];
            if (is_int != in_int) {
                fprintf(w->f, "    MARKER    'MARKER'   %s\n",
                        is_int ? "'INTORG'" : "'INTEND'");
                in_int = is_int;
            }
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
        if (in_int)
            fprintf(w->f, "    MARKER    'MARKER'   'INTEND'\n");

        fprintf(w->f, "RHS\n");
        if (m->obj_offset != 0.0) {
            wr_num(num, -m->obj_offset);
            fprintf(w->f, "    RHS       %-9s %s\n", obj, num);
        }
        for (int64_t i = 0; i < m->num_row; i++) {
            if (type[i] == 'N' || rhs[i] == 0.0)
                continue;
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

        fprintf(w->f, "BOUNDS\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            const double cl = m->col_lower[j], cu = m->col_upper[j];
            col_name(m, nm, j);
            if (m->col_semi != nullptr && m->col_semi[j]) {
                if (cu == INFINITY) {
                    wr_fail(w, JAOS_ERR_INVALID_INPUT,
                            "column %s is semi-continuous with no upper "
                            "bound, which the MPS SC bound cannot express",
                            nm);
                    break;
                }
                if (cl == -INFINITY)
                    fprintf(w->f, " MI BND       %s\n", nm);
                else if (cl != 0.0) {
                    wr_num(num, cl);
                    fprintf(w->f, " LO BND       %-9s %s\n", nm, num);
                }
                wr_num(num, cu);
                fprintf(w->f, " %s BND       %-9s %s\n",
                        m->col_integer != nullptr && m->col_integer[j]
                            ? "SI" : "SC", nm, num);
                continue;
            }
            if (cl == 0.0 && cu == INFINITY)
                continue;
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

        if (m->num_sos > 0) {
            fprintf(w->f, "SOS\n");
            for (int64_t k = 0; k < m->num_sos; k++) {
                fprintf(w->f, " S%d SOS       SOS%lld\n", m->sos_type[k],
                        (long long)(k + 1));
                for (int64_t t = m->sos_start[k]; t < m->sos_start[k + 1]; t++) {
                    col_name(m, nm, m->sos_col[t]);
                    wr_num(num, m->sos_weight[t]);
                    fprintf(w->f, "    %-9s %s\n", nm, num);
                }
            }
        }
        if (m->row_ind_col != nullptr) {
            bool any = false;
            for (int64_t i = 0; i < m->num_row; i++)
                any |= m->row_ind_col[i] >= 0;
            if (any) {
                fprintf(w->f, "INDICATORS\n");
                for (int64_t i = 0; i < m->num_row; i++) {
                    if (m->row_ind_col[i] < 0)
                        continue;
                    row_name(m, rn, i);
                    col_name(m, nm, m->row_ind_col[i]);
                    fprintf(w->f, " IF %-9s %-9s %d\n", rn, nm,
                            m->row_ind_val[i]);
                }
            }
        }

        fprintf(w->f, "ENDATA\n");
    }

    free(type);
    free(rhs);
    free(rng);
    return wr_close(w, path, &loc);
}

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

static void lp_comment_name(FILE *f, const char *s)
{
    for (; *s; s++)
        fputc((unsigned char)*s < 0x20 ? '?' : *s, f);
}

static void lp_write_map(wr *w, const jaos_model *orig)
{
    const jaos_model *m = w->m;
    char a[NAME_LEN], b[NAME_LEN];
    if (strcmp(jm_obj_name(orig), jm_obj_name(m)) != 0) {
        fprintf(w->f, "\\ objective %s was ", jm_obj_name(m));
        lp_comment_name(w->f, jm_obj_name(orig));
        fputc('\n', w->f);
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        col_name(orig, a, j);
        col_name(m, b, j);
        if (strcmp(a, b) == 0)
            continue;
        fprintf(w->f, "\\ column %s was ", b);
        lp_comment_name(w->f, a);
        fputc('\n', w->f);
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        row_name(orig, a, i);
        row_name(m, b, i);
        if (strcmp(a, b) == 0)
            continue;
        fprintf(w->f, "\\ row %s was ", b);
        lp_comment_name(w->f, a);
        fputc('\n', w->f);
    }
}

static jaos_status lp_substitute(jm_nmap *taken, const char *base, char *buf)
{
    snprintf(buf, NAME_LEN, "%s", base);
    int64_t v;
    while (jm_nmap_get(taken, buf, &v)) {
        const size_t n = strlen(buf);
        if (n + 1 > (size_t)JAOS_NAME_MAX)
            return JAOS_ERR_INVALID_INPUT;
        buf[n] = '_';
        buf[n + 1] = '\0';
    }
    return jm_nmap_insert(taken, buf, 0) ? JAOS_OK : JAOS_ERR_OUT_OF_MEMORY;
}

static jaos_status lp_spell_names(const jaos_model *m, jaos_model *c)
{
    jm_nmap taken = {0};
    char nm[NAME_LEN], sub[NAME_LEN], base[NAME_LEN];
    jaos_status st = JAOS_OK;
    if (jm_lp_name_ok(jm_obj_name(m)) &&
        !jm_nmap_insert(&taken, jm_obj_name(m), 0))
        st = JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++) {
        col_name(m, nm, j);
        if (jm_lp_name_ok(nm) && !jm_nmap_insert(&taken, nm, 0))
            st = JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t i = 0; st == JAOS_OK && i < m->num_row; i++) {
        row_name(m, nm, i);
        if (jm_lp_name_ok(nm) && !jm_nmap_insert(&taken, nm, 0))
            st = JAOS_ERR_OUT_OF_MEMORY;
    }
    if (st == JAOS_OK && !jm_lp_name_ok(jm_obj_name(m))) {
        st = lp_substitute(&taken, "obj", sub);
        if (st == JAOS_OK)
            st = jaos_set_objective_name(c, sub);
    }
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++) {
        col_name(m, nm, j);
        if (jm_lp_name_ok(nm))
            continue;
        snprintf(base, sizeof base, "c%" PRId64, j + 1);
        st = lp_substitute(&taken, base, sub);
        if (st == JAOS_OK)
            st = jaos_set_col_name(c, j, sub);
    }
    for (int64_t i = 0; st == JAOS_OK && i < m->num_row; i++) {
        row_name(m, nm, i);
        if (jm_lp_name_ok(nm))
            continue;
        snprintf(base, sizeof base, "r%" PRId64, i + 1);
        st = lp_substitute(&taken, base, sub);
        if (st == JAOS_OK)
            st = jaos_set_row_name(c, i, sub);
    }
    jm_nmap_free(&taken);
    if (st != JAOS_OK)
        jm_set_err(c, st == JAOS_ERR_OUT_OF_MEMORY
                          ? "out of memory"
                          : "no spelled name is free for a column or row LP "
                            "format cannot name");
    return st;
}

static jaos_status lp_write_body(wr *w, const char *path,
                                 const jaos_model *orig)
{
    const jaos_model *m = w->m;
    char nm[NAME_LEN], rn[NAME_LEN], num[NUM_LEN];
    jm_locale loc = {0};
    if (w->st != JAOS_OK || !wr_open(w, path, &loc))
        return w->st;

    {
        fprintf(w->f, "\\ written by JAOS %s\n", JAOS_VERSION_STRING);
        if (orig != nullptr)
            lp_write_map(w, orig);
        fprintf(w->f, "%s\n",
                m->sense == JAOS_MAXIMIZE ? "Maximize" : "Minimize");

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

            const bool ranged = rl != ru && rl != -INFINITY && ru != INFINITY;
            char lonum[NUM_LEN];
            row_name(m, rn, i);
            fprintf(w->f, " %s:", rn);
            col = (int)strlen(rn) + 2;
            if (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0) {
                char zn[JAOS_NAME_MAX + 1];
                col_name(m, zn, m->row_ind_col[i]);
                fprintf(w->f, " %s = %d ->", zn, m->row_ind_val[i]);
                col += (int)strlen(zn) + 8;
            }
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

        if (m->col_integer != nullptr) {
            bool any = false;
            for (int64_t j = 0; j < m->num_col; j++)
                any |= m->col_integer[j];
            if (any) {
                fprintf(w->f, "General\n");
                for (int64_t j = 0; j < m->num_col; j++)
                    if (m->col_integer[j]) {
                        col_name(m, nm, j);
                        fprintf(w->f, " %s\n", nm);
                    }
            }
        }
        if (m->col_semi != nullptr) {
            bool any = false;
            for (int64_t j = 0; j < m->num_col; j++)
                any |= m->col_semi[j];
            if (any) {
                fprintf(w->f, "Semi-continuous\n");
                for (int64_t j = 0; j < m->num_col; j++)
                    if (m->col_semi[j]) {
                        col_name(m, nm, j);
                        fprintf(w->f, " %s\n", nm);
                    }
            }
        }
        if (m->num_sos > 0) {
            fprintf(w->f, "SOS\n");
            for (int64_t k = 0; k < m->num_sos; k++) {
                fprintf(w->f, " SOS%lld: S%d::", (long long)(k + 1),
                        m->sos_type[k]);
                for (int64_t t = m->sos_start[k]; t < m->sos_start[k + 1]; t++) {
                    col_name(m, nm, m->sos_col[t]);
                    wr_num(num, m->sos_weight[t]);
                    fprintf(w->f, " %s:%s", nm, num);
                }
                fprintf(w->f, "\n");
            }
        }

        fprintf(w->f, "End\n");
    }

    return wr_close(w, path, &loc);
}

jaos_status jaos_write_lp(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    jaos_status rs = jm_model_ensure_rowwise(m);
    if (rs != JAOS_OK) {
        jm_set_err(m, "out of memory building the row-wise copy");
        return rs;
    }

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;

    char nm[NAME_LEN], rn[NAME_LEN];

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

    if (w->st == JAOS_OK)
        names_unique(w);
    if (w->st != JAOS_OK)
        return w->st;
    bool spelled = jm_lp_name_ok(jm_obj_name(m));
    for (int64_t j = 0; spelled && j < m->num_col; j++) {
        col_name(m, nm, j);
        spelled = jm_lp_name_ok(nm);
    }
    for (int64_t i = 0; spelled && i < m->num_row; i++) {
        row_name(m, rn, i);
        spelled = jm_lp_name_ok(rn);
    }
    if (spelled)
        return lp_write_body(w, path, nullptr);

    jaos_model *c = nullptr;
    if (jaos_model_copy(m, &c) != JAOS_OK) {
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
        return w->st;
    }
    jaos_status st = jm_model_ensure_rowwise(c);
    if (st == JAOS_OK)
        st = lp_spell_names(m, c);
    if (st == JAOS_OK) {
        wr wc = {.f = nullptr, .m = c, .st = JAOS_OK};
        st = lp_write_body(&wc, path, m);
    }
    if (st != JAOS_OK)
        jm_set_err(m, "%s", jaos_model_error(c));
    jaos_model_free(c);
    return st;
}

static void nl_bound_line(FILE *f, double lo, double hi)
{
    char a[NUM_LEN], b[NUM_LEN];
    if (lo == -INFINITY && hi == INFINITY) {
        fputs("3\n", f);
    } else if (lo == hi) {
        wr_num(a, lo);
        fprintf(f, "4 %s\n", a);
    } else if (lo == -INFINITY) {
        wr_num(b, hi);
        fprintf(f, "1 %s\n", b);
    } else if (hi == INFINITY) {
        wr_num(a, lo);
        fprintf(f, "2 %s\n", a);
    } else {
        wr_num(a, lo);
        wr_num(b, hi);
        fprintf(f, "0 %s %s\n", a, b);
    }
}

static void nl_names_file(wr *w, const char *base, const char *ext,
                          int64_t n, const int64_t *order)
{
    if (w->st != JAOS_OK)
        return;
    char *path = malloc(strlen(base) + strlen(ext) + 1);
    if (path == nullptr) {
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
        return;
    }
    strcpy(path, base);
    strcat(path, ext);
    FILE *f = fopen(path, "w");
    if (f == nullptr) {
        wr_fail(w, JAOS_ERR_IO, "cannot open '%s' for writing", path);
        free(path);
        return;
    }
    char nm[NAME_LEN];
    for (int64_t k = 0; k < n; k++) {
        if (order != nullptr)
            col_name(w->m, nm, order[k]);
        else
            row_name(w->m, nm, k);
        fprintf(f, "%s\n", nm);
    }
    if (order == nullptr)
        fprintf(f, "%s\n", jm_obj_name(w->m));
    if (ferror(f))
        wr_fail(w, JAOS_ERR_IO, "writing '%s' failed", path);
    if (fclose(f) != 0)
        wr_fail(w, JAOS_ERR_IO, "closing '%s' failed", path);
    if (w->st != JAOS_OK)
        remove(path);
    free(path);
}

static bool nl_binary(const jaos_model *m, int64_t j)
{
    return m->col_integer != nullptr && m->col_integer[j] &&
           m->col_lower[j] == 0.0 && m->col_upper[j] == 1.0;
}

static bool nl_general_integer(const jaos_model *m, int64_t j)
{
    return m->col_integer != nullptr && m->col_integer[j] && !nl_binary(m, j);
}

jaos_status jaos_write_nl(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    jaos_status rs = jm_model_ensure_rowwise(m);
    if (rs != JAOS_OK) {
        jm_set_err(m, "out of memory building the row-wise copy");
        return rs;
    }

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;
    const int64_t nc = m->num_col, nr = m->num_row;
    char nm[NAME_LEN], num[NUM_LEN];

    if (m->num_sos > 0)
        wr_fail(w, JAOS_ERR_INVALID_INPUT,
                "the model has %" PRId64 " SOS sets, which .nl carries only "
                "as solver suffixes JAOS does not write; write MPS instead",
                m->num_sos);
    for (int64_t j = 0; w->st == JAOS_OK && j < nc; j++) {
        if (m->col_semi != nullptr && m->col_semi[j]) {
            col_name(m, nm, j);
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' is semi-continuous, which .nl cannot "
                    "express; write MPS instead", nm);
        }
    }
    for (int64_t i = 0; w->st == JAOS_OK && i < nr; i++) {
        if (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0) {
            row_name(m, nm, i);
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' is an indicator constraint, which .nl cannot "
                    "express; write MPS instead", nm);
        }
    }
    if (w->st == JAOS_OK)
        names_unique(w);
    if (w->st != JAOS_OK)
        return w->st;

    int64_t *order = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *order);
    int64_t *pos = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pos);
    if (order == nullptr || pos == nullptr) {
        free(order);
        free(pos);
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
        return w->st;
    }
    int64_t n = 0, nbv = 0, niv = 0;
    for (int64_t j = 0; j < nc; j++)
        if (!nl_binary(m, j) && !nl_general_integer(m, j)) {
            pos[j] = n;
            order[n++] = j;
        }
    for (int64_t j = 0; j < nc; j++)
        if (nl_binary(m, j)) {
            pos[j] = n;
            order[n++] = j;
            nbv++;
        }
    for (int64_t j = 0; j < nc; j++)
        if (nl_general_integer(m, j)) {
            pos[j] = n;
            order[n++] = j;
            niv++;
        }

    int64_t nranges = 0, neqns = 0, nzo = 0;
    for (int64_t i = 0; i < nr; i++) {
        const double rl = m->row_lower[i], ru = m->row_upper[i];
        if (rl == ru)
            neqns++;
        else if (rl != -INFINITY && ru != INFINITY)
            nranges++;
    }
    for (int64_t j = 0; j < nc; j++)
        if (m->col_cost[j] != 0.0)
            nzo++;
    size_t maxrow = strlen(jm_obj_name(m)), maxcol = 0;
    for (int64_t i = 0; i < nr; i++) {
        row_name(m, nm, i);
        if (strlen(nm) > maxrow)
            maxrow = strlen(nm);
    }
    for (int64_t j = 0; j < nc; j++) {
        col_name(m, nm, j);
        if (strlen(nm) > maxcol)
            maxcol = strlen(nm);
    }

    jm_locale loc = {0};
    if (!wr_open(w, path, &loc)) {
        free(order);
        free(pos);
        return w->st;
    }
    FILE *f = w->f;
    fprintf(f, "g3 1 1 0\t# written by JAOS %s\n", JAOS_VERSION_STRING);
    fprintf(f, " %" PRId64 " %" PRId64 " 1 %" PRId64 " %" PRId64 " 0\n",
            nc, nr, nranges, neqns);
    fputs(" 0 0\n 0 0\n 0 0 0\n 0 0 0 1\n", f);
    fprintf(f, " %" PRId64 " %" PRId64 " 0 0 0\n", nbv, niv);
    fprintf(f, " %" PRId64 " %" PRId64 "\n", m->num_nz, nzo);
    fprintf(f, " %zu %zu\n", maxrow, maxcol);
    fputs(" 0 0 0 0 0\n", f);

    for (int64_t i = 0; i < nr; i++)
        fprintf(f, "C%" PRId64 "\nn0\n", i);
    wr_num(num, m->obj_offset);
    fprintf(f, "O0 %d\nn%s\n", m->sense == JAOS_MAXIMIZE ? 1 : 0, num);

    fputs("r\n", f);
    for (int64_t i = 0; i < nr; i++)
        nl_bound_line(f, m->row_lower[i], m->row_upper[i]);
    fputs("b\n", f);
    for (int64_t k = 0; k < nc; k++)
        nl_bound_line(f, m->col_lower[order[k]], m->col_upper[order[k]]);

    if (nc > 0) {
        fprintf(f, "k%" PRId64 "\n", nc - 1);
        int64_t cum = 0;
        for (int64_t k = 0; k + 1 < nc; k++) {
            const int64_t j = order[k];
            cum += m->a_start[j + 1] - m->a_start[j];
            fprintf(f, "%" PRId64 "\n", cum);
        }
    }
    for (int64_t i = 0; i < nr; i++) {
        fprintf(f, "J%" PRId64 " %" PRId64 "\n", i,
                m->ar_start[i + 1] - m->ar_start[i]);
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
            wr_num(num, m->ar_value[p]);
            fprintf(f, "%" PRId64 " %s\n", pos[m->ar_index[p]], num);
        }
    }
    fprintf(f, "G0 %" PRId64 "\n", nzo);
    for (int64_t k = 0; k < nc; k++) {
        const int64_t j = order[k];
        if (m->col_cost[j] == 0.0)
            continue;
        wr_num(num, m->col_cost[j]);
        fprintf(f, "%" PRId64 " %s\n", k, num);
    }

    if (wr_close(w, path, &loc) == JAOS_OK) {
        size_t len = strlen(path);
        char *base = malloc(len + 1);
        if (base == nullptr) {
            wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
        } else {
            memcpy(base, path, len + 1);
            if (len > 3 && strcmp(base + len - 3, ".gz") == 0) {
                base[len - 3] = '\0';
                len -= 3;
            }
            if (len > 3 && strcmp(base + len - 3, ".nl") == 0)
                base[len - 3] = '\0';
            nl_names_file(w, base, ".col", nc, order);
            nl_names_file(w, base, ".row", nr, nullptr);
            if (w->st != JAOS_OK) {
                char *col = malloc(strlen(base) + 5);
                if (col != nullptr) {
                    strcpy(col, base);
                    strcat(col, ".col");
                    remove(col);
                    free(col);
                }
            }
            free(base);
        }
        if (w->st != JAOS_OK)
            remove(path);
    }
    free(order);
    free(pos);
    return w->st;
}

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

    if (w->st == JAOS_OK)
        names_unique(w);
    if (w->st != JAOS_OK)
        return w->st;

    jm_locale loc = {0};
    if (!wr_open(w, path, &loc))
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

        fprintf(w->f, "# ray <row name> <multiplier>\n");
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(m, nm, i);
            wr_num(a, m->sol_farkas[i]);
            fprintf(w->f, "ray %s %s\n", nm, a);
        }
    } else {

        fprintf(w->f, "# ray <column name> <direction>\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            wr_num(a, m->sol_ray[j]);
            fprintf(w->f, "ray %s %s\n", nm, a);
        }
    }

    if (!optimal && m->sol_basis_ok) {
        fprintf(w->f, "# basis col|row <name> <status>\n");
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            fprintf(w->f, "basis col %s %s\n", nm,
                    basis_word(m->sol_col_status[j]));
        }
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(m, nm, i);
            fprintf(w->f, "basis row %s %s\n", nm,
                    basis_word(m->sol_row_status[i]));
        }
    }

    fprintf(w->f, "end\n");
    return wr_close(w, path, &loc);
}

static bool basis_of_word(const char *w, jaos_basis_status *out)
{
    if (strcmp(w, "basic") == 0)      { *out = JAOS_BASIS_BASIC;    return true; }
    if (strcmp(w, "lower") == 0)      { *out = JAOS_BASIS_AT_LOWER; return true; }
    if (strcmp(w, "upper") == 0)      { *out = JAOS_BASIS_AT_UPPER; return true; }
    if (strcmp(w, "free") == 0)       { *out = JAOS_BASIS_FREE;     return true; }
    return false;
}

static bool status_of_word(const char *w, jaos_solve_status *out)
{
    if (strcmp(w, "optimal") == 0)    { *out = JAOS_SOLVE_OPTIMAL;    return true; }
    if (strcmp(w, "infeasible") == 0) { *out = JAOS_SOLVE_INFEASIBLE; return true; }
    if (strcmp(w, "unbounded") == 0)  { *out = JAOS_SOLVE_UNBOUNDED;  return true; }
    return false;
}

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

typedef struct {
    jaos_solve_status status;
    double objective;
    double *col_value, *col_dual;
    jaos_basis_status *col_status;
    double *row_activity, *row_dual;
    jaos_basis_status *row_status;
    double *row_ray;
    double *col_ray;

    jaos_basis_status *cert_col_status;
    jaos_basis_status *cert_row_status;
    bool have_basis;
} sol_read;

static jaos_status read_solution_file(jaos_model *m, const char *path,
                                      int want_optimal, sol_read *o)
{
    FILE *f = fopen(path, "r");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s' for reading", path);
        return JAOS_ERR_IO;
    }

    jm_locale loc;
    jm_locale_c_enter(&loc);

    jaos_status st = JAOS_OK;
    char *line = nullptr;
    size_t lsz = 0;
    int64_t lno = 0, ncol = -1, nrow = -1, seen_col = 0, seen_row = 0,
            seen_ray = 0, seen_bcol = 0, seen_brow = 0;
    bool have_status = false, have_obj = false, ended = false;
    jaos_solve_status ss = JAOS_SOLVE_NOT_RUN;
    double obj = 0.0;
    char nm[NAME_LEN];

#define RD_FAIL(...)  do { st = JAOS_ERR_INVALID_INPUT; \
    jm_set_err(m, __VA_ARGS__); goto done; } while (0)

    while (jm_getline(&line, &lsz, f) >= 0) {
        lno++;
        if (ended)
            RD_FAIL("line %" PRId64 ": content after 'end'", lno);

        char *tok[8];
        int nt = 0;
        for (char *p = strtok(line, " \t\r\n");
             p != nullptr && nt < 8; p = strtok(nullptr, " \t\r\n"))
            tok[nt++] = p;
        if (nt == 0 || tok[0][0] == '#')
            continue;

        if (strcmp(tok[0], "status") == 0) {
            if (nt != 2)
                RD_FAIL("line %" PRId64 ": 'status' takes one word", lno);
            if (have_status)
                RD_FAIL("line %" PRId64 ": a second 'status' line", lno);

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
        } else if (strcmp(tok[0], "basis") == 0) {

            if (!have_status)
                RD_FAIL("line %" PRId64 ": a record before 'status'", lno);
            if (ss == JAOS_SOLVE_OPTIMAL)
                RD_FAIL("line %" PRId64 ": a 'basis' record in a file whose "
                        "status is 'optimal'; its 'col' and 'row' records "
                        "carry the basis", lno);
            if (ncol < 0 || nrow < 0)
                RD_FAIL("line %" PRId64 ": a record before both counts", lno);
            if (nt != 4)
                RD_FAIL("line %" PRId64 ": a 'basis' record takes 'col' or "
                        "'row', a name and a status", lno);
            bool is_col;
            if (strcmp(tok[1], "col") == 0)
                is_col = true;
            else if (strcmp(tok[1], "row") == 0)
                is_col = false;
            else
                RD_FAIL("line %" PRId64 ": a 'basis' record is over 'col' or "
                        "'row' and says '%s'", lno, tok[1]);
            const int64_t k = is_col ? seen_bcol : seen_brow;
            const int64_t lim = is_col ? ncol : nrow;
            if (k >= lim)
                RD_FAIL("line %" PRId64 ": more 'basis %s' records than the "
                        "count says", lno, tok[1]);
            if (is_col)
                col_name(m, nm, k);
            else
                row_name(m, nm, k);
            if (strcmp(tok[2], nm) != 0)
                RD_FAIL("line %" PRId64 ": expected '%s' here and the file "
                        "says '%s'; records are in index order and named "
                        "as the model names them", lno, nm, tok[2]);
            jaos_basis_status bs;
            if (!basis_of_word(tok[3], &bs))
                RD_FAIL("line %" PRId64 ": '%s' is not a basis status", lno,
                        tok[3]);
            if (is_col) {
                if (o->cert_col_status != nullptr)
                    o->cert_col_status[k] = bs;
                seen_bcol++;
            } else {
                if (o->cert_row_status != nullptr)
                    o->cert_row_status[k] = bs;
                seen_brow++;
            }
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

        if (seen_bcol != 0 || seen_brow != 0) {
            if (seen_bcol != ncol || seen_brow != nrow)
                RD_FAIL("the file carries %" PRId64 " of %" PRId64 " column "
                        "and %" PRId64 " of %" PRId64 " row basis records, "
                        "and a basis is all of it or none",
                        seen_bcol, ncol, seen_brow, nrow);
            o->have_basis = true;
        }
    }

    o->status = ss;
    o->objective = obj;
    jm_set_err(m, "%s", "");

#undef RD_FAIL
done:
    free(line);
    fclose(f);
    jm_locale_leave(&loc);
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

jaos_status jaos_read_basis(jaos_model *m, const char *path,
                            jaos_basis_status *col_status,
                            jaos_basis_status *row_status)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    sol_read o = {.col_status = col_status, .row_status = row_status,
                  .cert_col_status = col_status,
                  .cert_row_status = row_status};
    const jaos_status st = read_solution_file(m, path, 0, &o);
    if (st != JAOS_OK)
        return st;
    if (o.status != JAOS_SOLVE_OPTIMAL && !o.have_basis) {
        jm_set_err(m, "the file's status is '%s' and it carries no basis",
                   status_word(o.status));
        return JAOS_ERR_INVALID_INPUT;
    }
    return JAOS_OK;
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

static void basis_names_unique(wr *w)
{
    const jaos_model *m = w->m;
    char nm[NAME_LEN];
    int64_t prior;

    for (int side = 0; side < 2 && w->st == JAOS_OK; side++) {
        jm_nmap seen = {0};
        const int64_t n = side == 0 ? m->num_col : m->num_row;
        for (int64_t k = 0; w->st == JAOS_OK && k < n; k++) {
            if (side == 0)
                col_name(m, nm, k);
            else
                row_name(m, nm, k);
            if (jm_nmap_get(&seen, nm, &prior))
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "%ss %" PRId64 " and %" PRId64 " are both named "
                        "'%s', which no file can tell apart",
                        side == 0 ? "column" : "row", prior, k, nm);
            else if (!jm_nmap_insert(&seen, nm, k))
                wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
        }
        jm_nmap_free(&seen);
    }
}

jaos_status jaos_write_mps_basis(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;

    jaos_basis_status *cs = jm_alloc_array(m->num_col, sizeof *cs);
    jaos_basis_status *rs = jm_alloc_array(m->num_row, sizeof *rs);
    if (cs == nullptr || rs == nullptr)
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");

    if (w->st == JAOS_OK) {
        const jaos_status bst = jaos_basis(m, cs, rs);
        if (bst != JAOS_OK) {
            free(cs);
            free(rs);
            return bst;
        }
    }

    if (w->st == JAOS_OK) {
        int64_t basic = 0;
        for (int64_t j = 0; j < m->num_col; j++)
            basic += cs[j] == JAOS_BASIS_BASIC;
        for (int64_t i = 0; i < m->num_row; i++)
            basic += rs[i] == JAOS_BASIS_BASIC;
        if (basic != m->num_row)
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "the basis has %" PRId64 " basic variables and this "
                    "model has %" PRId64 " rows, so it is not a basis of it",
                    basic, m->num_row);
    }
    if (w->st == JAOS_OK)
        basis_names_unique(w);

    jm_locale loc = {0};
    if (w->st != JAOS_OK || !wr_open(w, path, &loc)) {
        free(cs);
        free(rs);
        return w->st;
    }

    {
        char nm[NAME_LEN], rn[NAME_LEN];
        fprintf(w->f, "* written by JAOS %s\n", JAOS_VERSION_STRING);
        fprintf(w->f, "NAME          %s\n",
                m->model_name != nullptr ? m->model_name : "JAOS");

        int64_t i = 0;
        for (int64_t j = 0; j < m->num_col; j++) {
            if (cs[j] == JAOS_BASIS_BASIC) {
                while (i < m->num_row && rs[i] == JAOS_BASIS_BASIC)
                    i++;

                assert(i < m->num_row);
                col_name(m, nm, j);
                row_name(m, rn, i);
                fprintf(w->f, " %s %-9s %s\n",
                        rs[i] == JAOS_BASIS_AT_UPPER ? "XU" : "XL", nm, rn);
                i++;
            } else if (cs[j] == JAOS_BASIS_AT_UPPER) {
                col_name(m, nm, j);
                fprintf(w->f, " UL %s\n", nm);
            }

        }
        fprintf(w->f, "ENDATA\n");
    }

    free(cs);
    free(rs);
    return wr_close(w, path, &loc);
}

static jaos_basis_status nonbasic_at_lower(double lo, double up)
{
    return (lo == -INFINITY && up == INFINITY) ? JAOS_BASIS_FREE
                                               : JAOS_BASIS_AT_LOWER;
}

jaos_status jaos_read_mps_basis(jaos_model *m, const char *path,
                                jaos_basis_status *col_status,
                                jaos_basis_status *row_status)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    FILE *f = fopen(path, "r");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s' for reading", path);
        return JAOS_ERR_IO;
    }

    jaos_status st = JAOS_OK;
    char *line = nullptr;
    size_t lsz = 0;
    int64_t lno = 0;
    bool ended = false;

    jaos_basis_status *cs = jm_alloc_array(m->num_col, sizeof *cs);
    jaos_basis_status *rs = jm_alloc_array(m->num_row, sizeof *rs);

    bool *cseen = jm_calloc_array(m->num_col, sizeof *cseen);
    bool *rseen = jm_calloc_array(m->num_row, sizeof *rseen);
    if (cs == nullptr || rs == nullptr || cseen == nullptr ||
        rseen == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }
    for (int64_t j = 0; j < m->num_col; j++)
        cs[j] = nonbasic_at_lower(m->col_lower[j], m->col_upper[j]);
    for (int64_t i = 0; i < m->num_row; i++)
        rs[i] = JAOS_BASIS_BASIC;

#define BAS_FAIL(...) do { st = JAOS_ERR_INVALID_INPUT; \
    jm_set_err(m, __VA_ARGS__); goto done; } while (0)

    while (jm_getline(&line, &lsz, f) >= 0) {
        lno++;
        if (ended)
            BAS_FAIL("line %" PRId64 ": content after 'ENDATA'", lno);
        if (line[0] == '*')
            continue;

        char *tok[8];
        int nt = 0;
        for (char *p = strtok(line, " \t\r\n");
             p != nullptr && nt < 8; p = strtok(nullptr, " \t\r\n"))
            tok[nt++] = p;
        if (nt == 0)
            continue;

        if (strcmp(tok[0], "ENDATA") == 0) {
            ended = true;
            continue;
        }
        if (strcmp(tok[0], "NAME") == 0)
            continue;

        const bool two = strcmp(tok[0], "XU") == 0 ||
                         strcmp(tok[0], "XL") == 0;
        const bool one = strcmp(tok[0], "UL") == 0 ||
                         strcmp(tok[0], "LL") == 0;
        if (!two && !one)
            BAS_FAIL("line %" PRId64 ": '%s' is not one of the four cards "
                     "XU, XL, UL and LL", lno, tok[0]);
        if (nt != (two ? 3 : 2))
            BAS_FAIL("line %" PRId64 ": '%s' takes %s", lno, tok[0],
                     two ? "a column and a row" : "a column");

        int64_t j = 0;
        if (jaos_col_index(m, tok[1], &j) != JAOS_OK)
            BAS_FAIL("line %" PRId64 ": no column is named '%s'", lno,
                     tok[1]);
        if (cseen[j])
            BAS_FAIL("line %" PRId64 ": a second card for column '%s'", lno,
                     tok[1]);
        cseen[j] = true;

        if (two) {
            int64_t i = 0;
            if (jaos_row_index(m, tok[2], &i) != JAOS_OK)
                BAS_FAIL("line %" PRId64 ": no row is named '%s'", lno,
                         tok[2]);
            if (rseen[i])
                BAS_FAIL("line %" PRId64 ": a second card for row '%s'", lno,
                         tok[2]);
            rseen[i] = true;
            cs[j] = JAOS_BASIS_BASIC;

            if (tok[0][1] == 'U') {
                if (m->row_upper[i] == INFINITY)
                    BAS_FAIL("line %" PRId64 ": row '%s' has no upper bound "
                             "to rest on", lno, tok[2]);
                rs[i] = JAOS_BASIS_AT_UPPER;
            } else {
                rs[i] = nonbasic_at_lower(m->row_lower[i], m->row_upper[i]);
                if (rs[i] == JAOS_BASIS_AT_LOWER &&
                    m->row_lower[i] == -INFINITY)
                    BAS_FAIL("line %" PRId64 ": row '%s' has no lower bound "
                             "to rest on", lno, tok[2]);
            }
        } else if (tok[0][0] == 'U') {
            if (m->col_upper[j] == INFINITY)
                BAS_FAIL("line %" PRId64 ": column '%s' has no upper bound "
                         "to rest on", lno, tok[1]);
            cs[j] = JAOS_BASIS_AT_UPPER;
        } else {
            cs[j] = nonbasic_at_lower(m->col_lower[j], m->col_upper[j]);
            if (cs[j] == JAOS_BASIS_AT_LOWER && m->col_lower[j] == -INFINITY)
                BAS_FAIL("line %" PRId64 ": column '%s' has no lower bound "
                         "to rest on", lno, tok[1]);
        }
    }

    if (!ended)
        BAS_FAIL("line %" PRId64 ": the file ends without 'ENDATA'", lno);

#ifndef NDEBUG
    {
        int64_t basic = 0;
        for (int64_t j = 0; j < m->num_col; j++)
            if (cs[j] == JAOS_BASIS_BASIC)
                basic++;
        for (int64_t i = 0; i < m->num_row; i++)
            if (rs[i] == JAOS_BASIS_BASIC)
                basic++;
        assert(basic == m->num_row);
    }
#endif

    if (col_status != nullptr)
        memcpy(col_status, cs, (size_t)m->num_col * sizeof *cs);
    if (row_status != nullptr)
        memcpy(row_status, rs, (size_t)m->num_row * sizeof *rs);
    m->err[0] = '\0';

#undef BAS_FAIL
done:
    free(line);
    free(cs);
    free(rs);
    free(cseen);
    free(rseen);
    fclose(f);
    return st;
}

enum pt_shape { PT_PLAIN, PT_MIPLIB, PT_SCIP, PT_HIGHS, PT_CPLEX };

static bool starts_with(const char *s, const char *pre)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return strncmp(s, pre, strlen(pre)) == 0;
}

static enum pt_shape sniff_shape(FILE *f)
{
    enum pt_shape shape = PT_PLAIN;
    char *line = nullptr;
    size_t lsz = 0;
    bool first = true;
    while (jm_getline(&line, &lsz, f) >= 0) {
        if (starts_with(line, "<?xml") || strstr(line, "<CPLEXSolution") != nullptr) {
            shape = PT_CPLEX;
            break;
        }
        if (starts_with(line, "# Columns")) {
            shape = PT_HIGHS;
            break;
        }
        char *t = line;
        while (*t == ' ' || *t == '\t')
            t++;
        if (*t == '\0' || *t == '\n' || *t == '\r' || *t == '#')
            continue;
        if (first) {
            if (starts_with(t, "=obj="))
                shape = PT_MIPLIB;
            else if (starts_with(t, "solution status:"))
                shape = PT_SCIP;
            first = false;
        }
    }
    free(line);
    rewind(f);
    return shape;
}

static bool xml_attr(const char *line, const char *key, char *out, size_t cap)
{
    char pat[32];
    snprintf(pat, sizeof pat, " %s=\"", key);
    const char *at = strstr(line, pat);
    if (at == nullptr)
        return false;
    at += strlen(pat);
    const char *close = strchr(at, '"');
    if (close == nullptr || (size_t)(close - at) >= cap)
        return false;
    memcpy(out, at, (size_t)(close - at));
    out[close - at] = '\0';
    return true;
}

static jaos_status read_named_values(jaos_model *m, const char *path,
                                     bool is_col, double *out)
{
    const int64_t n = is_col ? m->num_col : m->num_row;

    FILE *f = fopen(path, "r");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s' for reading", path);
        return JAOS_ERR_IO;
    }

    jm_locale loc;
    jm_locale_c_enter(&loc);

    jaos_status st = JAOS_OK;
    char *line = nullptr;
    size_t lsz = 0;
    int64_t lno = 0, seen = 0;
    const enum pt_shape shape = sniff_shape(f);
    const bool lenient = shape == PT_MIPLIB || shape == PT_SCIP;
    int64_t block_left = -1, blocks = 0;
    const char *want_block = is_col ? "# Columns" : "# Rows";
    const int want_index = is_col ? 1 : 2;
    bool *got = jm_calloc_array(n, sizeof *got);
    if (got == nullptr) {
        jm_set_err(m, "out of memory");
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }
    if (lenient)
        for (int64_t k = 0; k < n; k++)
            out[k] = 0.0;

#define PT_FAIL(...) do { st = JAOS_ERR_INVALID_INPUT; \
    jm_set_err(m, __VA_ARGS__); goto done; } while (0)

    while (jm_getline(&line, &lsz, f) >= 0) {
        lno++;
        char name[NAME_LEN], numtxt[64];
        const char *nm = nullptr, *val = nullptr;
        char *tok[8];
        int nt = 0;

        if (shape == PT_CPLEX) {
            if (strstr(line, is_col ? "<variable " : "<constraint ") == nullptr)
                continue;
            if (!xml_attr(line, "name", name, sizeof name) ||
                !xml_attr(line, is_col ? "value" : "dual", numtxt, sizeof numtxt))
                PT_FAIL("line %" PRId64 ": a CPLEX %s record needs name and %s",
                        lno, is_col ? "variable" : "constraint",
                        is_col ? "value" : "dual");
            nm = name;
            val = numtxt;
        } else if (shape == PT_HIGHS) {
            if (starts_with(line, "#")) {
                if (starts_with(line, want_block)) {
                    blocks++;
                    if (blocks == want_index) {
                        const char *t = line;
                        while (*t == ' ' || *t == '\t')
                            t++;
                        t += strlen(want_block);
                        block_left = strtoll(t, nullptr, 10);
                    }
                }
                continue;
            }
            if (block_left <= 0)
                continue;
            block_left--;
            for (char *q = strtok(line, " \t\r\n");
                 q != nullptr && nt < 8; q = strtok(nullptr, " \t\r\n"))
                tok[nt++] = q;
            if (nt < 2)
                PT_FAIL("line %" PRId64 ": a HiGHS record is a name and a "
                        "number", lno);
            nm = tok[0];
            val = tok[1];
        } else {
            if (shape == PT_MIPLIB && starts_with(line, "=obj="))
                continue;
            if (shape == PT_SCIP && (starts_with(line, "solution status:") ||
                                     starts_with(line, "objective value:")))
                continue;
            char *hash = strchr(line, '#');
            if (hash != nullptr)
                *hash = '\0';
            for (char *q = strtok(line, " \t\r\n");
                 q != nullptr && nt < 8; q = strtok(nullptr, " \t\r\n"))
                tok[nt++] = q;
            if (nt == 0)
                continue;
            if (shape == PT_PLAIN ? nt != 2 : nt < 2)
                PT_FAIL("line %" PRId64 ": a record is a name and one number, "
                        "and this line has %d field%s", lno, nt,
                        nt == 1 ? "" : "s");
            nm = tok[0];
            val = tok[1];
        }

        int64_t k = 0;
        const jaos_status fk = is_col ? jaos_col_index(m, nm, &k)
                                      : jaos_row_index(m, nm, &k);
        if (fk != JAOS_OK)
            PT_FAIL("line %" PRId64 ": no %s is named '%s'", lno,
                    is_col ? "column" : "row", nm);
        if (got[k])
            PT_FAIL("line %" PRId64 ": a second value for '%s'", lno, nm);

        double v = 0.0;
        if (!rd_num(val, &v))
            PT_FAIL("line %" PRId64 ": '%s' is not a finite number", lno, val);
        out[k] = v;
        got[k] = true;
        seen++;
    }

    if (seen != n && !lenient)
        for (int64_t k = 0; k < n; k++)
            if (!got[k]) {
                char nm2[NAME_LEN];
                if (is_col)
                    col_name(m, nm2, k);
                else
                    row_name(m, nm2, k);
                PT_FAIL("the file names %" PRId64 " of the model's %" PRId64
                        " %ss; '%s' is the first it does not name",
                        seen, n, is_col ? "column" : "row", nm2);
            }

    m->err[0] = '\0';

#undef PT_FAIL
done:
    free(line);
    free(got);
    jm_locale_leave(&loc);
    fclose(f);
    return st;
}

jaos_status jaos_read_point(jaos_model *m, const char *path,
                            double *col_value)
{
    if (m == nullptr || path == nullptr || col_value == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    return read_named_values(m, path, true, col_value);
}

jaos_status jaos_read_duals(jaos_model *m, const char *path, double *row_dual)
{
    if (m == nullptr || path == nullptr || row_dual == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    return read_named_values(m, path, false, row_dual);
}

jaos_status jaos_write_point_values(jaos_model *m, const char *path,
                                    const double *col_value)
{
    if (m == nullptr || path == nullptr ||
        (col_value == nullptr && m->num_col > 0))
        return JAOS_ERR_INVALID_INPUT;

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;

    double *x = jm_alloc_array(m->num_col, sizeof *x);
    if (x == nullptr)
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
    else if (m->num_col > 0)
        memcpy(x, col_value, (size_t)m->num_col * sizeof *x);

    if (w->st == JAOS_OK) {
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
    }

    for (int64_t j = 0; w->st == JAOS_OK && j < m->num_col; j++) {
        if (!isfinite(x[j])) {
            char nm[NAME_LEN];
            col_name(m, nm, j);
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "column '%s' holds a value no file can carry", nm);
        }
    }

    jm_locale loc = {0};
    if (w->st != JAOS_OK || !wr_open(w, path, &loc)) {
        free(x);
        return w->st;
    }

    {
        char nm[NAME_LEN], num[NUM_LEN];
        fprintf(w->f, "# written by JAOS %s\n", JAOS_VERSION_STRING);
        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            wr_num(num, x[j]);
            fprintf(w->f, "%-9s %s\n", nm, num);
        }
    }

    free(x);
    return wr_close(w, path, &loc);
}

jaos_status jaos_write_point(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    double *x = jm_alloc_array(m->num_col, sizeof *x);
    if (x == nullptr) {
        jm_set_err(m, "out of memory");
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    jaos_status st = jaos_solution(m, x, nullptr, nullptr, nullptr);
    if (st == JAOS_OK)
        st = jaos_write_point_values(m, path, x);
    free(x);
    return st;
}

jaos_status jaos_write_dual_values(jaos_model *m, const char *path,
                                   const double *row_dual)
{
    if (m == nullptr || path == nullptr ||
        (row_dual == nullptr && m->num_row > 0))
        return JAOS_ERR_INVALID_INPUT;

    wr ww = {.f = nullptr, .m = m, .st = JAOS_OK};
    wr *w = &ww;

    double *y = jm_alloc_array(m->num_row, sizeof *y);
    if (y == nullptr)
        wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
    else if (m->num_row > 0)
        memcpy(y, row_dual, (size_t)m->num_row * sizeof *y);

    if (w->st == JAOS_OK) {
        jm_nmap seen = {0};
        char nm[NAME_LEN];
        int64_t prior;
        for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++) {
            row_name(m, nm, i);
            if (jm_nmap_get(&seen, nm, &prior))
                wr_fail(w, JAOS_ERR_INVALID_INPUT,
                        "rows %" PRId64 " and %" PRId64 " are both named "
                        "'%s', which no file can tell apart", prior, i, nm);
            else if (!jm_nmap_insert(&seen, nm, i))
                wr_fail(w, JAOS_ERR_OUT_OF_MEMORY, "out of memory");
        }
        jm_nmap_free(&seen);
    }

    for (int64_t i = 0; w->st == JAOS_OK && i < m->num_row; i++) {
        if (!isfinite(y[i])) {
            char nm[NAME_LEN];
            row_name(m, nm, i);
            wr_fail(w, JAOS_ERR_INVALID_INPUT,
                    "row '%s' holds a multiplier no file can carry", nm);
        }
    }

    jm_locale loc = {0};
    if (w->st != JAOS_OK || !wr_open(w, path, &loc)) {
        free(y);
        return w->st;
    }

    {
        char nm[NAME_LEN], num[NUM_LEN];
        fprintf(w->f, "# written by JAOS %s\n", JAOS_VERSION_STRING);
        for (int64_t i = 0; i < m->num_row; i++) {
            row_name(m, nm, i);
            wr_num(num, y[i]);
            fprintf(w->f, "%-9s %s\n", nm, num);
        }
    }

    free(y);
    return wr_close(w, path, &loc);
}

jaos_status jaos_write_duals(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    double *y = jm_alloc_array(m->num_row, sizeof *y);
    if (y == nullptr) {
        jm_set_err(m, "out of memory");
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    jaos_status st = jaos_solution(m, nullptr, nullptr, y, nullptr);
    if (st == JAOS_OK)
        st = jaos_write_dual_values(m, path, y);
    free(y);
    return st;
}
