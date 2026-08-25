"""Guards phase 2 as well as phase 1, in a worktree, for one measurement.

D191 tried this and reported only its effect on the agreement count: 54
agreeing became 20, which it read as over-correcting. Nobody measured what it
does to the ITERATION SPLIT, and that is the question here — whether the
primal's phase 2 stops after one iteration because `update_dual` and
`pivot()` zero `d[v]` on every breached nonbasic, which is exactly what
`primal_price` reads.

`in_phase1` stays set across the whole of `run_primal` and is cleared at the
call site, so the dual's own re-entry still lends, as it must.
"""
import sys

PAIRS = [
 # keep the flag set when phase 1 hands over to phase 2
 ("""        st = run_primal_phase1(s, out, &feasible);
        s->in_phase1 = false;
""",
  """        st = run_primal_phase1(s, out, &feasible);
        /* DIAG: left set, so phase 2 is guarded too */
"""),
 # set it for the whole primal, clear it before anything else runs
 ("""        st = m->cfg.force_primal ? run_primal(&s, &outcome)""",
  """        s.in_phase1 = true;      /* DIAG: guard the whole primal */
        st = m->cfg.force_primal ? run_primal(&s, &outcome)"""),
]

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
for old, new in PAIRS:
    if src.count(old) != 1:
        sys.exit("substitution did not apply exactly once:\n%s" % old)
    src = src.replace(old, new)

# clear it as soon as run_primal returns, whatever it returned
TAIL_OLD = """        s.in_phase1 = true;      /* DIAG: guard the whole primal */
        st = m->cfg.force_primal ? run_primal(&s, &outcome)"""
i = src.index(TAIL_OLD)
j = src.index(";", src.index("run(&s, &outcome)", i)) + 1
src = src[:j] + "\n        s.in_phase1 = false;     /* DIAG */" + src[j:]
open(path, "w", encoding="utf-8").write(src)
print("patched")
