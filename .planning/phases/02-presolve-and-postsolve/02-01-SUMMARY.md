---
phase: 02-presolve-and-postsolve
plan: 01
subsystem: presolve/postsolve — the reduced model, the postsolve arena, and the first reduction
status: complete
tags: [presolve, postsolve, dual-simplex, d-01, d-03, d-06, d-07, d-08, d-10, d-13, d24, finnis, pilot87, fault-injection]

requires: []
provides:
  - "src/presolve.c: jm_presolve_init/_free/_run, jm_postsolve_expand, jm_postsolve_solved — the reduced-model builder, the LIFO postsolve arena, and the fixed-column reduction"
  - "jm_presolve/_counts/_tag/_rec/_outcome in jaos_internal.h, with every one of the eight families' counter fields declared now"
  - "JAOS_NO_PRESOLVE — the D-03 build switch, wired at both call sites in src/simplex.c"
  - "JAOS_PRESOLVE_FAULT_OFFBYONE — the D-10 fault-injection guard, validated against tests/test_presolve.c's negative test"
  - "the presolve= dimension field in bench/run.c's per-instance record, on both the optimal and infeasible paths"
  - "the finnis exception to D24 — measured, root-caused, and handed to 02-09 as an owed DECISIONS.md entry"
  - "the pilot87 near-miss — a long-double accumulator measured to cost 2.3x work with no offsetting benefit, dropped before commit"
affects:
  - "02-02 — jm_work billing for presolve's own passes; jm_presolve_run's w parameter is already threaded, unbilled"
  - "02-03..02-05 — every remaining reduction family should expect trajectory movement on the instances it touches, not treat it as a defect (see 'What the next plans inherit' below)"
  - "02-09 — owes DECISIONS.md an entry on finnis/D24; see 'What plan 02-09 owes' below"

tech-stack:
  added: []
  patterns:
    - "reduced model as a jaos_model VALUE (jm_presolve.reduced), struct-copied from the caller's model then every owned pointer field overwritten before anything reads it — never a partial alias"
    - "postsolve arena: append-only jm_presolve_rec, pushed ascending column order, replayed strictly LIFO — determinism by construction, D-07"
    - "publish() and jm_dual_simplex both branch on p->outcome rather than threading a bool, so JAOS_NO_PRESOLVE compiles the whole mechanism to dead code by making outcome permanently JM_PRESOLVE_NONE"
    - "fault-injection guard as a pure function (fixed_col_restore_index) reached from both postsolve replay loops, rather than #ifdef scattered at each call site"
    - "-Isrc as a narrow, documented, per-target Makefile exception for in-tree tooling that needs a reporting-only internal field with no public API (D-13, D64)"

key-files:
  created:
    - src/presolve.c
    - tests/test_presolve.c
  modified:
    - src/jaos_internal.h
    - src/simplex.c
    - bench/run.c
    - Makefile

decisions:
  - "D-06's mutate-vs-copy question resolved by struct-copying *m into jm_presolve.reduced and explicitly nulling every pointer field before allocating fresh ones — the only way to get sx_init's own working-copy pattern one layer up without a partial alias to the caller's model surviving by accident."
  - "jm_postsolve_expand takes only jm_presolve *p, not a separate orig/target pair — p->orig is set directly by jm_dual_simplex (which already has non-const access to the caller's model) rather than threaded through jm_presolve_run, which only ever needs const access under D-06."
  - "The fault-injection hook (JAOS_PRESOLVE_FAULT_OFFBYONE) is build-wide, per the plan's own instruction, which means it corrupts every reducing solve in the binary — not just the one test meant to exercise it. Every positive test in tests/test_presolve.c is therefore guarded to skip itself under that build, and the negative test is guarded the other way; both directions verified by running the suite under both builds."
  - "bench/run.c gains -Isrc (Makefile) — a deviation from the plan's declared files_modified, confirmed at checkpoint. D-13 requires the runner to print presolve's reduced dimensions; the counter struct has no public API by design (D64), so this in-tree tool reads two new reporting-only jaos_model fields (presolve_num_row/col/nz) directly, the same relationship tests/ already has via TEST_INC."
  - "A long-double accumulator for the row-bound/objective-offset shift was tried, measured, and removed from this plan — not because long double is wrong, but because this specific use of it (i) did not fix the problem it was tried for and (ii) cost pilot87 2.3x its work with nothing to show for it. See 'The second finding' below."
  - "Task 3's acceptance criterion of \"0 regressed, 0 improved, 0 new\" against bench/*.baseline is retired for this plan and replaced by: netlib-infeas and netlib-kennington clean, netlib clean except four named instances, each accounted for. No baseline file was rewritten — confirmed at checkpoint, twice."

