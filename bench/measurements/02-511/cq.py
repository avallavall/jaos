# cq.py TREE — clique_round extends a clique only over the first literal's neighbours, in the order of lits, and pushes its cut from the members
import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/mip.c', [
("""    litval *lits = jm_alloc_array(2 * ne, sizeof *lits);
    bool *used = calloc((size_t)(2 * nc), sizeof *used);
    int64_t *members = jm_alloc_array(2 * nc, sizeof *members);
    if (lits == nullptr || used == nullptr || members == nullptr) {
        free(lits); free(used); free(members);
        return -1;
    }""", """    litval *lits = jm_alloc_array(2 * ne, sizeof *lits);
    bool *used = calloc((size_t)(2 * nc), sizeof *used);
    int64_t *members = jm_alloc_array(2 * nc, sizeof *members);
    int64_t *pos = jm_alloc_array(2 * nc, sizeof *pos);
    int64_t *nbr = jm_alloc_array(2 * nc, sizeof *nbr);
    if (lits == nullptr || used == nullptr || members == nullptr ||
        pos == nullptr || nbr == nullptr) {
        free(lits); free(used); free(members); free(pos); free(nbr);
        return -1;
    }"""),
("""        nl = w;
    }
    for (int64_t s = 0; s < nl && !budget_gone(m, *work); s++) {
        if (used[lits[s].lit] || !(lits[s].v > tol))
            continue;
        int64_t nm = 0;
        members[nm++] = lits[s].lit;
        double sum = lits[s].v;
        for (int64_t t = 0; t < nl; t++) {
            if (t == s || used[lits[t].lit])
                continue;
            const int64_t v = lits[t].lit;""", """        nl = w;
    }
    for (int64_t k = 0; k < 2 * nc; k++)
        pos[k] = -1;
    for (int64_t k = 0; k < nl; k++)
        pos[lits[k].lit] = k;
    for (int64_t s = 0; s < nl && !budget_gone(m, *work); s++) {
        if (used[lits[s].lit] || !(lits[s].v > tol))
            continue;
        int64_t nm = 0;
        members[nm++] = lits[s].lit;
        double sum = lits[s].v;
        const int64_t u0 = lits[s].lit;
        int64_t nn = 0;
        for (int64_t p = t->adj_start[u0]; p < t->adj_start[u0 + 1]; p++)
            if (pos[t->adj[p]] >= 0)
                nbr[nn++] = pos[t->adj[p]];
        qsort(nbr, (size_t)nn, sizeof *nbr, pos_cmp);
        for (int64_t a = 0; a < nn; a++) {
            const int64_t tt = nbr[a];
            if ((a > 0 && nbr[a - 1] == tt) || tt == s ||
                used[lits[tt].lit])
                continue;
            const int64_t v = lits[tt].lit;"""),
("""            members[nm++] = v;
            sum += lits[t].v;
        }""", """            members[nm++] = v;
            sum += lits[tt].v;
        }"""),
("""        memset(cut, 0, (size_t)nc * sizeof *cut);
        int64_t ncompl = 0;
        for (int64_t q = 0; q < nm; q++) {
            const int64_t j = members[q] / 2;
            const bool compl = members[q] % 2 == 1;
            cut[j] = compl ? 1.0 : -1.0;
            ncompl += compl;
            used[members[q]] = true;
        }
        if (!cutbuf_push(cb, cut, nc, (double)ncompl - 1.0,
                         (sum - 1.0) / sqrt((double)nm))) {
            added = -1;
            break;
        }
        added++;
    }
    free(lits); free(used); free(members);
    return added;""", """        int64_t ncompl = 0;
        for (int64_t q = 0; q < nm; q++) {
            const int64_t j = members[q] / 2;
            const bool compl = members[q] % 2 == 1;
            cut[j] = compl ? 1.0 : -1.0;
            ncompl += compl;
            used[members[q]] = true;
            nbr[q] = j;
        }
        qsort(nbr, (size_t)nm, sizeof *nbr, pos_cmp);
        const bool pushed = cutbuf_push_sorted(cb, cut, nbr, nm,
                                               (double)ncompl - 1.0,
                                               (sum - 1.0) / sqrt((double)nm));
        for (int64_t q = 0; q < nm; q++)
            cut[nbr[q]] = 0.0;
        if (!pushed) {
            added = -1;
            break;
        }
        added++;
    }
    free(lits); free(used); free(members); free(pos); free(nbr);
    return added;"""),
("""static int cut_finish(const jaos_model *lp, cutbuf *cb, double *cut,""", """static bool cutbuf_push_sorted(cutbuf *cb, const double *cut,
                               const int64_t *col, int64_t n, double lo,
                               double eff)
{
    if (!JM_GROW(cb->start, cb->cap_start, cb->n + 2) ||
        !JM_GROW(cb->lo, cb->cap_lo, cb->n + 2) ||
        !JM_GROW(cb->eff, cb->cap_eff, cb->n + 2))
        return false;
    cb->eff[cb->n] = eff;
    int64_t nz = 0;
    for (int64_t k = 0; k < n; k++)
        nz += cut[col[k]] != 0.0;
    if (!JM_GROW(cb->idx, cb->cap_idx, cb->nnz + nz) ||
        !JM_GROW(cb->val, cb->cap_val, cb->nnz + nz))
        return false;
    cb->start[cb->n] = cb->nnz;
    for (int64_t k = 0; k < n; k++) {
        if (cut[col[k]] == 0.0)
            continue;
        cb->idx[cb->nnz] = col[k];
        cb->val[cb->nnz] = cut[col[k]];
        cb->nnz++;
    }
    cb->lo[cb->n] = lo;
    cb->n++;
    cb->start[cb->n] = cb->nnz;
    return true;
}

static int cut_finish(const jaos_model *lp, cutbuf *cb, double *cut,"""),
])
patch(root + '/src/mip.c', [
("""static int64_t clique_round(const jaos_model *m, const clique_table *t,""",
 """static int pos_cmp(const void *pa, const void *pb)
{
    const int64_t a = *(const int64_t *)pa, b = *(const int64_t *)pb;
    return a < b ? -1 : a > b;
}

static int64_t clique_round(const jaos_model *m, const clique_table *t,"""),
])
