---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 2
current_phase_name: Presolve and postsolve
status: executing
stopped_at: Completed 02-01-PLAN.md. D-01's scaffolding is proved end to end — reduced model, postsolve arena, the fixed-column reduction, the negative-test instrument, and the per-instance record — with two findings handed to 02-09 (the finnis exception to D24) and to 02-03..02-05 (trajectory movement is not a defect)
last_updated: "2026-08-13T06:45:00.000Z"
last_activity: 2026-08-13
last_activity_desc: Executed 02-01 — presolve/postsolve scaffolding, the fault-injection instrument, and the standard-set campaign with its one recorded checker exception (finnis, D24) and one caught-before-shipped near-miss (a long-double accumulator that cost pilot87 2.3x its work and was dropped)
progress:
  total_phases: 2
  completed_phases: 1
  total_plans: 14
  completed_plans: 6
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-12)

**Core value:** A correct answer, bit-identical on every machine and every run, proved by a checker that had no access to the solver that produced it.
**Current focus:** Phase 2 — Presolve and postsolve
**Milestone:** M2 — LP competitiveness

## Current Position

Phase: 2 (Presolve and postsolve) — EXECUTING
Plan: 2 of 9 (01 complete)
Status: Executing Phase 2
Last activity: 2026-08-13 — Executed 02-01 (presolve scaffolding, D-10 instrument, standard-set campaign)

Progress: [██▓░░░░░░░] 21%

## Performance Metrics

**Velocity:**

- Total plans completed: 6
- Average duration: ~68 min
- Total execution time: ~370 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 5 | - | - |
| 02 | 1 | - | - |

**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files | Commits |
|------|----------|-------|-------|---------|
| 01-01 | ~13 min | 2 | 3 | 2 |
| 01-02 | ~22 min | 1 | 3 | 1 |
| 01-03 | ~55 min | 2 | 8 | 2 |
| 01-04 | ~75 min | 3 | 1 | 1 |
| 01-05 | ~35 min | 2 | 4 | 3 |
| 02-01 | ~170 min | 3 | 7 | 6 |

**Recent Trend:**

- Trend: duration is set by machine time, not by task count or diff size.
  01-02 was 9 lines of code and cost 22 minutes in measure-revert cycles;
  01-03 wrote no code at all and cost 55, of which **34.4 min was campaign
  time under WSL** that no estimate models. Kennington alone is ~8 min and
  this plan ran it three times.

- **01-04 is the extreme of that trend.** It modified no repository file by
  design, so its whole diff is one summary — 7,400 tokens against an estimate
  of 70,000, an order of magnitude over. What it actually cost is **47 minutes
  of WSL machine time**: 12 sequential `J=1` passes over the standard set, two
  callgrind runs and three builds. No field of the estimate models that, and a
  plan whose deliverable is a number will always look free to a token estimate.

- **01-05 is the one plan the estimate could see**, and it is the shape that
  explains the other four: 16,400 realized against 45,000 estimated, under by
  2.7x rather than by 10x. A documentation plan's length is set by the number
  of things that have to be recorded, and that number was known in advance. No
  machine time at all — it builds nothing and runs no campaign.

- **02-01 is 01-03's pattern at a different scale: four full `J=12` campaigns
  of all three sets**, not because the plan asked for four but because a
  checkpoint-driven investigation did — one campaign found `finnis`, a second
  confirmed the tree, a third isolated an unplanned regression on `pilot87`
  to a specific line by A/B rebuild, and a fourth produced the record that
  was actually committed. Each `netlib`+`netlib-infeas`+`netlib-kennington`
  round is itself ~9–10 min at `J=12`; four of them is most of the 170.
  What the estimate could not see is that this plan's own `<verify>` asked
  for a campaign that was going to find something — D-01's whole rationale
  is that postsolve correctness is the part nothing else catches, and it
  did not stay abstract.

*Updated after each plan completion*

