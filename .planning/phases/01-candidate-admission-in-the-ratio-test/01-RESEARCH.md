# Phase 1: Candidate admission in the ratio test - Research

**Researched:** 2026-08-12
**Domain:** Revised dual simplex internals — sparse candidate-set maintenance in a hand-written C23 LP solver, no external dependencies
**Confidence:** MEDIUM — the code-level findings (call sites, struct layout, test-visibility) are [VERIFIED] against `src/simplex.c` and `src/jaos_internal.h` read this session; the data-structure choice for the list itself is a genuine open engineering question and is flagged [ASSUMED]/reasoned rather than settled by a citation, because none of the project's own bibliography addresses it directly.

## Summary

This phase replaces one loop, not one data structure question that turns out to be nontrivial. `dual_ratio_test`'s dense branch (`src/simplex.c:1573-1577`) calls `admit_candidate` once per variable, ascending in `v`, and `admit_candidate` itself is untouched (D-02). The work is entirely in producing, at zero cost beyond O(1) work per pivot, the same ascending sequence of nonbasic variable indices that `for (v = 0; v < nvar; v++)` currently produces — including every `JM_FREE` variable, which is nonbasic but has no bound in the `JM_AT_LOWER`/`JM_AT_UPPER` sense the rest of the solver reasons in.

The codebase already has a working precedent for "walk a sparse, order-sensitive subset of `[0, nvar)` fast" — `jm_pattern_order` (`src/simplex.c:1637-1673`), used for `apat`/`amark`. It is not directly reusable here because it *rebuilds* the pattern from a fresh scatter every iteration; this phase needs the reverse — a structure that is *never* rebuilt, only patched at exactly the two variables a pivot touches. That is a different problem (incremental maintenance vs. one-shot extraction), and the naive answer — a sorted doubly-linked list where a variable is "put back" using the neighbor pointers it had before it became basic — is unsound in general: those pointers can go stale while the variable was away, and reusing them silently drops whichever variable moved into the gap in the meantime. A concrete counterexample is worked through below (Common Pitfalls). The two research questions this raises — which structure to use, and how to test it given that `sx`, `admit_candidate` and `dual_ratio_test` are all file-private to `simplex.c` — are both left genuinely open, not resolved, because CONTEXT.md's own `<decisions>` block leaves representation to Claude's discretion and no closed decision in this repository already answers them.

Twenty call sites write `s->status[v]` across `src/simplex.c`; only two of them (inside `pivot()`) are the steady-state "one enters, one leaves" pivot D-03 describes. Four more (in `build_initial_basis`, `build_warm_basis`, `repair_singular_basis`) also change basis *membership* and must build or patch the list; three more (`apply_flips`, `repair_dual_infeasibility`, `arm_reentry`) only toggle `JM_AT_LOWER`/`JM_AT_UPPER` and never touch membership, so they need no maintenance at all. This is the exhaustive site table CONTEXT.md's research question 3 asked for.

**Primary recommendation:** build the list from `status[]` exactly once per membership-changing site (six of them, cold/warm start and singular repair, all rare) and patch it incrementally only at `pivot()`'s two writes (`src/simplex.c:2061,2064`) — this is the only site the dense scan's O(`nvar`) cost is actually paid at, every iteration. Whether the list itself is a bitmap walked with `__builtin_ctzll` (the pattern the codebase already uses and has already proven correct at `jm_pattern_order`) or an explicit sorted index array/linked list (which needs a real predecessor-maintenance argument, not an assumed one) is the one decision this research could not close and is carried into Open Questions.

## Architectural Responsibility Map

JAOS is a single-tier C library; the web/browser/API tiers this template assumes do not apply. The tiers below are this project's own, mapped from `.planning/codebase/ARCHITECTURE.md`'s module boundaries.

| Capability | Primary Tier | Secondary Tier | Rationale |
|---|---|---|---|
| Nonbasic candidate enumeration (the dense scan being replaced) | Solver core (`src/simplex.c`, `dual_ratio_test`) | — | Pure in-process state, no I/O |
| List/bitmap maintenance at the pivot | Solver core (`src/simplex.c`, `pivot()` and the five other membership sites) | — | Same translation unit as `status[]`, `basis[]`, `where[]` |
| Work-unit accounting of the new scan (D-09) | Solver core (`jm_work_add` call sites) | — | `jm_work` is solver-owned bookkeeping, not a separate tier |
| Differential-equivalence verification (D-07) | Test harness (`tests/`) | Solver core — needs a `jm_`-prefixed, non-`static` hook to reach into | `sx`, `admit_candidate`, `dual_ratio_test` are all file-private; see Common Pitfalls |
| Runtime debug-build cross-check (D-08) | Solver core (`src/simplex.c`, compiled out under `-DNDEBUG`) | — | Must live where `sx` is visible; cannot be a test-only artifact |
| Work-baseline rewrite (D-10) | Bench harness (`bench/results/*.txt`, `make netlib-baseline` family) | — | Separate command per `bench/README.md`, never a side effect of the gate |

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

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
- **D-05:** All **139 solution digests must be identical** across the three
  instance sets.
- **D-06:** The digests are what **authorise rewriting the work baseline**. Order matters.
- **D-07:** A **differential-equivalence test** runs the old scan and the new
  one over the same solver state and requires an identical candidate set *and*
  identical array positions. Must be shown to fail on a deliberately broken list.
