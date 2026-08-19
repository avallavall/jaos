# The count-repaired warm start publishes wrong optima through the termination hole, and the warm prize now waits behind it

Taken 2026-08-19. The D144-selected candidate, built, reviewed, gated and
refused by its own judge. Closed as D145.

## The candidate

`build_warm_basis` repaired a SHORT mapped count instead of rejecting it:
stored statuses copied to scratch, logicals promoted — uncovered rows
first, then fixed row order — until the count reached `nrow`; LONG still
refused. The cover pass billed per nonzero. Two pinned tests, the short
one validated to fail (1 vs 0 iterations) on the unrepaired tree.
`numerics-reviewer` delivered (third time this run) with four findings,
all dispositioned before the campaigns: the LONG reject case built as a
test, the cover pass billed, the producer comment fixed, the OOM rule
stated. `make test`, `make sanitize` and the `-DJAOS_NO_PRESOLVE` variant
green. The candidate is kept whole at `warm-count-repair-candidate.diff`.

## What the campaigns said

**The gate cannot see it and did not**: 94 + 29 + 16 instances
bit-identical to the committed records.

**Kennington warm is a clean win**: work geomean 0.0572 → 0.0070, all five
shortfall-1 solves recovered — `osa-60` from 7061 iterations and 7.3e9
work to **1 iteration** and 6.4e6 — zero instances worse.

**netlib warm is disqualifying** (`candidate-warm.txt`): the geomean
improves, 0.2553 → 0.1636, and underneath it

- **8 solves DISAGREE**: the warm solve publishes `optimal` with a WRONG
  objective, `checker=warm:REJECTED/cold:ok` — `dfl001` 3.099e8 against
  the true 1.127e7, `modszk1` 1135456 against 321, plus `cycle`, `d2q06c`,
  `degen2`, `greenbea`, `maros`, `woodw`;
- **2 more REJECTED**: `pilot87` and `scsd1` reach the objective and the
  checker refuses the warm point;
- 13 of 82 take more iterations warm than cold, worst `pilot-ja` at
  8.34x work, `bnl2` at 7.8x.

Before the candidate, all four counters read zero.

## What the refusal means, and what it does not

The promoted basis is structurally valid — count `nrow`, singularity
repaired by the machinery that exists for it — and the solve that starts
from it can stop at a suboptimal vertex and call it optimal. That is the
termination defect §5a already carries from D119 ("the termination test
never re-reads dual feasibility, so a numerically damaged solve publishes
`optimal`"), now with eight named reproductions driven from a valid basis.
The refusal is of the candidate AT HEAD, not of the design: nothing in the
count repair chose those vertices; the termination did.

It also puts `jaos.h`'s promise in doubt — a hostile basis "costs time and
cannot produce a wrong verdict". The candidate manufactured count-valid
bases and eight wrong verdicts. Whether a caller can do the same at HEAD
through `jaos_set_basis` alone is one probe away and `TODO.md` carries it.

## What landed anyway

The LONG-map pinned test (`test_a_long_mapped_basis_falls_back_cold`),
reworded for HEAD: it pins the cold fallback with its premise asserted
(`presolve_num_row == 1`), so any future count repair or trim moves a test
deliberately instead of landing silently.

## Reproducing it

Apply `warm-count-repair-candidate.diff` in a worktree; the campaign
readings are `candidate-warm.txt` and `candidate-warm-kennington.txt`
beside this file. The committed warm records are the other side.
