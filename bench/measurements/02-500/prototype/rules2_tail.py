import sys
p = sys.argv[1]
s = open(p).read()
def rep(old, new, cnt=1):
    global s
    assert s.count(old) == cnt, (old[:80], s.count(old))
    s = s.replace(old, new)
rep("""            rl_nodes++;
            rl_hits0 += pp.pruned + pp.fixed - rl_before;
            if (rl_payt > 0 && rl_nodes == rl_payt &&
                !((double)rl_hits0 > rl_pay * (double)rl_payt)) {
                rl_off = true;
                jm_log(m, JAOS_LOG_SUMMARY,
                       "node %lld: the proof pool stops, %lld cuts and bounds over %lld nodes",
                       (long long)nodes, (long long)rl_hits0, (long long)rl_nodes);
            }
""", """            rl_nodes++;
            rl_hits0 += pp.pruned + pp.fixed - rl_before;
""")
rep("""        if (nodes > 1 && pp.n > 0 && !rl_off) {""", """        if (rl_payt > 0 && !rl_off && !rl_decided) {
            if (pp.made >= rl_payt) {
                rl_decided = true;
                if (!((double)(pp.pruned + pp.fixed) > rl_pay * (double)pp.made)) {
                    rl_off = true;
                    jm_log(m, JAOS_LOG_SUMMARY,
                           "node %lld: the proof pool stops, %lld cuts and bounds from %lld proofs",
                           (long long)nodes, (long long)(pp.pruned + pp.fixed), (long long)pp.made);
                }
            } else if (pp.made == 0 && cutoff_nodes >= rl_pays) {
                rl_off = true;
                jm_log(m, JAOS_LOG_SUMMARY,
                       "node %lld: the proof pool stops, no proof from %lld stops",
                       (long long)nodes, (long long)cutoff_nodes);
            }
        }
        if (nodes > 1 && pp.n > 0 && !rl_off) {""")
rep("""    int64_t rl_nodes = 0, rl_hits0 = 0;
    bool rl_off = false;
""", """    int64_t rl_nodes = 0, rl_hits0 = 0;
    bool rl_off = false, rl_decided = false;
    const char *rl_e5 = getenv("JAOS_PAYS");
    const int64_t rl_pays = rl_e5 != nullptr ? atoll(rl_e5) : 200;
""")
open(p, 'w').write(s)
print("rules2 ok")
