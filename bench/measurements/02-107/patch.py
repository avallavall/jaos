"""Measures what each primal bound flip does to the quantity its phase owns.

The flip's destination comes from `real_upper`/`real_lower` and its origin from
`nonbasic_value`, which reads the raw `lo`/`up` and so may read a bound dual
phase 1 invented. `delta` can then be the size of an artificial bound.

**The two phases own different quantities and the first version of this probe
measured the wrong one.** Phase 2's invariant is primal feasibility, so
`primal_worst_violation` is its predicate. Phase 1 minimises the SUM of
violations and deliberately lets one basic go further out while others come in
— `primal_phase1_ratio` skips a row that is already under its bound and moving
further under, in as many words. So the worst growing inside phase 1 says
nothing, and 113 firings measured that way were all phase 1 and all innocent.
The total is phase 1's predicate.

`diag_total` sums the same violations `primal_phase1_costs` bills but writes no
`c1`, so it cannot disturb the objective the iteration is mid-way through
using. `s->work` is saved and restored, so the instrumented run bills the same
units as the shipping one.
"""
import sys

HELPER = """
/* DIAG: the sum of declared-bound violations, without writing c1. */
static double diag_total_violation(const sx *s)
{
    double t = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        if (isfinite(lo) && s->xb[i] < lo)
            t += lo - s->xb[i];
        else if (isfinite(up) && s->xb[i] > up)
            t += s->xb[i] - up;
    }
    return t;
}

static void primal_bound_flip(sx *s, int64_t q, double delta)"""

OLD_SIG = "static void primal_bound_flip(sx *s, int64_t q, double delta)"

OLD = """                if (fabs(delta) <= step) {
                    primal_bound_flip(s, q, delta);
"""

NEW = """                if (fabs(delta) <= step) {
                    const jm_work diag_w = s->work;
                    const double diag_wb = primal_worst_violation(s);
                    const double diag_tb = diag_total_violation(s);
                    const double diag_origin = nonbasic_value(s, q);
                    const double diag_declared =
                        s->status[q] == JM_AT_LOWER ? real_lower(s, q)
                                                    : real_upper(s, q);
                    primal_bound_flip(s, q, delta);
                    const double diag_wa = primal_worst_violation(s);
                    const double diag_ta = diag_total_violation(s);
                    s->work = diag_w;
                    jm_log(s->m, JAOS_LOG_DETAIL,
                           "DIAG flip in_phase1=%d delta=%.6g invented=%d "
                           "worst %.6g %.6g total %.6g %.6g tol=%.6g",
                           (int)s->in_phase1, delta,
                           (int)(diag_origin != diag_declared),
                           diag_wb, diag_wa, diag_tb, diag_ta, s->primal_tol);
"""

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
if src.count(OLD_SIG) != 2:      # forward use in the header comment + the definition
    pass
# insert the helper immediately before the definition (the one followed by '\n{')
marker = OLD_SIG + "\n{"
if src.count(marker) != 1:
    sys.exit("could not find primal_bound_flip's definition exactly once")
src = src.replace(marker, HELPER + "\n{", 1)
n = src.count(OLD)
if n != 2:
    sys.exit("expected both flip sites to match, found %d" % n)
src = src.replace(OLD, NEW)
open(path, "w", encoding="utf-8").write(src)
print("patched 2 sites plus the helper")