## Accumulated Context

### Decisions

`DECISIONS.md` at the repository root is the authority — D1 through D92, all
locked, each carrying the measurement that closed it. PROJECT.md's
`<decisions>` block carries the IDs that constrain this milestone; it does not
replace the file.

Roadmap-level decisions taken at milestone opening:

- Milestone 1 of this roadmap is the project's **M2 — LP competitiveness**
- **Candidate admission before presolve** (WARNING 1) — D81 measured presolve at
  1.417x/1.136x against a per-iteration gap of 2.53x, and says that reorders
  the plan

- **The candidate-admission decision is the first deliverable, before any code**
  — PLAN.md item 3a says so outright

- **The re-entry oscillation is roadmap work** (WARNING 2), not an accepted
  limitation, despite a measured cost of 0.24% on `pilot87` and no cure named

- **The M2 per-instance guard factor stays absent until measured** (WARNING 3)
  — no number is invented anywhere in these artifacts

Taken during execution of 01-01:

- **The nonbasic set is a persistent bitmap**, `s->nbmark`, deviating from the
  `amark` bitmap-clears-itself convention on purpose and saying so in the
  header and the struct

- **The D-08 cross-check runs over both branches**, not only the one it
  replaced, so the pattern/dense equivalence claim at `simplex.c:1562-1568` now
  has a run-time proof on every solve in the suite

- ~~**A missed `jm_nonbasic_remove` is a performance fault, not a correctness
  one**~~ — **this was falsified inside the same phase and is corrected in
  `62ac240`.** It held against the tree `f2ed4bc` left: a superset bitmap
  changes no candidate, because `admit_candidate` rejects `JM_BASIC` first, so
  the D-08 assertion was silent and right to be. Then `b65d9f2` made the dense
  branch bill `visited`, and a superset began inflating `s->work.units` — which
  is compared against `cfg.work_limit`, so the same model stops at a different
  point and publishes a different answer. Closed by
  `assert(visited == s->nvar - s->nrow)`, calibrated against the fault the
  cross-check is silent on. See D93's amendment. Neither plan's reading was
  wrong alone; the defect lived between them

Taken during execution of 01-02:

- **The dense branch bills the visited count; `pivot`'s sweep still bills
  `s->nvar`** and is right to — it reads every variable's status. The plan's
  `grep -c 'nvar * JM_WORK_NONZERO' == 0` criterion was therefore refused:
  meeting it would have corrupted work units on every instance with nothing
  pinned to catch it

- **The bitmap words are billed nothing.** A word is not a variable, and a rate
  for them would be a number with no measurement on either side. The
  `nvar/64`-read floor it leaves is recorded in `docs/work-units.md` beside the
  unbilled `alpha` sweep

- **The `<62000` ceiling's failure side was re-measured rather than carried**,
  which the plan permitted skipping. Once the correct side came in at 60701 —
  2.1% under the bound — a stale pair could no longer answer whether 62000 still
  separates correct from broken. It does: 60701 < 62000 < 64633

Taken during execution of 01-03:

- **The digest confirmation got its own commit before the rewrite ran**
  (44c0ef6, then e8c2f58), so D-06's ordering is a fact `git log` can be asked
  rather than a claim a summary makes about itself

- **Work is the only thing that moved, on all five records**, proved by masking
  the work field and diffing end to end against 64efcc6 rather than by reading
  `record_diff.py`'s per-instance list — which has a reporting gap on
  sub-threshold `drop` changes. Iterations are 1.0000x per instance, not merely
  in the mean

- **The saving is an exact whole multiple of the row count on all 139
  instances**, which inverts to the number of dense-branch calls without
  instrumenting anything. It counts **calls, not iterations**: `galenet` makes 2
  in a solve reporting `iters=1`, so `dual_ratio_test` runs somewhere that is
  not a counted iteration

