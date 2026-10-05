import sys
# sbfresh.py FILE (JAOS_SBFRESH: learn and pick with this node's gains; JAOS_SBNOLEARN: pick only) : under JAOS_SBNOLEARN, strong branching probes score this node's candidates only and leave the
# pseudocosts to the tree's own observations (scratch)
p = sys.argv[1]
s = open(p).read()
a = """                if (jaos_objective(lp, &obj) == JAOS_OK) {
                    double gain = sigma * obj - key;
                    if (gain < 0.0)
                        gain = 0.0;
                    pc_sum[d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                    pc_n[d * nc + j] += 1;
                }"""
assert s.count(a) == 1
b = """                if (jaos_objective(lp, &obj) == JAOS_OK) {
                    double gain = sigma * obj - key;
                    if (gain < 0.0)
                        gain = 0.0;
                    sb_gain[d * MIP_STRONG_CANDIDATES + c] = gain;
                    if (!getenv("JAOS_SBNOLEARN")) {
                        pc_sum[d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                        pc_n[d * nc + j] += 1;
                    }
                }"""
s = s.replace(a, b)
# storage and reset
a2 = """static jaos_status strong_probe(jaos_model *lp, const jaos_model *m,"""
s = s.replace(a2, """static double sb_gain[2 * 64];
static int64_t sb_ncand;
""" + a2, 1)
a3 = """    if (ncand == 0)
        return JAOS_OK;
    if (nc > 0)
        memcpy(cs, lp->sol_col_status, (size_t)nc * sizeof *cs);"""
assert s.count(a3) == 1
s = s.replace(a3, """    sb_ncand = ncand;
    for (int64_t c = 0; c < 2 * MIP_STRONG_CANDIDATES; c++)
        sb_gain[c] = -1.0;
    if (ncand == 0)
        return JAOS_OK;
    if (nc > 0)
        memcpy(cs, lp->sol_col_status, (size_t)nc * sizeof *cs);""")
a4 = """            branch = select_branch(m, x, rule, pc_sum, pc_n);
        }
        phase_to(&ph, work, PH_OTHER);"""
assert s.count(a4) == 1
s = s.replace(a4, """            branch = select_branch(m, x, rule, pc_sum, pc_n);
            if ((getenv("JAOS_SBNOLEARN") || getenv("JAOS_SBFRESH")) && rule != JAOS_BRANCH_MOST_FRACTIONAL) {
                int64_t best_j = -1;
                double best_s = -1.0, best_aw = -1.0;
                for (int64_t j = 0; j < nc; j++) {
                    if (!m->col_integer[j])
                        continue;
                    const double fj = x[j] - floor(x[j]);
                    if (fj <= MIP_INT_TOL || fj >= 1.0 - MIP_INT_TOL)
                        continue;
                    double qd = fj * pseudocost(j, 0, nc, pc_sum, pc_n);
                    double qu = (1.0 - fj) * pseudocost(j, 1, nc, pc_sum, pc_n);
                    for (int64_t c = 0; c < sb_ncand; c++)
                        if (cand[c] == j) {
                            if (sb_gain[c] >= 0.0)
                                qd = sb_gain[c];
                            if (sb_gain[MIP_STRONG_CANDIDATES + c] >= 0.0)
                                qu = sb_gain[MIP_STRONG_CANDIDATES + c];
                        }
                    const double sc = (qd > MIP_PC_EPS ? qd : MIP_PC_EPS) * (qu > MIP_PC_EPS ? qu : MIP_PC_EPS);
                    const double aw = fj < 1.0 - fj ? fj : 1.0 - fj;
                    if (sc > best_s || (sc == best_s && aw > best_aw)) {
                        best_s = sc;
                        best_aw = aw;
                        best_j = j;
                    }
                }
                if (best_j >= 0)
                    branch = best_j;
            }
        }
        phase_to(&ph, work, PH_OTHER);""")
open(p, 'w').write(s)
print('patched')
