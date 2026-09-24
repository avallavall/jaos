/* SPDX-License-Identifier: Apache-2.0 */
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <stdint.h>
#include <string.h>

#include "jaos.h"

enum { KEEP_LOG, KEEP_PROGRESS, KEEP_INCUMBENT, KEEP_NODE, KEEP_SLOTS };

static const char *const basis_names[] = {"basic", "at_lower", "at_upper",
                                          "free"};
static const char *const side_names[] = {"none", "lower", "upper", "both"};
static const char *const proof_names[] = {"optimal", "broken", "refused"};
static const char *const stage_names[] = {"none", "rank", "primal", "dual"};
static const char *const kind_names[] = {"optimal", "infeasible",
                                         "unbounded"};

static void model_finalize(SEXP p)
{
    jaos_model *m = R_ExternalPtrAddr(p);
    if (m != NULL) {
        jaos_model_free(m);
        R_ClearExternalPtr(p);
    }
}

static jaos_model *model_of(SEXP p)
{
    if (TYPEOF(p) != EXTPTRSXP || R_ExternalPtrTag(p) != install("jaos_model"))
        error("not a JAOS model");
    jaos_model *m = R_ExternalPtrAddr(p);
    if (m == NULL)
        error("the JAOS model was freed");
    return m;
}

static void check(jaos_model *m, jaos_status st)
{
    if (st != JAOS_OK) {
        const char *msg = jaos_model_error(m);
        error("JAOS: %s", msg != NULL && *msg ? msg : jaos_status_str(st));
    }
}

static SEXP wrap(jaos_model *m, SEXP keep)
{
    SEXP p = PROTECT(R_MakeExternalPtr(m, install("jaos_model"), keep));
    R_RegisterCFinalizerEx(p, model_finalize, TRUE);
    UNPROTECT(1);
    return p;
}

static SEXP kept_copy(SEXP p)
{
    SEXP keep = R_ExternalPtrProtected(p);
    return TYPEOF(keep) == VECSXP ? shallow_duplicate(keep)
                                  : allocVector(VECSXP, KEEP_SLOTS);
}

static void keep_fn(SEXP p, int slot, SEXP fn)
{
    SEXP keep = R_ExternalPtrProtected(p);
    if (TYPEOF(keep) == VECSXP && XLENGTH(keep) == KEEP_SLOTS)
        SET_VECTOR_ELT(keep, slot, fn);
}

static void need(SEXP v, int64_t n, const char *what)
{
    if ((int64_t)XLENGTH(v) != n)
        error("%s has %lld values, and %lld are needed", what,
              (long long)XLENGTH(v), (long long)n);
}

static int64_t *int64s(SEXP v)
{
    const R_xlen_t n = XLENGTH(v);
    int64_t *out = (int64_t *)R_alloc(n > 0 ? n : 1, sizeof *out);
    SEXP d = PROTECT(coerceVector(v, REALSXP));
    for (R_xlen_t k = 0; k < n; k++)
        out[k] = (int64_t)REAL(d)[k];
    UNPROTECT(1);
    return out;
}

static double *doubles(SEXP v)
{
    const R_xlen_t n = XLENGTH(v);
    double *out = (double *)R_alloc(n > 0 ? n : 1, sizeof *out);
    SEXP d = PROTECT(coerceVector(v, REALSXP));
    if (n > 0)
        memcpy(out, REAL(d), (size_t)n * sizeof *out);
    UNPROTECT(1);
    return out;
}

static double *scratch(int64_t n)
{
    return (double *)R_alloc(n > 0 ? n : 1, sizeof(double));
}

static int64_t *scratch_index(int64_t n)
{
    return (int64_t *)R_alloc(n > 0 ? n : 1, sizeof(int64_t));
}

static SEXP numeric(const double *x, int64_t n)
{
    SEXP v = PROTECT(allocVector(REALSXP, (R_xlen_t)n));
    if (n > 0)
        memcpy(REAL(v), x, (size_t)n * sizeof *x);
    UNPROTECT(1);
    return v;
}

static double index1(int64_t i)
{
    return i < 0 ? NA_REAL : (double)i + 1.0;
}

static SEXP indices(const int64_t *x, int64_t n)
{
    SEXP v = PROTECT(allocVector(REALSXP, (R_xlen_t)n));
    for (int64_t k = 0; k < n; k++)
        REAL(v)[k] = index1(x[k]);
    UNPROTECT(1);
    return v;
}

static SEXP basis_labels(const jaos_basis_status *s, int64_t n)
{
    SEXP v = PROTECT(allocVector(STRSXP, (R_xlen_t)n));
    for (int64_t k = 0; k < n; k++)
        SET_STRING_ELT(v, k, mkChar(s[k] >= 0 && s[k] <= 3 ? basis_names[s[k]]
                                                          : "free"));
    UNPROTECT(1);
    return v;
}

static SEXP side_labels(const jaos_iis_side *s, int64_t n)
{
    SEXP v = PROTECT(allocVector(STRSXP, (R_xlen_t)n));
    for (int64_t k = 0; k < n; k++)
        SET_STRING_ELT(v, k, mkChar(side_names[s[k] & 3]));
    UNPROTECT(1);
    return v;
}

static int *codes(SEXP v, int64_t n, const char *what)
{
    need(v, n, what);
    SEXP iv = PROTECT(coerceVector(v, INTSXP));
    int *out = (int *)R_alloc(n > 0 ? n : 1, sizeof *out);
    for (int64_t k = 0; k < n; k++) {
        const int c = INTEGER(iv)[k];
        if (c < 0 || c > 3)
            error("%s holds a value that is not one of its four names", what);
        out[k] = c;
    }
    UNPROTECT(1);
    return out;
}

static jaos_basis_status *basis_codes(SEXP v, int64_t n, const char *what)
{
    const int *c = codes(v, n, what);
    jaos_basis_status *out =
        (jaos_basis_status *)R_alloc(n > 0 ? n : 1, sizeof *out);
    for (int64_t k = 0; k < n; k++)
        out[k] = (jaos_basis_status)c[k];
    return out;
}

static jaos_iis_side *side_codes(SEXP v, int64_t n, const char *what)
{
    const int *c = codes(v, n, what);
    jaos_iis_side *out = (jaos_iis_side *)R_alloc(n > 0 ? n : 1, sizeof *out);
    for (int64_t k = 0; k < n; k++)
        out[k] = (jaos_iis_side)c[k];
    return out;
}

typedef struct {
    SEXP v, nm;
    int k;
} record;

static record record_new(int n)
{
    record r;
    r.v = PROTECT(allocVector(VECSXP, n));
    r.nm = PROTECT(allocVector(STRSXP, n));
    r.k = 0;
    return r;
}

static void put(record *r, const char *name, SEXP v)
{
    SET_VECTOR_ELT(r->v, r->k, v);
    SET_STRING_ELT(r->nm, r->k, mkChar(name));
    r->k++;
}

static SEXP record_done(record *r)
{
    setAttrib(r->v, R_NamesSymbol, r->nm);
    UNPROTECT(2);
    return r->v;
}

#define PUT_REAL(r, s, f) put(&(r), #f, ScalarReal((double)(s).f))
#define PUT_BOOL(r, s, f) put(&(r), #f, ScalarLogical((s).f))
#define PUT_INDEX(r, s, f) put(&(r), #f, ScalarReal(index1((s).f)))

static int64_t cone_total(jaos_model *m)
{
    int64_t total = 0;
    for (int64_t k = 0; k < jaos_num_cones(m); k++) {
        int64_t n = 0;
        check(m, jaos_cone(m, k, NULL, &n, NULL));
        total += n;
    }
    return total;
}

static SEXP r_version(void)
{
    return mkString(jaos_version());
}

static SEXP r_infinity(void)
{
    return ScalarReal(jaos_infinity());
}