- **D46 was wrong in both directions on the same change** — the ratio of totals
  understates it on the standard set (0.9933x against a per-instance 0.9779x)
  and overstates it on the infeasible set (0.9669x against 0.9857x)

Taken during execution of 01-04:

- **VERDICT: INCONCLUSIVE.** The `J=1` same-instance time ratio over the
  standard set is **0.9709x** — a 2.91% improvement against a 4.2% bar. Six
  defensible readings of the same data run from 2.16% to 4.07% and **none of
  them reaches it**, which is the only reason a verdict can be stated at all on
  a host this noisy. The code stays, under the developer's pre-authorisation

- **Callgrind says the change costs 1.60% MORE instructions on `truss`.**
  `admit_candidate` sheds 199M and `simplex.c:run`, which the scan is inlined
  into, gains 995M — a relocation that costs five times what it saves. It pays
  6.82 instructions on each of 145.9M visits to save exactly 12.0 on each of
  16.6M skips. Whatever seconds it may buy are not bought by doing less

- **Running order alone is worth 2.4 percentage points on this host.** Rounds
  1–3 candidate-first read 0.9865x, rounds 4–6 parent-first read 0.9624x. The
  plan's three-round alternation puts the candidate first every time and cannot
  see this; three rounds would have reported 1.35%

- **The 4.2% threshold is three times a 1.4% repeatability that belongs to
  D81**, not D83 as `01-04-PLAN.md` says twice — D83's 1.4% is Clp against
  HiGHS on total time. And this reading's own repeatability, measured the way
  D81 measured its own, is **6.27%**: the bar is three times a figure four
  times smaller than the noise the reading actually exhibits

- **`01-03`'s warning was right.** `pilot87` and `maros-r7` read 0.9972x and
  0.9987x on the clock, against work ratios of 0.9956x and 0.9987x. A time
  ratio taken on D46's two names alone would have read 0.998x — nothing

Taken during execution of 01-05:

- **D93 is closed and its heading names the finding rather than the verdict** —
  the bar cannot be measured on this host, which is what the negative control
  establishes and is *not* the same claim as the candidate missing the bar.
  428 insertions to `DECISIONS.md` and 0 deletions; the seven forward citations
  in `src/simplex.c`, `docs/work-units.md` and `tests/test_simplex.c` resolve

- **"All 139 answers unmoved" is false and was not written**, though the plan's
  own `must_have` required the sentence. Re-read from the record files: 94
  netlib + 16 Kennington lines carry `digest=` and the 29 infeasible lines
  carry **none** — their invariant is `expected=infeasible verdict=ok det=ok`.
  **The correct claim is 110 digests and 29 infeasibility verdicts over 139
  instances**, and `DECISIONS.md` already said 110 in four earlier entries, so
  writing 139 would have contradicted the file it was being added to

- **An independent audit accepted the verdict and refuted three of its stated
  grounds**, and both facts are recorded. The estimator-independence claim is
  false on the plan's literal three-round protocol (rounds 4–6 read 5.12%, over
  the bar); the negative control was in the data and was never run; and the
  callgrind profile contains two solves, so the per-call figures are 6.0 and
  3.41 rather than 12.0 and 6.82. **A marked correction section was appended to
  `01-04-SUMMARY.md`** rather than merged into it

- **`SPECS.md`'s competitive-gap figures were checked and deliberately left.**
  `bench/compare` did not run in this phase, so none was re-measured; each
  keeps the citation that dates it. That they are pre-phase is stated as a
  caveat rather than repaired by an invented correction

Taken during execution of 02-01:

- **`bench/run.c` gains `-Isrc` (Makefile), a deviation from the plan's
  declared `files_modified`.** D-13 requires the runner to print the
  reduced dimensions and the counter struct has no public API by design
  (D64); the only way to satisfy both was letting this one in-tree tool
  read two reporting-only `jaos_model` fields directly. Confirmed at
  checkpoint rather than assumed

