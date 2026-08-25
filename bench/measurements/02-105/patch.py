"""Adds one counting log line to refresh's repair sweep, in a worktree.

The sweep runs shift_to_feasible over every variable when repair_singular_basis
fired or a warm start armed shift_pending. The other two lending sites are
guarded by in_phase1; this one is not. The question is whether it ever runs
while in_phase1 is set, and whether it shifts anything when it does.

Counted rather than reasoned about, because a path that never executes needs a
different entry from one that executes and does nothing.
"""
import sys

OLD = """    bool sweep = repaired || s->shift_pending;
    s->shift_pending = false;
    if (sweep)
        for (int64_t v = 0; v < s->nvar; v++)
            shift_to_feasible(s, v);
"""

NEW = """    bool sweep = repaired || s->shift_pending;
    const bool diag_pend = s->shift_pending;
    s->shift_pending = false;
    if (sweep) {
        int64_t diag_nsh = 0;
        for (int64_t v = 0; v < s->nvar; v++) {
            const double diag_c0 = s->cost[v];
            shift_to_feasible(s, v);
            if (s->cost[v] != diag_c0)
                diag_nsh++;
        }
        jm_log(s->m, JAOS_LOG_DETAIL,
               "DIAG sweep repaired=%d pending=%d in_phase1=%d shifted=%lld",
               (int)repaired, (int)diag_pend, (int)s->in_phase1,
               (long long)diag_nsh);
    }
"""

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
if OLD not in src:
    sys.exit("the substitution did not apply: refresh's sweep is not the shape this expects")
if src.count(OLD) != 1:
    sys.exit("the substitution matched more than once")
open(path, "w", encoding="utf-8").write(src.replace(OLD, NEW))
print("patched")