static SEXP r_status_str(SEXP code)
{
    return mkString(jaos_status_str((jaos_status)asInteger(code)));
}

static SEXP r_model_error(SEXP p)
{
    return mkString(jaos_model_error(model_of(p)));
}

static SEXP r_option_names(void)
{
    const int64_t n = jaos_num_options();
    SEXP v = PROTECT(allocVector(STRSXP, (R_xlen_t)n));
    for (int64_t k = 0; k < n; k++)
        SET_STRING_ELT(v, k, mkChar(jaos_option_name(k)));
    UNPROTECT(1);
    return v;
}

static SEXP r_new(void)
{
    SEXP keep = PROTECT(allocVector(VECSXP, KEEP_SLOTS));
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        error("JAOS: jaos_model_new failed");
    SEXP p = wrap(m, keep);
    UNPROTECT(1);
    return p;
}

static SEXP r_copy(SEXP p)
{
    jaos_model *m = model_of(p);
    SEXP keep = PROTECT(kept_copy(p));
    jaos_model *c = NULL;
    check(m, jaos_model_copy(m, &c));
    SEXP out = wrap(c, keep);
    UNPROTECT(1);
    return out;
}

static SEXP r_read(SEXP p, SEXP path, SEXP kind)
{
    jaos_model *m = model_of(p);
    const char *f = CHAR(STRING_ELT(path, 0));
    const char *k = CHAR(STRING_ELT(kind, 0));
    jaos_status st = strcmp(k, "lp") == 0 ? jaos_read_lp(m, f)
                   : strcmp(k, "nl") == 0 ? jaos_read_nl(m, f)
                   : strcmp(k, "qplib") == 0 ? jaos_read_qplib(m, f)
                   : strcmp(k, "osil") == 0 ? jaos_read_osil(m, f)
                   : strcmp(k, "cbf") == 0 ? jaos_read_cbf(m, f)
                   : jaos_read_mps(m, f);
    check(m, st);
    return p;
}

static SEXP r_write(SEXP p, SEXP path, SEXP kind)
{
    jaos_model *m = model_of(p);
    const char *f = CHAR(STRING_ELT(path, 0));
    const char *k = CHAR(STRING_ELT(kind, 0));
    jaos_status st;
    if (strcmp(k, "mps") == 0)
        st = jaos_write_mps(m, f);
    else if (strcmp(k, "lp") == 0)
        st = jaos_write_lp(m, f);
    else if (strcmp(k, "nl") == 0)
        st = jaos_write_nl(m, f);
    else if (strcmp(k, "qplib") == 0)
        st = jaos_write_qplib(m, f);
    else if (strcmp(k, "cbf") == 0)
        st = jaos_write_cbf(m, f);
    else if (strcmp(k, "osil") == 0)
        st = jaos_write_osil(m, f);
    else if (strcmp(k, "solution") == 0)
        st = jaos_write_solution(m, f);
    else if (strcmp(k, "mps_basis") == 0)
        st = jaos_write_mps_basis(m, f);
    else if (strcmp(k, "point") == 0)
        st = jaos_write_point(m, f);
    else if (strcmp(k, "duals") == 0)
        st = jaos_write_duals(m, f);
    else if (strcmp(k, "proof") == 0)
        st = jaos_write_proof(m, f);
    else
        error("JAOS: no writer for kind '%s'", k);
    check(m, st);
    return p;
}

static SEXP r_write_values(SEXP p, SEXP path, SEXP values, SEXP rows)
{
    jaos_model *m = model_of(p);
    const char *f = CHAR(STRING_ELT(path, 0));
    if (asLogical(rows)) {
        need(values, jaos_num_row(m), "row_dual");
        check(m, jaos_write_dual_values(m, f, doubles(values)));
    } else {
        need(values, jaos_num_col(m), "x");
        check(m, jaos_write_point_values(m, f, doubles(values)));
    }
    return p;
}

static SEXP r_write_sol_ampl(SEXP p, SEXP path, SEXP message)
{
    jaos_model *m = model_of(p);
    const char *msg = isNull(message) || XLENGTH(message) == 0 ||
                              STRING_ELT(message, 0) == NA_STRING
                          ? NULL
                          : translateCharUTF8(STRING_ELT(message, 0));
    check(m, jaos_write_sol_ampl(m, CHAR(STRING_ELT(path, 0)), msg));
    return p;
}

static SEXP r_read_options(SEXP p, SEXP path)
{
    jaos_model *m = model_of(p);
    check(m, jaos_read_options(m, CHAR(STRING_ELT(path, 0))));
    return p;
}

static SEXP r_load_lp(SEXP p, SEXP sense, SEXP offset, SEXP cost, SEXP cl,
                      SEXP cu, SEXP rl, SEXP ru, SEXP start, SEXP index,
                      SEXP value)
{
    jaos_model *m = model_of(p);
    const int64_t nc = XLENGTH(cost), nr = XLENGTH(rl), nz = XLENGTH(value);
    need(cl, nc, "col_lower");
    need(cu, nc, "col_upper");
    need(ru, nr, "row_upper");
    need(index, nz, "a_index");
    if (nz > 0)
        need(start, nc + 1, "a_start");
    check(m, jaos_load_lp(m, nc, nr,
                          asInteger(sense) ? JAOS_MAXIMIZE : JAOS_MINIMIZE,
                          asReal(offset), doubles(cost), doubles(cl),
                          doubles(cu), doubles(rl), doubles(ru), nz,
                          nz > 0 ? int64s(start) : NULL,
                          nz > 0 ? int64s(index) : NULL,
                          nz > 0 ? doubles(value) : NULL));
    return p;
}

static SEXP r_add_cols(SEXP p, SEXP cost, SEXP cl, SEXP cu, SEXP start,
                       SEXP index, SEXP value)
{
    jaos_model *m = model_of(p);
    const int64_t n = XLENGTH(cost), nz = XLENGTH(value);
    need(cl, n, "lower");
    need(cu, n, "upper");
    need(index, nz, "the matrix's indices");
    if (nz > 0)
        need(start, n + 1, "the matrix's starts");
    check(m, jaos_add_cols(m, n, doubles(cost), doubles(cl), doubles(cu), nz,
                           nz > 0 ? int64s(start) : NULL,
                           nz > 0 ? int64s(index) : NULL,
                           nz > 0 ? doubles(value) : NULL));
    return p;
}

static SEXP r_add_rows(SEXP p, SEXP rl, SEXP ru, SEXP start, SEXP index,
                       SEXP value)
{
    jaos_model *m = model_of(p);
    const int64_t n = XLENGTH(rl), nz = XLENGTH(value);
    need(ru, n, "upper");
    need(index, nz, "the matrix's indices");
    if (nz > 0)
        need(start, n + 1, "the matrix's starts");
    check(m, jaos_add_rows(m, n, doubles(rl), doubles(ru), nz,
                           nz > 0 ? int64s(start) : NULL,
                           nz > 0 ? int64s(index) : NULL,
                           nz > 0 ? doubles(value) : NULL));
    return p;
}

static SEXP r_delete(SEXP p, SEXP idx, SEXP rows)
{
    jaos_model *m = model_of(p);
    const int64_t n = XLENGTH(idx);
    check(m, asLogical(rows) ? jaos_delete_rows(m, n, int64s(idx))
                             : jaos_delete_cols(m, n, int64s(idx)));
    return p;
}

static SEXP r_dims(SEXP p)
{
    jaos_model *m = model_of(p);
    SEXP v = PROTECT(allocVector(REALSXP, 3));
    REAL(v)[0] = (double)jaos_num_col(m);
    REAL(v)[1] = (double)jaos_num_row(m);
    REAL(v)[2] = (double)jaos_num_cones(m);
    UNPROTECT(1);
    return v;
}

static SEXP r_num_nz(SEXP p)
{
    return ScalarReal((double)jaos_num_nz(model_of(p)));
}