- **A long-double accumulator for presolve's row-bound/objective-offset
  shift was tried, measured, and dropped** — not refused as an idea, removed
  from this plan specifically, because it was unplanned, did not fix the
  problem it was tried for, and cost `pilot87` 2.3x its work. See
  STATE.md's Blockers/Concerns and 02-01-SUMMARY.md for the three-way
  measurement

- **`finnis`'s checker rejection under presolve is real, isolated to one
  instance, and not a defect** — confirmed by an `EXTRA_CFLAGS=
  -DJAOS_NO_PRESOLVE` control reproducing `DECISIONS.md` D24's own
  documented value on the nose, and by D24's own admission that the
  presolve-off pass was "luck". D24's "nothing is gained" reason for
  keeping the primal feasibility test absolute no longer holds the way it
  did when D24 closed — flagged for 02-09's `DECISIONS.md` entry, not
  reopened here

- **02-01's Task 3 acceptance criterion of "0 regressed, 0 improved, 0 new"
  was retired and replaced**, because no correct implementation of even
  this plan's one, simplest reduction can meet it against a baseline taken
  before presolve existed. The replacement: `netlib-infeas` and
  `netlib-kennington` clean, `netlib` clean except four named instances,
  each accounted for rather than waived

### Pending Todos

- **Nothing in the repository reads the `baseline: NOT COMPARED` line.** It
  exists so a record produced by a `-w` run can be told from a checked one, and
  it went unread long enough for such a record to sit committed as the standard
  set's record (`64efcc6:bench/results/netlib.txt`). Fixed there by 01-03's
  confirming run; the missing check is two lines and belongs in
  `preflight.sh`. Not added in 01-03, because a tooling edit mid-plan
  invalidates the campaign. **Handed on by D93.**

- **`bench/run.c` prints seconds as `%8.3f`.** 8 instances of the standard set
  carry no time ratio and 42 more read exactly 1.0000x, so every future time
  ratio on this set is half made of instances that cannot show the difference.
  A two-line change, not made mid-measurement. **Handed on by D93.**

- **`galenet` makes two calls to `dual_ratio_test` in a solve reporting one
  iteration.** The quotient `work_saved / rows` counts calls, not iterations —
  a distinction that holds on 1 instance in 139 and is exactly the kind that
  gets rounded away. Recorded in D93; not chased.

- **Phase 02 has no plans.** `/gsd-plan-phase` is the next action, and its
  Open Question is already on the roadmap: T0 is "the simplex and only the
  simplex", so presolve contributes nothing to the M2 close criterion as
  stated, and nothing says how the ladder is recalibrated once JAOS has one.

### Blockers/Concerns

- **[Phase 5] No controlled host is named.** D17 says a WSL number cannot close
  a gate, and the machine that builds and measures JAOS is a Windows host
  running WSL. Phase 5 cannot produce its closing number without one. **01-04
  measured what that costs rather than leaving it a principle:** the same binary
  against itself reads up to 6.27% across rounds, 8.67% on the instances the
  clock can see, and running order alone is worth 2.4 percentage points. A
  same-session A/B ratio divides most of the host out and did not divide out
  enough to resolve a 4.2% bar. Phases 1–4 are not blocked, but every time ratio
  they take inherits this floor.

- **[All phases] The runner prints seconds as `%8.3f`, and half the standard set
  is under ten quanta.** 8 of 94 instances print `0.000s` and carry no time
  ratio at all; 42 more read exactly 1.0000x because both minima land on the
  same millisecond. Any geometric mean of per-instance *time* ratios over this
  set is therefore half made of instances that could not have shown the
  difference. A two-line change to `bench/run.c` fixes it; not made in 01-04,
  because a source edit mid-measurement invalidates the measurement.

- **[Phase 5] The M2 guard factor is unset** and must be measured or explicitly
  dropped from the criterion. It is not to be guessed.

