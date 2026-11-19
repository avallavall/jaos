import sys

root = sys.argv[1]


def sub(text, a, b, count=1):
    n = text.count(a)
    assert n == count, (a[:60], n)
    return text.replace(a, b)


h = open(root + "/src/jaos_internal.h").read()
h = sub(h, "    jm_svec *urow;\n    jm_svec *ucol;\n    double  *u_diag;\n",
        "    jm_svec *urow;\n    jm_svec *ucol;\n    int64_t *uarena_idx;\n    double  *uarena_val;\n    int64_t uarena_n;\n    double  *u_diag;\n")
open(root + "/src/jaos_internal.h", "w").write(h)

s = open(root + "/src/lu.c").read()

s = sub(s, "void jm_lu_init(jm_lu *lu)\n{\n    memset(lu, 0, sizeof *lu);\n}\n",
        "void jm_lu_init(jm_lu *lu)\n{\n    memset(lu, 0, sizeof *lu);\n}\n\n"
        "static bool ucol_owned(const jm_lu *lu, const jm_svec *v)\n{\n"
        "    if (lu->uarena_idx == nullptr || v->idx == nullptr)\n        return false;\n"
        "    const uintptr_t p = (uintptr_t)v->idx, a = (uintptr_t)lu->uarena_idx;\n"
        "    return p >= a && p < a + (uintptr_t)lu->uarena_n * sizeof *lu->uarena_idx;\n}\n\n"
        "static void ucol_drop_arena(jm_lu *lu, jm_svec *ucol, int64_t n)\n{\n"
        "    for (int64_t s = 0; ucol != nullptr && s < n; s++)\n"
        "        if (ucol_owned(lu, &ucol[s]))\n            ucol[s] = (jm_svec){0};\n"
        "    free(lu->uarena_idx);\n    free(lu->uarena_val);\n"
        "    lu->uarena_idx = nullptr;\n    lu->uarena_val = nullptr;\n    lu->uarena_n = 0;\n}\n\n"
        "static bool ucol_push(jm_lu *lu, jm_svec *v, int64_t i, double x)\n{\n"
        "    if (v->n == v->cap && ucol_owned(lu, v)) {\n"
        "        const int64_t cap = v->n < 4 ? 8 : 2 * v->n;\n"
        "        int64_t *idx = jm_alloc_array(cap, sizeof *idx);\n"
        "        double *val = jm_alloc_array(cap, sizeof *val);\n"
        "        if (idx == nullptr || val == nullptr) {\n            free(idx);\n            free(val);\n            return false;\n        }\n"
        "        if (v->n > 0) {\n            memcpy(idx, v->idx, (size_t)v->n * sizeof *idx);\n            memcpy(val, v->val, (size_t)v->n * sizeof *val);\n        }\n"
        "        v->idx = idx;\n        v->val = val;\n        v->cap = cap;\n    }\n"
        "    return jm_svec_push(v, i, x);\n}\n")

s = sub(s, "    if (lu->ucol)\n        for (int64_t s = 0; s < lu->keep_dim; s++)\n            jm_svec_free(&lu->ucol[s]);\n    free(lu->urow);\n    free(lu->ucol);\n",
        "    if (lu->ucol)\n        for (int64_t s = 0; s < lu->keep_dim; s++)\n            if (!ucol_owned(lu, &lu->ucol[s]))\n                jm_svec_free(&lu->ucol[s]);\n"
        "    free(lu->uarena_idx);\n    free(lu->uarena_val);\n    lu->uarena_idx = nullptr;\n    lu->uarena_val = nullptr;\n    lu->uarena_n = 0;\n"
        "    free(lu->urow);\n    free(lu->ucol);\n")

s = sub(s, "    jm_svec *urow = lu->urow, *ucol = lu->ucol, ft = lu->ft;\n",
        "    jm_svec *urow = lu->urow, *ucol = lu->ucol, ft = lu->ft;\n    ucol_drop_arena(lu, ucol, lu->keep_dim);\n")

old_build = """    if (lu->rank == dim) {

        for (int64_t k = 0; k < lacc.n; k++) {
            assert(inv_row[lacc.idx[k]] >= 0);
            lacc.idx[k] = inv_row[lacc.idx[k]];
        }
        for (int64_t s = 0; s < dim; s++) {
            for (int64_t k = us_start[s]; k < us_start[s + 1]; k++) {
                int64_t c = lu->inv_col[uacc.idx[k]];
                assert(c >= 0);
                double v = uacc.val[k];
                if (!jm_svec_push(&lu->urow[s], c, v) ||
                    !jm_svec_push(&lu->ucol[c], s, v)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
            }
        }
    }
"""
new_build = """    if (lu->rank == dim) {

        for (int64_t k = 0; k < lacc.n; k++) {
            assert(inv_row[lacc.idx[k]] >= 0);
            lacc.idx[k] = inv_row[lacc.idx[k]];
        }
        const int64_t unnz = uacc.n;
        int64_t *ustart = jm_calloc_array(dim + 1, sizeof *ustart);
        lu->uarena_idx = jm_alloc_array(unnz > 0 ? unnz : 1,
                                        sizeof *lu->uarena_idx);
        lu->uarena_val = jm_alloc_array(unnz > 0 ? unnz : 1,
                                        sizeof *lu->uarena_val);
        if (ustart == nullptr || lu->uarena_idx == nullptr ||
            lu->uarena_val == nullptr) {
            free(ustart);
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        lu->uarena_n = unnz > 0 ? unnz : 1;
        for (int64_t k = 0; k < unnz; k++)
            ustart[lu->inv_col[uacc.idx[k]]]++;
        int64_t off = 0;
        for (int64_t c = dim - 1; c >= 0; c--) {
            const int64_t cnt = ustart[c];
            jm_svec_free(&lu->ucol[c]);
            if (cnt > 0)
                lu->ucol[c] = (jm_svec){ .idx = lu->uarena_idx + off,
                                         .val = lu->uarena_val + off,
                                         .n = 0, .cap = cnt };
            off += cnt;
        }
        free(ustart);
        for (int64_t s = 0; s < dim; s++) {
            for (int64_t k = us_start[s]; k < us_start[s + 1]; k++) {
                int64_t c = lu->inv_col[uacc.idx[k]];
                assert(c >= 0);
                double v = uacc.val[k];
                if (!jm_svec_push(&lu->urow[s], c, v)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
                jm_svec *cv = &lu->ucol[c];
                assert(cv->n < cv->cap);
                cv->idx[cv->n] = s;
                cv->val[cv->n] = v;
                cv->n++;
            }
        }
    }
"""
s = sub(s, old_build, new_build)

s = sub(s, "        if (!jm_svec_push(&lu->ucol[s_out], s, sp[s]) ||\n",
        "        if (!ucol_push(lu, &lu->ucol[s_out], s, sp[s]) ||\n")

if "#include <stdint.h>" not in s:
    s = s.replace("#include <stdlib.h>\n", "#include <stdint.h>\n#include <stdlib.h>\n", 1)
open(root + "/src/lu.c", "w").write(s)
print("patched")
