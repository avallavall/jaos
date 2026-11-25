import sys
root, v = sys.argv[1], sys.argv[2]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
sx = root + '/src/simplex.c'
dx = ("""            } else if (!s->m->cfg.node_solve || s->cold) {""",
      """            } else if (!s->m->cfg.node_solve || s->cold ||
                       s->m->cfg.cut_resolve) {""")
devex = ("""        if (s->m->cfg.node_devex && !s->dual_devex) {""",
         """        if ((s->m->cfg.node_devex || s->m->cfg.cut_resolve) &&
            !s->dual_devex) {""")
exact = ("""constexpr int64_t DSE_RESOLVE_EXACT = 1;""",
         """constexpr int64_t DSE_RESOLVE_EXACT = 0;""")
mip = root + '/src/mip.c'
swap = [
("""                const double key_before = key;
                for (int pass = 0; pass < 2; pass++) {""",
 """                const double key_before = key;
                jaos_model *taken = nullptr;
                for (int pass = 0; pass < 2; pass++) {"""),
("""                        work += lp->num_nz + lp->num_row + lp->num_col;
                        phase_to(&ph, work, PH_LP);
                        budget(pv, m, work);""",
 """                        work += lp->num_nz + lp->num_row + lp->num_col;
                        pv->cfg.node_keep = lp->cfg.node_keep;
                        phase_to(&ph, work, PH_LP);
                        budget(pv, m, work);"""),
("""                        const bool pays = solved && gain > MIP_MIR_AGG_GAIN;
                        jaos_model_free(pv);""",
 """                        const bool pays = solved && gain > MIP_MIR_AGG_GAIN;
                        if (pays)
                            taken = pv;
                        else
                            jaos_model_free(pv);"""),
("""                    const int64_t rr_nz = lp->num_nz;
                    if (cuts_add(lp, add) != JAOS_OK)
                        goto done;""",
 """                    const int64_t rr_nz = lp->num_nz;
                    if (taken == nullptr && cuts_add(lp, add) != JAOS_OK)
                        goto done;"""),
("""                    phase_to(&ph, work, PH_LP);
                    budget(lp, m, work);
                    const double rr_t = now_seconds();
                    lp->cfg.log_cb = m->cfg.log_cb;
                    lp->cfg.log_user = m->cfg.log_user;
                    st = jaos_solve(lp);
                    lp->cfg.log_cb = nullptr;
                    rr_log(m, lp, pass == 0 ? "cuts" : "agg", r, add->n,
                           rr_nz, rr_t, st, sigma);
                    solves++;
                    work += jaos_work_units(lp);
                    iters += jaos_iterations(lp);
                    phase_to(&ph, work, PH_CUTS);""",
 """                    if (taken != nullptr) {
                        jaos_model_free(lp);
                        lp = taken;
                        taken = nullptr;
                        st = JAOS_OK;
                    } else {
                    phase_to(&ph, work, PH_LP);
                    budget(lp, m, work);
                    const double rr_t = now_seconds();
                    lp->cfg.log_cb = m->cfg.log_cb;
                    lp->cfg.log_user = m->cfg.log_user;
                    st = jaos_solve(lp);
                    lp->cfg.log_cb = nullptr;
                    rr_log(m, lp, pass == 0 ? "cuts" : "agg", r, add->n,
                           rr_nz, rr_t, st, sigma);
                    solves++;
                    work += jaos_work_units(lp);
                    iters += jaos_iterations(lp);
                    phase_to(&ph, work, PH_CUTS);
                    }"""),
]
def gd(f):
    return [
("""        const int pushed = cut_finish(lp, cb, cut, rhs, x, nullptr, 0);
        if (pushed < 0) {
            added = -1;
            break;
        }
        added += pushed;""",
 """        int pushed = cut_finish(lp, cb, cut, rhs, x, nullptr, 0);
        if (pushed < 0) {
            added = -1;
            break;
        }
        if (pushed > 0 && (double)(cb->start[cb->n] - cb->start[cb->n - 1]) >
                              %s * (double)nc) {
            cb->n--;
            cb->nnz = cb->start[cb->n];
            pushed = 0;
        }
        added += pushed;""" % f)]
def gb(b):
    return [
("""static int64_t cutbuf_keep_best(cutbuf *cb, int64_t cap)""",
 """static int64_t cutbuf_keep_sparse(cutbuf *cb, double budget)
{
    if (cb->n == 0 || (double)cb->nnz <= budget)
        return cb->n;
    int64_t *ord = jm_alloc_array(cb->n, sizeof *ord);
    bool *keep = calloc((size_t)cb->n, sizeof *keep);
    if (ord == nullptr || keep == nullptr) {
        free(ord);
        free(keep);
        return -1;
    }
    for (int64_t r = 0; r < cb->n; r++)
        ord[r] = r;
    for (int64_t a = 1; a < cb->n; a++) {
        const int64_t t = ord[a];
        const int64_t tn = cb->start[t + 1] - cb->start[t];
        int64_t b = a;
        while (b > 0 && cb->start[ord[b - 1] + 1] - cb->start[ord[b - 1]] > tn) {
            ord[b] = ord[b - 1];
            b--;
        }
        ord[b] = t;
    }
    double used = 0.0;
    for (int64_t a = 0; a < cb->n; a++) {
        const int64_t r = ord[a];
        const double rn = (double)(cb->start[r + 1] - cb->start[r]);
        if (used + rn > budget)
            break;
        used += rn;
        keep[r] = true;
    }
    int64_t n = 0, nnz = 0;
    for (int64_t r = 0; r < cb->n; r++) {
        if (!keep[r])
            continue;
        const int64_t s = cb->start[r], e = cb->start[r + 1];
        if (nnz != s) {
            memmove(cb->idx + nnz, cb->idx + s, (size_t)(e - s) * sizeof *cb->idx);
            memmove(cb->val + nnz, cb->val + s, (size_t)(e - s) * sizeof *cb->val);
        }
        cb->start[n] = nnz;
        cb->lo[n] = cb->lo[r];
        cb->eff[n] = cb->eff[r];
        nnz += e - s;
        n++;
    }
    cb->start[n] = nnz;
    cb->n = n;
    cb->nnz = nnz;
    free(ord);
    free(keep);
    return n;
}

static int64_t cutbuf_keep_best(cutbuf *cb, int64_t cap)"""),
("""                    got = gomory_round(m, lp, gint, gom_row, nr, x, &cb, row,
                                       cut, &work);
                    if (got < 0)
                        goto done;""",
 """                    got = gomory_round(m, lp, gint, gom_row, nr, x, &cb, row,
                                       cut, &work);
                    if (got > 0)
                        got = cutbuf_keep_sparse(&cb, %s * (double)lp->num_nz);
                    if (got < 0)
                        goto done;""" % b)]
table = {'dx': [(sx, [dx])], 'devex': [(sx, [devex])],
         'exact': [(sx, [exact])], 'exactdx': [(sx, [exact, dx])],
         'swap': [(mip, swap)], 'swapdevex': [(mip, swap), (sx, [devex])],
         'swapdx': [(mip, swap), (sx, [dx])]}
for part in v.split('+'):
    if part.startswith('gd'):
        todo = [(mip, gd(part[2:]))]
    elif part.startswith('gb'):
        todo = [(mip, gb(part[2:]))]
    else:
        todo = table[part]
    for path, pairs in todo:
        patch(path, pairs)