- **[Phase 2 → Phase 5] T0 is "the simplex and only the simplex."** Presolve
  contributes nothing to the close criterion as stated; how the ladder is
  recalibrated once JAOS has a presolve is undecided in the source.

- **[All phases] Five of seven v1 requirements carry no acceptance criterion.**
  That is `PLAN.md`'s own state, not an extraction gap. Deriving each is
  plan-phase work.

- **[Phase 1 → all phases] A ceiling drifted 2800 units with nothing watching.**
  `test_a_clean_up_pass_dispatches_every_column_it_identified`'s correct-side
  figure was 58141 in its comment and measured 60941 at HEAD before 01-02
  touched anything, against a bound of 62000. A ceiling is read only when it
  trips, so it drifted to 1.7% of its headroom silently. Both sides are current
  now, but nothing in the repository re-measures a bound of this shape on the
  way up, and this is the only one that has been checked.

- **[All phases] The gate cannot see the defect class that produced three of
  last milestone's four closures** — those came from sweeping `REFACTOR_EVERY`
  over 16..256, which no target automates. `make clean` between settings or the
  sweep measures one binary N times.

- **[Phase 2, waves 3–9] The phase-2 gate policy, and the one named exception
  to it.** Presolve-off must stay bit-identical to the three committed
  baselines — that is the regression detector, and 02-01 confirmed it holds:
  `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` reproduces every one of `netlib`,
  `netlib-infeas` and `netlib-kennington` exactly. Presolve-on is judged on
  verdict, objective against Koch's reference and checker acceptance — not
  on bit-identity to those baselines, and not on the baseline's own 2x
  work-ratio rule, which predates presolve and does not know it exists.
  **Every remaining reduction family will move work units on the instances it
  touches, and that is by design (CONTEXT.md), not a regression to chase.**

  02-01's own single reduction (columns fixed as loaded) already shows this
  on 4 of the 26 standard-set instances it fires on, in the committed code:
  - `etamacro` — work 4377214 -> 11943717 (2.7x), iters 709 -> 1322
  - `greenbeb` — work 499796764 -> 999939308 (2.0x), iters 9016 -> 14675
  - `pilotnov` — suboptimality bound 6.52e-13 -> 6.48e-09 (9930x, relative to
    its own objective)
  - `finnis` — **the one recorded checker exception.** Its objective, dual
    conditions and relative row residue (`rowrel`, D24) are all clean; only
    the *absolute* primal row-proximity test flips, from `row=8.44e-07`
    (matching the historical, presolve-off value exactly) to `row=3.76e-06` —
    both a fraction of one ulp of a row whose terms total 4.0e10 (one ulp
    there is 7.6e-6), and `DECISIONS.md` D24 already named finnis's
    presolve-off pass as "luck rather than a property the solver controls."
    Root-caused in 02-01-SUMMARY.md, not a defect in that plan's own code —
    confirmed by an `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` control reproducing the
    documented D24 value on the nose.

  **A near-miss, caught rather than shipped: a long-double accumulator for
  the row-bound/objective-offset shift was tried and dropped.** It was an
  unplanned change, made mid-investigation of `finnis` and not asked for
  by 02-01-PLAN.md. It did not move finnis's residual at all — confirming
  the gap between builds is a different pivot path on a genuinely reduced
  problem, not lost precision in that shift — and cost `pilot87`,
  DECISIONS.md's own established amplifier (D74, D89, D92), **2.3x its
  work and 2.2x its iterations**:

  | build | iters | work | vs baseline |
  |---|---|---|---|
  | committed baseline (presolve-off) | 50850 | 22,977,661,512 | — |
  | committed code (presolve-on, no long double) | 53621 | 24,983,178,548 | 1.05x / 1.09x, `checker=ok`, not flagged |
  | with the long-double accumulator | 117653 | 58,042,043,010 | 2.31x / 2.53x, flagged REGRESSED |

  Found only because the campaigns were re-run against the exact tree about
  to be committed, and then only because the regressed lines were counted
  rather than the report read — the first, accumulator-era campaign had
  already been read and reported without noticing `pilot87` was in it.
  **Had that record been committed as written, a 2.3x cost from an
  unplanned change would have entered the tree labelled as presolve's
  ordinary noise, on the one instance this file already carries with an
  explicit trigger.** `pilot87` does not regress in the committed code —
  the four above are the phase's real inheritance, not five.

  **A `DECISIONS.md` entry is owed on this and is 02-09's to write** — see
  02-01-SUMMARY.md's "What plan 02-09 owes" for what it has to contain,
  including that D24's "nothing is gained" reason for keeping the primal
  test absolute no longer holds in the same way it did when D24 closed.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Backlog | Writers (MPS, LP, solution file), sensitivity and ranging, exportable certificates, exact rational verification, Python bindings | Deferred | 2026-08-12 |
