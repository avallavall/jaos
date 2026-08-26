"""Measures the margin at phase 1's hand-over to phase 2.

Phase 1 stops when `primal_phase1_costs` returns a total of exactly 0.0, which
means every basic is inside `primal_tol` **of the `xb` it carried through the
pivots**. `run_primal` then refreshes — recomputing `xb` from a fresh
factorization — and re-checks the worst violation against the same
`primal_tol`, exactly, with nothing between the two.

So the question is how far `xb` moves across that refresh, against the bar it
is then judged by. The probe records both readings and the tolerance.
"""
import sys

OLD = """        const double left = primal_worst_violation(s);
        if (left > s->primal_tol) {
"""

NEW = """        const double left = primal_worst_violation(s);
        jm_log(s->m, JAOS_LOG_DETAIL,
               "DIAG handover before=%.17g after=%.17g tol=%.17g",
               diag_before_refresh, left, s->primal_tol);
        if (left > s->primal_tol) {
"""

# The carried reading has to be taken before the refresh that rebuilds xb.
OLD2 = """        s->needs_refactor = true;
        bool ok2 = false;
        st = refresh(s, &ok2, false);
"""

NEW2 = """        const double diag_before_refresh = primal_worst_violation(s);
        s->needs_refactor = true;
        bool ok2 = false;
        st = refresh(s, &ok2, false);
"""

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
for old, new in ((OLD2, NEW2), (OLD, NEW)):
    if src.count(old) != 1:
        sys.exit("a substitution did not apply exactly once:\n%s" % old)
    src = src.replace(old, new)
open(path, "w", encoding="utf-8").write(src)
print("patched")
