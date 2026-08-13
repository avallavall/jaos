# Phase 2: Presolve and postsolve - Context

**Gathered:** 2026-08-12
**Status:** Ready for planning

<domain>
## Phase Boundary

A model can be reduced before the simplex sees it, and a solution to the
reduced model comes back as a solution to the one the caller loaded.

**In scope:** a new `src/presolve.c` that builds a reduced problem from
`jaos_model`'s arrays without mutating them, the eight reduction families
`REQ-presolve` names — empty and singleton rows and columns, forcing and
redundant constraints, bound tightening, fixed variables, duplicate rows and
columns, dominated columns — and the postsolve that maps values, activities,
duals and reduced costs back into the caller's row and column indices.

**Out of scope:** anything inside the simplex loop. Phase 3 owns the
factorization and the solves that read it, phase 4 the search path. Presolve
does not touch `admit_candidate`, the pricing rules, or the re-entry loop.

**Where the correctness risk lives.** Success criterion 2 states it and the
roadmap repeats it: the risk is in postsolve, not in the reductions. A
reduction that fires wrongly produces a different model and the checker
catches it. A postsolve that maps an index wrongly produces a *plausible*
answer in the right shape, and the only thing standing between that and a
published wrong answer is `jaos_check_solution` reading the model as loaded.

**Phase 1's safety net does not carry over.** Phase 1 could require all 139
digests unmoved because its change was designed to be an observable no-op.
Presolve changes the model the simplex sees, so iteration counts and work
units move on every instance **by design**. What survives as evidence is the
verdicts, the objectives against Koch's reference, and the checker's
acceptance through postsolve. Plan the verification around those.

</domain>

<decisions>
## Implementation Decisions

### Reduction set, firing order, and how presolve is switched

- **D-01: The first plan is the scaffolding, not a reduction.** The reduced
  model, the postsolve stack and one trivially-correct reduction, proved end
  to end over all 139 instances with almost nothing removed. Every subsequent
  reduction then arrives with its own postsolve record and its own measured
  contribution. All eight families are in scope; this orders them so the
  postsolve risk is paid in instalments rather than in one diff, and so that
  adding the ninth reduction later costs almost nothing.
  — **Rationale:** the correctness machinery is the part that can be wrong in
  a way nothing catches. Proving it while the model is nearly unchanged is the
  only moment it can be proved cheaply.
- **D-02: Presolve iterates to a fixed point, under a measured round cap.** The
  cap is deterministic and is measured rather than chosen. Reductions cascade — removing a
  singleton column creates an empty row — so a single pass leaves most of the
  value uncollected. The cap follows the precedent already in the tree:
  `IMPLIED_ROUNDS = 64` (`src/check.c:264`), set by sweeping the standard set
  and recorded in `docs/tolerances.md` with the sweep that set it (D91). The
  sweep is a deliverable of this phase, not an assumption of it.
- **D-03: Presolve is switched off by a build-time `-D` constant.** It lives in
  `src/presolve.c` and never becomes a public API option. D64 draws the line
  at contract versus method, and `ARCHITECTURE.md` names "folding a method
  choice into a caller-facing option" as an anti-pattern with this exact
  shape. `Makefile:94-101` already carries `EXTRA_CFLAGS` for sweeps. The
  switch is also what makes the phase's own measurement possible at all.
- **D-04: Presolve runs before scaling, on the model as loaded.** The reduced
  model is then scaled fresh inside `sx_init`. Bound tightening compares
  magnitudes, so running it in scaled space makes the reductions depend on the
  scaling — and the scaling depends on a matrix that presolve has just
  changed. D27, D89 and D92 are the standing warning that the two tolerance
  spaces do not convert into each other.

### Where it lives, what it owns, and how the solution comes back

- **D-05: A new module, `src/presolve.c`.** Its prototypes go in
  `src/jaos_internal.h`. `src/simplex.c` is already 3,829 lines. There is no
  per-module header in this tree, so no include cycle is possible by
  construction.
- **D-06: Presolve builds a reduced problem and never mutates the model.**
  `jaos_model`'s authoritative CSC arrays are untouched. This is what keeps
  `src/check.c` reading the model as loaded (criterion 2, D18), and it is the
  same move `sx_init` already makes when it builds the scaled working copy.
  — **Rejected alternative:** mutate in place and restore afterwards. Any
  early return — work limit, time limit, `JAOS_ERR_NUMERICAL` — leaves the
  caller's model reduced, and the paths that return early are exactly the ones
  nobody exercises by hand.
