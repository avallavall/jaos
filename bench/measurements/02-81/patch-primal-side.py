import sys, shutil, os
import os as _os
root = _os.environ.get("JAOS_SRC")
if root is None:
    root = "/mnt/c/Users/vall-/Desktop/projectes/jaos/src"
# JAOS_SRC, when set, is a directory holding the .c files directly.
SRC = root if _os.path.isfile(root + "/model.c") else root + "/src"
dst  = sys.argv[1]
os.makedirs(dst, exist_ok=True)
for f in os.listdir(SRC):
    shutil.copy(SRC + "/" + f, dst + "/" + f)
p = dst + "/simplex.c"
s = open(p, encoding="utf-8").read()

old = """    int64_t nz = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        double zi = z[i];
        if (zi == 0.0)
            continue;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                r[m->a_index[k]] -= s->av[k] * zi;
            nz += m->a_start[v + 1] - m->a_start[v];
        } else {
            r[v - s->ncol] += zi;      /* the column is -e_i */
            nz++;
        }
    }
    jm_work_add(&s->work, nz * JM_WORK_NONZERO);"""
assert old in s, "anchor A"
new = """    int64_t nz = 0;
    double *bc = s->resc;
    memset(bc, 0, (size_t)s->nrow * sizeof *bc);
    for (int64_t i = 0; i < s->nrow; i++) {
        double zi = z[i];
        if (zi == 0.0)
            continue;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
                const int64_t ii = m->a_index[k];
                const double t = -(s->av[k] * zi);
                const double a = r[ii], u = a + t;
                bc[ii] += (fabs(a) >= fabs(t)) ? ((a - u) + t) : ((t - u) + a);
                r[ii] = u;
            }
            nz += m->a_start[v + 1] - m->a_start[v];
        } else {
            const int64_t ii = v - s->ncol;   /* the column is -e_i */
            const double a = r[ii], u = a + zi;
            bc[ii] += (fabs(a) >= fabs(zi)) ? ((a - u) + zi) : ((zi - u) + a);
            r[ii] = u;
            nz++;
        }
    }
    for (int64_t i = 0; i < s->nrow; i++)
        if (isfinite(r[i]) && isfinite(bc[i]))
            r[i] += bc[i];
    jm_work_add(&s->work, nz * JM_WORK_NONZERO);"""
s = s.replace(old, new, 1)

s = s.replace("    double *rhsc;\n", "    double *rhsc;\n    double *resc;\n", 1)
s = s.replace("    s->rhsc   = jm_calloc_array(s->nrow, sizeof(double));\n",
              "    s->rhsc   = jm_calloc_array(s->nrow, sizeof(double));\n"
              "    s->resc   = jm_calloc_array(s->nrow, sizeof(double));\n", 1)
s = s.replace("!s->col || !s->raw || !s->rhsc ||",
              "!s->col || !s->raw || !s->rhsc || !s->resc ||", 1)
s = s.replace("free(s->col); free(s->raw); free(s->rhsc);",
              "free(s->col); free(s->raw); free(s->rhsc); free(s->resc);", 1)
for probe in ["double *resc;", "s->resc   = jm_calloc", "!s->resc", "free(s->resc)"]:
    assert probe in s, probe
open(p, "w", encoding="utf-8").write(s)
print("patched subtract_basis_times")
