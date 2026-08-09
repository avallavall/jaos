# The comparison harness

The gate JAOS is aiming at is a measured competitive gap against open
solvers. This is the machinery for it. Nothing here is part of what JAOS
ships, and nothing here writes a file the gate reads.

**It has been run.** `results/T0.txt` holds the readings, and `make compare`
reproduces them: against HiGHS 1.15.1 JAOS is 3.81x slower per solve and
against SoPlex 8.0.3 1.36x, on 1.47x and 0.70x their iteration counts. See
D52, D53 and D54 for what that decomposes into. Clp and the rungs above T0
are not built yet.

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
| **T0** | dual simplex, no presolve | dual forced, presolve off, no crash basis | the simplex, and only the simplex |
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
