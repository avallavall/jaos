"""Logs the shared iteration cap and how much of it is already spent.

Phase 1, phase 2 and the dual all test the CUMULATIVE `s->iters` against
`ITER_SANITY_FACTOR * (nrow + ncol + 1)`, computed fresh in each of the three
functions but never rebased. So phase 2's real allowance is the cap minus what
phase 1 spent, and the dual's re-entry is third in the queue. D195 measured
that phase 1 spends 39.5% of every iteration and the dual 60.5%, so the queue
is not hypothetical.

One line at each of the three sites, printed where the cap is computed, so it
carries the reading at that method's START.
"""
import sys

SITES = [
 ("run_primal_phase1",
  """    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);
    const int64_t entered = s->iters;""",
  """    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);
    const int64_t entered = s->iters;
    jm_log(s->m, JAOS_LOG_DETAIL, "DIAG cap phase1 cap=%lld spent=%lld",
           (long long)iter_cap, (long long)s->iters);"""),
]

GENERIC = """    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {"""

path = sys.argv[1]
src = open(path, encoding="utf-8").read()

for name, old, new in SITES:
    if src.count(old) != 1:
        sys.exit("phase-1 site did not match exactly once")
    src = src.replace(old, new)

# The other two sites are identical text; they are run_primal then run, in file
# order, so they are labelled by the order they are replaced in.
n = src.count(GENERIC)
if n != 2:
    sys.exit("expected run_primal and run to share the cap line, found %d" % n)
for label in ("primal2", "dual"):
    src = src.replace(GENERIC,
        """    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);
    jm_log(s->m, JAOS_LOG_DETAIL, "DIAG cap %s cap=%%lld spent=%%lld",
           (long long)iter_cap, (long long)s->iters);

    for (;;) {""" % label, 1)

open(path, "w", encoding="utf-8").write(src)
print("patched 3 sites")
