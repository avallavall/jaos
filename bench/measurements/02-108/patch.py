"""Logs how many iterations the primal phase 1 actually ran, on every exit.

**This exists because D194 got it wrong.** `split.c` read phase 1's count from
the log line `phase 1 reached a feasible point in N iterations`, which is
printed only on SUCCESS. When phase 1 runs and does not finish — a budget, a
refusal — no line is printed, the probe left the count at 0, and the instance
read as "phase 1 took zero iterations, so this is a pure phase-2 run". D194
published that as "the 8 that run a real phase 2 are exactly the 8 whose phase 1
is zero iterations". A later canary found 2634 phase-1 bound flips on those
same 8 instances, which cannot happen if phase 1 never ran.

One site, placed after the call rather than inside it, so every exit from
`run_primal_phase1` is covered. An instance that skips phase 1 entirely prints
no line at all, which is the honest reading of "phase 1 did not run".
"""
import sys

OLD = """        bool feasible = false;
        s->in_phase1 = true;
        st = run_primal_phase1(s, out, &feasible);
        s->in_phase1 = false;
"""

NEW = """        bool feasible = false;
        const int64_t diag_p1_start = s->iters;
        s->in_phase1 = true;
        st = run_primal_phase1(s, out, &feasible);
        s->in_phase1 = false;
        jm_log(s->m, JAOS_LOG_DETAIL,
               "DIAG phase1 ran %lld iterations feasible=%d st=%d out=%d",
               (long long)(s->iters - diag_p1_start), (int)feasible,
               (int)st, (int)*out);
"""

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
if src.count(OLD) != 1:
    sys.exit("the substitution did not apply exactly once")
open(path, "w", encoding="utf-8").write(src.replace(OLD, NEW))
print("patched")
