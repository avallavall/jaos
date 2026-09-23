/* SPDX-License-Identifier: Apache-2.0 */
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <stdint.h>
#include <string.h>

#include "jaos.h"

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
    if (TYPEOF(p) != EXTPTRSXP)
        error("not a JAOS model");
    jaos_model *m = R_ExternalPtrAddr(p);
    if (m == NULL)
        error("the JAOS model was freed");
    return m;
}

static void check(jaos_model *m, jaos_status st)
{
    if (st != JAOS_OK)
        error("JAOS: %s", jaos_model_error(m));
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

static SEXP numeric(const double *x, int64_t n)
{
    SEXP v = PROTECT(allocVector(REALSXP, (R_xlen_t)n));
    if (n > 0)
        memcpy(REAL(v), x, (size_t)n * sizeof *x);
    UNPROTECT(1);
    return v;
}

static SEXP r_version(void)
{
    return mkString(jaos_version());
}

static SEXP r_new(void)
{
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        error("JAOS: jaos_model_new failed");
    SEXP p = PROTECT(R_MakeExternalPtr(m, install("jaos_model"), R_NilValue));
    R_RegisterCFinalizerEx(p, model_finalize, TRUE);
    UNPROTECT(1);
    return p;
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
    return R_NilValue;
}

static SEXP r_write(SEXP p, SEXP path, SEXP kind)
{
    jaos_model *m = model_of(p);
    const char *f = CHAR(STRING_ELT(path, 0));
    const char *k = CHAR(STRING_ELT(kind, 0));
    jaos_status st = strcmp(k, "lp") == 0 ? jaos_write_lp(m, f)
                   : strcmp(k, "solution") == 0 ? jaos_write_solution(m, f)
                   : jaos_write_mps(m, f);
    check(m, st);
    return R_NilValue;
}

static SEXP r_load_lp(SEXP p, SEXP sense, SEXP offset, SEXP cost, SEXP cl,
                      SEXP cu, SEXP rl, SEXP ru, SEXP start, SEXP index,
                      SEXP value)
{
    jaos_model *m = model_of(p);
    check(m, jaos_load_lp(m, XLENGTH(cost), XLENGTH(rl),
                          asInteger(sense) ? JAOS_MAXIMIZE : JAOS_MINIMIZE,
                          asReal(offset), doubles(cost), doubles(cl),
                          doubles(cu), doubles(rl), doubles(ru),
                          XLENGTH(value), int64s(start), int64s(index),
                          doubles(value)));
    return R_NilValue;
}

static SEXP r_set_integer(SEXP p, SEXP cols)
{
    jaos_model *m = model_of(p);
    const int64_t *c = int64s(cols);
    for (R_xlen_t k = 0; k < XLENGTH(cols); k++)
        check(m, jaos_set_col_integer(m, c[k], true));
    return R_NilValue;
}

static SEXP r_set_quadratic(SEXP p, SEXP rows, SEXP cols, SEXP vals)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_quadratic(m, XLENGTH(vals), int64s(rows), int64s(cols),
                                doubles(vals)));
    return R_NilValue;
}

static SEXP r_set_row_quadratic(SEXP p, SEXP row, SEXP rows, SEXP cols,
                                SEXP vals)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_row_quadratic(m, (int64_t)asReal(row), XLENGTH(vals),
                                    int64s(rows), int64s(cols),
                                    doubles(vals)));
    return R_NilValue;
}

static SEXP r_add_cone(SEXP p, SEXP rotated, SEXP cols)
{
    jaos_model *m = model_of(p);
    check(m, jaos_add_cone(m, asLogical(rotated) ? JAOS_CONE_ROTATED
                                                 : JAOS_CONE_QUADRATIC,
                           XLENGTH(cols), int64s(cols)));
    return R_NilValue;
}

static SEXP r_set_option(SEXP p, SEXP name, SEXP value)
{
    jaos_model *m = model_of(p);
    check(m, jaos_set_option(m, CHAR(STRING_ELT(name, 0)),
                             CHAR(STRING_ELT(value, 0))));
    return R_NilValue;
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
    return R_NilValue;
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

static SEXP r_solution(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = (double *)R_alloc(nc + 1, sizeof *x);
    double *d = (double *)R_alloc(nc + 1, sizeof *d);
    double *a = (double *)R_alloc(nr + 1, sizeof *a);
    double *y = (double *)R_alloc(nr + 1, sizeof *y);
    check(m, jaos_solution(m, x, a, y, d));
    SEXP out = PROTECT(allocVector(VECSXP, 4));
    SET_VECTOR_ELT(out, 0, numeric(x, nc));
    SET_VECTOR_ELT(out, 1, numeric(a, nr));
    SET_VECTOR_ELT(out, 2, numeric(y, nr));
    SET_VECTOR_ELT(out, 3, numeric(d, nc));
    SEXP names = PROTECT(allocVector(STRSXP, 4));
    SET_STRING_ELT(names, 0, mkChar("x"));
    SET_STRING_ELT(names, 1, mkChar("row_activity"));
    SET_STRING_ELT(names, 2, mkChar("row_dual"));
    SET_STRING_ELT(names, 3, mkChar("reduced_cost"));
    setAttrib(out, R_NamesSymbol, names);
    UNPROTECT(2);
    return out;
}

static SEXP r_cone_dual(SEXP p, SEXP k)
{
    jaos_model *m = model_of(p);
    const int64_t kk = (int64_t)asReal(k);
    int64_t n = 0;
    check(m, jaos_cone(m, kk, NULL, &n, NULL));
    double *z = (double *)R_alloc(n + 1, sizeof *z);
    check(m, jaos_cone_dual(m, kk, z));
    return numeric(z, n);
}

static SEXP r_certificate(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nr = jaos_num_row(m);
    double *y = (double *)R_alloc(nr + 1, sizeof *y);
    return jaos_certificate(m, y) == JAOS_OK ? numeric(y, nr) : R_NilValue;
}

static SEXP r_unbounded_ray(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m);
    double *d = (double *)R_alloc(nc + 1, sizeof *d);
    return jaos_unbounded_ray(m, d) == JAOS_OK ? numeric(d, nc) : R_NilValue;
}