- **D-07: The postsolve record is a tagged arena, replayed strictly LIFO.** It
  is append-only, each record carrying the original row or column index it
  restores. The replay order is then the only order there is, which is what
  makes it deterministic by construction rather than by discipline.
- **D-08: The postsolve stack is solve-local.** Built inside
  `jm_dual_simplex`, consumed before `publish` returns, freed with `sx`.
  `start_col_status` and `start_row_status` stay in **original** indices; the
  warm basis is mapped into reduced indices on the way in and back on the way
  out, so the caller's warm-start contract (D77–D79) does not change.
  — **Rejected alternative:** caching the stack on the model across solves.
  Any model edit invalidates it and nothing in the tree would be watching.

### What proves postsolve right

- **D-09: The negative control is presolve compiled off.** With the reductions
  out, all 139 digests must be bit-identical to the baselines committed today.
  This is cheap, and it is the shape D93 established: it separates "presolve
  broke something" from "presolve changed the trajectory", which is exactly
  the distinction phase 1's headline could not make.
- **D-10: One round-trip test per reduction family.** A small model where the
  reduction fires, solved with presolve on and off, requiring the same
  verdict, an objective within tolerance, and **the checker accepting the
  postsolved solution**. Each test is validated before it is believed: hand
  postsolve a deliberately broken index map and confirm the checker rejects
  it. A green test never shown to fail is not evidence.
- **D-11: The existing checker is the instrument and is not touched.** It
  already derives row activities independently in long double and checks dual
  sign conditions, complementary slackness and the primal-dual gap. The only
  new obligation is on postsolve: it must fill `sol_dual` and `sol_redcost` in
  original indices, because criterion 2 names all four of values, activities,
  duals and reduced costs.
  — **Rejected alternative:** adding checks to `src/check.c`. That would give
  the checker knowledge of the reduced model and destroy the structural
  independence that makes it an oracle.
- **D-12: Criterion 4 is already enforced by `bench/run.c`.** This was written
  down wrong the first time. The original D-12 said the runner
  "currently requires bit-identical digests" and must be extended to compare
  iterations and work units. It already compares them, on both paths:
  `bench/run.c:437-438` requires `jaos_iterations(m) == iters &&
  jaos_work_units(m) == work` on the infeasible path, and `:574-577` requires
  iterations, work units, a `memcmp` of the objective **and** digest equality
  on the optimal path — after `jaos_clear_basis`, exactly as criterion 4
  words it. Found by the phase researcher and confirmed against the source
  before this correction was written.
  **What D-12 actually asks for, therefore, is narrower:** whatever *new* path
  presolve introduces must be covered by the same check. The candidate is a
  presolve-only short-circuit — a model proved infeasible, or fully solved, by
  the reductions alone, returning before the simplex runs. That path publishes
  a result the existing double solve has never seen. The plan must say whether
  it exists, and if it does, that it is judged the same way.

### What is reported and what number closes the phase

- **D-13: Each reduction reports what it removed, via a per-family counter.**
  The counter struct lives in `src/jaos_internal.h`, is logged at
  `JAOS_LOG_SUMMARY` and is printed
  by `bench/run.c` into the record. No public API — that is scope, and it is
  D64. The tests are white-box and read the struct directly.
- **D-14: Presolve bills the same `jm_work` counter every other kernel bills.**
  A presolve that is not billed makes the solver look free, and D16 makes the
  work unit a public contract. — **Reversibility:** one-way, exactly as D-09
  was in phase 1. Every historical work figure changes meaning, so this needs
  its own `DECISIONS.md` entry and a deliberate rewrite of all three
  baselines — never as a side effect of running the gate.
- **D-15: The deliverable number is a geometric mean of per-instance ratios**
  (D46) over the standard set, presolve-on against presolve-off, at `J=1`,
  **with a negative control that is not optional**: the instances where
  presolve removes nothing must read 1.00x, and whatever they actually read is
  the noise floor this phase's figure must clear. Work-unit ratio and time
  ratio are reported separately, because presolve trades setup work for
  iteration work and a single number hides which way the trade went.
- **D-16: This phase does not recalibrate the comparison ladder.** It records
  the rule and hands the `make compare` re-run to phase 5. D17 says a number
  taken under WSL is a development number and cannot close a gate; the on/off
  ratio this phase produces is a development number and is honest as one.

### Two plan tasks that are not optional

The roadmap and `CLAUDE.md` both require these, and **no GSD workflow spawns
either agent** — they run only because a plan says so. Phase 1's plans did not
say so, and its two most valuable findings still came from these two agents,
both arriving after its own gates had passed.

