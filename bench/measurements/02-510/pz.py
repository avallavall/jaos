import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/simplex.c', [
("""    int64_t dg_degen, dg_flips, dg_cand;""", """    int64_t dg_degen, dg_flips, dg_cand, dg_run;"""),
("""        if (fabs(theta_dual) <= 1e-12) s->dg_degen++;""", """        if (fabs(theta_dual) <= 1e-12) { s->dg_degen++; s->dg_run++; } else s->dg_run = 0;
        {
            static long long pz_k = -2;
            if (pz_k == -2) { const char *e = getenv("JAOS_PZ"); pz_k = e ? atoll(e) : -1; }
            if (pz_k >= 0 && s->m->cfg.cut_resolve && !s->costs_perturbed && !s->bland &&
                shifts_costs(s) && s->dg_run >= pz_k) {
                perturb_costs(s);
                s->early_perturbed = true;
                jm_log(s->m, JAOS_LOG_PROGRESS, "pz: perturbed at iter %lld after %lld degenerate steps", (long long)s->iters, (long long)s->dg_run);
            }
        }"""),
])