- **D-08:** The equivalence is also asserted **at run time in debug builds**,
  once per iteration, costing nothing in release.
- **D-09:** The counter charges **what is actually visited**, not `nvar`.
- **D-10:** The work baselines for all three campaigns are rewritten as a
  **deliberate step of this phase with its own entry**.
- **D-11:** **A same-instance time ratio at `J=1` gives the verdict; callgrind explains it.**
- **D-12:** Measured over the **standard set as a geometric mean of per-instance ratios** (D46), `truss` reported separately but does not decide alone.
- **D-13:** The result is conclusive at **4.2% or better** — three times the harness's measured repeatability of 1.4%. Below that: INCONCLUSIVE.

### Claude's Discretion

- Where the nonbasic list lives in `sx` and how it is represented.
- The exact form of the debug assertion, provided it compares both scans and
  compiles out of release builds.
- Whether the differential test lives in `tests/test_simplex.c` or its own file.

### Deferred Ideas (OUT OF SCOPE)

- **Widening the hyper-sparse path** so `s->anpat >= 0` holds more often — Phase 3 (`REQ-hyper-sparse-downstream-results`).
- **Restricting the candidate set** ahead of `bfrt_walk`/`jm_harris_pick` — not refused, not attempted here.
- **A trajectory sweep over `REFACTOR_EVERY`** — raised in roadmap Open Question 5, not scheduled here.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| REQ-ratio-test-candidate-admission | `admit_candidate` runs once per nonbasic variable of the pricing row and is 14.98% of instructions on `truss` under callgrind (D84's closing paragraph, `src/simplex.c:1527-1610`) — the O(`nvar`) half of D61's 36.5%, requiring its own decision (D93) before any code. | Site table (Architecture Patterns) gives the exhaustive set of places `status[v]` changes, so the planner knows every place the list must be built or patched. Common Pitfalls documents the two failure modes CONTEXT.md's research questions 1-2 asked about (stale-pointer reinsertion, bound flips vs. membership). Don't Hand-Roll section covers the order-maintenance data-structure question the "O(1) amortised" requirement (D-03) actually implies. Validation Architecture maps D-07/D-08's test requirements onto the codebase's existing Unity suite and flags the visibility gap that blocks a naive implementation of D-07. |
</phase_requirements>

## Standard Stack

Not applicable. D2/D11 forbid external dependencies of any kind, and this phase adds none — it is a change to one C translation unit (`src/simplex.c`) plus its test file(s). No `npm view`/`pip index`/`cargo search` equivalent exists for this ecosystem; the project's own `docs/archive/PLAN.md` bibliography (verified citations, D12) is the closest analogue and is used throughout this document instead.

## Package Legitimacy Audit

Not applicable — this phase installs no external packages. Skipped per the Package Legitimacy Gate protocol's own trigger condition ("whenever this phase installs external packages").

## Architecture Patterns

### Data flow through the ratio test today

```
price_and_select(s, r, below, violation, theta_dual)
        |
        v
dual_ratio_test(s, below, violation, theta_out)
        |
        +-- s->anpat >= 0 (pattern known)?
        |        |
        |        yes -> for t in 0..anpat: admit_candidate(s, s->apat[t], ...)
        |        |
        |        no  -> for v in 0..nvar:   admit_candidate(s, v, ...)   <-- THIS is the O(nvar) target
        |
        v
   n candidates in s->cand[0..n), ascending in v, with s->rnum/rden/rrange
        |
        +-- s->bland?  -> jm_bland_pick (exact min ratio, lowest index)
        |
        +-- else       -> bfrt_walk (how far)  -> jm_harris_pick (which, among live)
                                                        |
                                                        v
                                                apply_flips(s, live, n)  (bound-to-bound swaps of the retired candidates)
```

The dense branch is the only one this phase touches (`src/simplex.c:1573-1577`); the pattern branch, `bfrt_walk`, `jm_harris_pick`, and `apply_flips`'s own logic are out of scope (CONTEXT.md `<domain>`).

### Site table: every place `s->status[v]` is assigned — **INCOMPLETE, SHORT BY TWO**

> **CORRECTION (added after phase 1 closed).** This table was labelled
> "exhaustive" and `[VERIFIED]` and it is **neither**. It lists six
> membership-changing sites. There are **eight**. The two it misses —
> `take_best_if_better` (`src/simplex.c:2631`) and `restore_settled` (`:2656`) —
> change membership by `memcpy` and carry no assignment form at all, so the
> grep this table was compiled from could not see them. `01-PATTERNS.md`
> independently reproduced the same six; the planner caught it by reading the
> code paths instead (`8ef1e70`, `01-01-PLAN.md:132-164`).
>
> Hooking only these six would have desynchronised the bitmap on every
> `take_best_if_better`/`restore_settled` path and changed published answers
> with no test able to see it. Kept as written, with this warning, because the
> failure is the useful part: **a grep over assignment forms cannot enumerate
> membership changes in C, and a table that says `[VERIFIED]` is trusted.**

Compiled from a `Grep` over `src/simplex.c` for assignment (not comparison) forms, then read in context — this is CONTEXT.md's research question 3, "the single most useful concrete output you can produce." Line numbers as read that session.

| Line(s) | Function | Transition | Changes list membership? | Frequency |
|---|---|---|---|---|
| 720 | `build_initial_basis` | `-> JM_BASIC` (logicals of the slack basis) | Yes — builds the list from scratch | Once per cold-started solve |
| 740, 746, 748, 750, 752 | `build_initial_basis` | `-> JM_AT_LOWER` / `JM_AT_UPPER` / `JM_FREE` (structurals, by cost sign and finite bounds) | Yes — same build | Once per cold-started solve |
| 840 | `build_warm_basis` | `-> JM_BASIC` | Yes — builds the list from scratch (alternate path) | Once per warm-started solve |
| 851, 853, 855, 857 | `build_warm_basis` | `-> JM_AT_UPPER` / `JM_AT_LOWER` / `JM_AT_UPPER` / `JM_FREE`, by stored side then fallback | Yes — same build | Once per warm-started solve |
| 1155, 1157, 1159 | `repair_singular_basis` | evicted `leaving` `-> JM_AT_LOWER` / `JM_AT_UPPER` / `JM_FREE` | Yes — insert | Rare: only when `jm_lu_factor` finds `rank < dim` |
| 1163 | `repair_singular_basis` | logical `entering` `-> JM_BASIC` | Yes — remove | Same rare path, paired with the line above, once per repaired row, in a loop |
| 1470 | `apply_flips` | `JM_AT_LOWER <-> JM_AT_UPPER` (retired ratio-test candidates) | **No** — never touches `JM_BASIC` | Every iteration that has retired candidates (`live < n`) |
| 2061 | `pivot` | `leaving -> JM_AT_LOWER`/`JM_AT_UPPER` | Yes — insert | **Every successful pivot** |
| 2064 | `pivot` | `q -> JM_BASIC` | Yes — remove | **Every successful pivot** |
| 2164 | `repair_dual_infeasibility` | `JM_AT_LOWER <-> JM_AT_UPPER` (dual repair swap) | **No** | Only on the settle-up pass, and only for a variable `repair_dual_infeasibility` finds safe to swap |
| 2586 | `arm_reentry` | `JM_AT_LOWER <-> JM_AT_UPPER` (re-entry loop) | **No** | Only at the start of a re-entry round |

Twenty assignment sites total. Six change list membership (the six rows marked "Yes"); of those, four run once per solve (cold/warm start) or on a rare error-repair path, and can simply rebuild or splice-in without needing to be fast. **Exactly one pair — `src/simplex.c:2061` and `2064` inside `pivot()` — runs every iteration** and is the site D-03's "one variable enters and one leaves per iteration, so maintenance is O(1) amortised" describes. The three "No" rows confirm CONTEXT.md's research question 2 directly: `apply_flips` (`src/simplex.c:1460-1501`), `repair_dual_infeasibility` (`src/simplex.c:2115-2167`) and `arm_reentry` (`src/simplex.c:2582-2604`) each toggle only between the two bound-status values and never assign `JM_BASIC` or read/write `s->basis`/`s->where` — a list keyed on "is this variable currently basic" is unaffected by any of the three, and needs no hook at any of them.

### Free variables cannot be dropped by construction, not by discipline

Research question 5. `admit_candidate` (`src/simplex.c:1527-1555`) reads:

```c
if (s->status[v] == JM_BASIC)
    return;
...
if (s->status[v] == JM_AT_LOWER) {
    ...
} else if (s->status[v] == JM_AT_UPPER) {
    ...
} else {
    ok = true;               /* free: may move either way */
    dist = 0.0;              /* and must stay at zero */
}
```
`[VERIFIED: src/simplex.c:1529-1546]`

`jm_var_status` (`src/jaos_internal.h:150-155`) is a four-value enum — `JM_BASIC = 0, JM_AT_LOWER, JM_AT_UPPER, JM_FREE`. Since the `JM_BASIC` check has already returned, the `else` branch above is reached only when status is `JM_FREE` — free variables are exactly the ones `admit_candidate`'s bound checks never see, and it admits them unconditionally. All three places a `JM_FREE` variable is created are membership-changing sites already in the table above: `build_initial_basis:752` (zero cost, no finite bound), `build_warm_basis:857` (no stored side, neither bound finite), `repair_singular_basis:1159` (evicted variable, neither bound finite) — `[VERIFIED: src/simplex.c:752,857,1159]`. The correct invariant for whatever structure holds the list is therefore **"contains exactly `{v : status[v] != JM_BASIC}`"**, checked by transition (did this write leave `JM_BASIC`, enter it, or neither), never by which of the three non-basic statuses resulted. A maintenance routine written against "insert if the variable now has a finite bound" instead of "insert if the variable is no longer basic" silently drops every `JM_FREE` variable it creates — exactly the risk CONTEXT.md names.

### Harris's two-pass guarantee, and why this phase cannot touch it

Research question 4. Bibliography entry [7] — **P.M.J. Harris, "Pivot Selection Methods of the Devex LP Code," Mathematical Programming 5:1-28, 1973** — is already a checked citation in this project's own `docs/archive/PLAN.md:969-970` (D12: "verified citations — each checked against its publisher or archive before entering this list"). `jm_harris_pick` (`src/simplex.c:1678-1700`) implements it directly:

```c
double window = HUGE_VAL;
for (int64_t k = 0; k < n; k++) {
    double t = (num[k] + dual_tol) / den[k];
    if (t < window) window = t;
}
int64_t best = 0; double best_den = 0.0;
for (int64_t k = 0; k < n; k++) {
    if (num[k] / den[k] <= window && den[k] > best_den) {
        best_den = den[k]; best = k;
    }
}
```
`[VERIFIED: src/simplex.c:1684-1698]`

The guarantee: pass one computes a *window* — the minimum ratio plus a tolerance (`dual_tol`) — over the whole candidate set; pass two selects, among candidates whose ratio falls inside that window, the one with the **largest pivot magnitude** (`den`), not necessarily the exact minimum-ratio one. This trades a bounded amount of infeasibility for numerical stability: the file's own header comment states the bound explicitly — "every other candidate inside the window ends at worst `DUAL_TOL` past feasible, which is the whole of what Harris trades away" (`src/simplex.c:1605-1607`, `[VERIFIED]`).

This guarantee is a pure function of the **set** `{(num[k], den[k], var[k])}` and nothing else — it does not know or care how that set was assembled. D-02 keeps `admit_candidate`'s body byte-identical, and D-01/D-03 require the replacement scan to produce the *same* candidates in the *same* array positions as the dense loop does today. Given both, `jm_harris_pick` (and `jm_bland_pick`, and `apply_flips`'s tie-breaking, which the file's own comment says "break an exact tie by whichever candidate they meet first," `src/simplex.c:1566-1568`, `[VERIFIED]`) receive an input array that is bit-for-bit what they receive today — so the window, the tie-break, and therefore the trajectory are mechanically unaffected. This is exactly what CONTEXT.md's "Why the order is load-bearing" section states and what D-05's 139-digest requirement exists to confirm empirically.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---|---|---|---|
| "Walk a sparse, ascending subset of `[0, nvar)` fast" | A bespoke ad hoc index-shuffling scheme | The codebase's own `jm_pattern_order` (`src/simplex.c:1637-1673`) bitmap-plus-`__builtin_ctzll` pattern, already proven correct and already exercised by `apat`/`amark` and `rpat`/`rmark` | It is the one sparse-ascending-walk mechanism this project has already built, measured, and shipped (D40, D41, D45 cite its cost model in `docs/work-units.md`). Re-deriving a second one for this phase, differently, is the kind of duplicated delicate logic the codebase's own comment convention explicitly warns against ("a rule this delicate must not be written twice," `src/simplex.c:1524-1526`). |
| "Sorted-set insert/delete with O(1) neighbor queries" | A stale-pointer doubly linked list (see Common Pitfalls for why it is wrong) | Either (a) a bitmap walked with `__builtin_ctzll`/`__builtin_clzll`, which needs no neighbor bookkeeping at all — a status transition is a single bit flip — or (b) if an explicit index array/linked list is preferred, the general "order maintenance problem" literature (Dietz & Sleator, "Two Algorithms for Maintaining Order in a List," and its simplification by Bender, Cole, Demaine, Farach-Colton & Zito, "Two Simplified Algorithms for Maintaining Order in a List," ESA 2002) — `[CITED: web search this session, general CS literature, not a solver's source, so D12 does not exclude it]` | Full order-maintenance (arbitrary insert, delete, and "does u precede v" queries) needs a genuine two-level block structure to get O(1) amortised — see Common Pitfalls for the concrete failure of the naive version. This project's own `jm_pattern_order` already sidesteps the whole class of problem by never doing incremental sorted insertion at all. |
| Any GMP-style exact rational bookkeeping for this phase | — | — | Not relevant here; noted only because D11/Q8 excludes it project-wide and this phase does not touch it. |

**Key insight:** the phase description frames this as "replace a loop," but D-03's own wording — "ascending order must be preserved by construction, not restored by sorting" — is a data-structure requirement (sorted-set maintenance under single-element insert/delete from a fixed universe), not merely a refactor. The codebase already has the *right tool* for the ascending-walk half of this problem (`jm_pattern_order`'s bitmap technique) sitting one function above the code this phase touches; the risk is reaching for a hand-rolled linked list instead because "a list of nonbasic variables" reads like an obviously-linked-list problem, when it is not obviously a *correct* one.

