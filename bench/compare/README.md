# The comparison harness

The gate JAOS is aiming at is a measured competitive gap against open
solvers. This is the machinery for it. Nothing here is part of what JAOS
ships, and nothing here writes a file the gate reads.

**It has been run, and all four rungs exist.** `results/T0.txt` through
`T3.txt` hold the readings and `make compare` reproduces them. At T0, against
HiGHS 1.15.1 JAOS is 3.71x slower per solve and against SoPlex 8.0.3 1.35x,
on 1.47x and 0.70x their iteration counts (D52, D53, D60). The rungs above it
say what the missing features are worth: **free algorithm choice nothing, on
identical iteration counts; presolve 1.42x against HiGHS and 1.14x against
SoPlex; stock defaults nothing further** (D81).

**Clp is measured, and it did the job it was added for (D83).** 3.77x slower
per solve on 1.67x the iterations and 2.26x per iteration. The three
competitors disagree about the iteration count — 1.47x, 0.70x, 1.67x — and
agree about the cost of an iteration, 2.54x, 1.92x, 2.26x. That is a property
of JAOS rather than of any one rival, which is the whole reason a third
reading exists. `build_clp` fetches CoinUtils and Osi alongside it, both
pinned by checksum in `clp-deps.manifest` on the same terms as a competitor.

**JAOS gained a presolve, so the ladder recalibrated — P0 is the new bottom
rung (D103).** T0 through T3 keep their definitions and their records; nothing
was renamed underneath a stored number. P0 is T0 with one line changed per
competitor: presolve on. JAOS at P0 is dual simplex, presolve, no crash basis,
one thread, which is what it now ships.

**T0's record is historical from here.** It was taken when JAOS had no
presolve. Running it against a presolving JAOS would put presolve on one side
only, and that is not a rung.

**P0 was re-taken 2026-08-17, after the implied free column singleton
(D106).** That reduction took `maros-r7` from 21010708013 work units to
328053926, and `maros-r7` was the single worst instance here: 72.5x HiGHS at
the 2026-08-14 reading, 1.33x on 1.05x the iterations now. JAOS is faster
than SoPlex per solve for the first time, 0.95x. The worst instance is now
`stocfor3` at 30.0x HiGHS, and `TODO.md` §5 owns that gap. The 2026-08-14
reading is kept in `results/P0-2026-08-14.txt`; every figure on this page
that was computed from it says so.

| P0, `results/P0.txt`, 2026-08-17 | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|---|
| time per solve | 3.15x | **0.95x** | 2.57x |
| iterations | 1.55x | **0.63x** | 1.31x |
| **time per iteration** | **2.04x** | **1.51x** | **1.95x** |
| JAOS faster on | 1 of 17 | **11 of 20** | 2 of 13 |
| worst instance | `stocfor3` 30.0x | `grow22` 8.4x | `stocfor3` 26.4x |

Against T0's 3.72x / 1.34x / 3.77x (D83): closer on all three. **Read a rung
difference against each solver's own presolve, not through JAOS** — same
binary, two rungs, geometric mean of per-instance time ratios over the
instances each has above the 0.05 s floor. This table was computed from the
2026-08-14 reading, before D106:

| solver | its own presolve, T0 → P0 | its iterations |
|---|---|---|
| JAOS | 0.739x | 0.820x |
| HiGHS | 0.692x | 0.717x |
| SoPlex | 0.906x | 0.904x |
| Clp | 0.670x | 0.789x |

So JAOS's presolve is worth about what the others' are worth. At the
2026-08-14 reading two instances carried most of the remaining gap to HiGHS:
on `maros-r7` HiGHS's presolve read 0.378x and halved its iteration count
while JAOS's read 1.065x and removed nothing, and `stocfor3` was the same
shape, 0.198x against 0.965x. D106 closed the `maros-r7` half: JAOS now
removes 980 rows there and the instance reads 1.33x. `stocfor3` remains, at
30.0x HiGHS and 26.4x Clp, and it is now the worst instance in the
comparison.

**The rung reproduces two figures taken a different way**, which is what makes
it believable: HiGHS's 0.692x is 1.445x inverted, against the 1.417x of D81
and 1.421x of D83, and SoPlex's 0.906x is 1.104x against D83's 1.111x. Those
were measured as T2 − T1, with algorithm choice free; this is T0 → P0 at the
2026-08-14 reading, with the dual forced. Two routes, one number.

