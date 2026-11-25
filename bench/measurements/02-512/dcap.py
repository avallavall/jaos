# dcap.py TREE — each dive solve capped at the root LP's work; DCAP_DEVEX / DCAP_BATCH env at run time toggle Devex and batch fixing
import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/mip.c', [
("""static int dive_for_point(const jaos_model *m, const jaos_model *lp,
                          int64_t solves, const double *agree_a,
                          const double *agree_b, double *out, int64_t *work,
                          int64_t *solves_done, int64_t stop)
{
    const int64_t nc = m->num_col;
    jaos_model *hv = nullptr;
    if (jaos_model_copy(lp, &hv) != JAOS_OK)
        return -1;
""", """static int dc_dist_cmp(const void *pa, const void *pb)
{
    const double *a = pa, *b = pb;
    if (a[0] != b[0]) return a[0] < b[0] ? -1 : 1;
    return a[1] < b[1] ? -1 : a[1] > b[1];
}
static int dive_for_point(const jaos_model *m, const jaos_model *lp,
                          int64_t solves, const double *agree_a,
                          const double *agree_b, double *out, int64_t *work,
                          int64_t *solves_done, int64_t stop, int64_t solve_cap)
{
    const int64_t nc = m->num_col;
    jaos_model *hv = nullptr;
    if (jaos_model_copy(lp, &hv) != JAOS_OK)
        return -1;
    if (getenv("DCAP_OFF")) solve_cap = 0;
    if (getenv("DCAP_DEVEX")) hv->cfg.node_devex = true;
    double *dz_d = getenv("DCAP_BATCH") ? jm_alloc_array(2 * (nc > 0 ? nc : 1), sizeof *dz_d) : nullptr;
"""),
("""    for (int64_t s = 0; s < solves; s++) {
        if (!heur_budget(hv, m, *work, stop))
            break;
        if (jaos_solve(hv) != JAOS_OK)
            break;
        *work += jaos_work_units(hv);
        (*solves_done)++;
        if (jaos_status_of(hv) != JAOS_SOLVE_OPTIMAL)
            break;
        if (jaos_solution(hv, out, nullptr, nullptr, nullptr) != JAOS_OK)
            break;
        int64_t pick = -1;
        double near = 2.0;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j])
                continue;
            const double f = out[j] - floor(out[j]);
            const double d = f < 0.5 ? f : 1.0 - f;
            if (d <= MIP_INT_TOL)
                continue;
            if (d < near) {
                near = d;
                pick = j;
            }
        }
        if (pick < 0) {
            rc = 1;
            break;
        }
        double v = jm_round(out[pick]);
        if (v < hv->col_lower[pick])
            v = hv->col_lower[pick];
        if (v > hv->col_upper[pick])
            v = hv->col_upper[pick];
        if (jaos_set_col_bounds(hv, pick, v, v) != JAOS_OK)
            break;
    }
    jaos_model_free(hv);
    return rc;""", """    for (int64_t s = 0; s < solves; s++) {
        if (!heur_budget(hv, m, *work, stop))
            break;
        if (solve_cap > 0 &&
            (hv->cfg.work_limit <= 0 || hv->cfg.work_limit > solve_cap))
            hv->cfg.work_limit = solve_cap;
        if (jaos_solve(hv) != JAOS_OK)
            break;
        *work += jaos_work_units(hv);
        (*solves_done)++;
        if (jaos_status_of(hv) != JAOS_SOLVE_OPTIMAL)
            break;
        if (jaos_solution(hv, out, nullptr, nullptr, nullptr) != JAOS_OK)
            break;
        int64_t pick = -1;
        double near = 2.0;
        int64_t nfrac = 0;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j])
                continue;
            const double f = out[j] - floor(out[j]);
            const double d = f < 0.5 ? f : 1.0 - f;
            if (d <= MIP_INT_TOL)
                continue;
            if (dz_d) { dz_d[2 * nfrac] = d; dz_d[2 * nfrac + 1] = (double)j; }
            nfrac++;
            if (d < near) {
                near = d;
                pick = j;
            }
        }
        if (pick < 0) {
            rc = 1;
            break;
        }
        int64_t nfix = 1;
        if (dz_d) {
            const int64_t left = solves - s;
            nfix = (nfrac + left - 1) / left;
            if (nfix > 1) qsort(dz_d, (size_t)nfrac, 2 * sizeof *dz_d, dc_dist_cmp);
        }
        bool bad = false;
        for (int64_t k = 0; k < nfix; k++) {
            const int64_t pj = nfix > 1 ? (int64_t)dz_d[2 * k + 1] : pick;
            double v = jm_round(out[pj]);
            if (v < hv->col_lower[pj])
                v = hv->col_lower[pj];
            if (v > hv->col_upper[pj])
                v = hv->col_upper[pj];
            if (jaos_set_col_bounds(hv, pj, v, v) != JAOS_OK) { bad = true; break; }
        }
        if (bad)
            break;
    }
    free(dz_d);
    jaos_model_free(hv);
    return rc;"""),
("""    int64_t heur_amount = 0, heur_stop = 0, root_heur_amount = 0;""",
 """    int64_t heur_amount = 0, heur_stop = 0, root_heur_amount = 0;
    int64_t root_lp_work = 0;"""),
("""            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
""", """            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
            if (nodes == 1)
                root_lp_work = rec.lp_work;
"""),
("""            const int got = dive_for_point(m, lp, dive_heur, nullptr, nullptr,
                                           xr, &work, &solves,
                                           inc.have ? heur_stop
                                           : nodes == 1 ? work + root_heur_amount
                                           : 0);""", """            const int got = dive_for_point(m, lp, dive_heur, nullptr, nullptr,
                                           xr, &work, &solves,
                                           inc.have ? heur_stop
                                           : nodes == 1 ? work + root_heur_amount
                                           : 0, root_lp_work);"""),
("""            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves, inc.have ? heur_stop : 0);""", """            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves, inc.have ? heur_stop : 0,
                                           root_lp_work);"""),
])