## Common Pitfalls

### Pitfall 1: reinserting a variable using its own stale neighbor pointers is unsound

**What goes wrong:** a doubly linked list over `[0, nvar)`, deleting a variable by splicing its live neighbors together (correct, O(1)) but *reinserting* a later-returning variable by trusting the `prev`/`next` values it had the last time it was in the list, silently corrupts the list.

**Why it happens:** those stored neighbors can go stale. Concretely: variable `p` (say index 12) is nonbasic with active neighbors `L=8` and `R=20` (nothing active strictly between them), then `p` pivots into the basis and is spliced out, leaving its own `prev=8, next=20` fields untouched. While `p` is basic, some other variable `w=14` (with `8 < 14 < 12`... take `w=10` for a clean case, `8 < 10 < 12`) leaves the basis and is correctly inserted between `8` and `20`, giving `8 <-> 10 <-> 20`. Later, `p=12` returns to nonbasic and is reinserted using its *stored* `prev=8, next=20`: `next[8]` is overwritten from `10` back to `12`, and `p`'s own `next` still points at `20` — the list is now `8 <-> 12 <-> 20`, and `10` has been spliced out of the structure entirely even though it is still genuinely nonbasic. The candidate scan silently drops it from then on.

**How to avoid:** either (a) don't maintain neighbor pointers per element at all — use the bitmap-plus-`ctzll` approach, where a status transition is one bit flip and the scan derives order from bit position, so there is no "stale pointer" to go wrong; or (b) if a linked structure is preferred, use a real order-maintenance construction (see Don't Hand-Roll) rather than reusing a variable's pre-eviction neighbors, and prove — not assume — that reinsertion always finds the *current* correct neighbors.