Two readings in `results/P0.txt` are worth knowing about, and both hold in
both takes. On `pilot87` both SoPlex and Clp publish 301.7106806 where JAOS
and HiGHS agree on 301.7103474, so the harness discards their times — the
rule that a time without a verified answer is thrown away is doing its job,
and it is JAOS and HiGHS on one side of it. And `grow22` reads 8.8x slower
than at T0 (9.5x at the 2026-08-14 reading), which is presolve's own
regression on that model (D103) confirming in seconds what the work counter
said; `TODO.md` §2 owns it.

The header of `results/P0-2026-08-14.txt` says `WITH UNCOMMITTED CHANGES`.
Those were the three P0 tier files themselves, which are harness inputs;
`src/` was clean at `fd1bd6d` and the JAOS binary came from it. The
2026-08-17 take was run at `a88e99b` with a clean tree.

**Clp's summary was silently absent from T1, T2 and T3, and the records held
its data the whole time.** Clp reports zero iterations on `d6cube`, `maros-r7`
and `woodw` whenever it is free to choose its algorithm. Dividing by that is a
fatal error in awk rather than a NaN, so the block aborted before printing its
first line and the loop moved on. Nobody noticed for three days because no Clp
rung figure was ever quoted — which is the reading of "silent" that matters.
`run-compare.sh` now keeps such an instance in the time row, drops it from the
two iteration rows, and prints how many it dropped and which.

Recomputed from the stored records, so the ladder is complete for the first
time. Time per solve:

| | T0 | T1 | T2 | T3 | P0 (08-17) |
|---|---|---|---|---|---|
| vs HiGHS | 3.72x | 3.71x | 4.53x | 4.46x | **3.15x** |
| vs SoPlex | 1.34x | 1.33x | 1.46x | 1.50x | **0.95x** |
| vs Clp | 3.77x | 3.79x | 5.88x | 5.92x | **2.57x** |

T2 − T1 prices Clp's presolve at 1.55x, which the direct T0 → P0 reading of
Clp against itself put at 1.49x on the 2026-08-14 take. Two routes, and they
agree to within a little over the harness's own repeatability.

**The recomputation reproduces T0 exactly** — 3.72x, 1.34x, 3.77x, and
`maros-r7` at 25.7x — from a second implementation of the arithmetic that
shares no code with the harness. D83's table stands as published.

**The measured repeatability of this harness is 1.4%**, and it comes from the
best control available: JAOS is byte-identical at every rung, so its own
cross-rung ratio is a direct reading of the machine — 1.007x, 1.014x and
1.012x with iterations exactly 1.000x. Any claim below that is a claim about
the machine.

**Read a rung difference against the competitor itself, not through JAOS.**
JAOS is unchanged at every rung, so competitor-at-T2 against
competitor-at-T1 measures presolve directly; a ratio of two JAOS-versus-them
ratios says the same thing with two extra sources of noise in it.

## The record here is not the record

`bench/results/` and `bench/*.baseline` carry deterministic work units and
no wall-clock number, because a timing nobody can reproduce is not evidence
(D17). That rule is not relaxed here — this is a **different record**, in
`bench/compare/results/`, and it carries seconds because seconds are the
whole question a competitive comparison asks.

The two must never be confused, so every line written here names the machine
it came from, and a machine that D17 excludes for published figures says so
on every line. **A number taken under WSL is a development number.** It is
useful for catching a regression and for building this harness; it cannot
close a gate.

## The ladder, and why it is a ladder

Comparing JAOS at its current capability against another solver at full
capability answers one question badly. The same comparison run at four
settings answers four questions well, because **each rung's difference from
the one below attributes the gap to one feature**.

| tier | JAOS | the others | what the step measures |
|---|---|---|---|
| **T0** | dual simplex, no presolve | dual forced, presolve off, no crash basis | the simplex, and only the simplex. **Historical**: JAOS presolves now |
| **P0** | dual simplex, presolve | dual forced, presolve on, no crash basis | the same question, recalibrated. The rung M2's gate is judged on |
| **T1** | unchanged | free to pick primal or dual | what choosing the strategy is worth |
| **T2** | unchanged | presolve on | what presolve is worth |
| **T3** | unchanged | stock defaults | what a user actually experiences |

