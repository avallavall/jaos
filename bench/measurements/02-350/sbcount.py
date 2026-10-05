import sys
# sbcount.py FILE : under JAOS_SBCOUNT, a probe counts toward its column's reliability (pc_n[2nc + d nc + j]) but its gain
# stays out of the pseudocosts and only scores the node it was taken at (scratch)
p = sys.argv[1]
s = open(p).read()
def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (s.count(a), a)
    s = s.replace(a, b)
rep("""    pc_sum = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_sum);
    pc_n = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_n);""",
"""    pc_sum = calloc((size_t)(nc > 0 ? 4 * nc : 1), sizeof *pc_sum);
    pc_n = calloc((size_t)(nc > 0 ? 4 * nc : 1), sizeof *pc_n);""")
rep("""static jaos_status strong_probe(jaos_model *lp, const jaos_model *m,""",
"""static double sb_gain[2 * 64];
static int64_t sb_ncand;
static inline int64_t sb_seen(const int64_t *pc_n, int64_t nc, int d, int64_t j)
{
    return pc_n[d * nc + j] + (getenv("JAOS_SBCOUNT") ? pc_n[2 * nc + d * nc + j] : 0);
}
static jaos_status strong_probe(jaos_model *lp, const jaos_model *m,""")
rep("""        if (pc_n[j] >= reliability && pc_n[nc + j] >= reliability)
            continue;

        const double sc = pc_score(j, f, nc, pc_sum, pc_n);""",
"""        if (sb_seen(pc_n, nc, 0, j) >= reliability && sb_seen(pc_n, nc, 1, j) >= reliability)
            continue;

        const double sc = pc_score(j, f, nc, pc_sum, pc_n);""")
rep("""    if (ncand == 0)
        return JAOS_OK;
    if (nc > 0)
        memcpy(cs, lp->sol_col_status, (size_t)nc * sizeof *cs);""",
"""    sb_ncand = ncand;
    for (int64_t c = 0; c < 2 * MIP_STRONG_CANDIDATES; c++)
        sb_gain[c] = -1.0;
    if (ncand == 0)
        return JAOS_OK;
    if (nc > 0)
        memcpy(cs, lp->sol_col_status, (size_t)nc * sizeof *cs);""")
rep("""            if (pc_n[d * nc + j] >= reliability)
                continue;
            st = d == 0 ? jaos_set_col_bounds(lp, j, lo0, floor(x[j]))""",
"""            if (sb_seen(pc_n, nc, d, j) >= reliability)
                continue;
            st = d == 0 ? jaos_set_col_bounds(lp, j, lo0, floor(x[j]))""")
rep("""                    pc_sum[d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                    pc_n[d * nc + j] += 1;""",
"""                    if (getenv("JAOS_SBCOUNT")) {
                        sb_gain[d * MIP_STRONG_CANDIDATES + c] = gain;
                        pc_sum[2 * nc + d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                        pc_n[2 * nc + d * nc + j] += 1;
                    } else {
                        pc_sum[d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                        pc_n[d * nc + j] += 1;
                    }""")
rep("""            branch = select_branch(m, x, rule, pc_sum, pc_n);
        }
        phase_to(&ph, work, PH_OTHER);""",
"""            branch = select_branch(m, x, rule, pc_sum, pc_n);
            if (getenv("JAOS_SBCOUNT") && rule != JAOS_BRANCH_MOST_FRACTIONAL) {
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
