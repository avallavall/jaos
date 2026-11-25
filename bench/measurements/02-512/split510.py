# split510.py TREE — 02-501's split.patch on TREE, its five hunks that no longer apply put in by hand
import subprocess, sys, os
root = sys.argv[1]
r = subprocess.run(['patch', '-p1', '-i', '/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/measurements/02-501/split.patch'],
                   cwd=root, capture_output=True, text=True)
fails = [l for l in r.stdout.splitlines() if l.startswith('Hunk') and 'FAILED' in l]
assert len(fails) == 5, r.stdout
for f in os.listdir(root + '/src'):
    if f.endswith('.rej') or f.endswith('.orig'):
        os.remove(root + '/src/' + f)
p = root + '/src/mip.c'
s = open(p).read()
pairs = [
("""        nodes++;

        phase_to(&ph, work, PH_PROP);
        bb_rec rec;""", """        nodes++;

        phase_to(&ph, work, PH_PROP);
        rs_to(&rp, work, RS_PROP);
        if (nodes > 1 && !rp.tree) {
            rs_to(&rp, work, RS_TREE);
            rp.tree = true;
        }
        bb_rec rec;"""),
("""            sn.stop_duals = !pf.off;
            node_lane_b(&sn, &here, cur, 0, &rec);""", """            sn.stop_duals = !pf.off;
            if (nodes == 1)
                rs_to(&rp, work, RS_LP);
            node_lane_b(&sn, &here, cur, 0, &rec);"""),
("""        phase_to(&ph, work, PH_LP);
        if (lane_k < 0) {
            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
        } else {""", """        phase_to(&ph, work, PH_LP);
        rs_to(&rp, work, RS_LP);
        if (lane_k < 0) {
            solves += rec.solves;
            work += rec.lp_work;
            iters += rec.iters;
            if (nodes == 1) {
                rs_to(&rp, work, RS_OTHER);
                rp.w[RS_LPPRE] += lp->rs_pre_work;
                rp.w[RS_LP] -= lp->rs_pre_work;
                rp.s[RS_LPPRE] += lp->rs_pre_secs;
                rp.s[RS_LP] -= lp->rs_pre_secs;
                rp.lp_iters = jaos_iterations(lp);
                rp.lp_primal = lp->solve_primal_iters;
                rp.lp_phase1 = lp->solve_phase1_iters;
                rp.lp_status = rec.st == JAOS_OK ? (int)jaos_status_of(lp) : -2;
                rp.lp_rounds = lp->presolve_counts.rounds;
            }
        } else {"""),
("""                if (r < rounds || (r < gomory_rounds && gomory_live)) {
                    got = gomory_round(""", """                if (r < rounds || (r < gomory_rounds && gomory_live)) {
                    rs_to(&rp, work, RS_GOMORY);
                    got = gomory_round("""),
("""                    gomory_done++;
                }
                if (r < cover_rounds && !budget_gone(m, work)) {""", """                    gomory_done++;
                }
                rs_to(&rp, work, RS_COVER);
                if (r < cover_rounds && !budget_gone(m, work)) {"""),
("""                    } else {
                        phase_to(&ph, work, PH_LP);
                        budget(lp, m, work);
                        st = jaos_solve(lp);
                        solves++;
                        work += jaos_work_units(lp);
                        iters += jaos_iterations(lp);
                        phase_to(&ph, work, PH_CUTS);
                    }""", """                    } else {
                        phase_to(&ph, work, PH_LP);
                        rs_to(&rp, work, RS_CUTLP);
                        budget(lp, m, work);
                        st = jaos_solve(lp);
                        solves++;
                        work += jaos_work_units(lp);
                        iters += jaos_iterations(lp);
                        phase_to(&ph, work, PH_CUTS);
                        rs_to(&rp, work, RS_CUTSEL);
                    }"""),
]
for a, b in pairs:
    assert s.count(a) == 1, a
    s = s.replace(a, b, 1)
open(p, 'w').write(s)
print('split applied')