static SEXP r_col_cost(SEXP p, SEXP cols)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(cols);
    const int64_t *c = int64s(cols);
    SEXP out = PROTECT(allocVector(REALSXP, n));
    for (R_xlen_t k = 0; k < n; k++)
        check(m, jaos_col_cost(m, c[k], REAL(out) + k));
    UNPROTECT(1);
    return out;
}

static SEXP r_set_col_cost(SEXP p, SEXP cols, SEXP cost)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(cols);
    const int64_t *c = int64s(cols);
    need(cost, n, "cost");
    const double *v = doubles(cost);
    for (R_xlen_t k = 0; k < n; k++)
        check(m, jaos_set_col_cost(m, c[k], v[k]));
    return p;
}

static SEXP r_bounds(SEXP p, SEXP idx, SEXP rows)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(idx);
    const int64_t *c = int64s(idx);
    const int r = asLogical(rows);
    record out = record_new(2);
    SEXP lo = allocVector(REALSXP, n);
    put(&out, "lower", lo);
    SEXP hi = allocVector(REALSXP, n);
    put(&out, "upper", hi);
    for (R_xlen_t k = 0; k < n; k++)
        check(m, r ? jaos_row_bounds(m, c[k], REAL(lo) + k, REAL(hi) + k)
                   : jaos_col_bounds(m, c[k], REAL(lo) + k, REAL(hi) + k));
    return record_done(&out);
}

static SEXP r_set_bounds(SEXP p, SEXP idx, SEXP lower, SEXP upper, SEXP rows)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(idx);
    const int64_t *c = int64s(idx);
    need(lower, n, "lower");
    need(upper, n, "upper");
    const double *lo = doubles(lower), *hi = doubles(upper);
    const int r = asLogical(rows);
    for (R_xlen_t k = 0; k < n; k++)
        check(m, r ? jaos_set_row_bounds(m, c[k], lo[k], hi[k])
                   : jaos_set_col_bounds(m, c[k], lo[k], hi[k]));
    return p;
}

static SEXP r_sense(SEXP p)
{
    jaos_model *m = model_of(p);
    jaos_obj_sense s = JAOS_MINIMIZE;
    check(m, jaos_objective_sense(m, &s));
    return mkString(s == JAOS_MAXIMIZE ? "maximize" : "minimize");
}

static SEXP r_set_sense(SEXP p, SEXP maximize)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_objective_sense(m, asLogical(maximize) ? JAOS_MAXIMIZE
                                                             : JAOS_MINIMIZE));
    return p;
}

static SEXP r_offset(SEXP p)
{
    jaos_model *m = model_of(p);
    double v = 0.0;
    check(m, jaos_objective_offset(m, &v));
    return ScalarReal(v);
}

static SEXP r_set_offset(SEXP p, SEXP offset)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_objective_offset(m, asReal(offset)));
    return p;
}

static SEXP r_entries(SEXP p, SEXP k, SEXP rows)
{
    jaos_model *m = model_of(p);
    const int64_t kk = (int64_t)asReal(k);
    const int r = asLogical(rows);
    int64_t n = 0;
    check(m, r ? jaos_row_entries(m, kk, &n, NULL, NULL)
               : jaos_col_entries(m, kk, &n, NULL, NULL));
    int64_t *idx = scratch_index(n);
    double *val = scratch(n);
    check(m, r ? jaos_row_entries(m, kk, &n, idx, val)
               : jaos_col_entries(m, kk, &n, idx, val));
    record out = record_new(2);
    put(&out, "index", indices(idx, n));
    put(&out, "value", numeric(val, n));
    return record_done(&out);
}

static SEXP r_coefficient(SEXP p, SEXP row, SEXP col)
{
    jaos_model *m = model_of(p);
    double v = 0.0;
    check(m, jaos_coefficient(m, (int64_t)asReal(row), (int64_t)asReal(col),
                              &v));
    return ScalarReal(v);
}

static SEXP r_set_coefficient(SEXP p, SEXP row, SEXP col, SEXP value)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_coefficient(m, (int64_t)asReal(row),
                                  (int64_t)asReal(col), asReal(value)));
    return p;
}

static SEXP r_names(SEXP p, SEXP idx, SEXP rows)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(idx);
    const int64_t *c = int64s(idx);
    const int r = asLogical(rows);
    char buf[JAOS_NAME_MAX + 1];
    SEXP out = PROTECT(allocVector(STRSXP, n));
    for (R_xlen_t k = 0; k < n; k++) {
        check(m, r ? jaos_row_name(m, c[k], buf, sizeof buf)
                   : jaos_col_name(m, c[k], buf, sizeof buf));
        SET_STRING_ELT(out, k, mkCharCE(buf, CE_UTF8));
    }
    UNPROTECT(1);
    return out;
}

static const char *name_arg(SEXP s)
{
    return s == NA_STRING ? NULL : translateCharUTF8(s);
}

static SEXP r_set_names(SEXP p, SEXP idx, SEXP names, SEXP rows)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(idx);
    const int64_t *c = int64s(idx);
    need(names, n, "names");
    const int r = asLogical(rows);
    for (R_xlen_t k = 0; k < n; k++) {
        const char *s = name_arg(STRING_ELT(names, k));
        check(m, r ? jaos_set_row_name(m, c[k], s)
                   : jaos_set_col_name(m, c[k], s));
    }
    return p;
}

static SEXP r_index(SEXP p, SEXP names, SEXP rows)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(names);
    const int r = asLogical(rows);
    SEXP out = PROTECT(allocVector(REALSXP, n));
    for (R_xlen_t k = 0; k < n; k++) {
        int64_t i = -1;
        const char *s = translateCharUTF8(STRING_ELT(names, k));
        check(m, r ? jaos_row_index(m, s, &i) : jaos_col_index(m, s, &i));
        REAL(out)[k] = index1(i);
    }
    UNPROTECT(1);
    return out;
}

static SEXP r_name(SEXP p, SEXP which)
{
    jaos_model *m = model_of(p);
    char buf[JAOS_NAME_MAX + 1];
    check(m, asInteger(which) ? jaos_model_name(m, buf, sizeof buf)
                              : jaos_objective_name(m, buf, sizeof buf));
    return ScalarString(mkCharCE(buf, CE_UTF8));
}

static SEXP r_set_name(SEXP p, SEXP which, SEXP name)
{
    jaos_model *m = model_of(p);
    const char *s = XLENGTH(name) == 0 ? NULL : name_arg(STRING_ELT(name, 0));
    check(m, asInteger(which) ? jaos_set_model_name(m, s)
                              : jaos_set_objective_name(m, s));
    return p;
}

static SEXP r_set_flags(SEXP p, SEXP cols, SEXP on, SEXP semi)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(cols);
    const int64_t *c = int64s(cols);
    need(on, n, "on");
    SEXP o = PROTECT(coerceVector(on, LGLSXP));
    const int s = asLogical(semi);
    for (R_xlen_t k = 0; k < n; k++) {
        const bool v = LOGICAL(o)[k] == TRUE;
        check(m, s ? jaos_set_col_semicontinuous(m, c[k], v)
                   : jaos_set_col_integer(m, c[k], v));
    }
    UNPROTECT(1);
    return p;
}

static SEXP r_flags(SEXP p, SEXP cols, SEXP semi)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(cols);
    const int64_t *c = int64s(cols);
    const int s = asLogical(semi);
    SEXP out = PROTECT(allocVector(LGLSXP, n));
    for (R_xlen_t k = 0; k < n; k++) {
        bool v = false;
        check(m, s ? jaos_col_semicontinuous(m, c[k], &v)
                   : jaos_col_integer(m, c[k], &v));
        LOGICAL(out)[k] = v;
    }
    UNPROTECT(1);
    return out;
}

static SEXP r_has_integer(SEXP p)
{
    return ScalarLogical(jaos_model_has_integer(model_of(p)));
}

