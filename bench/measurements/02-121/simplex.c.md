# simplex.c
before: 5925 lines, 2960 comment (49%)
after:  3655 lines,  691 comment (18%)
strip-comments: IDENTICAL CODE

Two passes. The first agent took it from 2960 to 880 comment lines (3844
total, 22%). This pass re-read the whole file, trimmed the part it had
already thinned, and finished the rest: 123 exact-text edits, 189 more
comment lines out, 691/3655 = 18.9%.

The 15% target was not reached. See "Anything else" for what is left and
why it stayed.

## Contracts that survived and deserve an assert or a test

Whole file, both agents' parts. "(exists)" marks a check already in the
code.

Struct fields

- `cost0`: "written once and never again" (sx_init: "The one write to
  cost0 (D121)") — debug assert in `publish` that every structural's
  `cost0[j] == sigma * col_cost[j] * gamma[j]` bit-for-bit; or a test that
  a solve leaves `cost0` equal to a fresh scaling of the model.
- `shift`: "`shift` is written at three sites and nowhere else: the lend in
  `shift_to_feasible`, and the two repayments (D124)." — a script test that
  counts `s->shift[` / `->shift[` assignment sites in `src/simplex.c` and
  pins the number at 3 (the same shape as the `s->status` 8-site count that
  drifted before).
- `cost`/`cost0`/`shift`: "There is no `cost[v] == cost0[v] + shift[v]`
  invariant ... Every reader ... must compute `cost[v] - cost0[v]` and never
  read `shift[v]`." — grep-level: the only reads of `shift[v]` as a value
  should be the `== 0.0` tests in `repay_shifts`, `primal_cleanup`, and the
  two debug asserts. A pinned count.
- `rhsc`/`resc`: "Each is `memset` at the entry of its single reader and
  dead at its exit. A borrower whose value has to survive a call to
  `compute_primal(s, true)` is not safe." — debug poison: fill each with
  NaN at its reader's exit; any later reader that trusts the value trips
  the isfinite guards or the digests.
- `apat`/`anpat`: "Where `alpha` can be nonzero, ascending and without
  repeats, or `anpat < 0`" — debug assert at the end of `price_all`: when
  `anpat >= 0`, `apat` strictly ascending and `alpha[v] == 0.0` for every v
  not in it (O(nvar), debug only).
- `amark`: "zero between iterations" — debug assert at `price_all` entry
  that every word of `amark` is 0.
- `rpat`/`nrpat`: "Where `rho` is nonzero, ascending" — debug assert after
  `build_pricing_row`: when `nrpat >= 0`, ascending and `rho[i] == 0.0`
  off the pattern. The dense branch of `price_all` rebuilds `rpat` and
  "pivot's exact weight reads" it (D42): same assert covers both writers.
- `nbmark`: "bit v is set exactly when `status[v] != JM_BASIC`" — the
  NDEBUG cross-check in `dual_ratio_test` (exists) only runs on the dual
  path. The primal path (`run_primal`, `run_primal_phase1`,
  `primal_cleanup`) reaches `pivot()` and `repair_singular_basis` without
  it. A `nbmark_consistent(s)` debug helper asserted at the end of
  `refresh` would cover every path.
- `cpat`/`ncpat`: "Unordered: every reader is elementwise." — a caller
  contract. A test that permutes `cpat` after the FTRAN and checks the
  digest is unchanged would document it.
- `c1`/`c1_at`/`n_c1_at`: "At most `nrow`, because only a basic can be
  infeasible (D199)." — `assert(s->n_c1_at <= s->nrow)` after the loop in
  `primal_phase1_costs`. "`c1` is allocated zeroed" and the clear loop:
  debug assert after the clear that `c1` is all zero (O(nvar), debug).
- `c1`: "Swapped into `s->cost` for the duration of one `compute_duals`
  call and swapped straight back" — dropped sentence worth a test: "exact
  because `compute_duals` reads `cost` and writes only `y` and `d`". Debug
  build: snapshot `cost`, `cost0`, `shift`, `xb` around the call in
  `primal_phase1_duals` and memcmp.
- `in_phase1`: "Read by the three places a cost is lent: `update_dual`, the
  tail of `pivot()`, and `refresh`'s sweep" — the D191/D193 defect class.
  Assert in `run_primal`, right after `s->in_phase1 = false`: every
  `shift[v] == 0.0` and `cost[v] == cost0[v]` on a solve that entered
  phase 1 with no loans outstanding. A phase 1 that lends anything trips
  it.
- `infeas_best`: "the smallest total primal infeasibility this solve has
  reached" and run_primal: "`HUGE_VAL` until phase 1 has computed
  something; 0.0 only once the point really is feasible." — test through
  the progress callback: in phase 1 the reported value is finite and
  non-increasing; in phase 2 it is exactly 0.0.
- `duals_dirty`: "A dual step driven by the row's pattern can skip the rest
  only while this is clear." — debug assert in `pivot()`'s pattern branch,
  before the update: `dual_breach(s, v) == 0.0` for every nonbasic v off
  `apat` (O(nvar), debug).
- `verified`: every caller clears it before `pivot()` — `assert(!s->verified)`
  at `pivot()` entry.
- `shift_pending`: "Set by a warm start and by nothing else. The cold start
  is dual feasible by construction and its first refresh must leave the
  costs untouched." — test: cold solve, stop after the first `refresh`
  (work limit 0 or a callback), `shift` all zero and `cost == cost0`.
- `dbg_col`: "Its own buffer, not one of the four above." — grep-level.

Setup

- `sx_init`: "The same product in the same order of operations, laid out by
  row: that is what makes the row-wise pricing sums bit-identical." —
  debug assert after `sx_init`: for every nonzero, `arv[p]` equals the
  matching `av[k]` bit-for-bit (needs the CSC/CSR position map; O(nz)).
- `sx_init`: "Infinities survive: no bound changes side." —
  `assert(isfinite(s->lo[v]) == isfinite(model bound))` per variable.
- `real_lower`/`real_upper`: "A loan only ever replaced an infinity, so
  `fake` is enough to undo it." — debug assert: `fake[v] == FAKE_LO`
  implies `lo[v] == -ARTIFICIAL_BOUND`, and likewise for FAKE_UP.
- `build_initial_basis`: "every structural pinned to the bound that makes
  its reduced cost feasible, lent one if it has none" — assert after: with
  B = -I, `d = cost`, so `dual_breach(s, j) == 0.0` for every structural.
  A test with a free column of nonzero cost exercises the lend.
- `build_warm_basis`: "The stored arrays are the model's and are never
  written." — memcmp `start_col_status`/`start_row_status` before and
  after a warm solve. "A LONG count is still refused." — test: a stored
  basis with `nrow + 1` basics starts from "the slack basis" (log line).
  "Every return before this point is taken before any status is written."
  — a failed warm start followed by the cold fallback must give the same
  digest as a plain cold solve.
- `refactorize`: "jm_lu_factor is entitled to non-null arrays whenever
  dim > 0" — `assert(room >= 1)`.

Recomputation

- `compute_primal`: "Borrowed: `raw` belongs to pivot(), which rebuilds it
  from scratch before every use" and `compute_duals`: "Borrowed: `tau` is
  pivot()'s weight-update scratch, overwritten from `rho` before each use"
  — both hold by the structure of `pivot()` (`var_column` into `raw`,
  `memcpy` into `tau`, unconditionally). Nothing to assert; a comment-only
  contract that a change to `pivot()` could break silently.
- `compute_primal`/`subtract_basis_times`: the isfinite guard (D165) —
  no small model can reach an infinite partial sum; not testable here.
- `repair_singular_basis`: "The logical of an uncovered row cannot already
  be in the basis; the check below is kept anyway." (exists)
- `refresh`: "Never while the primal phase 1 runs: this is the third site
  that lends a cost (D193)." — covered by the `in_phase1` assert above.

One iteration

- `jm_dse_update`: "Each row's new weight depends on its own old one and
  nothing else, so the dense and sparse forms compute the same numbers." —
  unit test in `tests/`: same inputs with and without `pat`, weights
  bit-identical.
- `dual_ratio_test`: "the two scans admit the same candidates in the same
  array positions" (exists, NDEBUG). "Exactly nrow variables carry
  JM_BASIC." (exists).
- `bfrt_walk`/`apply_flips`: "[0, live) are still in play, [live, n) are to
  be flipped" — assert in `apply_flips` that `rrange[k]` is finite for every
  k in [at, n).
- `price_all`: "The sums are bit-identical to the column-wise ones by
  construction" (D35) — direct unit test: after `build_pricing_row`,
  `alpha[v] == price_entry(s, rho, v)` bit-for-bit for every nonbasic v, on
  a netlib instance in a debug build. The D35 proof was digests; this pins
  the mechanism.
- `price_all`: "A basic variable prices to zero by definition. They stay in
  the pattern" — debug assert after the zeroing: `alpha[basis[i]] == 0.0`
  for all i.
- `build_pricing_row`: "Ordered so price_all's sums come out the way the
  column-wise pass produced them (D35)." — `rpat` strictly ascending after
  `jm_pattern_order` (cheap, O(nrpat)).
- `jm_pattern_order`: "Reading back over the input is safe: the distinct
  count can only be smaller than what went in." — `assert(k <= n)`.
- `jm_nonbasic_expand`: "The testable mirror of the walk in
  dual_ratio_test, which does not call it." — the two walks are separate
  code. A test that the expanded list equals the candidates `admit_candidate`
  saw, on a random bitmap, keeps them from drifting apart.
- `shift_to_feasible`: "Shifting a nonbasic's cost changes only its own
  reduced cost." — test: recompute `d` after one shift; only `d[v]` moved.
  "The record is what the cost actually moved by, not what was asked for
  (D125)." — test: `need` below half an ulp of `cost[v]` leaves both
  `cost[v]` and `shift[v]` unchanged.
- `pivot`: "a declined pivot leaves every field exactly as it found them" —
  fault build: force the LU_AGREE_TOL disagreement, snapshot every array
  before, memcmp after `*took == false`. Also "It cannot spin: a rebuild
  leaves `n_updates` at zero" (run) — test: never two declines in a row.
- `pivot`: "rho still holds row r of B^-1 from build_pricing_row, the one
  piece of state this function inherits rather than derives." — debug
  recompute into `dbg_col` (BTRAN of e_r) and memcmp with `rho`, the same
  shape as `primal_bound_flip`'s check.
- `pivot`: "The bitmap moves with the status, on the same lines." — the
  `nbmark_consistent` helper above.

Settling and re-entry

- `repair_dual_infeasibility`: "Taken only when every basic stays inside
  its bounds" — debug assert after each accepted swap:
  `primal_worst_violation(s) <= primal_tol`.
- `repay_shifts`: "The test is on the COST and not on the record alone: a
  cost can move while its record cancels back to exactly zero." — the
  dropped example: lend +1e17 against a cost of 1, later lend -1e17. Needs
  a test hook into the static function; worth one.
- `settled_objective`: precondition (exists, NDEBUG).
- `save_settled`/`restore_settled`: "Restoring these five and rebuilding
  lands on exactly the saved point" — test: save, run a re-entry that is
  discarded (fault build), restore; `xb` and `d` digests equal the saved
  point's.
- `better_point`: the lexicographic rule — unit test on the four cases
  (both inside, both outside, one each way). Static; needs a `jm_` wrapper
  or a test hook.
- `reenter_after_settling`: "Anything other than a second optimum is
  discarded and the settled point stands." — fault build: make `run()`
  return INFEASIBLE on re-entry; the published point must equal the settled
  one.
- `primal_ratio_test`: "It leaves `B^-1 M_q` in `s->col`, and a bound flip
  reads it there." (exists: the memcmp in `primal_bound_flip`, NDEBUG).
- `primal_cleanup`: "each entry is pivoted at most once" — `cand` entries
  distinct by construction; "The point stays primal feasible and the
  objective cannot rise." — debug assert after each pivot:
  `primal_worst_violation(s) <= primal_tol`.
- `held_by_an_invented_bound`: "This runs after settle_shifts." — debug
  assert at `classify_optimum` entry: `shift` all zero.
- `improves_without_limit`: "lent bounds do not count, which makes the
  verdict independent of ARTIFICIAL_BOUND." — build-flag sweep:
  ARTIFICIAL_BOUND 1e6 and 1e10 give the same verdict on the infeas set.

Primal method

- `primal_phase1_costs`: "The tolerance is `primal_tol`, the same one
  `primal_worst_violation` uses." — the run-time check in `run_primal`
  ("still violated by %.6g") is the assert; a pinned test that reaches it
  would prove it can fire.
- `primal_phase1_duals`: "The swap is a pointer and the restore is
  unconditional, so `cost` is the same array on the way out." —
  `assert(s->cost == real_cost)` after; plus the memcmp test under `c1`.
- `run_primal_phase1`: "`*feasible` staying false says so: phase 2 must
  not start from a point phase 1 did not finish." — test: a work limit hit
  inside phase 1 gives WORK_LIMIT with `solve_phase1_iters ==
  solve_primal_iters`.
- `run_primal_phase1`: "Phase 1's stall accounting is the SHARED
  `s->last_gain` and `s->bland`; `run_primal` resets both" —
  `assert(!s->bland)` at phase-2 entry.
- `run_primal`: "Recorded here so it is written on EVERY exit from phase 1."
  — test each exit (work limit, time limit, interrupt, numerical): when
  phase 1 ran, `solve_phase1_iters > 0`.
- `run_primal`: "rebase the cap per phase if `ITER_SANITY_FACTOR` ever
  drops below about 60" (D196) — `static_assert(ITER_SANITY_FACTOR >= 60)`.
- `run`/`run_primal`/`run_primal_phase1`: "Optimality is not accepted on
  carried numbers (D20)" — test: every OPTIMAL solve passed through at
  least one `refresh(s, ok, true)`. This also covers `compute_primal`'s
  "the final `refine = true` refresh rebuilds `x_B` from scratch".
- LOG_EVERY / PROGRESS_EVERY / TIME_CHECK_EVERY "(D8)" — test: the progress
  callback fires at identical iteration numbers on two runs.

Entry point

- `jm_model_ensure_solution_arrays`: "All six or none." — fault build: fail
  the fourth allocation; all six pointers null afterwards.
- `published`: "A published zero is a zero (D21)." — test: no `-0.0` in any
  `sol_*` array (signbit scan).
- `published_status`: "Mapped rather than cast" — a test that each
  `jm_var_status` maps to its `jaos_basis_status` by name.
- `publish`: "No loan may still be outstanding here" (exists, NDEBUG).
- `publish`: "The basis is written, kept, and only then cleared." — test:
  after a WORK_LIMIT stop `jaos_basis` reports none, and the next solve logs
  "starting from the basis on the model".
- `jm_dual_simplex`: "A new solve owns these three ... Three returns below
  run before any of them is written." — test: solve A with phase 1, then
  solve B that presolve solves entirely; `solve_phase1_iters == 0` after B.
- `jm_dual_simplex`: "the work stays on the one accumulator (D16), the
  clock keeps its origin, the iteration count restarts with the sx" —
  test on a warm-then-cold restart: `solve_work` includes the warm attempt,
  `solve_iters` does not.
- `jm_dual_simplex`: "`st != JAOS_OK` stays FIRST: `outcome` is
  uninitialised on that branch." — the read is guarded by `&&` order only.
  Initialising `outcome` (`= JAOS_SOLVE_NOT_RUN`) would make the assert
  unnecessary; that is a code change and was not made.

## Left in, unsure

- File header, lines 9–18: the four sign conventions and "Pricing is dual
  steepest edge [8] and phase 1 is by artificial bounds." Kept whole by the
  rule (sign conventions stay). 21 lines.
- The ten section banners (30 comment lines). Untouched, as in lu.c.
  Collapsing each to one line saves 20 lines and touches no claim.
- Struct field contracts, about 95 comment lines. Every one is an
  ownership, aliasing or units statement; none was cut to a pointer.
- `primal_ratio_test` (16 lines): two paragraph breaks could go (2 lines).
  Left because the three paragraphs are three different contracts (the
  step, the `s->col` hand-off, the `bland` parameter).
- `dbg_col` / `primal_bound_flip` check comment (6 lines): kept whole. It
  is the `s->col` five-writers contract the maintainer named.
- `c1` field: kept the swap contract; dropped "exact because
  `compute_duals` reads `cost` and writes only `y` and `d`" to the test
  list above.
- `DSE_DRIFT` "(PLAN 2.6)" and `price_all` "(PLAN 2.11)": two PLAN.md
  section pointers kept. `PLAN.md` is archived; CLAUDE.md says the citing
  comments are why it is kept. The third one, "Draft tolerances (PLAN.md
  2.6)" above `PRIMAL_TOL`, was dropped as history.