metrics:
  duration: "~170 min, of which ~40 min was four full J=12 campaigns under WSL (netlib+netlib-infeas+netlib-kennington, run four times over the course of the investigation below) and the rest implementation, three checkpoints and their resolution"
  completed: 2026-08-13

actuals:
  tokens: 39140
  tasks: 3
  commits: 6
---

# Phase 02 Plan 01: The presolve scaffolding, proved end to end — and two findings neither of which is a footnote Summary

The reduced model, the LIFO postsolve arena, and the fixed-column reduction
all work — proved by a fault-injection test that was shown to catch a
postsolve index wrong by one before its passing counted as evidence — and
the standard-set campaign this plan's own `<verify>` asked for found two
things beyond that: one real, narrow, already-documented checker fragility
(`finnis`), and one near-miss caught before it shipped (an unplanned
`long double` change that cost `pilot87` 2.3x its work for no benefit).
Both are handed forward rather than resolved unilaterally, because both
touch ground this plan does not own — a locked `DECISIONS.md` entry (D24)
in one case, and a published number on the standard set's largest instance
in the other.

## Task Commits

1. **Task 1: reduced model, postsolve arena, fixed-column reduction, `JAOS_NO_PRESOLVE`** — `d770eaa` (feat)
2. **Task 2: the instrument, shown to reject before it is believed (D-10)** — `0d1074a` (test)
3. **Task 3: what each reduction removed, in the log and in the record** — `79f0608` (feat)

Plus, in the order the checkpoints required:

4. **The three regenerated campaign records, in their own commit** — `86a4706` (docs)
5. **STATE.md — the phase-2 gate policy and both findings** — `f40be9f` (docs)
6. **This summary** — committed immediately after, no text between the write and the commit

## Files Created/Modified

- `src/presolve.c` (new, 463 lines) — `jm_presolve_init`/`_free`/`_run`, `jm_postsolve_expand`, `jm_postsolve_solved`, the fixed-column reduction, the D-10 fault hook
- `src/jaos_internal.h` — `jm_presolve`/`jm_presolve_counts`/`jm_presolve_tag`/`jm_presolve_rec`/`jm_presolve_outcome`, four prototypes, `jm_model_ensure_solution_arrays` exposed, `jaos_model` gains `presolve_num_row/col/nz`
- `src/simplex.c` — `jm_dual_simplex` gains the presolve call and the `JAOS_PRESOLVE_SOLVED` early return; `publish` gains the postsolve-expand call and a `jm_presolve *` parameter; both guarded `#if !defined(JAOS_NO_PRESOLVE)`; the `presolve: ...` summary log line
- `tests/test_presolve.c` (new, 448 lines) — six tests: the round trip, the row-count basis invariant, the D-13 counter, the all-fixed short circuit, the D-06 byte-identity check, the off-by-one negative test, plus the original-index invariant across all six published arrays
- `bench/run.c` — `presolve=R/C/N->R/C/N` on both per-instance record lines
- `Makefile` — `-Isrc` for `bench/run.c` alone, documented as a narrow D-13 exception
- `bench/results/netlib.txt`, `netlib-infeas.txt`, `netlib-kennington.txt` — regenerated, committed as evidence (see below)
- `.planning/STATE.md` — the phase-2 gate policy and both findings, for waves 3–9