**Warning signs:** the differential-equivalence test (D-07) is exactly the instrument that would catch this class of defect, provided its constructed failure case (required by CONTEXT.md's `<specifics>`) specifically exercises "evict A, insert something else into A's old gap, then re-evict-and-reinsert A" rather than only single isolated pivots — a test that only ever does one clean enter/leave pair at a time will not exercise the interleaving that breaks the stale-pointer version.

### Pitfall 2: the differential test's home is not obvious — `sx`, `admit_candidate` and `dual_ratio_test` are all file-private

**What goes wrong:** D-07 asks for a test that "runs the old scan and the new one over the same solver state." `sx` is a `typedef struct { ... } sx;` defined *inside* `src/simplex.c` at line 283 and never declared in `src/jaos_internal.h` — confirmed by `Grep`ing the header for `struct sx`/`typedef struct.*sx` and finding nothing (`[VERIFIED: src/jaos_internal.h]`, only `jm_dual_simplex(jaos_model *m)` is declared there, `jaos_internal.h:163`). `admit_candidate` and `dual_ratio_test` are both `static` (`src/simplex.c:1527,1557`). Test binaries link `tests/test_*.c` against the *compiled* `.o` files of `src/*.c` (`Makefile:167`, `$(DEV_OBJ)`), not against `simplex.c`'s source text, so a `static` symbol and an undeclared type are both genuinely unreachable from `tests/test_simplex.c` as the file is built today — there is no way to construct an `sx`, let alone call `admit_candidate` or `dual_ratio_test` on one, from outside `src/simplex.c`.

**Why it happens:** this project's own established pattern for "a delicate rule split out because the scan around it comes in two forms" (`admit_candidate`'s own header comment, `src/simplex.c:1523-1526`) is precisely why it stayed `static` and `sx`-taking — it was factored for *internal* reuse (dense scan and pattern scan both call it), not for external testability. Contrast with `jm_bland_pick`, `jm_harris_pick`, `jm_pattern_order` and `jm_dse_update` — all four are non-`static`, `jm_`-prefixed, take **plain arrays** rather than `sx *`, are declared in `src/jaos_internal.h` (`jaos_internal.h:204,234,253,284`, `[VERIFIED]`), and are exactly what `tests/test_simplex.c` unit-tests directly today (`tests/test_simplex.c:1184-1366`, `[VERIFIED]` — e.g. `TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(2, num, den, HARRIS_TOL));`). That is the established, working precedent for "make a piece of `dual_ratio_test`'s machinery independently testable" in this codebase.

