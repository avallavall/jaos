# Codebase Concerns

**Analysis Date:** 2026-08-12

This document adds to, and cross-references, what the project already tracks
in `PLAN.md` (open work) and `SPECS.md` (feature status). It does not
restate their reasoning — it points at the section and adds what those two
files do not cover: line-level fragility, determinism risk, citation
integrity, and test-coverage gaps found by reading the source directly.

---

## What `PLAN.md` already says is open

Read `PLAN.md` in full before treating any of the below as unflagged — it is
the authoritative, actively maintained list, current through **D92**.

- **Phase 3 — Presolve** (`PLAN.md` "Phase 3 — Presolve"). Not started.
  Measured value is **1.42x** vs HiGHS and **1.14x** vs SoPlex, smaller than
  the per-iteration gap, so it is not the largest lever any more — phase 6
  item 1 (cheaper iterations) is argued to matter more.
- **Phase 4 — Complete the product** (`PLAN.md` "Phase 4"), now the largest
  block of open work: write MPS/LP, write a solution file, sensitivity and
  ranging, infeasibility/unboundedness certificates, exact rational
  verification (Q8). `jaos_solve_time` shipped from this phase already.
- **Phase 5 — Bindings** (`PLAN.md` "Phase 5"). Python, blocked on phases 2
  and 4 settling; phase 2 has, phase 4 has not.
