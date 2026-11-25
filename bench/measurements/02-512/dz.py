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
""", """static int64_t dz_root_lp_work = 0;
static int dz_dist_cmp(const void *pa, const void *pb)
{
    const double *a = pa, *b = pb;
    if (a[0] != b[0]) return a[0] < b[0] ? -1 : 1;
    return a[1] < b[1] ? -1 : a[1] > b[1];
}
static int dive_for_point(const jaos_model *m, const jaos_model *lp,
                          int64_t solves, const double *agree_a,
                          const double *agree_b, double *out, int64_t *work,
                          int64_t *solves_done, int64_t stop)
{
    const int64_t nc = m->num_col;
    jaos_model *hv = nullptr;
    if (jaos_model_copy(lp, &hv) != JAOS_OK)
        return -1;
    const bool dz_devex = getenv("DZ_DEVEX") != nullptr, dz_nopre = getenv("DZ_NOPRE") != nullptr,
               dz_batch = getenv("DZ_BATCH") != nullptr;
    const double dz_cap = getenv("DZ_CAP") ? atof(getenv("DZ_CAP")) : 0.0;
    if (dz_devex) hv->cfg.node_devex = true;
    if (dz_nopre) hv->cfg.node_no_presolve = true;
    double *dz_d = dz_batch ? jm_alloc_array(2 * (nc > 0 ? nc : 1), sizeof *dz_d) : nullptr;
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
        if (dz_cap > 0.0 && agree_a == nullptr && dz_root_lp_work > 0) {
            const int64_t c = (int64_t)(dz_cap * (double)dz_root_lp_work) + 1;
            if (hv->cfg.work_limit <= 0 || hv->cfg.work_limit > c) hv->cfg.work_limit = c;
        }
        const double dt0 = now_seconds();
        if (jaos_solve(hv) != JAOS_OK)
            break;
        *work += jaos_work_units(hv);
        (*solves_done)++;
        jm_log(m, JAOS_LOG_SUMMARY, "dz solve %lld: iters %lld work %lld secs %.3f status %s rootlp %lld",
               (long long)s, (long long)jaos_iterations(hv), (long long)jaos_work_units(hv),
               now_seconds() - dt0, jaos_solve_status_str(jaos_status_of(hv)), (long long)dz_root_lp_work);
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
        jm_log(m, JAOS_LOG_SUMMARY, "dz frac %lld", (long long)nfrac);
        if (pick < 0) {
            rc = 1;
            break;
        }
        int64_t nfix = 1;
        if (dz_d) {
            const int64_t left = solves - s;
            nfix = (nfrac + left - 1) / left;
            if (nfix > 1) qsort(dz_d, (size_t)nfrac, 2 * sizeof *dz_d, dz_dist_cmp);
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
("""            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
""", """            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
            if (nodes == 1) dz_root_lp_work = rec.lp_work;
"""),
])