**How to avoid:** whatever holds the new list/bitmap, its *maintenance primitives* (insert-on-leave, remove-on-enter, and the equivalence check itself) are good candidates to split out the same way — as small, non-`static`, `jm_`-prefixed functions taking plain arrays (a `status`-like array or bitmap, `nvar`, the variable index) rather than `sx *`. Built that way, they are unit-testable in isolation exactly like `jm_pattern_order` already is, without needing a real solved LP or access to the private `sx` type. The *comparison* against a real dense scan over live solver state (which does need `sx`) is a different, coarser check, and is naturally where D-08's runtime debug-build assertion belongs instead — inside `simplex.c`, where `sx` is visible, run once per iteration, compiled out under `-DNDEBUG`.

**Warning signs:** a plan that puts the whole differential-equivalence test in `tests/test_simplex.c` without first deciding how it reaches `admit_candidate`/`dual_ratio_test`/`sx` will stall at compile time, not at review time.

### Pitfall 3: `-DNDEBUG` compiles out release builds project-wide, but nothing in `src/` uses it yet

**What goes wrong / why it matters:** `Makefile:103-105` sets `-DNDEBUG` only in `RELEASE_CFLAGS`; `DEV_CFLAGS` and `ASAN_CFLAGS` (used for `make test`/`make sanitize`) do not define it, `[VERIFIED: Makefile:103-105]`. A `Grep` for `assert(`, `#ifdef.*DEBUG`, `NDEBUG` or `JAOS_DEBUG` across `src/` returns nothing — this project has never actually used the standard C `#ifndef NDEBUG` idiom in `src/` before, `[VERIFIED: grep over src/]`. D-08's "costs nothing in release" requirement is exactly what `-DNDEBUG` is for, and it is already wired correctly at the build-flag level, but this phase would be the *first* place `src/` relies on it. That is not a defect — it is the correct standard idiom, matching this project's own dev/release split exactly — but it is new enough to this codebase to be worth a deliberate look at `-Wunused`/`-Werror` interaction: any variable computed only for the debug-build comparison must not exist (or must be explicitly marked, e.g. via `[[maybe_unused]]`) in a release build that also runs `-Werror`.