static SEXP r_set_quadratic(SEXP p, SEXP rows, SEXP cols, SEXP vals)
{
    jaos_model *m = model_of(p);
    const int64_t n = XLENGTH(vals);
    need(rows, n, "rows");
    need(cols, n, "cols");
    check(m, jaos_set_quadratic(m, n, int64s(rows), int64s(cols),
                                doubles(vals)));
    return p;
}

static SEXP r_set_row_quadratic(SEXP p, SEXP row, SEXP rows, SEXP cols,
                                SEXP vals)
{
    jaos_model *m = model_of(p);
    const int64_t n = XLENGTH(vals);
    need(rows, n, "rows");
    need(cols, n, "cols");
    check(m, jaos_set_row_quadratic(m, (int64_t)asReal(row), n, int64s(rows),
                                    int64s(cols), doubles(vals)));
    return p;
}

static SEXP r_quadratic_nz(SEXP p, SEXP row)
{
    jaos_model *m = model_of(p);
    return ScalarReal((double)(isNull(row)
                                   ? jaos_quadratic_nz(m)
                                   : jaos_row_quadratic_nz(
                                         m, (int64_t)asReal(row))));
}

static SEXP r_quadratic(SEXP p, SEXP row)
{
    jaos_model *m = model_of(p);
    const int obj = isNull(row);
    const int64_t r = obj ? 0 : (int64_t)asReal(row);
    int64_t n = obj ? jaos_quadratic_nz(m) : jaos_row_quadratic_nz(m, r);
    if (n < 0)
        n = 0;
    int64_t *i = scratch_index(n), *j = scratch_index(n);
    double *v = scratch(n);
    if (n > 0)
        check(m, obj ? jaos_quadratic(m, i, j, v)
                     : jaos_row_quadratic(m, r, i, j, v));
    record out = record_new(3);
    put(&out, "rows", indices(i, n));
    put(&out, "cols", indices(j, n));
    put(&out, "values", numeric(v, n));
    return record_done(&out);
}

static SEXP r_set_col_quadratic(SEXP p, SEXP cols, SEXP q)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(cols);
    const int64_t *c = int64s(cols);
    need(q, n, "q");
    const double *v = doubles(q);
    for (R_xlen_t k = 0; k < n; k++)
        check(m, jaos_set_col_quadratic(m, c[k], v[k]));
    return p;
}

static SEXP r_col_quadratic(SEXP p, SEXP cols)
{
    jaos_model *m = model_of(p);
    const R_xlen_t n = XLENGTH(cols);
    const int64_t *c = int64s(cols);
    SEXP out = PROTECT(allocVector(REALSXP, n));
    for (R_xlen_t k = 0; k < n; k++)
        check(m, jaos_col_quadratic(m, c[k], REAL(out) + k));
    UNPROTECT(1);
    return out;
}

static SEXP r_add_sos(SEXP p, SEXP type, SEXP cols, SEXP weights)
{
    jaos_model *m = model_of(p);
    const int64_t n = XLENGTH(cols);
    need(weights, n, "weights");
    check(m, jaos_add_sos(m, asInteger(type), n, int64s(cols),
                          doubles(weights)));
    return p;
}

static SEXP r_num_sos(SEXP p)
{
    return ScalarReal((double)jaos_num_sos(model_of(p)));
}

static SEXP r_sos(SEXP p, SEXP k)
{
    jaos_model *m = model_of(p);
    const int64_t kk = (int64_t)asReal(k);
    int t = 0;
    int64_t n = 0;
    check(m, jaos_sos(m, kk, &t, &n, NULL, NULL));
    int64_t *c = scratch_index(n);
    double *w = scratch(n);
    check(m, jaos_sos(m, kk, NULL, NULL, c, w));
    record out = record_new(3);
    put(&out, "type", ScalarReal((double)t));
    put(&out, "cols", indices(c, n));
    put(&out, "weights", numeric(w, n));
    return record_done(&out);
}

static SEXP r_set_row_indicator(SEXP p, SEXP row, SEXP col, SEXP value)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_row_indicator(m, (int64_t)asReal(row),
                                    (int64_t)asReal(col), asInteger(value)));
    return p;
}

static SEXP r_row_indicator(SEXP p, SEXP row)
{
    jaos_model *m = model_of(p);
    int64_t c = -1;
    int v = 0;
    check(m, jaos_row_indicator(m, (int64_t)asReal(row), &c, &v));
    record out = record_new(2);
    put(&out, "col", ScalarReal(index1(c)));
    put(&out, "value", ScalarReal((double)v));
    return record_done(&out);
}

static SEXP r_add_cone(SEXP p, SEXP rotated, SEXP cols)
{
    jaos_model *m = model_of(p);
    check(m, jaos_add_cone(m, asLogical(rotated) ? JAOS_CONE_ROTATED
                                                 : JAOS_CONE_QUADRATIC,
                           XLENGTH(cols), int64s(cols)));
    return p;
}

static SEXP r_cone(SEXP p, SEXP k)
{
    jaos_model *m = model_of(p);
    const int64_t kk = (int64_t)asReal(k);
    jaos_cone_type t = JAOS_CONE_QUADRATIC;
    int64_t n = 0;
    check(m, jaos_cone(m, kk, &t, &n, NULL));
    int64_t *c = scratch_index(n);
    check(m, jaos_cone(m, kk, NULL, NULL, c));
    record out = record_new(2);
    put(&out, "type",
        mkString(t == JAOS_CONE_ROTATED ? "rotated" : "quadratic"));
    put(&out, "cols", indices(c, n));
    return record_done(&out);
}

static SEXP r_delete_cones(SEXP p, SEXP ks)
{
    jaos_model *m = model_of(p);
    check(m, jaos_delete_cones(m, XLENGTH(ks), int64s(ks)));
    return p;
}

static SEXP r_set_option(SEXP p, SEXP name, SEXP value)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_option(m, CHAR(STRING_ELT(name, 0)),
                             CHAR(STRING_ELT(value, 0))));
    return p;
}

static SEXP r_get_option(SEXP p, SEXP name)
{
    jaos_model *m = model_of(p);
    char buf[512];
    check(m, jaos_get_option(m, CHAR(STRING_ELT(name, 0)), buf, sizeof buf));
    return mkString(buf);
}

static SEXP r_solve(SEXP p)
{
    jaos_model *m = model_of(p);
    check(m, jaos_solve(m));
    return p;
}

static SEXP r_status(SEXP p)
{
    return mkString(jaos_solve_status_str(jaos_status_of(model_of(p))));
}

static SEXP r_objective(SEXP p)
{
    jaos_model *m = model_of(p);
    double v = 0.0;
    check(m, jaos_objective(m, &v));
    return ScalarReal(v);
}

static SEXP r_counter(SEXP p, SEXP which)
{
    jaos_model *m = model_of(p);
    switch (asInteger(which)) {
    case 0:
        return ScalarReal((double)jaos_iterations(m));
    case 1:
        return ScalarReal((double)jaos_work_units(m));
    default:
        return ScalarReal(jaos_solve_time(m));
    }
}

static SEXP r_solution(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = scratch(nc), *d = scratch(nc), *a = scratch(nr),
           *y = scratch(nr);
    check(m, jaos_solution(m, x, a, y, d));
    record out = record_new(4);
    put(&out, "x", numeric(x, nc));
    put(&out, "row_activity", numeric(a, nr));
    put(&out, "row_dual", numeric(y, nr));
    put(&out, "reduced_cost", numeric(d, nc));
    return record_done(&out);
}

static SEXP basis_pair(const jaos_basis_status *cs, int64_t nc,
                       const jaos_basis_status *rs, int64_t nr)
{
    record out = record_new(2);
    put(&out, "col_status", basis_labels(cs, nc));
    put(&out, "row_status", basis_labels(rs, nr));
    return record_done(&out);
}