static SEXP r_mip_result(SEXP p)
{
    jaos_model *m = model_of(p);
    jaos_mip_report r;
    check(m, jaos_mip_result(m, &r));
    SEXP out = PROTECT(allocVector(VECSXP, 5));
    SET_VECTOR_ELT(out, 0, ScalarReal((double)r.nodes));
    SET_VECTOR_ELT(out, 1, ScalarLogical(r.has_incumbent));
    SET_VECTOR_ELT(out, 2, ScalarReal(r.incumbent));
    SET_VECTOR_ELT(out, 3, ScalarReal(r.bound));
    SET_VECTOR_ELT(out, 4, ScalarReal((double)r.first_incumbent_node));
    SEXP names = PROTECT(allocVector(STRSXP, 5));
    SET_STRING_ELT(names, 0, mkChar("nodes"));
    SET_STRING_ELT(names, 1, mkChar("has_incumbent"));
    SET_STRING_ELT(names, 2, mkChar("incumbent"));
    SET_STRING_ELT(names, 3, mkChar("bound"));
    SET_STRING_ELT(names, 4, mkChar("first_incumbent_node"));
    setAttrib(out, R_NamesSymbol, names);
    UNPROTECT(2);
    return out;
}

static void r_log_line(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    REprintf("%s\n", line);
}

static SEXP r_set_log(SEXP p, SEXP level)
{
    jaos_model *m = model_of(p);
    const int lv = asInteger(level);
    check(m, jaos_set_log_callback(m, lv > 0 ? r_log_line : NULL, NULL));
    check(m, jaos_set_log_level(m, (jaos_log_level)lv));
    return R_NilValue;
}

static SEXP r_mip_incumbent(SEXP p)
{
    jaos_model *m = model_of(p);
    const int64_t nc = jaos_num_col(m);
    double *x = (double *)R_alloc(nc + 1, sizeof *x);
    double obj = 0.0;
    if (jaos_mip_incumbent(m, x, &obj) != JAOS_OK)
        return R_NilValue;
    SEXP out = PROTECT(allocVector(VECSXP, 2));
    SET_VECTOR_ELT(out, 0, numeric(x, nc));
    SET_VECTOR_ELT(out, 1, ScalarReal(obj));
    SEXP names = PROTECT(allocVector(STRSXP, 2));
    SET_STRING_ELT(names, 0, mkChar("x"));
    SET_STRING_ELT(names, 1, mkChar("objective"));
    setAttrib(out, R_NamesSymbol, names);
    UNPROTECT(2);
    return out;
}

static const R_CallMethodDef calls[] = {
    {"r_set_log", (DL_FUNC)&r_set_log, 2},
    {"r_mip_incumbent", (DL_FUNC)&r_mip_incumbent, 1},
    {"r_version", (DL_FUNC)&r_version, 0},
    {"r_new", (DL_FUNC)&r_new, 0},
    {"r_read", (DL_FUNC)&r_read, 3},
    {"r_write", (DL_FUNC)&r_write, 3},
    {"r_load_lp", (DL_FUNC)&r_load_lp, 11},
    {"r_set_integer", (DL_FUNC)&r_set_integer, 2},
    {"r_set_quadratic", (DL_FUNC)&r_set_quadratic, 4},
    {"r_set_row_quadratic", (DL_FUNC)&r_set_row_quadratic, 5},
    {"r_add_cone", (DL_FUNC)&r_add_cone, 3},
    {"r_set_option", (DL_FUNC)&r_set_option, 3},
    {"r_get_option", (DL_FUNC)&r_get_option, 2},
    {"r_solve", (DL_FUNC)&r_solve, 1},
    {"r_status", (DL_FUNC)&r_status, 1},
    {"r_objective", (DL_FUNC)&r_objective, 1},
    {"r_dims", (DL_FUNC)&r_dims, 1},
    {"r_solution", (DL_FUNC)&r_solution, 1},
    {"r_cone_dual", (DL_FUNC)&r_cone_dual, 2},
    {"r_certificate", (DL_FUNC)&r_certificate, 1},
    {"r_unbounded_ray", (DL_FUNC)&r_unbounded_ray, 1},
    {"r_mip_result", (DL_FUNC)&r_mip_result, 1},
    {NULL, NULL, 0},
};

void R_init_jaos(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, calls, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
}