- `shift_pending` third sentence ("The cold start is dual feasible by
  construction and its first refresh must leave the costs untouched.")
  kept: it is a must-not-break.
- `ARTIFICIAL_BOUND`: the citation moved from "(Koberstein [21] compares the
  alternatives)" to "by artificial bounds [21]". A reorder, not a new claim.
- `primal_price`: D81/D82/D84 regrouped into one parenthesis after "-1 when
  none is eligible"; "Dantzig and not Devex" and "full pricing" went with
  their arguments.
- `held_by_an_invented_bound`: dropped "A basic is not evidence." The code
  tests `status == JM_AT_LOWER/UPPER`, which says the same.

## D-pointers dropped because the number does not exist

- none. All 61 distinct pointers in the file (D8 … D199) match a
  `## D<n> —` heading in `DECISIONS.md`. Pointers removed with their
  sentence: none; every D-number present before this pass is still present
  after it.
- For the record: against the ORIGINAL file, eleven pointers are gone.
  The first agent removed them with their narratives (all eleven exist in
  `DECISIONS.md`): D46, D47, D69, D85, D87, D118, D139, D162, D177, D178,
  D190. Each was a measurement or rejected-attempt citation, not a pointer
  attached to a surviving claim.

## Anything else the maintainer should know

- Target not reached: 18.9% against 15%. Reaching 15% from here needs 169
  more comment lines out of 691. What is left, by size: the 21-line header
  (sign conventions), ~95 lines of struct field contracts, 30 lines of
  banners, and ~300 lines in function-header blocks of 4+ lines that are
  each an invariant, a hand-off (`s->col`, `rho`, `raw`, `tau`), or a
  sign/units statement. Rule 7 says not to cut those for the number.
  The only free 20 lines are the banners.