**How to avoid:** gate the whole comparison block, including anything it allocates or computes, inside `#ifndef NDEBUG ... #endif`, and confirm both `make test` (dev, no `NDEBUG`) and a plain release build (`NDEBUG` set) are clean under `-Werror` before this is considered done.

## Code Examples

### The established pattern for a testable, order-sensitive helper (to follow, not `admit_candidate`'s pattern)

```c
/* Source: src/simplex.c:1637-1673, declared src/jaos_internal.h:284 */
int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words);
```
Non-`static`, takes plain arrays and a `words` out-parameter for cost accounting, unit-tested directly in `tests/test_simplex.c:1280-1366` with no LP model or `sx` involved — `[VERIFIED]`.

### The current dense scan being replaced

```c
/* Source: src/simplex.c:1573-1577 */
} else {
    for (int64_t v = 0; v < s->nvar; v++)
        admit_candidate(s, v, below, &n);
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
}
```
The `jm_work_add` call is exactly what D-09 requires be replaced with a charge for what is actually visited — `docs/work-units.md` already documents the intended shape of this charge: "building the candidate set charges one per variable it looked at — every variable when the pricing row is read densely, and the size of its pattern when it is not (D40)" (`docs/work-units.md:62-64`, `[VERIFIED]`) — this phase changes what "every variable" means for the dense branch, and the doc will need updating alongside the code.

### The allocation convention the new field(s) should follow

