import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)

helper = """static void rr_log(jaos_model *m, jaos_model *lp, const char *what,
                   int64_t r, int64_t added, int64_t nz_before, double t0,
                   jaos_status st, double sigma)
{
    double obj = NAN;
    const bool ok = st == JAOS_OK;
    if (ok)
        (void)jaos_objective(lp, &obj);
    jm_log(m, JAOS_LOG_SUMMARY,
           "round %lld %s: added %lld rows %lld nz %lld (+%lld) iters %lld "
           "primal %lld phase1 %lld work %lld secs %.3f status %s bound %.17g",
           (long long)(r + 1), what, (long long)added,
           (long long)lp->num_row, (long long)lp->num_nz,
           (long long)(lp->num_nz - nz_before),
           (long long)jaos_iterations(lp), (long long)lp->solve_primal_iters,
           (long long)lp->solve_phase1_iters,
           (long long)jaos_work_units(lp), now_seconds() - t0,
           ok ? jaos_solve_status_str(jaos_status_of(lp)) : jaos_status_str(st),
           sigma * obj);
}

static bool root_certifies("""

patch(root + '/src/mip.c', [
("""static bool root_certifies(""", helper),
("""            const double key_first = key;
            bool stop = false;
            lp->cfg.cut_resolve = true;
""", """            const double key_first = key;
            bool stop = false;
            jm_log(m, JAOS_LOG_SUMMARY,
                   "round 0 root: rows %lld nz %lld iters %lld work %lld "
                   "secs %.3f bound %.17g", (long long)lp->num_row,
                   (long long)lp->num_nz, (long long)jaos_iterations(lp),
                   (long long)jaos_work_units(lp), now_seconds() - t0,
                   sigma * key);
            lp->cfg.cut_resolve = true;
"""),
("""                        rp_back += nk;
                        phase_to(&ph, work, PH_LP);
                        budget(lp, m, work);
                        st = jaos_solve(lp);
                        solves++;
""", """                        rp_back += nk;
                        phase_to(&ph, work, PH_LP);
                        budget(lp, m, work);
                        const int64_t rr_nz = lp->num_nz;
                        const double rr_t = now_seconds();
                        lp->cfg.log_cb = m->cfg.log_cb;
                        lp->cfg.log_user = m->cfg.log_user;
                        st = jaos_solve(lp);
                        lp->cfg.log_cb = nullptr;
                        rr_log(m, lp, "pool", r, nk, rr_nz, rr_t, st, sigma);
                        solves++;
"""),
("""                if (r < rounds || (r < gomory_rounds && gomory_live)) {
                    got = gomory_round(m, lp, gint, gom_row, nr, x, &cb, row,
                                       cut, &work);
""", """                const int64_t rr_c0 = covers, rr_q0 = cliques,
                              rr_z0 = zero_halves, rr_f0 = flow_covers,
                              rr_h0 = hulls, rr_m0 = mirs, rr_w0 = work;
                const double rr_s0 = now_seconds();
                if (r < rounds || (r < gomory_rounds && gomory_live)) {
                    got = gomory_round(m, lp, gint, gom_row, nr, x, &cb, row,
                                       cut, &work);
"""),
("""                    gomory_done++;
                }
""", """                    gomory_done++;
                }
                const int64_t rr_gnz = cb.nnz;
"""),
("""                if (got == 0 && agb.n == 0)
                    break;
                if (budget_gone(m, work)) {
                    outcome = gone_status(m, work);
                    stop = true;
                    break;
                }
                if (net) {""", """                jm_log(m, JAOS_LOG_SUMMARY,
                       "round %lld separate: gomory %lld cover %lld clique "
                       "%lld zerohalf %lld flowcover %lld hull %lld mir %lld "
                       "agg %lld work %lld secs %.3f gomory_nz %lld other_nz "
                       "%lld agg_nz %lld cols %lld",
                       (long long)(r + 1),
                       (long long)(got - (covers - rr_c0) - (cliques - rr_q0) -
                                   (zero_halves - rr_z0) -
                                   (flow_covers - rr_f0) - (hulls - rr_h0) -
                                   (mirs - rr_m0)),
                       (long long)(covers - rr_c0), (long long)(cliques - rr_q0),
                       (long long)(zero_halves - rr_z0),
                       (long long)(flow_covers - rr_f0),
                       (long long)(hulls - rr_h0), (long long)(mirs - rr_m0),
                       (long long)agb.n, (long long)(work - rr_w0),
                       now_seconds() - rr_s0, (long long)rr_gnz,
                       (long long)(cb.nnz - rr_gnz), (long long)agb.nnz,
                       (long long)nc);
                if (got == 0 && agb.n == 0)
                    break;
                if (budget_gone(m, work)) {
                    outcome = gone_status(m, work);
                    stop = true;
                    break;
                }
                if (net) {"""),
("""                        budget(pv, m, work);
                        const jaos_status pst = jaos_solve(pv);
                        solves++;
""", """                        budget(pv, m, work);
                        const int64_t rr_nz = pv->num_nz;
                        const double rr_t = now_seconds();
                        pv->cfg.log_cb = m->cfg.log_cb;
                        pv->cfg.log_user = m->cfg.log_user;
                        const jaos_status pst = jaos_solve(pv);
                        pv->cfg.log_cb = nullptr;
                        rr_log(m, pv, "probe", r, agb.n, rr_nz, rr_t, pst,
                               sigma);
                        solves++;
"""),
("""                    if (cuts_add(lp, add) != JAOS_OK)
                        goto done;
                    if (root_cut_drop) {
                        for (int64_t k = 0; k < add->n; k++) {""", """                    const int64_t rr_nz = lp->num_nz;
                    if (cuts_add(lp, add) != JAOS_OK)
                        goto done;
                    if (root_cut_drop) {
                        for (int64_t k = 0; k < add->n; k++) {"""),
("""                    cuts += pass == 0 ? got : add->n;
                    phase_to(&ph, work, PH_LP);
                    budget(lp, m, work);
                    st = jaos_solve(lp);
                    solves++;
""", """                    cuts += pass == 0 ? got : add->n;
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
"""),
])
