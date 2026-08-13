# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## 1. The postsolve dual-recovery defect — the gate is red

15 standard-set and 4 Kennington answers are refused by the checker on its
dual sign condition. All 110 objectives match Koch's reference and all 139
instances are deterministic, so the primal side is right and the published
multipliers are wrong. Presolve compiled off (`EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`)
reproduces all three baselines bit for bit (D96), so the defect is in what
postsolve publishes.

Bisection, each tree built detached and run against the committed baseline:

| tree | checker ok | rejected |
|---|---|---|
| `03c28c9` — end of 02-02 | 93/94 | 1 (`finnis`, D94) |
| `8425acc` — end of 02-03 | 78/94 | 16 |
| `d861b22` — end of 02-04 | 79/94 | 15 |

The whole fall is inside 02-03's diff (`9aba410`). Raw runs:
`bench/measurements/02-04/` (the `attribution-*` files).

Rejected at `d861b22` — netlib: `25fv47 bnl1 bnl2 czprob e226 finnis lotfi
perold pilot-ja pilot-we pilot87 pilotnov share1b tuff vtp-base`.
Kennington: `ken-07 ken-11 ken-13 ken-18`.

The approach, in order:

1. `numerics-reviewer` on `git diff 03c28c9..8425acc` with the table and the
   instance lists in hand. The families are empty row/column, singleton
   row/column and the mutual free-column-singleton; the question per family is
   whether the multiplier it publishes satisfies the checker's sign rule for
   the status the variable came back with. One case is already found and
   fixed — the singleton row's stale status discriminator — and its
   derivation-in-a-comment in `src/presolve.c` is the pattern to follow.
2. A throwaway traced build on one small rejected instance (`vtp-base` prints
   `dual=2.58e+03` against an objective of 1.3e+05): print every replayed
   record — family, row/column, multiplier, status, what the sign rule wants.
   The first disagreement names the site. Instrument before repairing; 02-04
   lost three campaigns to hypotheses before one trace answered it.
3. The fix, with a test shown failing on the pre-fix code first.

Prohibited: widening any tolerance, clamping a recovered value, reverting a
reduction family. The objectives are already right; a tolerance that admits a
wrong multiplier admits every wrong multiplier of that size.

## 2. Presolve, to finish (REQ-presolve)

- Duplicate rows, duplicate columns, dominated columns — the last three
  families. Both detection tolerances swept (`make clean` between settings, a
  canary that must move); the near-tie cases constructed and shown rejected,
  not hoped absent; the duplicate/dominance boundary stated in the source and
  tested on both sides. The duplicate-column split is the one postsolve record
  that returns two primal values from one.
- `numerics-reviewer` over the whole presolve/postsolve diff, after the last
  family lands and before the campaigns. Every finding gets a disposition:
  fixed with its commit, refused with its reason, or carried with its
  destination.
- All three sets plus the presolve-off negative control, then the deliberate
  three-baseline rewrite (`make netlib-baseline` and siblings), confirmed by a
  following gate run. Known by-design movements at `d861b22`: 26 netlib + 1
  infeasible (`bgindy`) + 4 Kennington (D96), minus whatever #1's fix changes.
  No rewrite while the gate is red.
- Presolve's measured number (the D-15 figure): work ratio and `J=1` time
  ratio reported separately, negative control beside them, raw readings to
  `bench/measurements/`, judged by `jaos-measurer`. The runner prints seconds
  as `%8.3f` and must be fixed first — see standing debts.
- Close-out: CHANGELOG entry updated, SPECS presolve row updated, this
  section deleted.

## 3. After presolve — the rest of M2, in order

- **Factorization** (REQ-lu-fill-and-markowitz, REQ-hyper-sparse-downstream):
  the stale live counts Markowitz chooses on, and the fill — factors carry
  2.673x the basis nonzeros (4.801 on `maros-r7`); keep sparse triangular
  results sparse downstream (`stocfor3`: 6.79x per iteration, solves 43%,
  memset/memcpy/malloc 18.8% against 11.3% on `dfl001`). Left-looking
  elimination is a rewrite and needs its own decision first. Struck off by
  measurement, do not re-cost: `compact_pivot_row`'s row-to-position lookup
  (<0.5% on `maros-r7`); per-column arrays vs one arena (0.73% + 0.30%; the
  locality argument needs a cache simulation before it is costed or dropped).
- **Search path** (REQ-devex-pricing — acceptance stated: full gate with
  iteration count and per-iteration cost reported separately;
  REQ-reentry-oscillation — investigative first: 0.24% on `pilot87` at
  interval 24, D51 names the mechanism, D74 closed the only proposed cure,
  D89 removed the consequence).
- **Close M2** (REQ-m2-competitive-gate): needs a controlled host — D17 says
  a WSL number cannot close a gate, and this machine is Windows/WSL with a
  measured repeatability of 6.27% (D93). The per-instance guard factor is
  unset and is measured, not guessed. T0 is presolve-off on both sides by
  definition; how the comparison ladder recalibrates once JAOS has a presolve
  is undecided (`bench/compare/README.md`). Standing: 3.72x HiGHS, 1.34x
  SoPlex, 3.77x Clp (D83).

## 4. After M2 — feature expansion (decided 2026-08-13)

Two decisions are locked: the two premises are absolute (no external code,
bit-identical everywhere; a feature that cannot be built under them is not
built), and the goal is the best open solver that is deterministic across
machines and ships its own checker — not matching Gurobi.
`docs/feature-matrix.md` is the scoreboard; read it at every close. Whether
M2 finishes as scoped is answered when presolve closes.

Proposed order: cheap breadth first (write MPS, write LP, write a solution
file, Python bindings, sensitivity and ranging, infeasibility certificates),
then primal simplex, then barrier with crossover, then MILP, then
QP/conic/NLP/MINLP. VIPR-format certificates are a cheap differentiator —
only SCIP 10.0 emits them and JAOS already ships a checker. For exact
rational verification, GMP is excluded (D11); the methods to weigh are
iterative refinement, interval arithmetic in double, or hand-rolled
rationals for the final basis only.

## Standing debts — small, real, none blocks the sections above

- `bench/run.c` prints seconds as `%8.3f`: 8 standard-set instances carry no
  time ratio at all and 42 read exactly 1.0000x. Two lines; legal now that no
  measurement is in flight.
- `preflight.sh` does not check committed records for `baseline: NOT
  COMPARED`; such a record sat committed once already.
- The `REFACTOR_EVERY` 16..256 trajectory sweep is manual; three of M1's four
  defect closures came from it and no target automates it.
- Test ceilings drift silently — the `<62000` one drifted 2800 units with
  nothing watching. Re-measure a ceiling's both sides when touching its
  subject.
- `pilot87`'s suboptimality bound is not understood (`gap_positive` moves
  0.0068–26.7 across D92's variants while every answer is inside tolerance).
  Deferred with a trigger: it re-enters the plan if it blocks a gate, and it
  already refused two of D92's three candidate repairs.
- Restricting the candidate set ahead of `bfrt_walk`/`jm_harris_pick` is open
  and not refused (D93); it puts Harris's guarantees at stake and needs its
  own decision before any code.
- `galenet` makes two `dual_ratio_test` calls in a one-iteration solve —
  calls are not iterations in any work-saved arithmetic (D93).
