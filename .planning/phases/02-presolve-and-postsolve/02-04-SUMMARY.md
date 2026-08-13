---
phase: 02-presolve-and-postsolve
plan: 04
subsystem: presolve/postsolve — the activity-range machinery, and the two constants that decide when a residue is a number
tags: [presolve, postsolve, activity-range, forcing-row, redundant-row, bound-tightening, sweep, canary, d-02, d-04, d23, d34, d91, determinism, refuted]

requires:
  - phase: 02-01
    provides: "the reduced model, the postsolve arena, the LIFO replay machinery, JAOS_NO_PRESOLVE, JAOS_PRESOLVE_FAULT_OFFBYONE"
  - phase: 02-02
    provides: "the D-14 billing rate (nonzero-only)"
  - phase: 02-03
    provides: "the cascading round loop, live degree tracking, the local row-wise mirror, ps_replay_one, and the structural round cap this plan replaces"
provides:
  - "src/presolve.c: ps_row_range, one routine computing what a row can reach over the current column boxes, read three ways — the model is infeasible, the row is forced to an extreme, the row can never bind"
  - "JM_PS_FORCING_ROW with its own dual derivation (one multiplier satisfies every column the row pins, and the row's own sign condition, at once) and JM_PS_REDUNDANT_ROW, whose multiplier is zero by construction"
  - "PRESOLVE_TIGHTEN_EPS = 1e-9 and JM_PRESOLVE_ROUNDS = 16, each set by a sweep with a canary that had to move and did, each with its table beside the constant and in docs/tolerances.md"
  - "a Neumaier-compensated accumulator in double for the activity sum, in place of the long double the plan asked for — D34 lists 'no long double' among the rules the cross-machine determinism claim rests on, and presolve is on the reproducible path where src/check.c is not"
  - "docs/tolerances.md: a third tolerance space, named, with presolve's two constants and the sweeps that set them"
  - "02-04-MEASUREMENT/: the sweep script, both canaries, per-setting records for 17 settings, the family-isolation runs, and the attribution runs that give every checker rejection on the standard and Kennington sets an owner"
  - "two pinned change-detector tests, one per constant, that fail if either is retuned without its sweep being redone"
affects:
  - "02-05 — inherits the activity range as a shared routine; duplicate rows/columns and dominated columns can read it rather than build their own"
  - "02-06 — numerics-reviewer now owes a pass on two things rather than one: ps_replay_one's dual recovery (02-03's flag) and the 15 standard-set plus 4 Kennington checker rejections this phase now carries, which are a postsolve dual-recovery defect and not a tolerance"
  - "02-07 — the baseline rewrite inherits 26 standard-set regressions, 1 infeasible-set regression (bgindy) and 4 Kennington regressions, and it inherits a gate that does not pass"
  - "02-09 — DECISIONS.md owes a fourth entry: bound tightening measured and refused, which is the most valuable kind of entry this project has"

tech-stack:
  added: []
  patterns:
    - "one activity-range routine per row per round, read for several outcomes rather than recomputed per family — the shape src/check.c's implied_bounds already has, with the sum compensated in double rather than accumulated in long double because this one decides the answer"
    - "two windows, two jobs, and never the same constant: whether two numbers that should be equal differ is DBL_EPSILON times the traffic that produced them and is not tunable; whether an improvement is worth taking is a judgement and is what PRESOLVE_TIGHTEN_EPS governs. Conflating them cost three campaigns"
    - "a canary per constant, kept beside the campaign it validates, and rebuilt when the family it measured is removed"
    - "attribution by building the parent commit in a scratch tree and pointing its runner at this tree's fetched instances — a checker rejection with no owner is a defect nobody will claim"

key-files:
  created:
    - .planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/
  modified:
    - src/presolve.c
    - src/jaos_internal.h
    - tests/test_presolve.c
    - docs/tolerances.md
    - docs/work-units.md

