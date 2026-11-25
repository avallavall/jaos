# rcap.py TREE — (on top of dive2.py) a root cut round's re-solve stops at RCAP1 (round 1) or RCAP (later) times the root LP's work, read from the environment; a re-solve stopped there undoes its round and ends the rounds
import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/mip.c', [
("""    int64_t dive_solve_cap = 0;""", """    int64_t dive_solve_cap = 0, rc_root_lp = 0;
    const double rc_k1 = getenv("RCAP1") ? atof(getenv("RCAP1")) : 0.0;
    const double rc_k = getenv("RCAP") ? atof(getenv("RCAP")) : 0.0;"""),
("""            if (nodes == 1 && rec.lp_work > 0)
                dive_solve_cap =""", """            if (nodes == 1)
                rc_root_lp = rec.lp_work;
            if (nodes == 1 && rec.lp_work > 0)
                dive_solve_cap ="""),
("""                    } else {
                        phase_to(&ph, work, PH_LP);
                        budget(lp, m, work);
""", """                    } else {
                        phase_to(&ph, work, PH_LP);
                        budget(lp, m, work);
                        {
                            const double kk = r == 0 ? rc_k1 : rc_k;
                            if (kk > 0.0 && rc_root_lp > 0) {
                                const int64_t c = (int64_t)(kk * (double)rc_root_lp) + 1;
                                if (lp->cfg.work_limit <= 0 || lp->cfg.work_limit > c) {
                                    lp->cfg.work_limit = c;
                                    rc_capped = true;
                                }
                            }
                        }
"""),
("""                    if (undo && (st == JAOS_ERR_NUMERICAL ||
                                 (st == JAOS_OK &&
                                  jaos_status_of(lp) ==
                                      JAOS_SOLVE_NUMERICAL_ERROR))) {""", """                    if (undo && (st == JAOS_ERR_NUMERICAL ||
                                 (st == JAOS_OK &&
                                  (jaos_status_of(lp) ==
                                       JAOS_SOLVE_NUMERICAL_ERROR ||
                                   (rc_capped && jaos_status_of(lp) ==
                                                     JAOS_SOLVE_WORK_LIMIT &&
                                    !budget_gone(m, work)))))) {
                        jm_log(m, JAOS_LOG_SUMMARY, "rcap: round %lld undone, status %s", (long long)(r + 1), jaos_solve_status_str(jaos_status_of(lp)));"""),
("""                    const int64_t copy0 = in_copy.n;
                    const bool undo = taken == nullptr && lp->sol_basis_ok;""", """                    const int64_t copy0 = in_copy.n;
                    const bool undo = taken == nullptr && lp->sol_basis_ok;
                    bool rc_capped = false;"""),
])
print('rcap applied')