## Decisions Made

See frontmatter `decisions:` for the compact list. The two substantive ones —
`bench/run.c`'s `-Isrc` and dropping the long-double accumulator — are each
their own section below, because both were checkpoint decisions rather than
in-plan choices and both need the measurement beside them to be worth
anything to a future reader.

## Must-haves, checked against the tree rather than asserted

- **Ascending scan, strictly LIFO replay, no address- or hash-order
  dependence.** `jm_presolve_run`'s Pass 1 is a single ascending loop over
  `m->num_col`; every postsolve replay in `jm_postsolve_expand`/`_solved`
  walks `p->arena` from `arena_len - 1` down to `0`. Two solves of the same
  instance with the basis cleared between them agree bit for bit — this is
  exactly what `bench/run.c`'s own `det=ok` column checks on every
  instance in all three sets, and all 139 read `ok`.
- **Presolve-on vs presolve-off agree on verdict, objective and every
  published value, and `jaos_check_solution` accepts both, on a model with
  a fixed column.** `test_fixed_column_round_trip`.
- **A model every column of which presolve fixes still publishes a
  complete answer through postsolve, checker-accepted, with no simplex
  run.** `test_all_columns_fixed_solves_with_no_iterations` —
  `jaos_iterations() == 0`, `checker=ok`.
- **Every published array is in original indices, on both builds.**
  `test_original_index_invariant_across_all_six_arrays` — four columns,
  distinct costs, one fixed touching both of two rows with distinct
  bounds; every one of `sol_col`/`sol_row`/`sol_dual`/`sol_redcost`/
  `sol_col_status`/`sol_row_status` asserted by exact value via
  `TEST_ASSERT_EQUAL_MEMORY`, and the same assertions hold verbatim under
  `JAOS_NO_PRESOLVE` (same model, same expected arrays — by construction,
  since removing an already-pinned column changes nothing this test can
  see about the final numbers).
- **`jaos_model`'s CSC and bound arrays are byte-identical before and
  after a reducing solve.** `test_original_arrays_survive_a_reducing_solve`
  — `memcmp` on `a_start`/`a_index`/`a_value`/`col_lower`/`col_upper`,
  left deliberately unguarded against the fault-injection build, since
  D-06's guarantee lives entirely in `jm_presolve_run`, which the fault
  never touches.
- **`src/check.c` is unmodified and carries no link to any presolve
  symbol.** `git diff --stat` on it is empty; `nm build/dev/check.o | grep
  presolve` and `| grep postsolve` are both empty.
- **A round-trip test counts as evidence only after it has been shown to
  reject a postsolve index wrong by one.** `test_fixed_column_index_map_off_by_one`
  — see the D-10 section below.
- **Every reduction that fires increments a named counter field, read
  white-box.** `test_fixed_col_counter_is_exact` — `fixed_col` asserted
  exactly, every other one of the eight families' fields asserted exactly
  zero, so a future family that forgets to increment its own field is
  caught here rather than inferred from a total that moved for an
  unrelated reason.

All eight must-haves hold against the tree at `f40be9f`.

## D-10: the instrument, shown to reject before it was believed

`JAOS_PRESOLVE_FAULT_OFFBYONE` adds one to a `JM_PS_FIXED_COL` record's
restore index, reached from both postsolve replay loops via one pure
function (`fixed_col_restore_index`). Because the guard is build-wide, it
corrupts *every* reducing solve in the binary — including the plan's own
positive tests — so every positive test in `tests/test_presolve.c` is
guarded to skip itself (`TEST_IGNORE_MESSAGE`) under that build, and the
negative test is guarded the other way. Verified by running the whole
suite under all four build combinations: default, `JAOS_NO_PRESOLVE`,
`JAOS_PRESOLVE_FAULT_OFFBYONE`, and `sanitize` — all exit 0, and under the
fault build the negative test is the only one that runs its real
assertions.