So T1 − T0 is the price of having no primal simplex, and T2 − T1 the price
of having no presolve. That is a build order derived from measurement rather
than from opinion, which is the same move `JAOS_DIAG`'s per-line attribution
makes inside the solver (PLAN 3.5).

**The ladder is recalibrated as JAOS grows.** When JAOS gains a presolve,
presolve moves into T0 for everyone and a new rung appears above. Records
therefore carry the tier definition they were taken under; a comparison
against a differently-defined rung is not a comparison, and the same mistake
in a different costume already cost this project a wrong target three times
(PLAN 3.5).

## Rules the harness enforces

- **A time without a verified answer is discarded.** Every competitor run is
  judged the way JAOS is: did it reach an optimum, and does its objective
  match Koch's reference within the gate's tolerance. A solver that is fast
  and wrong wins any table that does not check.
- **Tolerances are equalised explicitly.** HiGHS defaults to 1e-7 primal and
  dual, SoPlex to 1e-6, JAOS sits at the stricter of the two (PLAN 2.10). A
  timing taken at different tolerances is not a comparison of solvers.

  **And equal settings were checked to mean equal answers**, because they
  need not: two solvers can spend the same tolerance differently, and a JAOS
  that quietly delivered more precision than anyone asked for would be paying
  for it in the time column. Against Koch's reference, with each solver's
  full-precision output rather than its log line: `25fv47` JAOS 15.0 correct
  digits against HiGHS 14.9, `bandm` 14.8 against 14.8, `capri` 15.8 against
  ~16. **Equivalent.** JAOS is not being held to a stricter standard than the
  gap credits it with.

  The first attempt at that check said JAOS delivered 3.47 digits more, and
  it was wrong in an instructive way: it compared the objectives as printed
  in each solver's log, and HiGHS prints eleven significant digits while
  SoPlex prints nine. It was measuring `printf`. The giveaway was in the
  spread — every HiGHS reading fell between 10.0 and 12.1, every SoPlex one
  between 8.4 and 10.3, each pinned just under its own format width, which is
  not how accuracy is distributed.
- **Competitor versions are pinned by checksum**, like the instances
  (PLAN 2.10). Otherwise "the competitive gap" moves on its own every time
  somebody upstream tags a release.
- **Two times per run**: what the solver reports for its own solve, and what
  the process took. SCIP starts up heavily; the first is the fairer number
  for comparing simplex implementations and the second for comparing what a
  user gets. Both are recorded because they answer different questions.
- **The minimum of N runs, not the mean.** The mean measures how busy the
  machine was.

## Which solvers, and why those

For **M2**, which is LP:

| solver | why it is here |
|---|---|
| **HiGHS** | Its dual simplex is Huangfu and Hall's, which is reference [10] in JAOS's own bibliography. The direct academic lineage of what JAOS implements. |
| **SoPlex** | The other serious open dual simplex, and what SCIP runs underneath. Comparing against it is comparing against SCIP without the wrapper. |
| **Clp** | The veteran. A third reading, and the one that shows whether the other two agree by coincidence. |

For **M3 and M4**, which are MILP: **SCIP**, which is the rival worth having
there, with branch-and-cut rungs of its own. It is pinned here from the
start and not run until then — measuring JAOS's simplex against SCIP would
measure SoPlex plus SCIP's overhead.

GLPK is deliberately absent: it would be a floor rather than a target, and
nothing is learned from clearing a floor.

Licences are recorded in `solvers.manifest` and **verified at fetch time
rather than trusted**, because they change between versions. Running another
solver as a benchmark competitor is not what D12 forbids — that rule is
about writing JAOS's code from someone else's, and the same distinction was
already drawn for netlib's `emps` (PLAN Q6). No competitor's source is read.

## What closes M2

**JAOS strictly faster than the best competitor at T0**, on the geometric
mean of per-instance time ratios over the standard set, with a guard that no
single instance is more than a stated factor slower. The geometric mean is
what ratios require; an arithmetic mean of ratios flatters whichever side
wins the big instances.

That is an ambitious target and this file says so plainly: HiGHS's dual
simplex is mature and heavily tuned. The ladder exists so that progress
short of it is visible and attributable rather than a repeated "not yet".
