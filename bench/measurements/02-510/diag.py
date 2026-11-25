import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/simplex.c', [
("""    double farkas_sign;
    int64_t farkas_basic;
""", """    double farkas_sign;
    int64_t farkas_basic;
    int64_t dg_degen, dg_flips, dg_cand;
    double dg_total, dg_theta_sum;
"""),
("""    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    if (best >= 0 && total < s->infeas_best) {""", """    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    s->dg_total = total;

    if (best >= 0 && total < s->infeas_best) {"""),
("""    int64_t live = bfrt_walk(s, n, violation);
    if (live == 0)
        return -1;
""", """    int64_t live = bfrt_walk(s, n, violation);
    if (live == 0)
        return -1;
    s->dg_flips += n - live;
    s->dg_cand += n;
"""),
("""            if (s->iters % LOG_EVERY == 0)
                jm_log(s->m, JAOS_LOG_PROGRESS,
                       "iter %lld: best infeasibility %.6g, work %lld",
                       (long long)s->iters, s->infeas_best,
                       (long long)s->work.units);""", """            if (s->iters % LOG_EVERY == 0) {
                double zc = 0.0, z0 = 0.0;
                int64_t nsh = 0, ninf = 0;
                for (int64_t v = 0; v < s->nvar; v++) {
                    const double xv = s->status[v] == JM_BASIC ? s->xb[s->where[v]] : nonbasic_value(s, v);
                    zc += s->cost[v] * xv;
                    z0 += s->cost0[v] * xv;
                    if (s->shift[v] != 0.0) nsh++;
                }
                for (int64_t w = 0; w < (s->nrow + 63) / 64; w++) ninf += __builtin_popcountll(s->ibits[w]);
                jm_log(s->m, JAOS_LOG_PROGRESS,
                       "iter %lld: best infeasibility %.6g, work %lld; diag total %.6g rows %lld zc %.12g z0 %.12g degen %lld flips %lld cand %lld shifted %lld devex %d perturbed %d",
                       (long long)s->iters, s->infeas_best,
                       (long long)s->work.units, s->dg_total, (long long)ninf, zc, z0,
                       (long long)s->dg_degen, (long long)s->dg_flips, (long long)s->dg_cand,
                       (long long)nsh, (int)s->dual_devex, (int)s->costs_perturbed);
                s->dg_degen = 0; s->dg_flips = 0; s->dg_cand = 0;
            }"""),
("""        set_verified(s, false);
        bool took = false;
        st = pivot(s, r, q, below, theta_dual, &took);
        if (st != JAOS_OK)
            return st;

        if (!took)
            continue;
""", """        set_verified(s, false);
        bool took = false;
        if (fabs(theta_dual) <= 1e-12) s->dg_degen++;
        st = pivot(s, r, q, below, theta_dual, &took);
        if (st != JAOS_OK)
            return st;

        if (!took)
            continue;
"""),
])