The negative test's model: `x1` fixed at 2.0, `x2` bounded `[0,1]`, both
touching the same row. Under the fault, `x1`'s record restores at index 2
instead of 1 — `x2`'s slot, not its own — overwriting `x2`'s correct
optimal value (0.0) with `x1`'s (2.0), which sits outside `x2`'s own
bound. `x1`'s own slot is pre-seeded (via a white-box `calloc` before
`jaos_solve`) with what a correct postsolve would have written there,
specifically so the test's one deterministic finding is what lands on
`x2` and not whatever an uninitialized allocation happens to contain.

**Caught by two independent report fields, not one:**

```
r.primal_feasible = false,  max_col_violation  = 1.0   (2.0 published against x2's bound of 1.0)
r.dual_feasible   = false,  max_dual_violation = 1.0   (x2 no longer at its lower bound, so
                                                          sign_condition's w>0 branch can't waive it)
```

`jaos_check_solution` never reads `sol_redcost` at all — it recomputes
`d = c - A'y` itself from the model's own cost and the row duals handed to
it — so the dual breach traces to `x2`'s corrupted *value*, the same root
cause as the primal one, not to two independent bugs.

## The first finding: `finnis`, and D24's expired premise

`make netlib J=12` against the committed baseline (presolve-off) regresses
on 4 of the 26 standard-set instances presolve's fixed-column reduction
actually reduces. Three are ordinary trajectory movement, explicit-by-design
per this phase's own `CONTEXT.md`:

| instance | what moved | why |
|---|---|---|
| `etamacro` | work 4377214 → 11943717 (2.7x), iters 709 → 1322 | genuine trajectory change, see below |
| `greenbeb` | work 499796764 → 999939308 (2.0x), iters 9016 → 14675 | same |
| `pilotnov` | suboptimality bound 6.52e-13 → 6.48e-09 (9930x) | a diagnostic field, decides nothing (`bench/README.md`) |

The fourth is not trajectory noise — it is a checker flip, and it is the
one recorded exception:

```
finnis   baseline (presolve-off)   checker=ok        row=8.44e-07  rowrel=8.65e-17  iters=333
finnis   presolve-on               checker=REJECTED  row=3.76e-06  rowrel=8.92e-17  iters=331
```

**Read the two lines side by side rather than either alone.** The absolute
residue moves 4.5x. `rowrel` — the same quantity `DECISIONS.md` D23/D24
already compute and publish, and decide nothing with — moves **3%**,
8.65e-17 to 8.92e-17. By the one measure D24 built to say how good an
answer actually is, the two answers are the same quality. The verdict
flips on the absolute test alone, at a bar sitting **0.13 ulp** of a row
whose terms total 4.0e10 — one ulp there is 7.6e-6, and `DECISIONS.md`
D24 already wrote, of `finnis`'s presolve-off pass specifically: *"that it
passes is luck rather than a property the solver controls."*

**Root-caused, not guessed at:**
- `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` reproduces `row=8.44e-07`, `checker=ok`
  exactly — the documented D24 value, on the nose. Presolve's own write
  side (`jm_presolve_run`) never touches `m`'s arrays regardless (D-06,
  independently verified by `test_original_arrays_survive_a_reducing_solve`);
  the only thing that changed is which of `finnis`'s (evidently multiple,
  comparably-optimal) vertices the reduced-space simplex reaches — the
  objective itself differs from the unreduced solve's at the 6th
  significant digit, both inside the `1e-6 * scale` acceptance window.
- `finnis.mps` genuinely declares 45 `FX` bounds (`grep -c '^ FX ' finnis.mps`)
  — this is the instance's own authored structure, not an artifact of how
  JAOS parses bounds.
- Every dual-side condition is clean in both runs (`dual=0`, `gap` on the
  order of 1e-10 to 1e-11) — this is the primal absolute row-proximity
  test alone.