decisions:
  - "The activity sum is accumulated with Neumaier compensation in double, not in long double. The plan asked for long double on the checker's precedent, but the checker may use it precisely because its arithmetic never reaches the answer — presolve's decides which reduced model the simplex sees. long double is 80-bit on x86-64 and something else on aarch64, and D34 lists avoiding it among the four construction rules the cross-machine determinism claim rests on. It appears nowhere in this tree outside src/check.c and still does."
  - "Bound tightening is not shipped. Six designs were built and measured against the standard set and every one returned INFEASIBLE on models that have an optimum: the bound reasoned with but never published (92/94 solved), published (90/94), published with the outward rounding cut to the arithmetic's own error (89/94), published without the collapse-to-fixed (91/94), published with the row window at DBL_EPSILON (89/94), against 94/94 with the family off. Nine epsilon settings from 1e-12 to 1e-4 moved none of it. The other three readings of the same range ship and measure better than the tree they came from."
  - "The three readings that compare an activity against a row bound use DBL_EPSILON times the row's traffic, not PRESOLVE_TIGHTEN_EPS. They ask whether two numbers are equal, which has nothing to tune; only whether a tightening is worth taking is a judgement. Using the tunable there declared every row whose minimum activity came within 1e-3 of its upper bound to be forcing, and a forcing row pins every column in it."
  - "The empty-row feasibility test stops comparing exactly. 02-03 was right to compare exactly when its families rarely emptied a row; this plan's do, and a row emptied by removing its columns carries bounds that are a running difference of terms far larger than what is left of them. The window is PRESOLVE_TIGHTEN_EPS times the traffic those subtractions carried, and a row nothing was ever removed from still gets the exact test."
  - "The singleton row's dual recovery asks whether a zero multiplier already satisfies the column's sign condition, instead of reading a status. 02-03's status-based discriminator works while the reduced solve is what put the column where it is; it stops working once presolve determines the column itself, because the fixed-column rule republishes it against its ORIGINAL bounds and that status no longer says which side the row was responsible for."

requirements:
  - REQ-presolve

estimate:
  tokens: 85000
actuals:
  tokens: 23500
  tasks: 3
  commits: 3

metrics:
  duration: "~300 min, and almost none of it was writing the code. Three campaigns were spent on wrong hypotheses about why two feasible models were being refused — the epsilon, then the outward rounding, then the row window — before the fourth diagnostic, a traced build printing which of the four sites set JM_PRESOLVE_INFEASIBLE, answered it in one run. Two full sweeps of 9 and 8 settings, a four-way family isolation, and three attribution campaigns against the parent commit."
  completed: 2026-08-13

status: complete
---

# Phase 02 Plan 04: One activity range, read three ways, and the fourth reading the standard set refused Summary

**One routine computes what a row can reach given the current column boxes and three families read that one result — the model is infeasible, the row is forced to an extreme, the row can never bind — while the fourth reading the plan asked for, bound tightening, was built six ways, measured against the standard set every time, and refused every time because it returns INFEASIBLE on models that have an optimum.**

## Performance

- **Duration:** ~300 min
- **Tasks:** 3
- **Files modified:** 5, plus a new measurement directory of 128 files

## Accomplishments

- `ps_row_range` computes a row's minimum and maximum attainable activity over the live column boxes, once per row per round, with the infinite contributions counted rather than summed so one unbounded column does not poison the range for the others. Three families and the infeasibility branch all read that one result; there is no second pass.
- The sum is accumulated with **Neumaier compensation in `double`**, not in `long double` — see Deviations.
- `JM_PS_FORCING_ROW` carries a dual derivation rather than a zero: for a row forced at its upper bound, every column it pins needs `d_j >= 0` at a lower bound or `d_j <= 0` at an upper one, and both reduce to the same inequality `y_i <= d_j^0 / a_ij`, so one multiplier satisfies all of them at once and the row's own sign condition besides. It fires only where that derivation has nothing stale in it: every pinned bound must be one the caller's own model carried, and no pinned column may still touch a row already removed by a family whose multiplier is not zero.
- `JM_PS_REDUNDANT_ROW` is the family that needs no derivation at all — a row that can never bind carries a zero multiplier, which satisfies the checker's sign condition unconditionally. Its activity is seeded from the surviving columns before the replay and accumulated by the removed ones during it, so the two halves can arrive in either order.
- **`JM_PRESOLVE_ROUNDS = 16` and `PRESOLVE_TIGHTEN_EPS = 1e-9`**, each set by a sweep with `make clean` between every setting and a canary that had to move and did. Both tables are beside their constants and in `docs/tolerances.md`.
- Two pinned change-detector tests, one per constant, each asserting an exact integer or an exact verdict that moves the moment its constant does.
- `docs/tolerances.md` names a third tolerance space and carries presolve's constants with the sweep behind each.

