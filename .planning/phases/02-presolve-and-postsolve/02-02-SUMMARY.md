---
phase: 02-presolve-and-postsolve
plan: 02
subsystem: presolve/postsolve — work-unit billing (D-14)
tags: [presolve, work-units, jm_work, D-14, D-16, checkpoint, pinned-test]

requires:
  - phase: 02-01
    provides: "the reduced model, the postsolve arena, the fixed-column reduction, and jm_presolve_run's already-threaded (but unused) jm_work *w parameter"
provides:
  - "src/presolve.c: jm_presolve_run charges JM_WORK_NONZERO per nonzero a firing column's row-bound shift visits; jm_postsolve_solved publishes that charge on the presolve-only path where no sx ever runs"
  - "src/simplex.c: jm_dual_simplex threads a real jm_work into jm_presolve_run and seeds sx's own accumulator (s.work) from it, so presolve's units and the reduced solve's are one total"
  - "tests/test_presolve.c: a pinned exact work count for the one-fixed-column model, both with presolve (8202) and without it (8206, JAOS_NO_PRESOLVE), plus a determinism check across a cleared basis"
  - "docs/work-units.md: presolve's entry under \"Where it is charged\", the non-comparability warning, and two new unbilled floors (the classification pass's column scan, the reduced model's one-time copy)"
affects:
  - "02-07 — owns the deliberate rewrite of the three committed baselines under this new meaning; not touched here (git diff --stat bench/ is empty)"
  - "02-03..02-05 — the remaining seven reduction families inherit the same rate (nonzero-only) and the same unbilled-floor reasoning for their own per-round bookkeeping"
  - "02-09 — DECISIONS.md still owes an entry recording this rate and the measurement that set it, per jaos-record's rule that a decision entry is where the reasoning lives"

tech-stack:
  added: []
  patterns:
    - "the presolve/simplex work accumulator is one jm_work, not two: jm_dual_simplex declares it, jm_presolve_run charges into it, and jm_dual_simplex seeds sx's own s.work from it right after sx_init's memset zeroes it — so publish()'s own m->solve_work = s->work.units already includes presolve's share without either side needing to add the two together"
    - "a charge recorded on jm_presolve.reduced.solve_work (set once, at the end of jm_presolve_run) survives for the one path nothing else would publish it on — JM_PRESOLVE_SOLVED, where no sx runs — and is silently superseded by the fuller figure on every other path, which is deliberate and commented where it happens"

key-files:
  created: []
  modified:
    - src/presolve.c
    - src/simplex.c
    - tests/test_presolve.c
    - docs/work-units.md

decisions:
  - "Checkpoint (Task 1), developer's selection: nonzero-only — JM_WORK_NONZERO per nonzero a round actually visits, and nothing else. No JM_WORK_ELIMINATED on removal, no per-round fixed cost, no new JM_WORK_* constant. See 'The checkpoint decision, recorded verbatim' below for the full reasoning the developer accepted."
  - "Which nonzeros 'a round actually visits' resolved, against the actual code, as Pass 1's row-bound shift only (the entries of a firing column, visited while its cost and matrix contribution fold into the rows it touches) — not Pass 2/3's copy of every surviving column's nonzeros into the reduced model's own arrays, which the checkpoint's own <context> names as the third, explicitly separate touch point ('the reduced model is built once by copying nonzeros') and which the resolution text calls out by name as unbilled."
  - "jm_dual_simplex declares one jm_work (pre_work) unconditionally, outside the #if !defined(JAOS_NO_PRESOLVE) guard, so the same code path (s.work = pre_work after sx_init) runs regardless of the build — under JAOS_NO_PRESOLVE it is simply always {0}, which is the same as not seeding s.work at all."
  - "jm_presolve_run records its own total on p->reduced.solve_work unconditionally (even though the REDUCED path immediately supersedes it once the reduced model's own solve publishes s->work.units there) so that jm_postsolve_solved — the one path with no sx to read a total from — has somewhere to read the charge back from, without a new function parameter or a new struct field."

requirements:
  - REQ-presolve

actuals:
  tokens: 3990
  tasks: 3
  commits: 2