**D24 gave four reasons for keeping the primal feasibility test absolute.
Three still stand.** The gap identity `P - D = sum w_v(v - bound_v)` rests
on primal feasibility as its hypothesis, not a diagnostic that can be
waived without breaking the identity it licenses. The window is gameable
by a zero-cost `+M`/`-M` column pair held at a fixed width. The field
(Gurobi, HiGHS) keeps its own tolerances absolute for the same reason. The
fourth — *"nothing is gained: across the standard 94 the change buys no
verdict"* — **no longer holds.** It bought no verdict when D24 was
measured against a presolve-off tree. Now one exists: `finnis` itself,
under presolve. **This is not grounds to reopen D24 unilaterally** — it is
one instance, the other three reasons are untouched, and reopening a
locked, measured decision on a phase plan's own authority is exactly what
this project's process exists to prevent. It is grounds for `DECISIONS.md`
to say, next to D24, that the premise moved.

### What plan 02-09 owes

A `DECISIONS.md` entry recording: `finnis`'s checker outcome is
presolve-path-dependent (cite the two-line comparison above); D24's
"nothing is gained" reason has a counterexample now and the other three
reasons do not; whether the phase's own gate treats `finnis` as a standing,
named exception (the position this plan takes provisionally, since
`netlib-infeas` and `netlib-kennington` are unaffected and the checker's
other three conditions never move) or whether D24 itself needs revisiting
is 02-09's decision to make, not this plan's.

## The second finding: a near-miss, caught before it shipped

While investigating `finnis`, this plan added a `long double` accumulator
for the row-bound and objective-offset shift `jm_presolve_run` computes for
each fixed column — the same discipline `src/check.c`'s own row-activity
scatter already uses, on the theory that summing many fixed columns into a
`double` row bound one at a time could lose precision at `finnis`-scale
cancellation. It was not in the plan; it was added mid-task.

**It did not fix `finnis`.** The residual read the same 3 significant
figures with and without it, confirming the gap between builds is a
different pivot path on a genuinely reduced problem, not lost precision in
this specific shift.

**It cost `pilot87` — measured, not assumed, by reverting only that one
change, rebuilding, and re-running `netlib J=12`:**

| build | iters | work | vs. baseline |
|---|---|---|---|
| committed baseline (presolve-off) | 50850 | 22,977,661,512 | — |
| **committed code** (presolve-on, plain double) | 53621 | 24,983,178,548 | 1.05x iters, 1.09x work, `checker=ok`, not flagged |
| with the long-double accumulator | 117653 | 58,042,043,010 | **2.31x iters, 2.53x work, flagged REGRESSED** |

`pilot87` is `DECISIONS.md`'s own established amplifier (D74, D89, D92): a
small perturbation to its re-entry/loan mechanism turns into a large
iteration swing, on every change that has touched it before this one. This
was another one.

**Found only because the campaigns were re-run against the exact tree
about to be committed, and then only because the regressed lines were
counted rather than the report read.** The first, accumulator-era campaign
had already been read and summarized — four regressions, `finnis` named,
`pilot87` not — before anyone counted that a second campaign's regressed
list had five lines, not four. Had the earlier record been the one
committed, a 2.3x work cost from a change the plan never asked for would
have entered the tree labelled as ordinary presolve noise, on the one
instance `STATE.md` already carries with an explicit trigger for
re-entering the milestone.

**Decision: the accumulator comes out of this plan.** Not refused as an
idea — removed from *this* plan, because it was unplanned, it failed the
reason it was tried, and it has a measured cost this project's own rule
("every number needs a measurement on both sides") does not let stand
unexamined. `src/presolve.c` keeps a comment recording the measurement at
the point the accumulator would have gone, so the experiment is not
repeated by a future reader who has the same idea for the same reason.

## The campaigns, and why there were four of them

01-03's pattern repeated at a different scale: not because the plan asked
for four full `J=12` runs of all three sets, but because a
checkpoint-driven investigation did.