- **`numerics-reviewer` on the postsolve diff**, after the code lands and
  *before* the campaigns run. A campaign is only valid for the tree that
  produced it, so a finding after the campaign costs the campaign.
- **`jaos-measurer` as the verdict step**, so the figures are judged by a
  context that did not produce them.

### Claude's Discretion

Area 1 was resolved at Claude's discretion at the user's request, aimed at the
best achievable presolve rather than the safest one. D-01 through D-04 are
that resolution. Beyond them:

- The internal representation of the reduced model and of the index maps.
- Which reduction is the "trivially correct" one D-01 ships first.
- Whether the round-trip tests live in a new `tests/test_presolve.c` or extend
  an existing file.
- The order the remaining seven families are added in, subject to D-13 making
  that order derivable from measurement rather than assumed.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### The decision record — authoritative, locked
- `DECISIONS.md` — D1 through D93, all locked. For this phase specifically:
  **D18** (the checker reads the model as loaded and never the reduced one),
  **D81** (presolve's measured worth — 1.417x HiGHS, 1.136x SoPlex, readings
  of two competitors and *not* targets for JAOS), **D16** (the work unit is a
  public contract — the reason D-14 is one-way), **D45** (how a change is
  judged), **D46** (geometric mean of per-instance ratios), **D17** (no claim
  without a run; WSL cannot publish), **D64** (contract versus method — the
  reason D-03 is a build flag), **D77–D79** (the warm-start contract D-08 must
  not change), **D27/D89/D92** (the two tolerance spaces), **D8/D34**
  (determinism), **D91** (`IMPLIED_ROUNDS` — the precedent D-02 follows),
  **D93** (the negative control D-09 imitates).

### The gate, the comparison and how cost is counted
- `bench/README.md` — the acceptance gate: three sets, four per-instance
  predicates, all-or-nothing, plus the per-instance baseline diff.
- `bench/compare/README.md` — the ladder. **Lines 67-72 answer the open
  question the roadmap says nobody answers.** See Specific Ideas below.
- `docs/work-units.md` — what a work unit is; D-14 adds to it.
- `docs/tolerances.md` — the frozen set (D31), and the `IMPLIED_ROUNDS` entry
  that shows what a measured round cap looks like written up.

### The code
- `src/check.c` — the checker. Read it to understand what postsolve must
  produce; do not modify it, and do not give it an include chain to
  `scale.c`, `lu.c` or `simplex.c`.
- `src/check.c:264,331` — `IMPLIED_ROUNDS` and its propagation loop, the
  in-tree precedent for a capped fixed-point iteration.
- `src/model.c` — `jaos_load_lp`, the CSC arrays, `model_matrix_is_stale`,
  the CSR mirror. Presolve reads these and writes none of them.
- `src/simplex.c` — `sx_init` (the scaled working copy D-06 imitates),
  `build_warm_basis` (the mapping D-08 must wrap), `publish` (where postsolve
  runs before). **Grep for the symbol; the line numbers in
  `.planning/codebase/ARCHITECTURE.md` are stale by construction.**
- `bench/run.c` — the double solve D-12 extends, and the record D-13 prints
  into.
- `.planning/codebase/ARCHITECTURE.md`, `CONVENTIONS.md`, `TESTING.md` — the
  tree as mapped. Do not modify. Its own header lists which line numbers no
  longer land.

### Milestone framing
- `.planning/PROJECT.md` — constraints, and the locked-decision table naming
  what `REQ-presolve` inherits.
- `.planning/REQUIREMENTS.md` — `REQ-presolve`'s full text, and its acceptance
  recorded as **absent**: no numeric target is stated for JAOS's own presolve.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`sx_init` already builds a working copy of the problem** from the model's
  arrays without mutating them. D-06 is the same pattern one layer up, and the
  precedent means the reviewer has something to compare against.
- **The checker needs nothing added.** It derives activities by a column-order
  CSC scatter-add in long double and reports magnitudes rather than a boolean,
  so it already tells postsolve *how* wrong rather than only *whether*.
- **`bench/run.c` already solves twice and already enforces criterion 4 in
  full** — status, iterations, work units, the objective's bits and the digest,
  after `jaos_clear_basis`. See the corrected D-12: the work is covering the
  new path presolve may introduce, not rebuilding the check.
- **`jm_work` is a single accounting entry point**, so D-14 is a bounded set
  of call sites.
- **`jm_alloc_array`/`jm_calloc_array`** are the only allocation paths in the
  tree; the postsolve arena goes through them.

