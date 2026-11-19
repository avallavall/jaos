import sys
p = sys.argv[1]
s = open(p).read()


def rep(old, new, cnt=1):
    global s
    assert s.count(old) == cnt, (old[:80], s.count(old))
    s = s.replace(old, new)


rep("""        const bool gomory_here = nodes > 1 && cur->depth <= cut_depth;""",
    """        const bool gomory_here = nodes > 1 && cur->depth <= cut_depth &&
                                 !(rl_symcut && sym.ngen > 0);""")
rep("""    pp.colwise = (sc_mode & 4096) != 0;
""", """    pp.colwise = (sc_mode & 4096) != 0;
    const char *rl_e1 = getenv("JAOS_SYMCUT");
    const bool rl_symcut = rl_e1 != nullptr && atoll(rl_e1) != 0;
    const char *rl_e2 = getenv("JAOS_PAYT");
    const int64_t rl_payt = rl_e2 != nullptr ? atoll(rl_e2) : 0;
    const char *rl_e3 = getenv("JAOS_PAY");
    const double rl_pay = rl_e3 != nullptr ? atof(rl_e3) : 0.0;
    const char *rl_e4 = getenv("JAOS_PREIT");
    const double rl_preit = rl_e4 != nullptr ? atof(rl_e4) : MIP_NODE_PRESOLVE_ITERS;
    int64_t rl_nodes = 0, rl_hits0 = 0;
    bool rl_off = false;
""")
rep("""        if (nodes > 1 && pp.n > 0) {""", """        if (nodes > 1 && pp.n > 0 && !rl_off) {
            const int64_t rl_before = pp.pruned + pp.fixed;""")
rep("""            sc_prop_work += sc_w;
            if ((sc_mode & 512) == 0)
                work += sc_w;
            if (got == -2)
                goto done;
            if (got == -1) {
                sc_cut_nodes++;
                continue;
            }
        }
""", """            sc_prop_work += sc_w;
            if ((sc_mode & 512) == 0)
                work += sc_w;
            if (got == -2)
                goto done;
            rl_nodes++;
            rl_hits0 += pp.pruned + pp.fixed - rl_before;
            if (rl_payt > 0 && rl_nodes == rl_payt &&
                !((double)rl_hits0 > rl_pay * (double)rl_payt)) {
                rl_off = true;
                jm_log(m, JAOS_LOG_SUMMARY,
                       "node %lld: the proof pool stops, %lld cuts and bounds over %lld nodes",
                       (long long)nodes, (long long)rl_hits0, (long long)rl_nodes);
            }
            if (got == -1) {
                sc_cut_nodes++;
                continue;
            }
        }
""")
rep("""        lp->cfg.node_stop_duals = stopc_on && conflicts_on;""",
    """        lp->cfg.node_stop_duals = stopc_on && conflicts_on && !rl_off;""")
rep("""                    if ((sc_mode & 8) != 0 && (got >= 0 || got == -3) &&""",
    """                    if ((sc_mode & 8) != 0 && !rl_off && (got >= 0 || got == -3) &&""")
rep("""                    MIP_NODE_PRESOLVE_ITERS * (double)MIP_NODE_PRESOLVE_TRIAL) {
                lp->cfg.node_no_presolve = true;""", """                    rl_preit * (double)MIP_NODE_PRESOLVE_TRIAL) {
                lp->cfg.node_no_presolve = true;""")
open(p, 'w').write(s)
print("rules ok")
