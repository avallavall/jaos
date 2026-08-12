# Phase 1: Candidate admission in the ratio test - Context

**Gathered:** 2026-08-12
**Status:** Ready for planning

<domain>
## Phase Boundary

How the dual ratio test decides which nonbasic variables may enter — the sign
and magnitude filter in `admit_candidate` (`src/simplex.c:1527`) and the scan
that surrounds it in `dual_ratio_test` (`src/simplex.c:1557`).

**In scope:** the dense scan taken when the pricing row has no pattern
(`s->anpat < 0`), which visits all `s->nvar` variables to reject nearly all of
them. This is the O(`nvar`) half of D61's 36.5% and 14.98% of instructions on
`truss` under callgrind, against `ftran_prefix` at 6.68%.

**Out of scope:** the choice *among* candidates (`jm_harris_pick`), how far the
step reaches (`bfrt_walk`), and the pattern scan taken when `s->anpat >= 0`.
Widening the hyper-sparse path so the pattern exists more often was considered
and belongs to Phase 3 (`REQ-hyper-sparse-downstream-results`), not here.

**The phase's first deliverable is a closed decision, not code.** `PLAN.md`
states this item "needs its own decision before any code", and the roadmap's
success criterion 1 requires that decision to exist before any implementation
commit does. It will be `DECISIONS.md`'s next entry — D93.

</domain>

<decisions>
## Implementation Decisions

### What is being changed

- **D-01:** The target is the **dense scan**, not the admission rule. A list of
  nonbasic variables replaces the walk over all `nvar`, so basics are never
  visited to be rejected. — **Reversibility:** costly — undoing it means
  reverting both the scan and the maintenance hook at the pivot site; the
  scan itself is one loop, the list's lifecycle is not.
- **D-02:** **The admission rule itself does not change.** Same candidates, same
  array positions, same order. `admit_candidate`'s body — the `JM_BASIC` test,
  `PIVOT_MIN`, the per-status sign test, the clamped numerator — is untouched.
  This is what keeps Harris's two-pass guarantee out of the blast radius, and
  it is the reason this phase is not the refused half of D82/D84.
- **D-03:** The list is **maintained incrementally at the pivot** — one variable
  enters and one leaves per iteration, so maintenance is O(1) amortised.
  Ascending order must be preserved by construction, not restored by sorting.
- **D-04:** If the measurement says it does not pay, the phase closes with a
  **refusal written up like D82 and D84** — a `DECISIONS.md` entry stating that
  it does not pay and what it cost to find out. The phase does not roll on to
  the next candidate path. A closed refusal is a valid outcome.

### Why the order is load-bearing

`src/simplex.c:1562-1568` states it outright: the pattern scan and the dense
scan admit the same candidates *in the same array positions*, and `bfrt_walk`,
`jm_harris_pick` and `apply_flips` each break an exact tie by whichever
candidate they meet first. **Any other order is a different trajectory.** The
list must therefore be ascending in `v`, exactly as `for (v = 0; v < nvar; v++)`
produced.

### Verification

- **D-05:** All **139 solution digests must be identical** across the three
  instance sets. This is a hard acceptance criterion, not a hope: the change is
  designed to be an observable no-op, so a moved digest means it touched
  something it should not have.
- **D-06:** The digests are what **authorise rewriting the work baseline**.
  Order matters — prove the trajectory did not move first, then rewrite. Doing
  it the other way round fills the per-instance diff with noise and destroys the
  only thing that can report a regression at all.
- **D-07:** A **differential-equivalence test** runs the old scan and the new
  one over the same solver state and requires an identical candidate set *and*
  identical array positions. The instrument is validated before it is believed:
  hand it a list with a candidate missing, and confirm it fails. A green test
  that was never shown to reject is not evidence.
- **D-08:** The equivalence is also asserted **at run time in debug builds**,
  once per iteration, costing nothing in release. D30 was caused by violating a
  contract that was documented correctly and prominently in the very function it
  protected — a contract that matters does not stay a sentence.

### Work units

- **D-09:** The counter charges **what is actually visited**, not `nvar`.
  `simplex.c:1576` currently charges `s->nvar * JM_WORK_NONZERO` for the dense
  scan; once basics are no longer visited, that number stops describing the work
  done and the improvement would be invisible in the project's own currency.
  — **Reversibility:** one-way — D16 makes the work unit a public contract, so
  figures taken before and after are not comparable. Every historical work
  number for the dense scan becomes a number from a different definition.
- **D-10:** The work baselines for all three campaigns are rewritten as a
  **deliberate step of this phase with its own entry**, never as a side effect
  of running the gate.

### Evidence that closes D93

- **D-11:** **A same-instance time ratio at `J=1` gives the verdict; callgrind
  explains it.** This is D45 as written. Callgrind cannot give the verdict here
  because it cannot see locality, and locality is exactly where trading a dense
  sequential walk for an indirect one is won or lost. `perf` is not installed on
  the development machine.
- **D-12:** Measured over the **standard set as a geometric mean of per-instance
  ratios** (D46), never a sum — two instances are 74% of that set's total.
  `truss` is reported separately because it is where the cost was found, but it
  does not decide alone.
