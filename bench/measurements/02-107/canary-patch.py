"""Forces every flip to damage feasibility, so the probe must report it.

The first canary raised PIVOT_MIN from 1e-9 to 1e-3, meaning to make D191's
hazard a million times more reachable. It reported nothing — but it also took
the flip count from 10604 to 221, because refusing that many pivots changes the
whole trajectory rather than isolating the hazard. A canary that moves the
thing it is holding still is not a canary.

This one asks the narrower and answerable question: **can the probe see a
growth at all?** Every PHASE-2 flip is followed by a deliberate 1e6 push on every
basic, which must violate a declared bound and must grow the worst. If the
probe still reports 0, its predicate is broken and the census's 0 means
nothing.

Scoped to phase 2 because the first version damaged both phases and produced
**zero phase-2 flips over all 94**: the damage kept every solve inside phase 1,
so it never reached the half of the predicate that D191's claim rests on. It
validated the phase-1 half 260750 times and the phase-2 half not at all.

Applied on top of patch.py, so the DIAG line is already in place.
"""
import sys

OLD = """    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] -= delta * s->col[i];
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
"""

NEW = """    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] -= delta * s->col[i];
    if (!s->in_phase1)
        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] += 1e6;          /* CANARY: forced damage, phase 2 only */
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
"""

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
if src.count(OLD) != 1:
    sys.exit("the canary substitution did not apply exactly once")
open(path, "w", encoding="utf-8").write(src.replace(OLD, NEW))
print("canary applied")