## Task Commits

1. **Task 1: one activity range, read four ways** — `7c7375c` (feat)
2. **Task 2: the sweep that set both constants, and the family it refused** — `c268fdc` (feat)
3. **Task 3: the frozen set gains a third space** — `88949f4` (docs)

## The finding this plan exists for

### Bound tightening returns INFEASIBLE on models that have an optimum, in every variant built

The plan's fourth reading of the activity range is what the rest of them cascade through: what the rest of a row leaves for a column implies a bound on it, and imposing that bound is what lets forcing, redundant and the fixed-column rule fire on the next round. It was built, and then built again five more times.

| design | solved | objective ok | checker ok | refused |
|---|---|---|---|---|
| tightening off (what ships) | 94/94 | 94 | 79 | — |
| bound reasoned with, never published | 92/94 | 92 | 75 | `pilot`, `pilot87` |
| bound published | 90/94 | 90 | 52 | `agg`, `pilot`, `pilot87` |
| published, outward rounding at `DBL_EPSILON` | 89/94 | 89 | 48 | `agg`, `maros`, `pilot`, `pilot87` |
| published, no collapse-to-fixed | 91/94 | 91 | 48 | `pilot`, `pilot87` |
| published, row window at `DBL_EPSILON` | 89/94 | 89 | 48 | `agg`, `maros`, `pilot`, `pilot87` |

Nine epsilon settings from 1e-12 to 1e-4 moved none of it. The last row is what settles it: with every window reduced to the arithmetic's own error, four models still come back INFEASIBLE, so the implied bounds themselves are too tight on those models rather than the comparisons around them being too loose.

**The worked case.** `pilot` row 1095 is an equality row on one column. A forcing row pinned column 3554 at 1.15, its own upper bound, and row 1095 needed it at 0; the row went empty holding −1.15 and presolve — correctly — called the model infeasible. The forcing row only became forcing because the boxes had been narrowed first.

**What a later plan needs before trying again**, in the order the evidence puts it:

1. A derivation of why the implied bound over-tightens on `pilot`, `pilot87`, `agg` and `maros` specifically. It is not the epsilon and it is not the rounding — both were measured on both sides.
2. A dual postsolve for an imposed bound. A column resting at a presolve-derived bound is interior in the caller's own box, where the checker requires its reduced cost to be zero, and the only thing that can pay for it is the implying row's multiplier. Shifting that multiplier is sound when every other column in the row sits exactly at its own extreme, which holds in exact arithmetic and not otherwise.
3. Both under a campaign, not under an argument. Every design in the table above looked right when it was written.

### The standard set was already failing, and nobody had run it

The tree this plan started from, `8425acc`, reads **78 checker ok of 94** on the standard set and **8 of 16** on Kennington. The records `02-01` committed read 93 and 16. `02-03` scoped its own verification to the infeasible set and said so in its summary; the other two sets were not re-run for it.

This plan's tree reads **79 and 12**, so it improves both. The full attribution, with every instance named, is in `02-04-MEASUREMENT/`:

| set | 02-01 record | 8425acc (02-03) | this plan |
|---|---|---|---|
| netlib | 1 rejected (`finnis`) | 16 rejected | 15 rejected |
| netlib-kennington | 0 rejected | 8 rejected | 4 rejected |
| netlib-infeas | gate PASS | — | gate PASS |

On the standard set this plan clears `80bau3b`, `boeing1`, `bore3d` and `maros`, and adds `bnl1`, `bnl2` and `e226`. On Kennington it clears `pds-02`, `pds-06`, `pds-10` and `pds-20` and adds none.

**This is a postsolve dual-recovery defect, not a tolerance.** Every one of the 94 objectives matches Koch's reference and every instance is deterministic; what fails is the checker's dual sign condition on a postsolved answer, which is exactly the defect class this phase's own boundary names as the real risk and exactly what `02-06`'s `numerics-reviewer` pass exists for.

## Deviations from Plan

### 1. [Rule 4 — refuted premise, resolved by measurement] Bound tightening is not shipped

Documented in full above. This is the plan's own central family and three of its `must_haves` are written around it. It is refused on the evidence of six designs and nine settings rather than on an argument, which is what the plan's own deviation rule asks for: *a refuted premise is a valuable deliverable here*. The three readings that do ship measure better than the tree they came from on all three sets.