1. First campaign (accumulator present) — found `finnis`; reported four
   regressions, missed that `pilot87` was a fifth.
2. Second campaign (accumulator present, re-run to confirm the tree that
   would be committed) — `pilot87` counted this time; five regressions.
3. Third campaign (accumulator reverted, A/B isolation) — `pilot87` clean
   again, confirming the accumulator as the cause rather than assuming it
   from the sequence of two readings.
4. Fourth campaign (final tree, all three sets) — the one committed at
   `86a4706`.

`netlib-infeas` and `netlib-kennington` read `gate: PASS`, `0 regressed, 0
improved, 0 new` in every one of the four runs — presolve fires on zero of
the 16 Kennington instances and does not change any of the 29 infeasible
verdicts, so those two sets could not have shown either finding regardless
of which tree produced them.

## Deviations from Plan

### 1. [Rule 4 — architectural, checkpointed] `bench/run.c` gains `-Isrc`

- **Found during:** Task 3, populating the `presolve=` field
- **Issue:** D-13 requires `bench/run.c` to print the reduced dimensions.
  The plan's `files_modified` does not list `Makefile`, and the existing
  Makefile comment on `$(B)/bench/run` stated the runner "gets no
  privileged view of the solver" — directly in tension with what D-13
  asks for, since the counter/dimension data has no public API by design.
- **Resolution:** checkpointed rather than resolved unilaterally. Accepted:
  the two new fields (`presolve_num_row/col/nz`) are reporting-only,
  `bench/run.c` already links the static archive and reads the baselines
  (a privileged relationship in every sense that matters), and nothing
  changes about `src/check.c`'s structural independence. `-Isrc` added,
  documented in both the Makefile and `jaos_internal.h`.
- **Committed in:** `79f0608`

### 2. [Rule 1, escalated to Rule 4 mid-investigation] The `long double` accumulator

- **Found during:** Task 3, investigating `finnis`'s checker rejection
- **Issue:** an unplanned precision change, made to test a hypothesis
  about `finnis`, that turned out to have a real (negative) effect on a
  different, unrelated instance.
- **Resolution:** measured, found to buy nothing on the instance it was
  tried for and cost 2.3x work on the set's largest instance, removed.
  See "The second finding" above for the full measurement.
- **Committed in:** never committed as a change; `79f0608` carries the
  comment recording the experiment.

### 3. [Process] Task 3's own acceptance criterion retired

- **Found during:** Task 3, first campaign
- **Issue:** "0 regressed, 0 improved, 0 new" against a presolve-off
  baseline cannot be met by any correct implementation of a reduction that
  genuinely changes the model the simplex sees — which is the entire
  point of presolve, and which `CONTEXT.md` already says explicitly.
- **Resolution:** checkpointed; criterion replaced with `netlib-infeas`
  and `netlib-kennington` clean, `netlib` clean except four named,
  accounted-for instances. No baseline rewritten.
- **Committed in:** `86a4706` (the records), `f40be9f` (STATE.md stating
  the substitution explicitly)

---

**Total deviations:** 3 — one architectural (accepted at checkpoint), one
an unplanned change removed after measurement, one a process substitution
(accepted at checkpoint). No scope creep beyond what D-13 and the
investigation of a real checker rejection required.

## Issues Encountered

None beyond the two findings above, which are the substance of this plan's
Task 3 rather than incidents to route around.

## What the next plans inherit

- **02-02 (work billing):** `jm_presolve_run`'s `w` parameter is already
  threaded and unused (`(void)w`); billing starts there.
- **02-03 through 02-05 (the remaining seven families): do not assume a
  reduction is near-inert because the removed structure looked pointless.**
  02-01's own reduction — removing a column already pinned by equal
  bounds, which contributes nothing to any pivot the simplex would
  actually choose — was expected to be close to behavior-preserving, and
  on 4 of the 26 instances it touches, it was not: removing the column
  removes a *pivot path* (a degenerate one, but a real one) that the
  unreduced solver could still take, and that changes the basis and
  weight trajectory even though no published value moves as a result. This
  is not a defect signature; it is what firing correctly looks like.
  Expect it, measure it per-family the way this plan measured it, and do
  not treat a work-ratio or iteration-count move as evidence of a bug
  without first checking whether the answer is still correct.