metrics:
  duration: "~20 min: two clean WSL rebuild-and-test cycles (default + JAOS_NO_PRESOLVE) to measure the pinned constants, one earlier cycle that caught a stale-object-file trap from skipping `make clean` between EXTRA_CFLAGS settings, and the implementation/documentation itself"
  completed: 2026-08-13

status: complete
---

# Phase 02 Plan 02: Presolve bills the work counter, nonzero-only, and the one-way door closes with a measurement on both sides of it Summary

**Presolve now charges `JM_WORK_NONZERO` for exactly the nonzeros it visits shifting a fixed column's rows — nothing else — and a pinned test fixes the one-fixed-column model at 8202 units with the charge and 8206 without it, both measured under WSL rather than derived.**

## Performance

- **Duration:** ~20 min (dominated by two clean WSL rebuild-and-test cycles, not machine time on a campaign — this plan runs no netlib set)
- **Tasks:** 3 (checkpoint decision, implementation, documentation)
- **Files modified:** 4

## Accomplishments

- `jm_presolve_run` bills `JM_WORK_NONZERO` per nonzero visited while a fixed column's cost and matrix contribution shift the rows it touches — the rate the checkpoint selected, and the only charge presolve makes anywhere in this plan.
- `jm_dual_simplex` threads one `jm_work` through presolve and into `sx`'s own accumulator (`s.work = pre_work`, set right after `sx_init`'s memset zeroes it), so a caller's `jaos_set_work_limit` sees one total for the whole solve, not two added together after the fact.
- `jm_postsolve_solved` publishes that same charge on the one path nothing else would — a model presolve fixes completely, where no `sx` is ever built.
- A pinned change detector in `tests/test_presolve.c` fixes the one-fixed-column model's total at two exact, measured values: 8202 under presolve, 8206 under `JAOS_NO_PRESOLVE` — plus a determinism check that two solves of the same model, basis cleared between them, report equal units.
- `docs/work-units.md` gains presolve's entry under "Where it is charged," the warning that a work figure taken before this section existed and one taken after are not comparable on a model presolve reduces, and two new unbilled floors recorded beside the two the document already carried.
- No baseline touched: `git diff --stat -- bench/` is empty, `docs/tolerances.md` is empty. The rewrite is `02-07`'s task, not this one's.

## The checkpoint decision, recorded verbatim (Task 1)

**Selection: `nonzero-only`** — charge `JM_WORK_NONZERO` per nonzero each round actually visits, and nothing else. No `JM_WORK_ELIMINATED` on removal. No per-round fixed cost. No new `JM_WORK_*` constant.

The reasoning the developer accepted:

- It is the smallest claim that is still true. One weight, already defined as "a nonzero touched in a solve," no number this project does not have, no sweep left owed.
- The stated objection to it — that the counter cannot distinguish a productive round from the fixed point's last, empty one — is answered by a different mechanism this phase already built: `02-01`'s per-family counters (D-13) report exactly what each reduction removed. The work counter does not need to carry that signal.
- `JM_WORK_ELIMINATED` is documented as "a nonzero eliminated, **in a factorization or in a basis update**." Presolve is neither. Charging it there would widen a weight's documented meaning at the same moment the weight becomes contract.
- It is the move phase 1 already made: `01-02` declined to bill the bitmap words because a word is not a variable and a rate for them would be a number with no measurement on either side, and recorded the unbilled floor in `docs/work-units.md` instead.

**What that leaves unbilled, written down rather than left implicit:** presolve's per-round `O(rows + cols)` loop overhead (the classification pass that reads every column once, whether or not it fires) and the one-time reduced-model copy (Pass 2's CSC prefix and Pass 3's copy of every surviving column's nonzeros) are not charged. Both are recorded in `docs/work-units.md` beside the two unbilled floors already there — the `alpha` sweep and the `nvar/64`-read floor — in the same voice and for the same reason: real work, no reduction being computed, no measurement to set a rate from.

## Task Commits

1. **Task 1: the checkpoint** — no separate commit; the decision was already resolved before this plan ran (see above), carries no file changes of its own, and is recorded here per its own acceptance criteria.
2. **Task 2: Bill it, and pin what it costs** — `3f82216` (feat)
3. **Task 3: The billing, written where the currency is defined** — `3e2257a` (docs)

