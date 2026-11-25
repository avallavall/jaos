# dive2.py TREE — each dive solve prices by dual Devex and stops at MIP_DIVE_SOLVE_CAP times the root LP's work
import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/mip.c', [
("""constexpr int64_t MIP_DIVE_HEURISTIC = 50;
""", """constexpr int64_t MIP_DIVE_HEURISTIC = 50;

constexpr double MIP_DIVE_SOLVE_CAP = 1.0;
"""),
("""static int dive_for_point(const jaos_model *m, const jaos_model *lp,
                          int64_t solves, const double *agree_a,
                          const double *agree_b, double *out, int64_t *work,
                          int64_t *solves_done, int64_t stop)
{
    const int64_t nc = m->num_col;
    jaos_model *hv = nullptr;
    if (jaos_model_copy(lp, &hv) != JAOS_OK)
        return -1;
""", """static int dive_for_point(const jaos_model *m, const jaos_model *lp,
                          int64_t solves, const double *agree_a,
                          const double *agree_b, double *out, int64_t *work,
                          int64_t *solves_done, int64_t stop,
                          int64_t solve_cap)
{
    const int64_t nc = m->num_col;
    jaos_model *hv = nullptr;
    if (jaos_model_copy(lp, &hv) != JAOS_OK)
        return -1;
    hv->cfg.node_devex = true;
"""),
("""    for (int64_t s = 0; s < solves; s++) {
        if (!heur_budget(hv, m, *work, stop))
            break;
        if (jaos_solve(hv) != JAOS_OK)
            break;
        *work += jaos_work_units(hv);
        (*solves_done)++;
        if (jaos_status_of(hv) != JAOS_SOLVE_OPTIMAL)
            break;""", """    for (int64_t s = 0; s < solves; s++) {
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
            break;"""),
("""    int64_t heur_amount = 0, heur_stop = 0, root_heur_amount = 0;""",
 """    int64_t heur_amount = 0, heur_stop = 0, root_heur_amount = 0;
    int64_t dive_solve_cap = 0;"""),
("""            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
""", """            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
            if (nodes == 1 && rec.lp_work > 0)
                dive_solve_cap =
                    (int64_t)(MIP_DIVE_SOLVE_CAP * (double)rec.lp_work) + 1;
"""),
("""            const int got = dive_for_point(m, lp, dive_heur, nullptr, nullptr,
                                           xr, &work, &solves,
                                           inc.have ? heur_stop
                                           : nodes == 1 ? work + root_heur_amount
                                           : 0);""", """            const int got = dive_for_point(m, lp, dive_heur, nullptr, nullptr,
                                           xr, &work, &solves,
                                           inc.have ? heur_stop
                                           : nodes == 1 ? work + root_heur_amount
                                           : 0, dive_solve_cap);"""),
("""            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves, inc.have ? heur_stop : 0);""", """            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves, inc.have ? heur_stop : 0,
                                           dive_solve_cap);"""),
])
print('dive2 applied')