- **Phase 6 — Speed** (`PLAN.md` "Phase 6"), ranked and re-ranked by
  measurement, not opinion:
  1. The factorization / fill-in — 77.8% of the standard set's billed work,
     partly paid (D58, D59); what is left is fill reduction itself
     (`maros-r7` carries 4.801 nonzeros in its factors per nonzero of the
     basis against a 2.673 set average), which the plan says needs
     left-looking elimination and "its own decision."
  2. ~~`fit2p`'s unbilled work~~ — closed (D55, D56).
  3. ~~Partial and multiple pricing~~ — both built, swept and **refused**
     (D82, D84): correctness failures, not just a bad trade (`pilot` publishes
     an out-of-tolerance OPTIMAL under partial pricing).
  3a. **`admit_candidate`, the ratio test's candidate admission** — new head
     of the list. 14.98% of instructions under callgrind on `truss`, "the
     half neither refused pricing scheme touched." Stated rather than started
     because restricting it touches which column enters, risking Harris's
     two-pass guarantee — "needs its own decision before any code."
  4. `stocfor3`, a memory-traffic instance (18.8% in `memset`/`memcpy`/
     `malloc`), measured and left.
  5. BTRAN's `L'` pass and the eta passes — small (5.15%, 1.69%), "recorded
     so not costed again."
  6. Devex pricing as an alternative to exact steepest-edge — the named cure
     for the four instances whose weights are discarded on 80–93% of
     iterations.
  7. A primal simplex, also needed by phase 4's crossover.
- **Phase 7 — The long ones** (`PLAN.md` "Phase 7"): barrier + crossover,
  deterministic parallel B&B (`jaos_thread.h`, not yet written), MILP,
  QP/MIQP, conic, NLP, MINLP. Entirely unscheduled beyond the ordering
  stated there.
- **Open questions** (`PLAN.md` "Open questions"): **Q2** (LP/MPS dialect
  edge cases, fixed as encountered — the pattern already burned once on the
  objective-row `RHS` sign, see `docs/format-support.md`); **Q5** (NLP
  derivative strategy, deferred to phase 7); **Q8** (how exact rational
  verification of a final basis gets done — GMP is excluded by D11, and the
  alternatives are still just listed, not chosen).
- **One item is open and explicitly *not* a defect**: `PLAN.md` "Open, and it
  is what closing that defect exposed" — **`pilot87`'s suboptimality bound is
  not understood**. Across four variants `gap_positive` moves between 0.0068
  and 26.7 while every answer is inside tolerance and dual-feasible, and
  nothing today separates "the bound going slack" from "the answer getting
  worse." This gates real work: it refused two of D92's three candidate
  repairs, and the plan itself says "a change detector on a quantity nobody
  can interpret is a gate that can only ever be obeyed." Lives in
  `src/check.c:686-704` (`gap_positive`, `certified_suboptimality`,
  `relative_suboptimality`) and `src/simplex.c:2214` (`dual_breach`),
  `src/simplex.c:2360` (`settled_dual_violation`).
- **The re-entry loop's oscillation itself has no cure named** (`PLAN.md`,
  "Known defects, carried" §2, closed consequence but open cause): the loop
  now publishes its best round rather than its last (D89), but *why*
  `pilot87` oscillates at all is still unexplained, and the entry says
  plainly "whatever repairs this loop, it is not that [taking the loan
  away]." Code: `src/simplex.c:271` (`SETTLE_ROUNDS`), the re-entry loop
  around `src/simplex.c:2892`.

## What `SPECS.md` marks as absent or partial

Section numbers below are `SPECS.md`'s own.

- **§1 Reading a problem**: LP format is **partial** (CPLEX-style core only,
  documented subset in `docs/format-support.md`); `.gz` input is **missing**,
  "handled outside the library today."
- **§3 Solving**: presolve **missing**; primal simplex **missing** (needed
  for crossover, and no longer needed for defect 4 per D85); crash basis
  **missing**, and refused rather than merely undone — it destroys the exact
  steepest-edge weights the slack basis gives; hyper-sparsity in the
  triangular solves **partial** — "both solves report their pattern, the
  passes billed for every slot are not all reduced" (ties to phase 6 item
  3a/5 above); barrier/crossover, MILP, deterministic parallelism all
  **missing**.
- **§5 Reading an answer**: a certified lower bound on suboptimality is
  **partial** — `certified_suboptimality` is sound but "reads the same
  ~1e-25 on answers known to be 1.04e-3 wrong as on correct ones," because
  the step it uses cannot move at a vertex (D73). Sensitivity/ranging,
  infeasibility/unboundedness certificates, and exact rational verification
  are all **missing**.
- **§6 Writing**: MPS, LP and solution-file writers are all **missing** —
  JAOS can read a problem it cannot write back out, and cannot export a
  solution in any standard format.
- **§7 Bindings**: only the C API is **done**; Python and everything else
  **missing**.
- **§8 Bars to clear**: MIPLIB 2017 (both subsets) **not started** — there is
  no MILP path to benchmark yet, consistent with §3.

## Numerical/robustness fragility

The pattern in this codebase is unusual and worth stating up front: nearly
every tolerance and trigger below already carries a multi-paragraph,
measured justification in its own comment, with a decision ID. That is a
strength — but it also means the actual risk is not "undocumented magic
numbers," it is that **the measurement backing each constant is tied to the
current algorithm and instance set, and nothing re-validates it when either
changes.** Below, "measured (Dn)" means the project already closed this;
"open" means the fragility itself is still live.

- **`src/simplex.c:190` `ARTIFICIAL_BOUND = 1e10`** — dual phase 1's borrowed
  bound. The comment states its own limits: "large enough to keep the loan
  out of the way and small enough to stay numerically sane... a draft, like
  the tolerances themselves." Unlike most constants here, no sweep is cited
  for this one — it is the one tolerance in the file whose comment does not
  point at a measurement, only at the intent.
- **`src/simplex.c:236` `NOISE_MARGIN = 1e5`** — the re-entry's threshold for
  when a reduced cost is signal rather than cancellation noise. Measured
  (5 of 110 instances have any live case, ratio saturates at 1e5..1e7), but
  it is a threshold on a *quantity derived from column traffic*, i.e. it
  scales with model size in a way the comment does not sweep independently —
  it is validated on this gate's models, not proven for arbitrary ones.
- **`src/simplex.c:271` `SETTLE_ROUNDS = 32`** and the re-entry loop
  (`src/simplex.c:2892`) — measured as a backstop that does not bind (D89),
  but the oscillation it backstops is the open item above. This is the one
  constant in the file that is explicitly *not* believed to be the real fix
  for what it guards against.
- **`src/simplex.c:143` `LU_AGREE_TOL = 1e-5`** and the stability trigger at
  `src/simplex.c:1930-1972` (D86) — the newest and best-measured of the
  triggers (a four-decade plateau between the worst healthy pivot and the
  broken one), but the comment itself states its limit: "**256 is still
  broken**, slow now instead of wrong. Past some interval the updates
  accumulate faster than a disagreement-driven rebuild clears them" — i.e.
  the fix is bounded, and the bound is not characterized beyond "some
  interval."
- **`src/simplex.c:215` `STALL_FACTOR = 10`** and Bland's-rule fallback
  (`src/simplex.c:1271-1341`) — measured over the standard 94 (D17), with two
  orders of magnitude of daylight between the worst healthy plateau and the
  one cycling instance (`grow15`). `grow15` itself is carried, unresolved,
  as taking **21.7x HiGHS's iterations** once the fallback fires — cheap
  insurance against non-termination, expensive when it actually engages,
  and no cure is proposed for that cost (`PLAN.md` phase 1, "Still carried
  from here").
- **`src/lu.c:34` `PIVOT_SEARCH_LIMIT = 4`**, **`src/lu.c:39` `DROP_REL =
  1e-14`**, **`src/lu.c:42` `TINY = 1e-300`** — Markowitz threshold pivoting
  constants; `PIVOT_SEARCH_LIMIT` is swept (D46) but `DROP_REL` and `TINY`
  carry no comment-adjacent sweep in this file — they read as classical
  numerical-linear-algebra defaults rather than JAOS-measured values, which
  is a gap against the project's own stated rule ("every number needs a
  measurement on both sides," `CLAUDE.md`).
- **Singular / rank-deficient basis path**, `src/lu.c:519-523` (pivot search
  gives up, "singular; the rank is what we have") through `src/lu.c:702-738`
  (columns past `lu->rank` get `perm = -1`, `u_diag = 0`). Well tested (see
  Test Coverage below), but every consumer that reads `lu->rank != n` and
  returns early (`src/lu.c:798`, `925`, `1014`) is a silent-degrade path by
  construction — the caller must know to check the flag rather than trust
  the solve, and a future call site that forgets to check would fail
  silently rather than loudly. No enforcement beyond code review catches
  that today.
- **`src/check.c:264` `IMPLIED_ROUNDS = 64`** — the independent checker's
  fixed-point cap for row-based bound propagation (D91). Measured with a
  canary (certified-answer count) that moved as advertised (17→48), and the
  comment itself records the danger class this checker keeps re-discovering:
  "8 was here first, chosen by nothing, and it left ten answers uncertified"
  — i.e. this exact constant has already been wrong once from being picked
  without a sweep, which is why it is worth flagging as the kind of value
  that could regress silently if the propagation logic changes shape again.
- **`src/scale.c:41-47`** (`CR_MAX_ITER=30`, `CR_TOL=1e-8`, `GEO_MAX_PASS=20`,
  `GEO_TOL=1e-3`, `EXP_LIMIT=512.0`) — Curtis-Reid scaling iteration caps.
  These carry no per-constant measurement comment in the file (unlike the
  simplex constants); `EXP_LIMIT` in particular caps the exponent scaling
  factors can reach, and its consequence on badly-scaled real-world models
  outside the 139-instance gate is untested by construction — the gate is
  the only evidence for any of the five.

## Determinism risk

The codebase is unusually disciplined here, and this section is mostly a
confirmation rather than a finding:

- **Clock usage is confined and self-documenting.** Every `clock_gettime`
  call (`src/simplex.c:3126`, `3132`, `3555`) is gated to the time-limit
  check or to a progress line, both explicitly paced by iteration count
  (`TIME_CHECK_EVERY`, `LOG_EVERY`, `PROGRESS_EVERY`, all at
  `src/simplex.c:145-162`) rather than by the clock itself. No pivot
  selection reads the clock.
- **No `rand`/`srand`/`qsort` anywhere in `src/`.** Checked directly —
  none found. Sorting where it matters (Markowitz tie-breaking, bucket
  ordering in `src/lu.c`) is done with fixed-size buckets and explicit
  index order, not a comparator whose stability the standard does not
  guarantee.
- **No file-scope mutable state.** Checked for `static` data at file scope
  across every `.c` file in `src/`; every `static` declaration found is a
  function. There is no hidden global the solver could accidentally thread
  through calls in an address- or order-dependent way.
- **The name→index hash map** (`src/util.c:35-125`, FNV-1a, open addressing)
  hashes on the *string content* of row/column names, not on pointers, so
  MPS/LP parsing order cannot leak into iteration order through the hash
  table. Confirmed by reading `jm_nmap_insert`/`jm_nmap_get` directly.
- **The one place this is not free by construction**: the parallel
  benchmark runner (`PLAN.md` "1.2 The parallel runner"). This is already
  flagged by the project itself — parallel runs are declared to invalidate
  only the *timing* column, verified by diffing all 139 result lines against
  sequential-run records — so it is documented, not a live risk, but it is
  the one piece of infrastructure in the repository that runs the solver
  concurrently at all.

## Comment/doc drift — decision-ID citation audit

Checked every `Dn` citation in `src/`, `include/`, `tests/`, `bench/*.md`
and `docs/` (excluding `bench/instances*/**.mps`, which are LP data files
whose numeric tokens collide with the `D[0-9]+` pattern and are not
citations) against the headings in `DECISIONS.md`.

- **`DECISIONS.md` contains exactly `D1` through `D92`, contiguous, no gaps
  and no duplicates** — verified by diffing the extracted heading numbers
  against `seq 1 92`.
- **Every citation found resolves to an ID in that range.** No dangling
  citation — no `Dn` in a comment or doc pointing at a decision that does
  not exist — was found anywhere in `src/`, `include/`, `tests/`,
  `bench/README.md`, `bench/compare/README.md`, or `docs/`.
- Citation volume: 165 in `src/`+`include/`+`tests/` (`.c`/`.h` files), 25
  more across `bench/README.md` and `bench/compare/README.md`, and 84 in
  `docs/`. These counts differ from the ~198/~113 figures in the task brief;
  the difference is almost certainly counting methodology (whether
  `bench/*.md` is grouped with `bench/` code, whether repeated citations in
  one paragraph are counted once or per-occurrence) rather than a
  disagreement about which citations exist. Re-run:
  `grep -rhoE '\bD[0-9]+\b' --include="*.c" --include="*.h" --include="*.md" <dirs>`
  to reproduce.
- **The coupling itself is the risk, independent of today's clean result.**
  Nothing in the repository — no test, no script under any `Makefile`
  target, no CI-equivalent — checks that a citation resolves. The check
  above was done by hand with `grep` and `sort`/`diff`. If `DECISIONS.md`
  is ever renumbered, or an entry is deleted rather than marked closed
  in place (the file's own convention is to close in place, e.g. the
  `~~1. **The checker...**~~` **Closed (D91)** pattern in `PLAN.md`, never to
  delete), every one of these ~274 citations across a dozen files would
  need a synchronized edit, and nothing would fail loudly if one were
  missed — a stale citation is silent until a human follows it.

## Test coverage gaps

- **`src/alloc.c` and `src/status.c` have no dedicated test file** (no
  `test_alloc.c`/`test_status.c` alongside the other one-to-one pairs —
  `src/lu.c`→`tests/test_lu.c`, `src/model.c`→`tests/test_model.c`, etc.).
  `status.c`'s string tables are low-risk (pure lookup, would fail loudly).
  `alloc.c` is not: `jm_alloc_array`/`jm_calloc_array`
  (`src/alloc.c:11-32`) are the single choke point for every array
  allocation in the library and guard against `int64_t` size-overflow via
  `ckd_mul`, but no test in `tests/` directly forces the overflow branch —
  `grep -rn "jm_alloc_array\|jm_calloc_array\|ckd_mul" tests/*.c` finds
  nothing. `tests/test_fuzz.c:232` accepts `JAOS_ERR_OUT_OF_MEMORY` as a
  valid fuzz outcome but that is incidental coverage of the *symptom*, not a
  targeted test of the overflow guard itself.
- **The 40 `JAOS_ERR_OUT_OF_MEMORY` return sites across `src/*.c`** (an
  actual `malloc`/`realloc`/`calloc` returning `nullptr`, as opposed to the
  overflow guard above) have no fault-injection test anywhere in `tests/` —
  expected for a project with no mocking/injection harness, and not unusual
  for C libraries generally, but it means roughly 40 error-handling branches
  are compiled and shipped but never executed by the test suite.
- **The project's own testing methodology names a structural coverage
  gap, and it is worth restating here because it is easy to miss inside
  `PLAN.md`'s narrative:** three of the four defects closed this milestone
  (D72/D86, D85, and the D92 pair) were found by *sweeping*
  `REFACTOR_EVERY` over 16..256 — a parameter the acceptance gate never
  varies — not by the 139-instance gate itself at its shipped setting of 64.
  `PLAN.md`'s own words: "neither will be found again by running the gate,
  and neither will go away on its own." This means the standing test suite
  (`make test`, `make sanitize`, and the three netlib gates) structurally
  cannot catch this class of defect; only a deliberate trajectory sweep can,
  and that sweep is not part of any automated target — it is a manual
  campaign run occasionally by `EXTRA_CFLAGS=-DREFACTOR_EVERY=...` per the
  Makefile hook at line 94-101.
- **No test exercises `lu->rank != n` propagation into a live solve** beyond
  the LU-level singular-matrix tests in `tests/test_lu.c` (well covered,
  see `test_singular_matrices_are_reported_not_hidden` at
  `tests/test_lu.c:274`). Whether `src/simplex.c`'s callers correctly refuse
  or repair a mid-solve singular basis (as opposed to `jaos_set_basis`
  rejecting one on entry) is exercised indirectly through the netlib gate,
  not through a targeted unit test that hands the simplex loop a
  deliberately rank-deficient basis mid-solve.
- **MILP, barrier/crossover, presolve, and deterministic-parallelism code
  paths have zero tests**, but this is not a gap — none of that code exists
  yet (SPECS §3, all **missing**), so there is nothing to test.

## Anything genuinely surprising

- ~~**`CLAUDE.md` and the entire `.claude/` directory … are excluded from
  version control.**~~ **RESOLVED 2026-08-12 — this finding is now false, and
  it was acted on.** It was correct when written: `.gitignore` did exclude both,
  so the measurement discipline, the tolerance rules and the skill routing
  table lived only on one machine, and a clone got none of it. Commits
  `0c99249` and `7df2d00` reversed it. `CLAUDE.md`, the three subagents, the
  seven project skills and `jaos-measure`'s three scripts are tracked —
  14 files — and `.gitignore` now says so in as many words.

  Two files are still deliberately excluded, for stated reasons:
  `settings.local.json`, because it sets `defaultMode: bypassPermissions` and
  committing it would impose "run without asking" on anyone who clones this;
  and `scheduled_tasks.lock`, which is a session id and a pid rewritten every
  run.

  Worth keeping the finding visible rather than deleting it: the audit that
  closed it also found `geomean.py` — the only runnable form of D46 — among the
  untracked files, and `preflight.sh`, which is the whole of T-01-08's
  mitigation.
- **Third-party competitor solver binaries (HiGHS, SoPlex, Clp) live under
  `bench/compare/solvers/` on disk but are correctly gitignored**
  (`.gitignore` line for `/bench/compare/solvers/`) and pinned instead by
  `bench/compare/solvers.manifest` with checksums (D52). Confirmed with
  `git ls-files` returning nothing for that directory. Not a concern — flagged
  here only because an early grep for stray decision-ID-shaped tokens
  matched inside these binaries, which is a reminder that any future
  automated citation-checker (see the comment/doc drift section above) needs
  to exclude `bench/compare/solvers/` and `bench/instances*` the same way
  this analysis did, or it will drown in false positives from committed data
  and gitignored binaries alike.
- **Zero uses of `assert()` anywhere in `src/`** — checked directly. Given
  the shipping build defines `-DNDEBUG` (`Makefile`, `RELEASE_CFLAGS`), any
  invariant expressed as `assert()` would silently compile out in the build
  that actually runs the gate; the project avoids that trap entirely by
  using explicit runtime checks and `jaos_status` returns instead. This is a
  strength, not a gap, but it is a deliberate absence worth confirming
  rather than assuming.
- **The most-measured constant in the file is also the one whose comment
  admits the fix is incomplete**: `LU_AGREE_TOL`'s stability trigger
  (D86, `src/simplex.c:122-143`) reads as the codebase's best piece of
  numerical engineering — two solves the iteration was already paying for,
  compared for free — and its own comment still ends "256 is still broken."
  The project's habit of writing down what remains wrong even in the
  entry that closes a defect (see also the `pilot87` suboptimality-bound
  item above) is unusual and makes this document shorter than it would be
  for a codebase that only records its successes.

---

*Concerns audit: 2026-08-12*