**Plan metadata:** committed immediately after this summary, no text between the write and the commit.

## Files Created/Modified

- `src/presolve.c` — `jm_presolve_run` charges `JM_WORK_NONZERO` for a fixed column's visited nonzeros; sets `p->reduced.solve_work` unconditionally at the end so the SOLVED path has a total to read; `jm_postsolve_solved` publishes that total instead of a hardcoded zero
- `src/simplex.c` — `jm_dual_simplex` declares `jm_work pre_work = {0}` unconditionally, passes `&pre_work` to `jm_presolve_run` (was `nullptr`), and seeds `sx`'s own `s.work` from it right after `sx_init`
- `tests/test_presolve.c` — `PRESOLVE_MODEL_WORK_PINNED` (two branches, `#if defined(JAOS_NO_PRESOLVE)`), `test_presolve_bills_the_work_counter`, `test_presolve_work_is_deterministic_across_re_solves`
- `docs/work-units.md` — presolve's own paragraph under "Where it is charged" (with the non-comparability warning ahead of it), and two new unbilled-floor paragraphs beside the existing two

## Decisions Made

See frontmatter `decisions:` for the compact list. The substantive one beyond the checkpoint itself: which nonzeros "a round actually visits" means in the *current* code, since the checkpoint's own `<context>` names three distinct touch points (the round's scan, an elimination's removal, and the reduced model's one-time copy) and the resolution text explicitly rules out the last one. Read against `jm_presolve_run` as it stands after `02-01`, only Pass 1's row-bound-shift loop is "the round" computing anything — Pass 2 and Pass 3 are the reduced model's construction, not a reduction being computed — so that loop is the one and only charge site.

## Deviations from Plan

### 1. [Rule 3 — blocking] `src/simplex.c` needed editing despite not being in `files_modified`

- **Found during:** Task 2, implementing the checkpoint's own instruction to "charge before the reduced model is handed to `sx_init`, so the units are on the same accumulator `sx_init` continues"
- **Issue:** the plan's frontmatter lists only `src/presolve.c`, `docs/work-units.md`, `tests/test_presolve.c`. But the call site that must change — `jm_presolve_run(m, &p, nullptr)` — lives in `src/simplex.c`, and it is the only place `nullptr` can become a real `jm_work *`. Without touching it, `jm_work_add`'s own `if (w != nullptr)` guard makes every charge added in Task 2 a silent no-op.
- **Resolution:** edited `src/simplex.c` directly: declared `pre_work`, passed `&pre_work` into `jm_presolve_run`, and seeded `sx`'s `s.work` from it after `sx_init`. This is exactly what the task's own `<action>` text asks for ("Charge before the reduced model is handed to `sx_init`") — the file list was incomplete, not the instruction.
- **Verification:** both `make -j12 test` and `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE test` pass; the pinned test would otherwise read 0 always (the no-op case), which it does not.
- **Committed in:** `3f82216`

### 2. [Process] A stale-object-file trap caught before the pinned values were finalized

- **Found during:** the first attempt to measure the pinned constants
- **Issue:** the plan's own `<verify>` runs `make -j12 test && make -j12 clean && make -j12 EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE test` — one `clean` between the two builds. Running the *default* build a second time afterward (to re-check a fixed placeholder value) without an intervening `clean` reused object files still compiled with `-DJAOS_NO_PRESOLVE` from the prior run, since `EXTRA_CFLAGS` carries no dependency tracking in this Makefile (confirmed: no `.flags` file, no `FORCE` target). The "default" run then silently reported the `JAOS_NO_PRESOLVE` figure (8206) instead of its own (8202).
- **Resolution:** this is exactly `CLAUDE.md`'s and the project's own memory's standing warning — "`make clean` between settings and a canary that must move, or the sweep measures one binary N times." Rebuilt with an explicit `make clean` immediately before *each* build in the measurement script; both final values (8202, 8206) confirmed identical across two independent clean rebuilds each.
- **Committed in:** not a code change — a process correction to how the two pinned constants were measured, kept here so a future reader repeating this measurement does not lose an afternoon to the same trap.