Three `must_haves` are therefore not met as written, and saying so is more useful than a partial claim:

- *"A bound tightening that lands exactly on the opposite bound fixes the variable rather than declaring infeasibility; one step past it declares infeasibility."* **Met, on the singleton-row fold** rather than on the activity-range tightening — the fold is where an implied bound meets a column's own bound in the shipped code, and both branches are named, commented and tested one step apart. See the third deviation for what "one step" had to become.
- *"A bound is only ever tightened, never loosened, and a new bound is rounded away from tightening rather than toward it."* **Not applicable to shipped code**: no activity-range bound is imposed. The monotonicity and the outward rounding were both built and are recorded in the measurement directory.
- *"The fixed-point round cap is set where the propagation stops changing, by a sweep over the standard set with a canary that had to move and did."* **Met.**

### 2. [Rule 2 — a project rule overrides a plan instruction] The activity sum is compensated `double`, not `long double`

The plan asks for `long double` explicitly and puts it in Task 1's acceptance criteria, on the precedent that `src/check.c` accumulates its activities that way. The precedent does not carry: the checker may use `long double` because its arithmetic never reaches the answer, and presolve's decides which reduced model the simplex is handed. `long double` is the 80-bit x87 type on x86-64 and 128-bit or plain `double` elsewhere, so the same source would build a different reduced model on a different architecture — and **D34 lists "no `long double`" among the four construction rules the cross-machine determinism claim rests on**, alongside no FMA contraction and no address-ordered iteration. It appears nowhere in this tree outside `src/check.c`, and after this plan it still does not.

Neumaier compensation buys the accuracy without the type: portable, deterministic, about a factor of two, and `-ffp-contract=off` is what makes its two-term error recovery exact rather than approximate.

### 3. [Rule 1 — the plan's own test cannot be built as written] "One representable step" is below any epsilon a sum can carry

The plan asks for a test whose model moves a bound "one representable step" past the opposite bound and asserts `JAOS_SOLVE_INFEASIBLE`. At the magnitudes involved one representable step is 8.9e-16, and an epsilon able to separate that would be smaller than the rounding in the row-bound shifts that produce the bound in the first place. The sibling test moves the bound by 1e-6, comfortably past the window, and a **third** test was added for what the plan's literal case actually buys: a conflict of one representable step does not flip the verdict, and the un-presolved build agrees, which is the property that matters.

### 4. [Rule 1 — bug, found by running] The empty-row feasibility test could not stay exact

02-03 compared a row's shifted bounds against zero exactly, on the argument that an empty row's activity is exactly zero. Its activity is; its bounds are not, once every column removed from it has subtracted its own contribution from both. On an equality row emptied that way the exact test asks a sum to cancel to the last bit. It refused `pilot` and `pilot87` at every epsilon setting, which is how three campaigns came to be spent on the wrong constant.

Fixed by tracking what those subtractions carried, per row, and giving the test a window of `PRESOLVE_TIGHTEN_EPS` times that traffic. A row nothing was ever removed from carries zero traffic and still gets the exact comparison it deserves.

### 5. [Rule 1 — bug, found by running] The singleton row's dual recovery read a status that had stopped being able to answer

02-03 discriminated on `sol_col_status[j]` together with `row_tightens_lo/hi`. That works while the reduced solve is what put `x_j` where it is. It stops working once presolve determines `x_j` itself: the fixed-column rule republishes the column against its **original** bounds, and that status no longer says which side the row was responsible for. A row inducing a lower bound of 3 on a column whose own upper bound is also 3 lands exactly in that hole, and this plan's own families produce it.

Replaced with the question the checker actually asks — does leaving this row's multiplier at zero already satisfy the column's sign condition? — and, where it does not, the row absorbs the whole reduced cost. The row's own sign condition then comes out right by construction rather than by luck, and the derivation is in the comment.

### 6. [Rule 3 — blocking] `docs/work-units.md` gains the range scan's charge

Not in this plan's `files_modified`. The activity range is billed `JM_WORK_NONZERO` per live entry, which is a charge that scales differently from every other one in the file — it is paid on every live row of every round rather than once per reduction, so a round that finds nothing still bills the whole live matrix once. D-14 makes the work unit a public contract and 02-02 set the precedent that a new charge is recorded there; leaving it out would have made the file wrong rather than incomplete.

