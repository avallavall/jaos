import sys
root = sys.argv[1]
def patch(path, pairs):
    s = open(path).read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a)
        s = s.replace(a, b, 1)
    open(path, 'w').write(s)
patch(root + '/src/simplex.c', [
("""                jm_log(s->m, JAOS_LOG_PROGRESS, "pz: perturbed at iter %lld after %lld degenerate steps", (long long)s->iters, (long long)s->dg_run);
            }
        }""", """                jm_log(s->m, JAOS_LOG_PROGRESS, "pz: perturbed at iter %lld after %lld degenerate steps", (long long)s->iters, (long long)s->dg_run);
            }
            static long long pzr_k = -2;
            if (pzr_k == -2) { const char *e = getenv("JAOS_PZR"); pzr_k = e ? atoll(e) : -1; }
            if (pzr_k >= 0 && s->m->cfg.node_solve && !s->m->cfg.node_devex && !s->m->cfg.cut_resolve &&
                !s->costs_perturbed && !s->bland && !s->early_perturb_off &&
                shifts_costs(s) && s->dg_run >= pzr_k) {
                perturb_costs(s);
                s->early_perturbed = true;
                jm_log(s->m, JAOS_LOG_SUMMARY, "pzr: perturbed at iter %lld after %lld degenerate steps", (long long)s->iters, (long long)s->dg_run);
            }
        }"""),
])