- No code token changed (strip-comments: IDENTICAL CODE). 13 lines are
  over 79 characters; all 13 are in the original (trailing comments and the
  three 80-character `work_limit` lines) and were not touched. The
  original had 17.
- The first agent's part (constants through `pivot()`) was re-read in full
  and trimmed by a further ~90 lines; its cuts were correct, and nothing it
  kept was found to be wrong or paraphrased.
- Removed in this pass, by class: the determinant argument in
  `repair_singular_basis` (the B' block matrix), the measurement asides
  ("1e5 and 1e7 give identical answers", "no long map has been measured",
  "the margin was measured"), the D20/D29 caller lists in `refresh`, the
  "why a cold start is dual feasible" reasoning in `build_warm_basis`, the
  termination argument in `primal_bound_flip`, the phase-2-barely-runs
  mechanism in `run_primal` (D194/D197 carry it), and the paragraph
  explaining the `== NUMERICAL_ERROR` condition in `jm_dual_simplex`
  (kept: the condition and the FIRST ordering).
- Whole-block deletions (5): "Billed like every other per-nonzero walk",
  "So a later restart names the basis it threw away as repaired", the
  Bland leaving-variable note in `price_row`, "And here, for the reason
  given in take_best_if_better", and "Phase 1, when the point is not primal
  feasible." Each restated the code beside it.
- The edit script is `PURGE/simplex-finish.py` (123 exact-text
  replacements, each required to match once); `PURGE/blocks2.py` lists
  comment blocks by size and over-long lines.