---

**Total deviations:** 2 — one blocking (necessary plumbing the plan's own action text required but its file list omitted), one process (a measurement trap caught before a wrong number was pinned, not shipped).
**Impact on plan:** No scope creep beyond what D-14's own instruction required. Both pinned constants are measured, not derived, and were re-confirmed after the doc-only Task 3 edit left the compiled tree unchanged.

## Issues Encountered

None beyond the two deviations above.

## Next Phase Readiness

- `02-03` through `02-05` inherit the rate (nonzero-only) and the two unbilled-floor precedents for their own per-round bookkeeping — a new reduction family that visits nonzeros to *compute* a reduction bills them; a family's own classification scan or any one-time structural cost does not, unless a future checkpoint says otherwise.
- `02-07` owns the deliberate baseline rewrite this plan's own `<verification>` explicitly declines to do — `git diff --stat -- bench/` is empty here, by design, and will read differently the moment `02-07` runs its own campaign.
- `02-09` still owes `DECISIONS.md` two entries this phase has now generated: the `finnis`/D24 entry `02-01` already flagged, and a new one for this plan's own rate — the checkpoint's reasoning, the two measured figures (8202/8206), and what was refused (`JM_WORK_ELIMINATED` on removal, a per-round fixed cost, a new constant) — per `jaos-record`'s own rule that a refusal is a closed decision and belongs in the settled table, not only in this summary.
- `numerics-reviewer` and `jaos-measurer` were not run in this plan. `CLAUDE.md` requires them once per phase that touches solver internals, not once per plan; this phase's own two owed tasks (per its `02-CONTEXT.md`) are not yet scheduled in a specific plan number and remain open for whichever later plan in this phase's sequence carries them.

## Known Stubs

None. Every charge site added is either exercised by the pinned test or explicitly, and correctly, a no-op (`jm_work_add`'s own `w != nullptr` guard, exercised by `test_fixed_col_counter_is_exact`'s pre-existing call with `nullptr`).

## Threat Flags

None beyond the register already carried in `02-02-PLAN.md`'s own `<threat_model>` (T-02-05, T-02-06, T-02-07), which this plan's own acceptance criteria enforce directly: `git diff --stat -- bench/` empty (T-02-07), the checkpoint gate itself and this summary's explicit non-comparability statement (T-02-06), and the pinned exact-value test plus the two-solve determinism check (T-02-05).

## Self-Check: PASSED

1. `src/presolve.c`, `src/simplex.c`, `tests/test_presolve.c`, `docs/work-units.md` present at the paths named and match `git show 3f82216`/`3e2257a` exactly.
2. `git log --oneline -3`: `3e2257a`, `3f82216`, `71f9f95` (02-01's own final metadata commit) — both this plan's commits present.
3. `git diff --stat -- bench/`: empty. `git diff --stat -- docs/tolerances.md`: empty.
4. `make -j12 test` and `make -j12 EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE test` both re-run clean (`0 Failures`, exit 0) against the final tree, each preceded by its own `make clean`, confirming the pinned values 8202/8206 twice over.
5. `grep -n JM_WORK_ELIMINATED src/presolve.c`: no match — the checkpoint's `nonzero-only` selection is not just recorded, it is the only weight the file references (`JM_WORK_NONZERO` is the sole `JM_WORK_*` symbol `src/presolve.c` uses).

## Estimate vs actuals

The plan estimated `tokens: 45000` at `confidence: low`. Realized: **3,990** on the chars/4 scale (11,259 + 4,700 raw chars across the two commits), under by more than 11x. Consistent with `02-01`'s own reading of this phase's estimator: a plan whose deliverable is a small, precisely-targeted charge plus its own pinned test is short by construction — most of this plan's actual cost was the two WSL rebuild cycles needed to *measure* rather than guess the two pinned constants, which is exactly the kind of cost `01-04`'s and `02-01`'s own summaries already flagged as invisible to a token estimate.

---
*Phase: 02-presolve-and-postsolve*
*Completed: 2026-08-13*