### Established Patterns
- Every `src/*.c` includes only `jaos_internal.h` plus standard headers. There
  is no per-module header, which is why D-05 cannot create a cycle.
- Method constants are `constexpr`/`#define` in the module that owns them,
  tunable only through `EXTRA_CFLAGS`/`-D`. D-03 follows this exactly.
- No global state anywhere in `src/`. The postsolve stack is not an exception
  to that — D-08 keeps it on the stack-local solve state.
- Input validation is concentrated at the boundary, and **trivially infeasible
  data is deliberately not rejected there** — deciding feasibility is the
  solver's job. A presolve that detects infeasibility must therefore report it
  as a solve outcome, not as a load error.

### Integration Points
- `jm_dual_simplex` — presolve runs at its top, before `sx_init`'s scaling
  call; postsolve runs before `publish` returns.
- `build_warm_basis` — the index mapping D-08 wraps in both directions.
- `bench/results/*.txt` — the three work baselines, rewritten deliberately
  under D-14 with their own entry.
- `docs/work-units.md` — gains presolve's billing under D-14.

</code_context>

<specifics>
## Specific Ideas

- **The roadmap's open question is already answered on disk, and the answer is
  not the one the roadmap assumes.** `ROADMAP.md` and `STATE.md` both say
  nothing states how the comparison ladder is recalibrated once JAOS has a
  presolve. `bench/compare/README.md:67-72` states it outright: *"When JAOS
  gains a presolve, presolve moves into T0 for everyone and a new rung appears
  above"*, and adds that records carry the tier definition they were taken
  under, because "a comparison against a differently-defined rung is not a
  comparison". This should be corrected in the roadmap and in `STATE.md`
  rather than re-derived in phase 5.

- **A presolve does not close the gap it is being built to close, and the
  planner should not write as if it does.** D83 decomposes the 3.72x against
  HiGHS into 1.47x iterations and **2.54x per iteration**. Presolve reduces
  the model; it does not make an iteration cheaper. Combined with the
  recalibration rule above — presolve moves into T0 for everyone — the honest
  statement is that this phase changes what is being compared rather than
  where JAOS stands in the comparison. The per-iteration factor is phases 3
  and 4.

- **SoPlex is the reachable competitor, not HiGHS.** At 1.34x it is the
  closest of the three, and it is the only one JAOS already beats on iteration
  count (0.70x) while losing on per-iteration cost (1.92x). SCIP is not in the
  comparison set; its LP solver is SoPlex.

- **The acceptance for `REQ-presolve` is recorded as absent, and that is
  deliberate.** D81's 1.417x and 1.136x are readings of two competitors. Using
  either as JAOS's target would be fitting a number taken on someone else's
  code, which is the failure mode this project's first rule exists to prevent.
  Criterion 3 asks for a measured number *for JAOS*, and D-15 is how it is
  taken.

- **The round-trip tests must be shown to fail before their passing counts.**
  Feed postsolve an index map that is wrong by one and confirm the checker
  rejects the result. This is `jaos-testing`'s standing rule and the reason
  D-10 is written as two obligations rather than one.

</specifics>

<deferred>
## Deferred Ideas

- **Re-running `make compare` and rebuilding the four rungs** under the
  recalibrated ladder. It is phase 5's work by D-16 and by D17 — the ladder is
  a published figure and this host cannot produce one.

- **Presolve reductions that need the dual, or that interact with the basis**
  — anything in the family of dual fixing or reduced-cost fixing. They need a
  solved relaxation to fire and belong to a later decision, not to a first
  presolve.

- **A trajectory sweep over `REFACTOR_EVERY` with presolve on.** Three of the
  previous milestone's four defect closures came from sweeping 16..256, which
  no target automates and the gate never varies. Presolve changes the model
  the factorization sees, so it is a candidate. Raised, not scheduled — the
  roadmap's Open Question 5.

- **The two carried defects D93 handed on**, both two-line changes deliberately
  not made mid-measurement: nothing reads the `baseline: NOT COMPARED` line in
  `preflight.sh`, and `bench/run.c` prints seconds as `%8.3f` so 8 standard-set
  instances carry no time ratio and 42 more read exactly 1.0000x. **The second
  one directly degrades D-15's time ratio** and is worth fixing before this
  phase's campaigns rather than after — but as its own commit, before any
  measurement starts, never mid-campaign.

</deferred>

---

*Phase: 2-Presolve and postsolve*
*Context gathered: 2026-08-12*