### 7. [Process] Task 3's pinned tests landed in Task 2's commit

Task 3's declared files are `docs/tolerances.md` and `tests/test_presolve.c`, and its acceptance asks for `git diff --stat` to show those two and nothing else. Splitting `tests/test_presolve.c` across two commits would have left an intermediate commit whose test suite does not build — the activity-range family's own tests and the constants' pins are in one file. Task 2 carries the tests, Task 3 carries the document.

### 8. [Process] Three campaigns were spent on wrong hypotheses before the tree was instrumented

Recorded because it is the most expensive thing that happened here and it was avoidable. Two feasible models were being refused. The first guess was the epsilon (a sweep of nine settings said no), the second the outward rounding (a campaign said no and made it worse), the third the row window (a campaign said no). The fourth action was a throwaway build printing which of the four sites that can set `JM_PRESOLVE_INFEASIBLE` had fired, and it answered in one run — then a second trace naming the column and the family answered the rest. `jaos-debug`'s own rule, applied three campaigns late: instrument before repairing.

---

**Total deviations:** 8 — one refuted premise resolved by measurement, one project rule overriding a plan instruction, one test the plan asked for that cannot be built as written, two bugs found by running, one required documentation file outside the declared set, and two process notes.

## Measurements

### The round-cap sweep

```
rounds         1     2     4     8    16    32    64   128
canary links   1     2     4     8    16    32    64   128
rows removed  6060  7178  7549  7596  7598  7598  7598  7598
cols removed  22671 24300 24629 24693 24695 24695 24695 24695
checker ok      82    79    79    79    79    79    79    79
wall (J=12)  103.6  97.2  98.9  98.1  99.1  99.0  99.5  99.1
```

16 is where it stops changing; the cost is flat, so there is nothing to trade against.

The checker column is worth reading and worth not acting on. One round certifies three answers more than sixteen do, because a reduction that does not fire cannot get its dual wrong. Capping the propagation to hide a postsolve defect would leave the defect and lose the reductions.

### The epsilon sweep

```
eps        1e-12 1e-11 1e-10 1e-9  1e-8  1e-7  1e-6  1e-5  1e-4
solved        94    94    94    94    94    94    94    94    94
objective ok  94    94    94    94    94    94    94    94    94
checker ok    79    79    79    79    79    79    79    79    79
rows removed 7596  7596  7596  7596  7596  7596  7596  7596  7596
canary       INF   INF   INF   INF   OPT   OPT   OPT   OPT   OPT
```

Flat over nine decades, and the canary row is what makes that a reading rather than a broken instrument. The plateau is at least nine decades wide with neither edge found; 1e-9 is interior to the grid the plan asked for and is where the canary flips, so a future edit that stops the constant reaching the binary changes the canary rather than nothing.

### The three sets, on the tree this plan ships

| set | verdicts | checker | determinism | baseline |
|---|---|---|---|---|
| netlib | 94 solved, 94 objective ok | 79 ok, 15 rejected | 94 ok | 26 regressed, 0 improved, 0 new |
| netlib-infeas | 29 correctly refused | — | 29 ok | 1 regressed (`bgindy`, work 2.0x, iters unmoved) |
| netlib-kennington | 16 solved, 16 objective ok | 12 ok, 4 rejected | 16 ok | 4 regressed, 0 improved, 0 new |

`netlib-infeas` reads `gate: PASS`. The other two read `gate: NOT MET`, on the checker, and both read better than the tree this plan started from.

No baseline and no file under `bench/results/` was touched: the runner was invoked with `-o` into the measurement directory throughout, and `-b` only reads. `git diff --stat bench/` is empty.

## Files Created/Modified

- `src/presolve.c` — `ps_acc` (the compensated accumulator and why it is not `long double`), `ps_row_range`, `ps_min_act`/`ps_max_act`/`ps_row_tol`/`ps_bound_scale`, the activity pass with its three readings and the block recording the fourth, `PRESOLVE_TIGHTEN_EPS` and `JM_PRESOLVE_ROUNDS` with their sweep tables, `row_traffic` and the empty-row window, the singleton-row dual recovery rewritten, `ps_replay_one` gains the presolve workspace and its arena index
- `src/jaos_internal.h` — `JM_PS_FORCING_ROW` and `JM_PS_REDUNDANT_ROW` with their per-tag field documentation; a note where the bound-tightening tag would have gone
- `tests/test_presolve.c` — forcing and redundant round trips with must-fail-first siblings, the three boundary-case tests, the optimum-on-the-boundary test, the white-box counter test, and the two pinned change detectors; three pre-existing pins updated with the reason each moved
- `docs/tolerances.md` — the preamble's two spaces become three, and a presolve section with both constants
- `docs/work-units.md` — the activity range's charge
- `.planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/` — 128 files: the sweep and its canaries, 17 per-setting records, the family isolation, the attribution runs, and a README saying what each is

