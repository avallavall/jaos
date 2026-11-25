import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/jaos_internal.h', [
("""    int64_t n_updates;
""", """    int64_t n_updates;
    int64_t u_nnz;
    int64_t u_base;
    int64_t lu_base;
"""),
("""void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w);
""", """bool jm_lu_filled(const jm_lu *lu);

void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w);
"""),
])
patch(root + '/src/lu.c', [
("""constexpr double TINY = 1e-300;
""", """constexpr double TINY = 1e-300;

#ifndef JAOS_LU_FILL_X
#define JAOS_LU_FILL_X 3
#endif
constexpr int64_t LU_FILL_X = JAOS_LU_FILL_X;
"""),
("""    svec_release(&lacc, &lu->l_index, &lu->l_value);
""", """    svec_release(&lacc, &lu->l_index, &lu->l_value);
    lu->u_nnz = uacc.n;
    lu->u_base = uacc.n;
    lu->lu_base = uacc.n + lu->l_start[dim];
"""),
("""    for (int64_t k = 0; k < lu->urow[s_out].n; k++)
        jm_svec_erase(&lu->ucol[lu->urow[s_out].idx[k]], s_out);""", """    lu->u_nnz -= lu->urow[s_out].n + lu->ucol[s_out].n;
    for (int64_t k = 0; k < lu->urow[s_out].n; k++)
        jm_svec_erase(&lu->ucol[lu->urow[s_out].idx[k]], s_out);"""),
("""        if (!ucol_push(lu, &lu->ucol[s_out], s, sp[s]) ||
            !jm_svec_push(&lu->urow[s], s_out, sp[s])) {
            lu->rank = -1;
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }
""", """        if (!ucol_push(lu, &lu->ucol[s_out], s, sp[s]) ||
            !jm_svec_push(&lu->urow[s], s_out, sp[s])) {
            lu->rank = -1;
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        lu->u_nnz++;
    }
"""),
("""jaos_status jm_lu_update(jm_lu *lu, int64_t col_out, const double *new_col,
                         double min_pivot_ratio, jm_work *w)""", """bool jm_lu_filled(const jm_lu *lu)
{
    return lu->u_nnz + lu->ft.n - lu->u_base > LU_FILL_X * lu->lu_base;
}

jaos_status jm_lu_update(jm_lu *lu, int64_t col_out, const double *new_col,
                         double min_pivot_ratio, jm_work *w)"""),
])
patch(root + '/src/simplex.c', [
("""    if (s->lu.n_updates >= REFACTOR_EVERY) {
        s->needs_refactor = true;
        return JAOS_OK;
    }""", """    if (s->lu.n_updates >= REFACTOR_EVERY || jm_lu_filled(&s->lu)) {
        s->needs_refactor = true;
        return JAOS_OK;
    }"""),
])