| Backlog | Primal simplex, barrier and crossover, deterministic parallel B&B, MILP, QP/conic/NLP/MINLP, NLP derivative strategy, MIPLIB subsets | Deferred | 2026-08-12 |
| Backlog | LP/MPS dialect edges (Q2) | Deferred | 2026-08-12 |
| Backlog — with trigger | `pilot87`'s suboptimality bound — re-enters the milestone if it blocks a gate | Deferred | 2026-08-12 |

## Session Continuity

Last session: 2026-08-13T06:45:00.000Z
Stopped at: Completed 02-01-PLAN.md. D-01's scaffolding proved end to end: reduced model, postsolve arena, the fixed-column reduction, the negative-test instrument (D-10), the per-instance record (D-13). Two findings handed forward, neither a defect.
Resume file: None

Next: **plan or execute 02-02** (this phase's next plan — presolve billed by `jm_work`, per D-14). `.planning/phases/02-presolve-and-postsolve/` carries 02-01's PLAN and SUMMARY; 02-02 onward do not exist yet.

**02-01 closed with two findings, both handed forward rather than resolved locally.**

1. **`finnis`'s checker verdict is presolve-path-dependent, and it is the one
   recorded exception to an otherwise clean gate.** `netlib-infeas` (29/29) and
   `netlib-kennington` (16/16) are clean against their committed baselines;
   `netlib` regresses on 4 of the 26 standard-set instances presolve actually
   reduces — `etamacro`, `greenbeb` and `pilotnov` are ordinary trajectory
   movement (CONTEXT.md says every remaining family will produce this), and
   `finnis` alone is a checker flip, root-caused to `DECISIONS.md` D24's own
   "luck" — the absolute primal row test sitting at 0.13 ulp of a row whose
   terms total 4.0e10. **02-09 owes `DECISIONS.md` an entry**; see
   02-01-SUMMARY.md for what it has to contain.

2. **A long-double accumulator was tried, found to cost `pilot87` 2.3x its
   work with no offsetting benefit, and dropped before it was committed** —
   caught only because the campaigns were re-run against the exact tree
   about to ship and the regressed lines were counted rather than the
   report read. The near-miss is recorded in STATE.md's Blockers/Concerns
   and in 02-01-SUMMARY.md, not because the code shipped, but because the
   measurement is worth more than the mistake it prevented.

**What 02-03 through 02-05 inherit:** 02-01's own reduction was expected to
be near-inert on well-posed instances and was not — removing a column already
pinned by equal bounds removes a pivot path that was pointless but
trajectory-altering, and 4 of the 26 instances it touches show it. The
remaining seven reduction families should expect trajectory movement as the
default outcome of firing correctly, not treat it as evidence of a bug.

Phase 01's negative control (the same-instance time-ratio floor, 0.9699x
paired / 0.9356x pooled) and its running-order warning (2.4 percentage
points) both still stand for any future time ratio on this host; 02-01 took
no time ratio and does not add to that record.