- **D-13:** The result is conclusive at **4.2% or better — three times the
  harness's measured repeatability of 1.4%**. Below that the phase reports
  INCONCLUSIVE and does not close with a yes. The threshold is derived from an
  existing measurement rather than chosen by eye.

### Claude's Discretion

- Where the nonbasic list lives in `sx` and how it is represented.
- The exact form of the debug assertion, provided it compares both scans and
  compiles out of release builds.
- Whether the differential test lives in `tests/test_simplex.c` or its own file.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### The decision record — authoritative, locked
- `DECISIONS.md` — 92 closed entries, all locked. Specifically for this phase:
  **D82** (partial pricing on the leaving-row sweep, refused on correctness —
  `pilot` publishes an out-of-tolerance OPTIMAL), **D84** (multiple pricing,
  refused; its profile is what points at `admit_candidate`), **D61** (the 36.5%
  this is the O(`nvar`) half of), **D45** (how a change is judged), **D46**
  (geometric mean; `PIVOT_SEARCH_LIMIT` stays 4), **D16** (the work unit is a
  public contract), **D17** (no claim without a run; WSL cannot publish),
  **D30** (a documented contract was violated anyway — the reason for D-08),
  **D8/D34** (determinism).
- `docs/archive/PLAN.md` — archived. Phase 6 item 3a is this requirement's
  source text, including "needs its own decision before any code". Read only for
  that; the roadmap supersedes it and contradicts it deliberately in three
  places, listed in its own header.

### The gate and how cost is counted
- `bench/README.md` — the acceptance gate: three sets, four per-instance
  predicates, all-or-nothing, plus the per-instance baseline diff.
- `docs/work-units.md` — what a work unit is and what the weights mean.
- `docs/tolerances.md` — the frozen tolerance set (D31). Nothing in this phase
  changes a tolerance.

### The code
- `src/simplex.c:1503-1610` — the ratio test: the comment block that states the
  eligibility rule, `admit_candidate`, and `dual_ratio_test` with both scans.
- `src/simplex.c:1562-1568` — the ordering contract this phase must not break.
- `src/simplex.c:1675-1704` — `jm_harris_pick` and the window it trades.
- `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/CONVENTIONS.md`,
  `.planning/codebase/TESTING.md` — the tree as mapped. Do not modify.

### Milestone framing
- `.planning/PROJECT.md` — constraints, and the locked-decision table naming
  what each M2 requirement inherits.
- `.planning/INGEST-CONFLICTS.md` — why this phase precedes presolve.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `admit_candidate` is already factored out precisely because the scan around it
  comes in two forms — the source comment says "a rule this delicate must not be
  written twice". The new scan reuses it unchanged; that is the point.
- `jm_work_add` is the single accounting entry point, so D-09 is one call site.
- The Unity test suite (`tests/test_simplex.c`) already exercises solver
  internals directly, so the differential test has a home and a precedent.

### Established Patterns
- The two scans must stay observationally identical. `s->apat` is ascending, and
  the dense loop is ascending in `v`; the replacement inherits that obligation.
- Free variables (`else` branch: `ok = true`, `dist = 0.0`) are admitted with a
  zero numerator — a list built by status must not accidentally drop them.
- `src/check.c` must never gain an include chain back to `simplex.c`. Nothing
  here goes near it, but the differential test must not become the thing that
  creates one.

### Integration Points
- The pivot site, wherever `status[v]` transitions to and from `JM_BASIC` — that
  is the one place the list is maintained, and the one place it can desynchronise.
- `bench/results/*.txt` — the work baselines, rewritten deliberately under D-10.

</code_context>

<specifics>
## Specific Ideas

- The threshold is not a preference: 4.2% is three times the 1.4% repeatability
  that has already been measured for this harness. It is a derived number, and
  D93 should show the derivation rather than assert the figure.
- The refusal path is a first-class outcome. D82 and D84 are the models — both
  refused a direction on correctness rather than on a trade, and both are cited
  since as settled ground.
- The differential test must be shown to fail on a deliberately broken list
  before its passing is treated as evidence.

</specifics>

<deferred>
## Deferred Ideas

- **Widening the hyper-sparse path** so `s->anpat >= 0` holds more often. This
  attacks the same cost at its root but is `REQ-hyper-sparse-downstream-results`
  — Phase 3. Considered and deliberately not folded in.
- **Restricting the candidate set** ahead of `bfrt_walk`/`jm_harris_pick`. The
  higher-ceiling, higher-risk path, and the only one that puts Harris's
  guarantees at stake. Not refused — not attempted in this phase. If the dense
  scan is measured and closed, this remains available as its own decision.
- **A trajectory sweep over `REFACTOR_EVERY`.** Three of last milestone's four
  defect closures came from sweeping 16..256, which the gate never varies and no
  target automates. This phase changes the pivot path, so it is a candidate for
  such a sweep — raised in the roadmap's Open Question 5, not scheduled here.

</deferred>

---

*Phase: 1-Candidate admission in the ratio test*
*Context gathered: 2026-08-12*