```c
/* Source: src/simplex.c:572-573 — apat is the closest existing analogue */
s->apat   = jm_alloc_array(s->nvar, sizeof(int64_t));
s->amark  = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
```
Whatever the new list/bitmap is, it is sized off `s->nvar` exactly like `apat`/`amark`, allocated in `sx_init` (`src/simplex.c:524-600`) alongside every other per-solve array, and must be added to `sx_free`'s unconditional `free()` list (`src/simplex.c:489-507`) — omitting it there is exactly the "field-by-field save list" class of bug `.planning/codebase/CONVENTIONS.md` warns about elsewhere in this codebase (config survival), applied to a different list.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | No published simplex-implementation text (Maros, Koberstein, or the project's other cited authors) specifically addresses *incremental, order-preserving* nonbasic-list maintenance as a named technique — this appears to be treated as a software-engineering detail left to the implementer, not a numerical-method question. Based on a web search of Maros's table of contents and abstracts only, not a full read of the book/thesis. | Architecture Patterns, Don't Hand-Roll | If a citable technique does exist in Maros or Koberstein, the planner should use it directly instead of the general-CS order-maintenance literature substituted here; the risk of being wrong is a missed opportunity to reuse a domain-specific construction, not a correctness risk (D2/D11/D12 already restrict this project to papers/theses, and the order-maintenance papers cited are papers, not solver source, so citing them is not itself a violation even if a more specific source exists). |
| A2 | The bitmap-plus-`__builtin_ctzll` approach (Don't Hand-Roll option a) is presented as lower-risk than an explicit sorted array/linked list, based on it being structurally identical to `jm_pattern_order`'s already-shipped, already-measured mechanism. This is architectural judgment, not a closed decision — CONTEXT.md explicitly leaves representation to Claude's discretion. | Don't Hand-Roll, Common Pitfalls | If the planner instead chooses an explicit index array, the O(1)-amortised claim needs its own proof or measurement before being trusted; Pitfall 1's counterexample shows the naive version is not merely slower but wrong. |

**If this table is empty:** N/A — see above.

## Open Questions (RESOLVED)

Both were closed by the planner in `01-01-PLAN.md`'s `<design_decisions>`
section. They are kept here with their reasoning intact, because the trade-off
each states is what D93 has to account for.

1. **Which representation — bitmap or explicit ordered list/array?**
   → **Resolved: a persistent bitmap `s->nbmark`.** See `01-01-PLAN.md`
   `<design_decisions>`. Maintenance is one bit flip — O(1) outright rather than
   amortised — ascending order falls out of bit position so there is no
   insertion position to compute, Pitfall 1's stale-neighbour failure cannot be
   written at all, and the scan stays ascending-sequential over `alpha[]`, which
   is what D-11 actually measures.
   - What we know: a bitmap walked with `__builtin_ctzll` gives O(1) membership toggling with no reinsertion-position problem at all, and is structurally identical to a mechanism (`jm_pattern_order`/`amark`) this codebase has already built, shipped, and measured. An explicit sorted array or linked list is more literally "a list of nonbasic variables" (CONTEXT.md's own phrase) but needs a real predecessor/successor argument to be correct under interleaved insert/delete — the naive version is shown broken in Pitfall 1.
   - What's unclear: whether the bitmap's per-scan cost (proportional to the touched *range* in 64-bit words, like `jm_pattern_order`'s `lo`/`hi` tracking at `src/simplex.c:1646-1655`) or an explicit list's cost (proportional to the *count* of nonbasic variables, if a correct O(1) construction is used) is actually cheaper on this project's instance mix — this is exactly the kind of thing D-11's same-instance time ratio is positioned to answer, but only after one is built.
   - Recommendation: research could not close this because it is a genuine design trade-off CONTEXT.md deliberately left open, not a gap in available information. The planner should pick one, and if it is the linked-list route, budget explicit time to prove (not assume) the reinsertion primitive is correct — a unit test built the way `jm_pattern_order`'s are, exercising the interleaved-eviction case from Pitfall 1, is the natural instrument.

2. **Where does the differential-equivalence test (D-07) actually live, given `sx`/`admit_candidate`/`dual_ratio_test` are unreachable from `tests/test_simplex.c` today?**
   → **Resolved: exactly the two layers this section recommends.** See
   `01-01-PLAN.md` `<design_decisions>`. Plain-array unit tests for the
   maintenance primitives in `tests/test_simplex.c`, in a new cluster after
   `test_pattern_order_*`; and the D-08 cross-check as a `#ifndef NDEBUG` block
   inside `simplex.c` that re-runs the dense loop into debug scratch and
   compares all four candidate arrays position for position.
   - What we know: the codebase's own precedent (`jm_bland_pick`/`jm_harris_pick`/`jm_pattern_order`) is to split delicate logic into non-`static`, `jm_`-prefixed, plain-array functions specifically so it is testable without `sx`.
   - What's unclear: whether the *maintenance* primitives alone (insert/remove) can be tested that way while the *end-to-end* dense-scan-vs-list-scan comparison (which needs a real `sx` with real `alpha`, `d`, bounds, etc. to be a meaningful equivalence test rather than a bookkeeping test) is better done as the D-08 runtime assertion itself, exercised indirectly by running the existing `bench`/gate campaigns rather than as a separate unit test.
   - Recommendation: treat D-07 and D-08 as two layers, not one artifact — a plain-array unit test for list-maintenance correctness (buildable and testable today, no visibility problem), plus a `#ifndef NDEBUG` runtime cross-check inside `simplex.c` itself that is exercised by every existing `make test`/`make sanitize`/`make netlib*` run once the change lands, which is where the "same solver state" comparison CONTEXT.md asks for is actually cheap to get right.

## Environment Availability

Skipped — this phase has no external dependencies beyond the toolchain already documented in project root `CLAUDE.md` (GCC 14 minimum, WSL, `make test`/`make sanitize`/`make netlib*`), which is unchanged by this phase and already verified working by the existing test suite and gate.

## Validation Architecture

### Test Framework

| Property | Value |
|---|---|
| Framework | Unity v2.7.0, vendored at `tests/vendor/unity/` (`.planning/codebase/TESTING.md`) |
| Config file | none — Makefile-discovered (`$(wildcard tests/test_*.c)`, `Makefile:120`) |
| Quick run command | `wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && make test"` |
| Full suite command | `wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && make test && make sanitize"` |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| REQ-ratio-test-candidate-admission | New list/bitmap maintenance primitives produce, after any interleaving of insert/remove, exactly `{v : status[v] != JM_BASIC}` in ascending order, including `JM_FREE` variables | unit (plain-array, `jm_pattern_order`-style) | `./build/dev/test_simplex` (new `RUN_TEST` entries) | ❌ Wave 0 |
| REQ-ratio-test-candidate-admission | The stale-pointer failure mode (Pitfall 1) is deliberately constructed and confirmed rejected before the maintenance test is trusted (CONTEXT.md `<specifics>`) | unit, negative case | `./build/dev/test_simplex` | ❌ Wave 0 |
| REQ-ratio-test-candidate-admission | Old dense scan and new list-based scan agree on candidate set and array positions over real solver state, once per iteration, in debug builds only | runtime assertion (D-08), not a standalone test | exercised by `make test`, `make sanitize`, and every `make netlib*` run once wired | ❌ Wave 0 — no `#ifndef NDEBUG` precedent exists yet in `src/` (Pitfall 3) |
| REQ-ratio-test-candidate-admission | All 139 solution digests identical (D-05) | integration / gate | `make netlib && make netlib-kennington && make netlib-infeas` (all at `J=12` per project root `CLAUDE.md`) | ✅ — existing gate infrastructure, `bench/README.md` |
| REQ-ratio-test-candidate-admission | Work counter charges what is visited, not `nvar` (D-09) | gate + baseline diff | `make netlib` (diffs against `bench/netlib.baseline`, never rewritten as a side effect) | ✅ — existing baseline-diff mechanism |
| REQ-ratio-test-candidate-admission | Same-instance time ratio at `J=1`, geometric mean over the standard set, 4.2% threshold (D-11/D-12/D-13) | performance measurement, not a pass/fail test | `make netlib J=1` before/after, per project root `CLAUDE.md`'s campaign timings | ✅ — existing campaign infrastructure; the `jaos-measure` skill governs how this is read |

### Sampling Rate

- **Per task commit:** `make test` (dev build, no `NDEBUG` — exercises the D-08 assertion if wired)
- **Per wave merge:** `make test && make sanitize`
- **Phase gate:** `make netlib && make netlib-kennington && make netlib-infeas` all green, per-instance baseline diff clean, before D-05/D-06's digest requirement is considered satisfied and the baseline may be rewritten

### Wave 0 Gaps

- [ ] A plain-array unit test file/section for the list/bitmap maintenance primitives — covers REQ-ratio-test-candidate-admission's membership-invariant requirement, buildable today without touching `sx` visibility.
- [ ] The interleaved-eviction negative test from Pitfall 1 (or whatever failure mode the chosen representation is actually vulnerable to) — must be shown to fail before the positive test is trusted (CONTEXT.md `<specifics>`, D-07).
- [ ] A `#ifndef NDEBUG` block inside `simplex.c` wiring the runtime cross-check (D-08) — first use of this idiom in `src/`, needs a clean `-Werror` build on both `DEV_CFLAGS` and `RELEASE_CFLAGS` before it is considered done (Pitfall 3).
- [ ] A `docs/work-units.md` update describing the new charge for the dense branch, alongside the D-09 code change — the doc currently states the pre-change formula explicitly (`docs/work-units.md:62-64`) and will read as stale otherwise.

## Security Domain

This phase touches no network-facing, authentication, session, or user-input surface — it is an internal data-structure change inside a linked C library's solve loop, reachable only through the existing `jaos_solve`/`jaos_load_*` public API, which is unchanged by this phase. Most ASVS categories do not apply.

| ASVS Category | Applies | Standard Control |
|---|---|---|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | indirectly | Not user input in the web sense — the equivalent risk is *internal* invariant corruption (a desynchronised list). Covered by D-07's differential test and D-08's runtime assertion, not by a separate input-validation control. |
| V6 Cryptography | no | — |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---|---|---|
| Silent memory corruption from an off-by-one in list splicing, invisible until a much later iteration reads a dropped or duplicated candidate | Tampering (of internal state, not by an external actor) | `make sanitize` (ASan+UBSan, already run on every `test_*.c`, `.planning/codebase/TESTING.md`), plus D-07/D-08's equivalence checks specifically designed to catch exactly this class before it reaches a published answer |
| A desynchronised list producing a *plausible but wrong* candidate set that still passes the digest/checker gate on most instances but silently changes the trajectory on others | Tampering | D-05's 139-digest requirement across all three sets, not a subset — this is precisely why the gate is all-instances-or-nothing (`bench/README.md`, "The gate is all-or-nothing") |

## Sources

### Primary (HIGH confidence — read this session, code and repo documents)
- `src/simplex.c` (full `admit_candidate`/`dual_ratio_test`/`sx` struct/`pivot`/all twenty `status[v]=` sites) — `[VERIFIED]`, line numbers cited throughout
- `src/jaos_internal.h` — `jm_var_status` enum, declarations of `jm_bland_pick`/`jm_harris_pick`/`jm_pattern_order`/`jm_dse_update`, absence of `sx` — `[VERIFIED]`
- `tests/test_simplex.c` — existing unit-test precedent for the four non-`static` helpers — `[VERIFIED]`
- `Makefile` — `DEV_CFLAGS`/`RELEASE_CFLAGS`/`ASAN_CFLAGS`, `-DNDEBUG` placement, test-binary link rule — `[VERIFIED]`
- `DECISIONS.md` D61, D82, D83, D84 (full text read) — `[VERIFIED]`
- `docs/archive/PLAN.md` bibliography (verified citations, D12) — `[VERIFIED]`
- `bench/README.md`, `docs/work-units.md`, `.planning/codebase/TESTING.md`, `.planning/codebase/CONVENTIONS.md` — `[VERIFIED]`
- `.planning/phases/01-candidate-admission-in-the-ratio-test/01-CONTEXT.md`, `.planning/REQUIREMENTS.md`, `.planning/STATE.md`, `.planning/PROJECT.md`, `.planning/INGEST-CONFLICTS.md` — `[VERIFIED]`

### Secondary (MEDIUM confidence — web search, general CS literature, cross-checked)
- P.F. Dietz, D.D. Sleator, "Two Algorithms for Maintaining Order in a List," STOC 1987 — `[CITED: web search this session]`, general order-maintenance-problem literature, not a solver's source (D12 does not exclude it)
- M.A. Bender, R. Cole, E.D. Demaine, M. Farach-Colton, J. Zito, "Two Simplified Algorithms for Maintaining Order in a List," ESA 2002 — `[CITED: web search this session]`

### Tertiary (LOW confidence — not independently verified beyond an abstract/ToC search)
- Whether I. Maros, *Computational Techniques of the Simplex Method* (already a project-verified citation, [2] in `docs/archive/PLAN.md`) or A. Koberstein's thesis ([1]) discuss incremental nonbasic-list maintenance specifically — search returned only that "data structures and basic operations" is a section title; content not confirmed. See Assumptions Log A1.

## Metadata

**Confidence breakdown:**
- Code-level findings (site table, struct layout, test visibility, work-unit charge): HIGH — every claim read from the actual source this session with line numbers.
- Data-structure recommendation (bitmap vs. linked list): MEDIUM — reasoned from first principles and this codebase's own precedent, not settled by a citation specific to this exact problem.
- Harris's-guarantee-is-unaffected argument: HIGH — the guarantee's inputs (candidate set, order, tie-break mechanics) are read directly from code and shown to be preserved by D-01/D-02/D-03's own constraints.

**Research date:** 2026-08-12
**Valid until:** this research is tied to the current shape of `src/simplex.c`; treat as stale the moment any of the twenty cited line numbers move (e.g., after this phase's own implementation commit) or after 30 days, whichever comes first.
