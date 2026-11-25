import sys
root = sys.argv[1]
steps = sys.argv[2] if len(sys.argv) > 2 else '1'
gate = sys.argv[3] if len(sys.argv) > 3 else 'all'
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)

cond = {'all': 's->quadratic', 'far': 's->far_duals',
        'retry': 's->quadratic && s->m->cfg.barrier_refine',
        'refused': 's->quadratic && s->m->cfg.barrier_refine'}[gate]

walk = """static jaos_status qp_walk_refined(jaos_model *m)
{
    const double tlim = m->cfg.time_limit, spent = m->solve_time;
    const int64_t wlim = m->cfg.work_limit, work = m->solve_work;
    const int64_t iters = m->solve_iters;
    if ((tlim > 0.0 && spent >= tlim) || (wlim > 0 && work >= wlim))
        return JAOS_OK;
    jm_log(m, JAOS_LOG_SUMMARY, "the barrier ended without an answer after "
           "%lld iterations and %lld work units; it walks again with a "
           "refinement step on each Newton solve", (long long)iters,
           (long long)work);
    m->err[0] = '\\0';
    m->cfg.barrier_refine = true;
    if (tlim > 0.0)
        m->cfg.time_limit = tlim - spent;
    if (wlim > 0)
        m->cfg.work_limit = wlim - work;
    const jaos_status st = jm_dual_simplex(m);
    m->cfg.barrier_refine = false;
    m->cfg.time_limit = tlim;
    m->cfg.work_limit = wlim;
    m->solve_work += work;
    m->solve_iters += iters;
    m->solve_time += spent;
    return st;
}

jaos_status jaos_solve(jaos_model *m)
{"""

if gate in ('retry', 'refused'):
    patch(root + '/src/jaos_internal.h', [
("""    bool barrier_augmented;
""", """    bool barrier_augmented;
    bool barrier_refine;
"""),
("""    bool cutoff_hit;
""", """    bool cutoff_hit;
    bool barrier_refused;
"""),
])
    retry_if = '' if gate == 'retry' else ' &&\n        m->barrier_refused'
    patch(root + '/src/model.c', [
("""    const jaos_status st = jm_dual_simplex(m);
    if (st == JAOS_OK && m->solve_status == JAOS_SOLVE_NUMERICAL_ERROR &&
        jm_model_has_quadratic(m))
        return jm_conic_after_barrier(m);""",
"""    jaos_status st = jm_dual_simplex(m);
    if (st == JAOS_OK && m->solve_status == JAOS_SOLVE_NUMERICAL_ERROR &&
        jm_model_has_quadratic(m)%s)
        st = qp_walk_refined(m);
    if (st == JAOS_OK && m->solve_status == JAOS_SOLVE_NUMERICAL_ERROR &&
        jm_model_has_quadratic(m))
        return jm_conic_after_barrier(m);""" % retry_if),
("""jaos_status jaos_solve(jaos_model *m)
{""", walk),
])

pairs = [
("""constexpr double  BARRIER_DELTA    = 1e-10;
""", """constexpr double  BARRIER_DELTA    = 1e-10;
constexpr int64_t BARRIER_QP_REFINE = %s;
""" % steps),
("""    mul_e(s, s->tmp, s->rhs);
    for (int64_t i = 0; i < s->nrow; i++)
        dy[i] = r[i] + s->rhs[i];
    solve_normal(s, dy);
    mul_et(s, dy, dz);
""", """    mul_e(s, s->tmp, s->rhs);
    for (int64_t i = 0; i < s->nrow; i++)
        dy[i] = r[i] + s->rhs[i];
    const int64_t refine = %s && s->pin == nullptr &&
                           s->dec == nullptr ? BARRIER_QP_REFINE : 0;
    if (refine > 0)
        memcpy(s->rhs, dy, (size_t)s->nrow * sizeof *dy);
    solve_normal(s, dy);
    for (int64_t k = 0; k < refine; k++) {
        mul_et(s, dy, dz);
        for (int64_t j = 0; j < s->nvar; j++)
            dz[j] = s->kind[j] == FIXED ? 0.0 : s->theta[j] * dz[j];
        mul_e(s, dz, s->tmp);
        for (int64_t i = 0; i < s->nrow; i++)
            s->tmp[i] = s->rhs[i] - s->tmp[i] - s->delta * dy[i];
        solve_normal(s, s->tmp);
        for (int64_t i = 0; i < s->nrow; i++)
            dy[i] += s->tmp[i];
        jm_work_add(&s->work, (s->nvar + 3 * s->nrow) * JM_WORK_NONZERO);
    }
    mul_et(s, dy, dz);
""" % cond),
]
if gate == 'far':
    pairs += [
("""    bool quadratic;
""", """    bool quadratic;
    bool far_duals;
"""),
("""            const double dgrow = dn / (1.0 + s->norm_c);
""", """            const double dgrow = dn / (1.0 + s->norm_c);
            s->far_duals = s->quadratic && dgrow > BARRIER_DIVERGE;
"""),
]
if gate == 'refused':
    pairs += [
("""    *crossover = false;
    *handoff = false;
    *iters = 0;
""", """    *crossover = false;
    *handoff = false;
    *iters = 0;
    m->barrier_refused = false;
"""),
("""    if (st == JAOS_OK && refused) {
        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
        m->solve_status = JAOS_SOLVE_NUMERICAL_ERROR;
        m->solve_work = s.work.units;""", """    if (st == JAOS_OK && refused) {
        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
        m->barrier_refused = true;
        m->solve_status = JAOS_SOLVE_NUMERICAL_ERROR;
        m->solve_work = s.work.units;"""),
]
patch(root + '/src/barrier.c', pairs)