static SEXP r_basis(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    jaos_basis_status *cs = (jaos_basis_status *)R_alloc(nc + 1, sizeof *cs);
    jaos_basis_status *rs = (jaos_basis_status *)R_alloc(nr + 1, sizeof *rs);
    check(m, jaos_basis(m, cs, rs));
    return basis_pair(cs, nc, rs, nr);
}

static SEXP r_set_basis(SEXP p, SEXP cs, SEXP rs)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_basis(m, basis_codes(cs, jaos_num_col(m), "col_status"),
                            basis_codes(rs, jaos_num_row(m), "row_status")));
    return p;
}

static SEXP r_clear_basis(SEXP p)
{
    jaos_clear_basis(model_of(p));
    return p;
}

static SEXP r_read_solution(SEXP p, SEXP path)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double obj = 0.0;
    double *cv = scratch(nc), *cd = scratch(nc), *ra = scratch(nr),
           *rd = scratch(nr);
    jaos_basis_status *cs = (jaos_basis_status *)R_alloc(nc + 1, sizeof *cs);
    jaos_basis_status *rs = (jaos_basis_status *)R_alloc(nr + 1, sizeof *rs);
    check(m, jaos_read_solution(m, CHAR(STRING_ELT(path, 0)), &obj, cv, cd,
                                cs, ra, rd, rs));
    record out = record_new(7);
    put(&out, "objective", ScalarReal(obj));
    put(&out, "x", numeric(cv, nc));
    put(&out, "row_activity", numeric(ra, nr));
    put(&out, "row_dual", numeric(rd, nr));
    put(&out, "reduced_cost", numeric(cd, nc));
    put(&out, "col_status", basis_labels(cs, nc));
    put(&out, "row_status", basis_labels(rs, nr));
    return record_done(&out);
}

static SEXP r_read_certificate(SEXP p, SEXP path)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    jaos_solve_status st = JAOS_SOLVE_NOT_RUN;
    double *rr = scratch(nr), *cr = scratch(nc);
    check(m, jaos_read_certificate(m, CHAR(STRING_ELT(path, 0)), &st, rr,
                                   cr));
    record out = record_new(2);
    put(&out, "status", mkString(jaos_solve_status_str(st)));
    put(&out, "ray", st == JAOS_SOLVE_INFEASIBLE ? numeric(rr, nr)
                                                 : numeric(cr, nc));
    return record_done(&out);
}

static SEXP r_read_cone_duals(SEXP p, SEXP path)
{
    jaos_model *m = model_of(p);
    const int64_t nk = jaos_num_cones(m);
    int64_t *size = scratch_index(nk);
    int64_t total = 0;
    for (int64_t k = 0; k < nk; k++) {
        check(m, jaos_cone(m, k, NULL, size + k, NULL));
        total += size[k];
    }
    double *z = scratch(total);
    check(m, jaos_read_cone_duals(m, CHAR(STRING_ELT(path, 0)), z));
    SEXP out = PROTECT(allocVector(VECSXP, (R_xlen_t)nk));
    int64_t at = 0;
    for (int64_t k = 0; k < nk; k++) {
        SET_VECTOR_ELT(out, k, numeric(z + at, size[k]));
        at += size[k];
    }
    UNPROTECT(1);
    return out;
}

static SEXP r_read_basis(SEXP p, SEXP path, SEXP mps)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    jaos_basis_status *cs = (jaos_basis_status *)R_alloc(nc + 1, sizeof *cs);
    jaos_basis_status *rs = (jaos_basis_status *)R_alloc(nr + 1, sizeof *rs);
    const char *f = CHAR(STRING_ELT(path, 0));
    check(m, asLogical(mps) ? jaos_read_mps_basis(m, f, cs, rs)
                            : jaos_read_basis(m, f, cs, rs));
    return basis_pair(cs, nc, rs, nr);
}

static SEXP r_read_vector(SEXP p, SEXP path, SEXP rows)
{
    jaos_model *m = model_of(p);
    const int r = asLogical(rows);
    const int64_t n = r ? jaos_num_row(m) : jaos_num_col(m);
    double *v = scratch(n);
    const char *f = CHAR(STRING_ELT(path, 0));
    check(m, r ? jaos_read_duals(m, f, v) : jaos_read_point(m, f, v));
    return numeric(v, n);
}

static SEXP r_solution_file_status(SEXP p, SEXP path)
{
    jaos_model *m = model_of(p);
    jaos_solve_status st = JAOS_SOLVE_NOT_RUN;
    check(m, jaos_solution_file_status(m, CHAR(STRING_ELT(path, 0)), &st));
    return mkString(jaos_solve_status_str(st));
}

static SEXP r_cone_dual(SEXP p, SEXP k)
{
    jaos_model *m = model_of(p);
    const int64_t kk = (int64_t)asReal(k);
    int64_t n = 0;
    check(m, jaos_cone(m, kk, NULL, &n, NULL));
    double *z = scratch(n);
    check(m, jaos_cone_dual(m, kk, z));
    return numeric(z, n);
}

static SEXP r_certificate(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nr = jaos_num_row(m);
    double *y = scratch(nr);
    return jaos_certificate(m, y) == JAOS_OK ? numeric(y, nr) : R_NilValue;
}

static SEXP r_unbounded_ray(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m);
    double *d = scratch(nc);
    return jaos_unbounded_ray(m, d) == JAOS_OK ? numeric(d, nc) : R_NilValue;
}

static SEXP r_check_solution(SEXP p, SEXP x, SEXP y, SEXP z, SEXP tol)
{
    jaos_model *m = model_of(p);
    need(x, jaos_num_col(m), "x");
    const double *yd = NULL;
    if (!isNull(y)) {
        need(y, jaos_num_row(m), "row_dual");
        yd = doubles(y);
    }
    jaos_check_report c;
    memset(&c, 0, sizeof c);
    if (isNull(z)) {
        check(m, jaos_check_solution(m, doubles(x), yd, asReal(tol), &c));
    } else {
        need(z, cone_total(m), "cone_dual");
        check(m, jaos_check_conic_solution(m, doubles(x), yd, doubles(z),
                                           asReal(tol), &c));
    }
    record r = record_new(20);
    PUT_REAL(r, c, max_col_violation);
    PUT_REAL(r, c, max_row_violation);
    PUT_REAL(r, c, max_row_violation_relative);
    PUT_REAL(r, c, max_dual_violation);
    PUT_REAL(r, c, primal_objective);
    PUT_REAL(r, c, dual_objective);
    PUT_REAL(r, c, objective_gap);
    PUT_REAL(r, c, gap_positive);
    PUT_REAL(r, c, gap_negative);
    PUT_REAL(r, c, max_dropped_multiplier);
    PUT_REAL(r, c, dropped_terms);
    PUT_REAL(r, c, certified_suboptimality);
    PUT_REAL(r, c, unquantified_rays);
    PUT_REAL(r, c, relative_suboptimality);
    PUT_BOOL(r, c, primal_feasible);
    PUT_BOOL(r, c, dual_feasible);
    PUT_BOOL(r, c, checked_duals);
    PUT_BOOL(r, c, gap_certified);
    PUT_REAL(r, c, max_integrality_violation);
    PUT_REAL(r, c, max_cone_violation);
    return record_done(&r);
}

static SEXP r_check_certificate(SEXP p, SEXP y, SEXP z, SEXP tol)
{
    jaos_model *m = model_of(p);
    need(y, jaos_num_row(m), "row_ray");
    jaos_certificate_report c;
    memset(&c, 0, sizeof c);
    if (isNull(z)) {
        check(m, jaos_check_certificate(m, doubles(y), asReal(tol), &c));
    } else {
        need(z, cone_total(m), "cone_ray");
        check(m, jaos_check_conic_certificate(m, doubles(y), doubles(z),
                                              asReal(tol), &c));
    }
    record r = record_new(4);
    PUT_REAL(r, c, sup_columns);
    PUT_REAL(r, c, inf_rows);
    PUT_REAL(r, c, gap);
    PUT_BOOL(r, c, certified);
    return record_done(&r);
}

