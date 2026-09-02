# 02-159 — why agg's re-entry dies at round 0

D248. The forced primal's re-entry hands its armed point to the dual and
the dual answers `JAOS_SOLVE_INFEASIBLE` immediately, on a model it solves
to a checked optimum from cold. D245's census left this as the family to
read next and named `agg` the instance to read it on.

## What is here

| file | what it does |
|---|---|
| `agg-diag.sh` | throwaway diagnostic build in a mktemp worktree: three probes at the INFEASIBLE exit and inside `bfrt_walk` |
| `agg-diag.txt` | the run, as taken |

Derives the repository root and runs from anywhere (D217). The repository
tree is never touched; the patch asserts each anchor matched exactly once.
`agg-diag.txt` is the run as taken on the defective tree. On a tree
carrying D249's repair the walk probe still fires on `agg` — the walk
still exhausts with the 7.28e-12 leftover — but the INFEASIBLE probe no
longer does: the exhaustion branch converts the leftover into a blocker
and the solve continues. A re-run is that negative control, not a
replacement for the record.

## The finding, in one line

The bound-flip ratio test absorbs the entire 57911.196 violation to within
**7.28e-12** — one ulp of the 5e4-magnitude terms it summed — and the
blocking test `remaining - den*width > 0.0` reads that residue as a real
leftover, retires every candidate, and `live == 0` is published as "the
model has no feasible point".

The walk, from `agg-diag.txt`: candidates v=0 (absorbs 4337.11), v=5
(absorbs 53574.08), and v=528 — a FIXED column, `lo == up == 0`, width 0 —
which absorbs nothing, blocks nothing, and still drains `live` on its way
out. Both of the verdict's passes (carried, then verified fresh) print the
identical walk, so the failure is deterministic, and the residue is five
orders of magnitude below `primal_tol` and eleven below the violation. Two
flips put the basic within 7.28e-12 of its bound, which every other
predicate in the solver calls feasible.

## What was refuted on the way, and by which probe

- **The priced row's bound is lent, so the certificate is about the loaned
  model.** Refuted by probe 1: the violated bound is real (`lo=0 ==
  real_lo`, `fake=0`), a basic 57911 under a genuine floor.
- **Sign-locked candidates sit on lent bounds, so the Farkas reading rests
  on a loan.** Refuted by probe 2: the four lent-bound columns in the row
  are sign-locked the harmless way (their movement lowers the basic
  further) and the three admissible candidates all sit on real bounds.
- **The first census instrument read the dense alpha array over every
  nonbasic.** Wrong thing to read: outside the sparse pattern those
  entries can be stale, and only an assert no NDEBUG build runs checks
  them. The kept probe walks exactly what the ratio test walked, and an
  assert-enabled run (`dn == n`) confirmed the pattern is complete — the
  seven entries are the whole row.

## What this hands forward

The repair is open work in `TODO.md`: a `live == 0` return whose remaining
violation is at or under `primal_tol` is a repaired row, not an infeasible
model — the flips should be applied and the row re-priced. Whether the 29
reference infeasibles ever cross that branch with a sub-tolerance margin is
what the campaign on the repair must read. The fixed column admitted as a
candidate is a second, separate question: it can never flip usefully and
never enter, and today it only distorts `live`.
