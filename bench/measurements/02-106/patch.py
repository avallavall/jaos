"""Prints what the D146 guard sees, in a worktree.

`settled_dual_violation` returning non-zero on a cold start is the one
refusal in this solver that writes no message, so `jaos_model_error` is empty
for every instance that hits it. This adds a line naming the worst breach, the
column carrying it, and how many loans `settle_shifts` had to call in.

The loan census is taken BEFORE settle_shifts, so it says what the point was
carrying rather than what is left after the repayment.
"""
import sys

OLD = """        settle_shifts(&s);
        if (settled_dual_violation(&s) != 0.0) {
"""

NEW = """        {
            int64_t diag_n = 0;
            double diag_max = 0.0;
            for (int64_t v = 0; v < s.nvar; v++)
                if (s.shift[v] != 0.0) {
                    diag_n++;
                    if (fabs(s.shift[v]) > diag_max)
                        diag_max = fabs(s.shift[v]);
                }
            jm_log(m, JAOS_LOG_DETAIL,
                   "DIAG loans before settle: %lld columns, largest %.6g",
                   (long long)diag_n, diag_max);
        }
        settle_shifts(&s);
        {
            int64_t diag_at = -1;
            double diag_worst = 0.0;
            for (int64_t v = 0; v < s.nvar; v++) {
                const double br = published_breach(&s, v);
                if (br > diag_worst) { diag_worst = br; diag_at = v; }
            }
            jm_log(m, JAOS_LOG_DETAIL,
                   "DIAG settled violation %.6g at var %lld of %lld, "
                   "status %d, d %.6g, cost %.6g, cost0 %.6g",
                   diag_worst, (long long)diag_at, (long long)s.nvar,
                   diag_at >= 0 ? (int)s.status[diag_at] : -1,
                   diag_at >= 0 ? s.d[diag_at] : 0.0,
                   diag_at >= 0 ? s.cost[diag_at] : 0.0,
                   diag_at >= 0 ? s.cost0[diag_at] : 0.0);
        }
        if (settled_dual_violation(&s) != 0.0) {
"""

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
if src.count(OLD) != 1:
    sys.exit("the substitution did not apply exactly once: the guard is not the shape this expects")
open(path, "w", encoding="utf-8").write(src.replace(OLD, NEW))
print("patched")