## Known Stubs

None. Bound tightening is not a stub — no code for it remains, and what is in the file in its place is the measurement that refused it. `jm_presolve_counts.tightened_bound` stays declared and stays zero, the way every unfired family's field does, and a test pins it at zero so relighting it without re-running the campaign fails.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: postsolve-dual-recovery-defect | src/presolve.c (`ps_replay_one`) | 15 standard-set and 4 Kennington instances are refused by the checker on the dual sign condition while every objective matches Koch's reference. This is no longer a risk to review, it is a defect with a count; `02-06` is where it gets read. |
| threat_flag: gate-does-not-pass | bench/ | Two of the three sets read `gate: NOT MET`. Inherited at 16 and 8 rejections, shipped at 15 and 4. `02-07`'s baseline rewrite cannot be a rewrite of a passing gate. |

T-02-11 does not arise: no activity-range bound is imposed, so no tightening can narrow the region the simplex searches. T-02-12 arose and is the finding above. T-02-13 is bounded by `JM_PRESOLVE_ROUNDS` on top of the structural cap, both measured. T-02-14 is mitigated by the compensated accumulation, with the portability argument that replaced `long double` recorded above.

## Next Phase Readiness

- **02-05** can read `ps_row_range` rather than building its own; duplicate and dominated columns need the same numbers.
- **02-06** has a count rather than a concern: 19 rejections across two sets, all dual-side, all on answers whose objectives are right.
- **02-07** inherits 26 + 1 + 4 baseline regressions and a gate that does not pass on two sets.
- **02-09** owes a fourth `DECISIONS.md` entry — bound tightening measured and refused — alongside the `finnis`/D24 correction, the cost-0 scope restriction and the `netlib-infeas` policy correction.

## Estimate vs actuals

Estimated 85,000 tokens at `confidence: low`. Realized **23,500** on the chars/4 scale over the source, test and documentation diff — under by 3.6x, and for the third plan running the estimate could not see what the plan actually cost. The diff is small because five of the six bound-tightening designs were deleted rather than shipped. What this plan cost is **machine time and wrong hypotheses**: two sweeps of 9 and 8 settings, a four-way family isolation, three attribution campaigns against the parent commit, and three campaigns spent on the wrong constant before the tree was instrumented. `confidence: low` was right again.

## Self-Check: PASSED

1. `src/presolve.c`, `src/jaos_internal.h`, `tests/test_presolve.c`, `docs/tolerances.md`, `docs/work-units.md` present at the paths named.
2. `.planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/` present with 128 files, including `sweep.sh`, `canary.c`, `summarise.sh`, `README.md` and `final-netlib.txt`.
3. `git log --oneline --all`: `7c7375c`, `c268fdc`, `88949f4` all present.
4. Neither constant's comment still says PROVISIONAL (`grep -c PROVISIONAL src/presolve.c` reads 0), and the shipped values are `JAOS_PRESOLVE_TIGHTEN_EPS_VALUE 1e-9` and `JAOS_PRESOLVE_ROUNDS_VALUE 16`.
5. `git status --porcelain bench/` is empty — no baseline and no record touched.
6. `git status --porcelain .planning/REQUIREMENTS.md` is empty — the generic mark-requirements-complete step did not fire, and `REQ-presolve` is still `[ ]`.
7. `make -j12 test`, `make -j12 sanitize`, `make -j12 EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE test` all exit 0; `test_presolve` built in isolation under each fault switch exits 0.
8. All three campaigns run against the committed baselines with `-o` into the measurement directory: `netlib-infeas` reads `gate: PASS`, the other two read `gate: NOT MET` on the checker and both read better than `8425acc`.

---
*Phase: 02-presolve-and-postsolve*
*Completed: 2026-08-13*