- **02-09 (documentation):** owes `DECISIONS.md` the `finnis`/D24 entry
  described above.
- **Every plan touching a campaign:** the phase's gate policy is now
  written down in `STATE.md`'s Blockers/Concerns — presolve-off
  bit-identical to the three committed baselines (the regression
  detector, confirmed working), presolve-on judged on verdict/objective/
  checker acceptance and not on the baseline's own 2x work-ratio rule.

## Known Stubs

None. D-01's whole rationale is that a stub in the postsolve arena is
exactly the defect shape this plan exists to rule out, and every reduction
this plan ships (one: fixed columns) carries a complete, tested postsolve
record.

## Threat Flags

None beyond the register already carried in `02-01-PLAN.md`'s
`<threat_model>`, which this plan's own acceptance criteria enforce
directly: `assert` bounds-checks on every restored index in both postsolve
replay loops (T-02-02), every allocation through `jm_alloc_array`/
`jm_calloc_array`/`JM_GROW` with none direct (T-02-01, confirmed by `nm`
carrying no undefined reference to `malloc`/`calloc`/`realloc` in
`presolve.o`), and every one of the six published arrays' entries written
before the checker ever sees the model (T-02-04, confirmed by
`test_original_index_invariant_across_all_six_arrays`'s distinct-value
construction). `bench/run.c`'s new `-Isrc` reads two `int64_t` fields and
nothing else; no include chain reaches `src/check.c`, checked directly.

## Self-Check: PASSED

1. `src/presolve.c`, `tests/test_presolve.c` present at the paths named.
2. `git diff --stat -- src/check.c` between `034be8c` and `HEAD`: empty.
3. `nm build/dev/check.o | grep -iE 'presolve|postsolve'`: empty.
4. `grep -c '^ FX ' bench/instances/finnis.mps`: 45, confirmed directly
   rather than assumed from `docs/research/netlib-campaign.md`.
5. All six commit hashes (`d770eaa`, `0d1074a`, `79f0608`, `86a4706`,
   `f40be9f`, and this summary's own) present in `git log --oneline`.
6. `grep '^pilot87 ' bench/results/netlib.txt`: `iters=53621
   work=24983178548 checker=ok`, and absent from the file's own
   `REGRESSED` list — matches the committed-code row of the three-way
   table above exactly.
7. `grep '^finnis ' bench/results/netlib.txt`: `checker=REJECTED
   row=3.76e-06` — matches.
8. `make -j12 test`, `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE test`,
   `EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE test`, and `sanitize` all
   re-run clean against the final tree at `f40be9f`'s parent (the code
   commits) before this summary was written — 0 failures across every
   build, `test_fixed_column_index_map_off_by_one` the only test with
   real assertions under the fault build.
9. `git stash list`: exactly one entry, `stash@{0}: WIP on
   fix/grow22-regression`, pre-existing and untouched — confirmed rather
   than assumed, since the A/B measurement above used a direct file edit
   and revert, never `git stash`.

## Estimate vs actuals

The plan estimated `tokens: 90000` at `confidence: low`. Realized:
**39,140** on the chars/4 scale, under by 2.3x — but the token estimate
was always going to be the wrong instrument for this plan's actual cost,
which was machine time and checkpoint round-trips: four full `J=12`
campaigns of all three sets (~40 min under WSL) and three checkpoints
whose resolution required a measurement, not a judgement call, before any
of the three could close. `confidence: low` was the right call on the
estimate; the diff size was never where this plan's risk lived.

---
*Phase: 02-presolve-and-postsolve*
*Completed: 2026-08-13*