static SEXP r_check_ray(SEXP p, SEXP d, SEXP tol)
{
    jaos_model *m = model_of(p);
    need(d, jaos_num_col(m), "col_ray");
    jaos_ray_report c;
    memset(&c, 0, sizeof c);
    check(m, jaos_check_ray(m, doubles(d), asReal(tol), &c));
    record r = record_new(5);
    PUT_REAL(r, c, rate);
    PUT_REAL(r, c, max_col_escape);
    PUT_REAL(r, c, max_row_escape);
    PUT_REAL(r, c, curvature);
    PUT_BOOL(r, c, certified);
    return record_done(&r);
}

static SEXP r_verify(SEXP p, SEXP cs, SEXP rs)
{
    jaos_model *m = model_of(p);
    jaos_verify_report v;
    memset(&v, 0, sizeof v);
    if (isNull(cs))
        check(m, jaos_verify(m, &v));
    else
        check(m, jaos_verify_basis(
                     m, basis_codes(cs, jaos_num_col(m), "col_status"),
                     basis_codes(rs, jaos_num_row(m), "row_status"), &v));
    record r = record_new(11);
    put(&r, "status", mkString(proof_names[v.status <= 2 ? v.status : 2]));
    put(&r, "stage", mkString(stage_names[v.stage <= 3 ? v.stage : 0]));
    PUT_REAL(r, v, bound_bits);
    PUT_REAL(r, v, capacity_bits);
    PUT_REAL(r, v, blocks);
    PUT_REAL(r, v, largest_block);
    PUT_INDEX(r, v, at_row);
    PUT_INDEX(r, v, at_col);
    PUT_REAL(r, v, violation);
    PUT_REAL(r, v, bytes_held);
    PUT_REAL(r, v, terms);
    return record_done(&r);
}

static SEXP r_exact(SEXP p, SEXP idx, SEXP which)
{
    jaos_model *m = model_of(p);
    const int w = asInteger(which);
    const R_xlen_t n = w == 2 ? 1 : XLENGTH(idx);
    const int64_t *c = int64s(idx);
    SEXP out = PROTECT(allocVector(STRSXP, n));
    for (R_xlen_t k = 0; k < n; k++) {
        const char *s = NULL;
        jaos_status st;
        switch (w) {
        case 0:
            st = jaos_exact_col_value(m, c[k], &s);
            break;
        case 1:
            st = jaos_exact_row_dual(m, c[k], &s);
            break;
        case 2:
            st = jaos_exact_objective(m, &s);
            break;
        case 3:
            st = jaos_exact_row_multiplier(m, c[k], &s);
            break;
        default:
            st = jaos_exact_col_direction(m, c[k], &s);
            break;
        }
        check(m, st);
        SET_STRING_ELT(out, k, mkChar(s != NULL ? s : ""));
    }
    UNPROTECT(1);
    return out;
}

static SEXP r_exact_ray(SEXP p, SEXP unbounded)
{
    jaos_model *m = model_of(p);
    jaos_exact_ray_report e;
    memset(&e, 0, sizeof e);
    check(m, asLogical(unbounded) ? jaos_exact_unbounded_ray(m, &e)
                                  : jaos_exact_certificate(m, &e));
    record r = record_new(8);
    PUT_BOOL(r, e, derived);
    PUT_REAL(r, e, bound_bits);
    PUT_REAL(r, e, capacity_bits);
    PUT_REAL(r, e, blocks);
    PUT_REAL(r, e, largest_block);
    PUT_INDEX(r, e, at_row);
    PUT_REAL(r, e, bytes_held);
    PUT_REAL(r, e, terms);
    return record_done(&r);
}

static SEXP r_check_proof(SEXP p, SEXP path)
{
    jaos_model *m = model_of(p);
    jaos_proof_report c;
    memset(&c, 0, sizeof c);
    check(m, jaos_check_proof(m, CHAR(STRING_ELT(path, 0)), &c));
    record r = record_new(8);
    PUT_BOOL(r, c, primal);
    PUT_BOOL(r, c, dual);
    PUT_BOOL(r, c, objective);
    PUT_INDEX(r, c, bad_row);
    PUT_INDEX(r, c, bad_col);
    PUT_REAL(r, c, terms);
    put(&r, "kind", mkString(kind_names[c.kind <= 2 ? c.kind : 0]));
    PUT_BOOL(r, c, certified);
    return record_done(&r);
}

static SEXP r_iis(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    jaos_iis_side *rs = (jaos_iis_side *)R_alloc(nr + 1, sizeof *rs);
    jaos_iis_side *cs = (jaos_iis_side *)R_alloc(nc + 1, sizeof *cs);
    jaos_iis_report c;
    memset(&c, 0, sizeof c);
    check(m, jaos_iis(m, rs, cs, &c));
    record r = record_new(3);
    put(&r, "row_side", side_labels(rs, nr));
    put(&r, "col_side", side_labels(cs, nc));
    record q = record_new(5);
    PUT_REAL(q, c, members);
    PUT_REAL(q, c, candidates);
    PUT_REAL(q, c, solves);
    PUT_REAL(q, c, work_units);
    PUT_BOOL(q, c, from_certificate);
    put(&r, "report", record_done(&q));
    return record_done(&r);
}

static SEXP r_iis_model(SEXP p, SEXP rs, SEXP cs)
{
    jaos_model *m = model_of(p);
    const jaos_iis_side *r = side_codes(rs, jaos_num_row(m), "row_side");
    const jaos_iis_side *c = side_codes(cs, jaos_num_col(m), "col_side");
    SEXP keep = PROTECT(kept_copy(p));
    jaos_model *out = NULL;
    check(m, jaos_iis_model(m, r, c, &out));
    SEXP v = wrap(out, keep);
    UNPROTECT(1);
    return v;
}

static SEXP r_feasrelax(SEXP p, SEXP scope)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *rm = scratch(nr), *cm = scratch(nc);
    jaos_relax_report c;
    memset(&c, 0, sizeof c);
    check(m, jaos_feasrelax(m, (jaos_relax_scope)asInteger(scope), rm, cm,
                            &c));
    record r = record_new(3);
    put(&r, "row_move", numeric(rm, nr));
    put(&r, "col_move", numeric(cm, nc));
    record q = record_new(8);
    PUT_REAL(q, c, total);
    PUT_REAL(q, c, rows_moved);
    PUT_REAL(q, c, cols_moved);
    PUT_INDEX(q, c, at_row);
    PUT_INDEX(q, c, at_col);
    PUT_REAL(q, c, largest);
    PUT_REAL(q, c, work_units);
    put(&q, "status", mkString(jaos_solve_status_str(c.status)));
    put(&r, "report", record_done(&q));
    return record_done(&r);
}

static SEXP r_cost_ranging(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m);
    double *lo = scratch(nc), *hi = scratch(nc);
    check(m, jaos_cost_ranging(m, lo, hi));
    record r = record_new(2);
    put(&r, "lower", numeric(lo, nc));
    put(&r, "upper", numeric(hi, nc));
    return record_done(&r);
}

static SEXP r_bound_ranging(SEXP p, SEXP rows)
{
    jaos_model *m = model_of(p);
    const int rw = asLogical(rows);
    const int64_t n = rw ? jaos_num_row(m) : jaos_num_col(m);
    double *a = scratch(n), *b = scratch(n), *c = scratch(n), *d = scratch(n);
    check(m, rw ? jaos_rhs_ranging(m, a, b, c, d)
                : jaos_bound_ranging(m, a, b, c, d));
    record r = record_new(4);
    put(&r, "lower_lo", numeric(a, n));
    put(&r, "lower_hi", numeric(b, n));
    put(&r, "upper_lo", numeric(c, n));
    put(&r, "upper_hi", numeric(d, n));
    return record_done(&r);
}

