import sys
p = sys.argv[1]
s = open(p).read()

subs = [
    ("""                if (0.0 < cur_rl[i] || 0.0 > cur_ru[i]) {
                    p->outcome = JM_PRESOLVE_INFEASIBLE;""",
     """                if (0.0 < cur_rl[i] || 0.0 > cur_ru[i]) {
                    fprintf(stderr, "INF site=emptyrow i=%lld rl=%.17g ru=%.17g\\n", (long long)i, cur_rl[i], cur_ru[i]);
                    p->outcome = JM_PRESOLVE_INFEASIBLE;"""),
    ("""                if (new_lo > new_hi + btol) {
                    /* PAST the opposite bound. The intersection is empty by
                     * more than rounding in the row-bound shifts can
                     * explain, so the model has no feasible point. */
                    p->outcome = JM_PRESOLVE_INFEASIBLE;""",
     """                if (new_lo > new_hi + btol) {
                    fprintf(stderr, "INF site=fold i=%lld j=%lld new_lo=%.17g new_hi=%.17g btol=%.17g\\n", (long long)i, (long long)j, new_lo, new_hi, btol);
                    p->outcome = JM_PRESOLVE_INFEASIBLE;"""),
    ("""            if ((isfinite(ru) && min_act > ru + rtol) ||
                (isfinite(rl) && max_act < rl - rtol)) {
                p->outcome = JM_PRESOLVE_INFEASIBLE;""",
     """            if ((isfinite(ru) && min_act > ru + rtol) ||
                (isfinite(rl) && max_act < rl - rtol)) {
                fprintf(stderr, "INF site=actrange i=%lld rl=%.17g ru=%.17g min_act=%.17g max_act=%.17g rtol=%.17g\\n", (long long)i, rl, ru, min_act, max_act, rtol);
                p->outcome = JM_PRESOLVE_INFEASIBLE;"""),
    ("""                     * usually reports the infeasibility first. Kept as the
                     * guard it is rather than removed as unreachable. */
                    p->outcome = JM_PRESOLVE_INFEASIBLE;""",
     """                     * usually reports the infeasibility first. Kept as the
                     * guard it is rather than removed as unreachable. */
                    fprintf(stderr, "INF site=crossrow\\n");
                    p->outcome = JM_PRESOLVE_INFEASIBLE;"""),
]
for old, new in subs:
    if s.count(old) != 1:
        sys.exit("anchor not unique: %r" % old[:60])
    s = s.replace(old, new)
open(p, "w").write("#include <stdio.h>\n" + s)
print("patched 4 sites")
