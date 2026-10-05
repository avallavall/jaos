import sys
# nodeadapt.py (any arg) : under JAOS_NPA, the tree counts the simplex iterations of its first JAOS_NPA_TRIAL (50) node
# solves below the root; when they average more than JAOS_NPA_ITERS (20), later node solves that start from a basis skip
# presolve (cfg.node_no_presolve) (scratch)
def rep(path, a, b, n=1):
    s = open(path).read()
    assert s.count(a) == n, (path, s.count(a), a)
    s = s.replace(a, b)
    open(path, 'w').write(s)
rep('src/jaos_internal.h', '    bool node_solve;\n', '    bool node_solve;\n    bool node_no_presolve;\n')
rep('src/simplex.c', """        jaos_status pst = quadratic ? JAOS_OK : jm_presolve_run(m, &p, &pre_work);
        if (pst != JAOS_OK) {
            jm_presolve_free(&p);
            return pst;
        }
        if (quadratic) {""", """        const bool skip_pre = m->cfg.node_solve && m->cfg.node_no_presolve &&
                              m->start_col_status != nullptr && m->start_row_status != nullptr;
        jaos_status pst = quadratic || skip_pre ? JAOS_OK : jm_presolve_run(m, &p, &pre_work);
        if (pst != JAOS_OK) {
            jm_presolve_free(&p);
            return pst;
        }
        if (skip_pre && !quadratic)
            p.outcome = JM_PRESOLVE_NONE;
        if (quadratic) {""")
rep('src/mip.c', "    bheap heap = { .by_est = node_select == 1 };\n",
    "    bheap heap = { .by_est = node_select == 1 };\n    int64_t npa_n = 0, npa_it = 0;\n")
NL = "\\n"
rep('src/mip.c', """        const int64_t node_work = jaos_work_units(lp);
        work += node_work;
        iters += jaos_iterations(lp);
""", """        const int64_t node_work = jaos_work_units(lp);
        work += node_work;
        iters += jaos_iterations(lp);
        if (getenv("JAOS_NPA") && nodes > 1 && !lp->cfg.node_no_presolve) {
            const int64_t trial = getenv("JAOS_NPA_TRIAL") ? atoll(getenv("JAOS_NPA_TRIAL")) : 50;
            const double thr = getenv("JAOS_NPA_ITERS") ? atof(getenv("JAOS_NPA_ITERS")) : 20.0;
            npa_n++;
            npa_it += jaos_iterations(lp);
            if (npa_n == trial && (double)npa_it > thr * (double)trial) {
                lp->cfg.node_no_presolve = true;
                if (getenv("JAOS_NPA_LOG"))
                    fprintf(stderr, "NPA off at node %lld, mean %.1f""" + NL + """", (long long)nodes, (double)npa_it / (double)trial);
            }
        }
""")
s = open('src/mip.c').read()
if '#include <stdio.h>' not in s:
    s = s.replace('#include <stdlib.h>', '#include <stdio.h>\n#include <stdlib.h>', 1)
    open('src/mip.c', 'w').write(s)
print('patched')