static SEXP r_statistics(SEXP p)
{
    jaos_model *m = model_of(p);
    jaos_model_stats s;
    memset(&s, 0, sizeof s);
    check(m, jaos_model_statistics(m, &s));
    record r = record_new(26);
    PUT_REAL(r, s, num_row);
    PUT_REAL(r, s, num_col);
    PUT_REAL(r, s, num_nz);
    PUT_REAL(r, s, integer_col);
    PUT_REAL(r, s, binary_col);
    PUT_REAL(r, s, equality_row);
    PUT_REAL(r, s, ranged_row);
    PUT_REAL(r, s, one_sided_row);
    PUT_REAL(r, s, free_row);
    PUT_REAL(r, s, fixed_col);
    PUT_REAL(r, s, ranged_col);
    PUT_REAL(r, s, one_sided_col);
    PUT_REAL(r, s, free_col);
    PUT_REAL(r, s, empty_row);
    PUT_REAL(r, s, empty_col);
    PUT_REAL(r, s, obj_nz);
    PUT_REAL(r, s, min_abs);
    PUT_REAL(r, s, max_abs);
    PUT_REAL(r, s, obj_min_abs);
    PUT_REAL(r, s, obj_max_abs);
    PUT_REAL(r, s, semicontinuous_col);
    PUT_REAL(r, s, sos_set);
    PUT_REAL(r, s, indicator_row);
    PUT_REAL(r, s, quadratic_col);
    PUT_REAL(r, s, cone_set);
    PUT_REAL(r, s, quadratic_row);
    return record_done(&r);
}

static SEXP r_presolve_report(SEXP p)
{
    jaos_model *m = model_of(p);
    jaos_presolve_report s;
    memset(&s, 0, sizeof s);
    check(m, jaos_presolve_result(m, &s));
    record r = record_new(18);
    PUT_REAL(r, s, num_row);
    PUT_REAL(r, s, num_col);
    PUT_REAL(r, s, num_nz);
    PUT_REAL(r, s, rounds);
    PUT_REAL(r, s, fixed_col);
    PUT_REAL(r, s, empty_row);
    PUT_REAL(r, s, empty_col);
    PUT_REAL(r, s, singleton_row);
    PUT_REAL(r, s, singleton_col);
    PUT_REAL(r, s, free_col_singleton);
    PUT_REAL(r, s, forcing_row);
    PUT_REAL(r, s, redundant_row);
    PUT_REAL(r, s, implied_free_col);
    PUT_REAL(r, s, tightened_bound);
    PUT_REAL(r, s, duplicate_row);
    PUT_REAL(r, s, duplicate_col);
    PUT_REAL(r, s, dominated_col);
    PUT_REAL(r, s, aggregated_col);
    return record_done(&r);
}

static SEXP r_mip_result(SEXP p)
{
    jaos_model *m = model_of(p);
    jaos_mip_report s;
    memset(&s, 0, sizeof s);
    check(m, jaos_mip_result(m, &s));
    record r = record_new(12);
    PUT_REAL(r, s, nodes);
    PUT_REAL(r, s, lp_solves);
    PUT_BOOL(r, s, has_incumbent);
    PUT_REAL(r, s, incumbent);
    PUT_REAL(r, s, bound);
    PUT_REAL(r, s, cuts);
    PUT_REAL(r, s, heuristic_points);
    PUT_REAL(r, s, first_incumbent_node);
    PUT_REAL(r, s, fixed_cols);
    PUT_REAL(r, s, tightened);
    PUT_REAL(r, s, symmetry_generators);
    PUT_REAL(r, s, symmetry_orbits);
    return record_done(&r);
}

static SEXP r_mip_incumbent(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m);
    double *x = scratch(nc);
    double obj = 0.0;
    if (jaos_mip_incumbent(m, x, &obj) != JAOS_OK)
        return R_NilValue;
    record r = record_new(2);
    put(&r, "x", numeric(x, nc));
    put(&r, "objective", ScalarReal(obj));
    return record_done(&r);
}

static SEXP r_mip_pool(SEXP p)
{
    jaos_model *m = model_of(p);
    int64_t n = 0;
    check(m, jaos_mip_pool_count(m, &n));
    const int64_t nc = jaos_num_col(m);
    SEXP out = PROTECT(allocVector(VECSXP, (R_xlen_t)n));
    for (int64_t k = 0; k < n; k++) {
        record r = record_new(2);
        put(&r, "objective", ScalarReal(0.0));
        put(&r, "x", allocVector(REALSXP, (R_xlen_t)nc));
        check(m, jaos_mip_pool_solution(m, k, REAL(VECTOR_ELT(r.v, 1)),
                                        REAL(VECTOR_ELT(r.v, 0))));
        SET_VECTOR_ELT(out, k, record_done(&r));
    }
    UNPROTECT(1);
    return out;
}

static SEXP r_set_mip_start(SEXP p, SEXP x)
{
    jaos_model *m = model_of(p);
    if (isNull(x)) {
        check(m, jaos_set_mip_start(m, NULL));
    } else {
        need(x, jaos_num_col(m), "x");
        check(m, jaos_set_mip_start(m, doubles(x)));
    }
    return p;
}

typedef struct {
    SEXP call;
    int stop;
} r_call;

static void eval_call(void *d)
{
    r_call *c = d;
    SEXP r = eval(c->call, R_GlobalEnv);
    c->stop = isString(r) && XLENGTH(r) > 0 &&
              STRING_ELT(r, 0) != NA_STRING &&
              strcmp(CHAR(STRING_ELT(r, 0)), "stop") == 0;
}

static int call_r(SEXP call)
{
    PROTECT(call);
    r_call c = {call, 0};
    const Rboolean ok = R_ToplevelExec(eval_call, &c);
    UNPROTECT(1);
    return !ok || c.stop;
}

static void r_log_line(void *user, jaos_log_level level, const char *line)
{
    static const char *const levels[] = {"off", "summary", "progress",
                                         "detail"};
    if (user == NULL) {
        REprintf("%s\n", line);
        return;
    }
    SEXP a = PROTECT(mkString(levels[level >= 0 && level <= 3 ? level : 3]));
    SEXP b = PROTECT(mkString(line));
    call_r(lang3((SEXP)user, a, b));
    UNPROTECT(2);
}

static jaos_callback_action r_progress(const jaos_progress *pr, void *user)
{
    record r = record_new(3);
    put(&r, "iterations", ScalarReal((double)pr->iterations));
    put(&r, "work_units", ScalarReal((double)pr->work_units));
    put(&r, "primal_infeasibility", ScalarReal(pr->primal_infeasibility));
    SEXP arg = PROTECT(record_done(&r));
    const int stop = call_r(lang2((SEXP)user, arg));
    UNPROTECT(1);
    return stop ? JAOS_CALLBACK_STOP : JAOS_CALLBACK_CONTINUE;
}

static jaos_callback_action r_incumbent(const jaos_incumbent *inc, void *user)
{
    record r = record_new(5);
    put(&r, "node", ScalarReal((double)inc->node));
    put(&r, "objective", ScalarReal(inc->objective));
    put(&r, "bound", ScalarReal(inc->bound));
    put(&r, "x", numeric(inc->col_value, inc->num_col));
    put(&r, "by_rounding", ScalarLogical(inc->by_rounding));
    SEXP arg = PROTECT(record_done(&r));
    const int stop = call_r(lang2((SEXP)user, arg));
    UNPROTECT(1);
    return stop ? JAOS_CALLBACK_STOP : JAOS_CALLBACK_CONTINUE;
}

static void set_var(SEXP env, const char *name, SEXP v)
{
    PROTECT(v);
    defineVar(install(name), v, env);
    UNPROTECT(1);
}

static jaos_callback_action r_node(jaos_node *ev, void *user)
{
    SEXP env = PROTECT(R_NewEnv(R_EmptyEnv, FALSE, 0));
    SEXP ptr = PROTECT(R_MakeExternalPtr(ev, install("jaos_node"),
                                         R_NilValue));
    set_var(env, ".event", ptr);
    set_var(env, "node", ScalarReal((double)ev->node));
    set_var(env, "depth", ScalarReal((double)ev->depth));
    set_var(env, "objective", ScalarReal(ev->objective));
    set_var(env, "bound", ScalarReal(ev->bound));
    set_var(env, "x", numeric(ev->col_value, ev->num_col));
    set_var(env, "integral", ScalarLogical(ev->integral));
    set_var(env, "branch_col", ScalarReal(index1(ev->branch_col)));
    const int stop = call_r(lang2((SEXP)user, env));
    R_ClearExternalPtr(ptr);
    SEXP b = findVarInFrame(env, install("branch_col"));
    if (b != R_UnboundValue && (isReal(b) || isInteger(b)) && XLENGTH(b) == 1) {
        const double v = asReal(b);
        ev->branch_col = ISNAN(v) ? -1 : (int64_t)v - 1;
    }
    UNPROTECT(2);
    return stop ? JAOS_CALLBACK_STOP : JAOS_CALLBACK_CONTINUE;
}

static SEXP r_node_add_row(SEXP ptr, SEXP idx, SEXP val, SEXP lower,
                           SEXP upper)
{
    if (TYPEOF(ptr) != EXTPTRSXP ||
        R_ExternalPtrTag(ptr) != install("jaos_node") ||
        R_ExternalPtrAddr(ptr) == NULL)
        error("this node event is over");
    const int64_t n = XLENGTH(idx);
    need(val, n, "values");
    if (jaos_node_add_row(R_ExternalPtrAddr(ptr), n, int64s(idx),
                          doubles(val), asReal(lower),
                          asReal(upper)) != JAOS_OK)
        error("the row is not one the node callback may add: a column index "
              "out of range, a non-finite coefficient, or bounds that cross");
    return R_NilValue;
}

static SEXP r_node_add_solution(SEXP ptr, SEXP val)
{
    if (TYPEOF(ptr) != EXTPTRSXP ||
        R_ExternalPtrTag(ptr) != install("jaos_node") ||
        R_ExternalPtrAddr(ptr) == NULL)
        error("this node event is over");
    const int64_t n = XLENGTH(val);
    if (jaos_node_add_solution(R_ExternalPtrAddr(ptr), n,
                               doubles(val)) != JAOS_OK)
        error("the point is not one the node callback may hand: one value "
              "per column, every value finite");
    return R_NilValue;
}

static SEXP r_set_log(SEXP p, SEXP level, SEXP fn)
{
    jaos_model *m = model_of(p);
    const int lv = asInteger(level);
    if (!isNull(fn) && !isFunction(fn))
        error("the log sink is a function or NULL");
    const int to_fn = lv > 0 && !isNull(fn);
    check(m, jaos_set_log_callback(m, lv > 0 ? r_log_line : NULL,
                                   to_fn ? (void *)fn : NULL));
    keep_fn(p, KEEP_LOG, to_fn ? fn : R_NilValue);
    check(m, jaos_set_log_level(m, (jaos_log_level)lv));
    return p;
}

static SEXP r_set_callback(SEXP p, SEXP which, SEXP fn)
{
    jaos_model *m = model_of(p);
    const int w = asInteger(which);
    const int on = !isNull(fn);
    if (on && !isFunction(fn))
        error("a callback is a function or NULL");
    void *user = on ? (void *)fn : NULL;
    jaos_status st;
    if (w == KEEP_PROGRESS)
        st = jaos_set_progress_callback(m, on ? r_progress : NULL, user);
    else if (w == KEEP_INCUMBENT)
        st = jaos_set_incumbent_callback(m, on ? r_incumbent : NULL, user);
    else if (w == KEEP_NODE)
        st = jaos_set_node_callback(m, on ? r_node : NULL, user);
    else
        error("no callback number %d", w);
    check(m, st);
    keep_fn(p, w, fn);
    return p;
}

#define CALL(name, n) {#name, (DL_FUNC)&name, n}

static const R_CallMethodDef calls[] = {
    CALL(r_version, 0),
    CALL(r_infinity, 0),
    CALL(r_status_str, 1),
    CALL(r_model_error, 1),
    CALL(r_option_names, 0),
    CALL(r_new, 0),
    CALL(r_copy, 1),
    CALL(r_read, 3),
    CALL(r_write, 3),
    CALL(r_write_values, 4),
    CALL(r_write_sol_ampl, 3),
    CALL(r_read_options, 2),
    CALL(r_load_lp, 11),
    CALL(r_add_cols, 7),
    CALL(r_add_rows, 6),
    CALL(r_delete, 3),
    CALL(r_dims, 1),
    CALL(r_num_nz, 1),
    CALL(r_col_cost, 2),
    CALL(r_set_col_cost, 3),
    CALL(r_bounds, 3),
    CALL(r_set_bounds, 5),
    CALL(r_sense, 1),
    CALL(r_set_sense, 2),
    CALL(r_offset, 1),
    CALL(r_set_offset, 2),
    CALL(r_entries, 3),
    CALL(r_coefficient, 3),
    CALL(r_set_coefficient, 4),
    CALL(r_names, 3),
    CALL(r_set_names, 4),
    CALL(r_index, 3),
    CALL(r_name, 2),
    CALL(r_set_name, 3),
    CALL(r_set_flags, 4),
    CALL(r_flags, 3),
    CALL(r_has_integer, 1),
    CALL(r_set_quadratic, 4),
    CALL(r_set_row_quadratic, 5),
    CALL(r_quadratic_nz, 2),
    CALL(r_quadratic, 2),
    CALL(r_set_col_quadratic, 3),
    CALL(r_col_quadratic, 2),
    CALL(r_add_sos, 4),
    CALL(r_num_sos, 1),
    CALL(r_sos, 2),
    CALL(r_set_row_indicator, 4),
    CALL(r_row_indicator, 2),
    CALL(r_add_cone, 3),
    CALL(r_cone, 2),
    CALL(r_delete_cones, 2),
    CALL(r_set_option, 3),
    CALL(r_get_option, 2),
    CALL(r_solve, 1),
    CALL(r_status, 1),
    CALL(r_objective, 1),
    CALL(r_counter, 2),
    CALL(r_solution, 1),
    CALL(r_basis, 1),
    CALL(r_set_basis, 3),
    CALL(r_clear_basis, 1),
    CALL(r_read_solution, 2),
    CALL(r_read_certificate, 2),
    CALL(r_read_cone_duals, 2),
    CALL(r_read_basis, 3),
    CALL(r_read_vector, 3),
    CALL(r_solution_file_status, 2),
    CALL(r_cone_dual, 2),
    CALL(r_certificate, 1),
    CALL(r_unbounded_ray, 1),
    CALL(r_check_solution, 5),
    CALL(r_check_certificate, 4),
    CALL(r_check_ray, 3),
    CALL(r_verify, 3),
    CALL(r_exact, 3),
    CALL(r_exact_ray, 2),
    CALL(r_check_proof, 2),
    CALL(r_iis, 1),
    CALL(r_iis_model, 3),
    CALL(r_feasrelax, 2),
    CALL(r_cost_ranging, 1),
    CALL(r_bound_ranging, 2),
    CALL(r_statistics, 1),
    CALL(r_presolve_report, 1),
    CALL(r_mip_result, 1),
    CALL(r_mip_incumbent, 1),
    CALL(r_mip_pool, 1),
    CALL(r_set_mip_start, 2),
    CALL(r_node_add_row, 5),
    CALL(r_node_add_solution, 2),
    CALL(r_set_log, 3),
    CALL(r_set_callback, 3),
    {NULL, NULL, 0},
};

void R_init_jaos(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, calls, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
}
