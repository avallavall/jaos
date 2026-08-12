# Phase 2: Presolve and postsolve - Research

**Researched:** 2026-08-12
**Domain:** LP presolve/postsolve for a from-scratch dual-simplex solver, no external dependencies
**Confidence:** MEDIUM — the reduction families and their classical treatment are well-documented
in the literature (HIGH confidence, citations verified against the publisher this session); the
exact postsolve dual/reduced-cost recovery formula for each reduction is reported at a conceptual
level only and is **not independently re-derived or verified against JAOS's own sign conventions
in this session** (LOW confidence on the formulas specifically — see Assumptions Log). That gap is
exactly where D-10's round-trip tests and the `numerics-reviewer` task must do the checking this
research could not.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Phase Boundary.** A model can be reduced before the simplex sees it, and a solution to the
reduced model comes back as a solution to the one the caller loaded. In scope: a new
`src/presolve.c` that builds a reduced problem from `jaos_model`'s arrays without mutating them,
the eight reduction families REQ-presolve names — empty and singleton rows and columns, forcing
and redundant constraints, bound tightening, fixed variables, duplicate rows and columns,
dominated columns — and the postsolve that maps values, activities, duals and reduced costs back
into the caller's row and column indices. Out of scope: anything inside the simplex loop (phase 3
owns factorization, phase 4 the search path; presolve does not touch `admit_candidate`, pricing,
or the re-entry loop). The correctness risk lives in postsolve, not the reductions (success
criterion 2). Phase 1's "all 139 digests unmoved" safety net does not carry over — presolve
changes the model the simplex sees, so iteration counts and work units move on every instance by
design; what survives as evidence is verdicts, objectives against Koch's reference, and the
checker's acceptance through postsolve.

- **D-01: The first plan is the scaffolding, not a reduction.** Reduced model, postsolve stack,
  and one trivially-correct reduction, proved end to end over all 139 instances with almost
  nothing removed. Every subsequent reduction arrives with its own postsolve record and its own
  measured contribution.
- **D-02: Presolve iterates to a fixed point, with a deterministic round cap that is measured
  rather than chosen.** Follows the `IMPLIED_ROUNDS = 64` precedent (`src/check.c:264`), set by
  sweeping the standard set. The sweep is a deliverable of this phase.
- **D-03: Presolve is switched off by a build-time `-D` constant in `src/presolve.c`. It never
  becomes a public API option.** D64 draws contract-vs-method; `Makefile:94-101`'s `EXTRA_CFLAGS`
  is the existing hook. This switch is also what makes the phase's own on/off measurement possible.
- **D-04: Presolve runs before scaling, on the model as loaded.** The reduced model is then scaled
  fresh inside `sx_init`. Running bound tightening in scaled space would make the reductions depend
  on a scaling that depends on the matrix presolve has just changed. D27, D89, D92 stand as warning
  that the two tolerance spaces do not convert into each other.
- **D-05: A new module, `src/presolve.c`, prototypes in `src/jaos_internal.h`.** No per-module
  header exists in this tree, so no include cycle is possible by construction.
- **D-06: Presolve builds a reduced problem alongside the model and never mutates `jaos_model`'s
  authoritative CSC arrays.** Keeps `src/check.c` reading the model as loaded. Same move `sx_init`
  already makes for its scaled working copy. Rejected: mutate-and-restore — an early return (work
  limit, time limit, `JAOS_ERR_NUMERICAL`) would leave the caller's model reduced.
- **D-07: The postsolve record is one append-only arena of tagged records, replayed strictly
  LIFO**, each carrying the original row or column index it restores. Replay order is the only
  order, deterministic by construction.
- **D-08: The postsolve stack is solve-local.** Built inside `jm_dual_simplex`, consumed before
  `publish` returns, freed with `sx`. `start_col_status`/`start_row_status` stay in **original**
  indices; the warm basis is mapped into reduced indices on the way in and back on the way out, so
  the warm-start contract (D77–D79) does not change. Rejected: caching the stack on the model —
  any model edit invalidates it with nothing watching.
- **D-09: The negative control is presolve compiled off.** With reductions out, all 139 digests
  must be bit-identical to the baselines committed today.
- **D-10: One round-trip test per reduction family.** A small model where the reduction fires,
  solved with presolve on and off, requiring the same verdict, an objective within tolerance, and
  **the checker accepting the postsolved solution**. Each test is validated before it is believed:
  hand postsolve a deliberately broken index map and confirm the checker rejects it.
- **D-11: The existing checker is the instrument and is not touched.** Only new obligation: on
  postsolve, it must fill `sol_dual` and `sol_redcost` in original indices — criterion 2 names all
  four of values, activities, duals and reduced costs. Rejected: adding checks to `src/check.c` —
  would give the checker knowledge of the reduced model.
- **D-12: Criterion 4 is proved by extending the double solve `bench/run.c` already performs.**
  Framed in CONTEXT.md as: "It currently requires bit-identical digests across the two solves; it
  must also require equal iterations and equal work units." **Research finding: as read this
  session, `bench/run.c` already requires iteration-count and work-unit equality as a precondition
  of computing `det` on both the optimal path (lines 574–580) and the infeasible path (lines
  437–438) — see Code Insights below.** The planner should treat D-12 as "confirm/extend if
  presolve introduces a path this check does not already cover" rather than assume the check does
  not exist yet.
- **D-13: Each reduction reports what it removed through a per-family counter struct in
  `src/jaos_internal.h`**, logged at `JAOS_LOG_SUMMARY`, printed by `bench/run.c` into the record.
  No public API. Tests are white-box, read the struct directly.
- **D-14: Presolve bills the same `jm_work` counter every other kernel bills.** One-way reversal:
  every historical work figure changes meaning, needs its own `DECISIONS.md` entry and a deliberate
  rewrite of all three baselines — never as a side effect of running the gate.
- **D-15: The deliverable number is a geometric mean of per-instance ratios (D46)** over the
  standard set, presolve-on against presolve-off, at `J=1`, **with a negative control that is not
  optional**: instances where presolve removes nothing must read 1.00x, and whatever they actually
  read is the noise floor this phase's figure must clear. Work-unit ratio and time ratio reported
  separately.
- **D-16: This phase does not recalibrate the comparison ladder.** Records the rule (T2 rung), hands
  `make compare` re-run to phase 5. A WSL number cannot close a gate (D17).

**Two plan tasks that are not optional**, and no GSD workflow spawns either agent — they run only
because a plan says so:
- `numerics-reviewer` on the postsolve diff, after the code lands and *before* the campaigns run.
- `jaos-measurer` as the verdict step, on the finished numbers, judged by a context that did not
  produce them.

### Claude's Discretion

Area 1 (D-01 through D-04) was already resolved at the user's request, aimed at the best achievable
presolve rather than the safest one. Beyond them, at the planner's discretion:
- The internal representation of the reduced model and of the index maps.
- Which reduction is the "trivially correct" one D-01 ships first.
- Whether the round-trip tests live in a new `tests/test_presolve.c` or extend an existing file.
- The order the remaining seven families are added in, subject to D-13 making that order derivable
  from measurement rather than assumed.

### Deferred Ideas (OUT OF SCOPE)

- Re-running `make compare` and rebuilding the four comparison rungs (phase 5's work, D-16, D17).
- Presolve reductions that need the dual or interact with the basis (dual fixing, reduced-cost
  fixing) — need a solved relaxation to fire, belong to a later decision.
- A trajectory sweep over `REFACTOR_EVERY` with presolve on (raised, not scheduled — roadmap Open
  Question 5).
- The two carried defects D93 handed on (`baseline: NOT COMPARED` unread; `bench/run.c`'s `%8.3f`
  seconds format). **The second one directly degrades D-15's time ratio and is worth fixing before
  this phase's campaigns — but as its own commit, before any measurement starts, never mid-campaign.**
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| REQ-presolve | Build presolve and postsolve — empty and singleton rows and columns, forcing and redundant constraints, bound tightening, fixed variables, duplicate rows and columns, dominated columns, and the postsolve that puts a solution back, "which is where the correctness risk lives, not in the reductions." Acceptance: **absent** — no numeric target stated for JAOS's own presolve; D81's 1.417x/1.136x are readings of two competitors, not targets. | Reduction Families section gives what each rule is, what it removes, and its literature source (all CITED against the publisher). Postsolve Correctness section gives the general recovery obligation per family and flags the exact formula as unverified (ASSUMED), routed to D-10's round-trip tests and the `numerics-reviewer` task. Validation Architecture section maps REQ-presolve's four success criteria to concrete, automatable checks. |
</phase_requirements>

## Summary

REQ-presolve's eight named families are not a mixed LP/MIP list — every one of them is a core LP
presolve reduction from Andersen & Andersen's 1995 survey (*Presolving in linear programming*,
Math. Program. 71, 221–245), the paper that also names the restoration (postsolve) procedure this
phase must build. Gondzio (1997, INFORMS J. Computing 9(1), 73–91) and Brearley, Mitra & Williams
(1975, Math. Program. 8, 54–83) describe the same family under different names and confirm the
same reduction-then-restore shape decades apart. None of the eight is a MIP-only reduction (no
probing, no clique detection, no GCD/coefficient tightening) — so the planner does not need to
triage "which of these even applies to a pure LP"; all eight do, and the literature reports
material speedups from them on LP relaxations generally, though **not one paper source gives a
number specific to JAOS or to the Netlib set at the granularity D-15 needs** — that number is what
this phase's own measurement produces, and the literature's role is technique, not target,
exactly as D81's readings were competitor numbers and not a JAOS target.

The correctness risk the phase boundary names is real and specific: every reduction's primal
recovery is close to mechanical (the removed variable's value falls out of the row/column equation
that was folded away), but the **dual value and reduced-cost recovery for a removed row or column
is a derived quantity that depends on which bound the reduced-problem solve actually used**, and
getting that derivation wrong produces a *plausible* wrong answer — a solution in the right shape,
with the right primal objective, that a superficial glance would accept. This research reports the
recovery obligation for each family at a conceptual level, sourced from the same papers, but does
**not** independently re-derive or verify the exact sign/formula against JAOS's own minimize-
canonical dual convention (`src/check.c`'s `sign_condition`) in this session. That is flagged
explicitly per reduction and belongs to implementation plus the two review agents D-10 and the
CONTEXT's "two reviews" rule both insist on.

Presolve's own tolerances are a genuinely new, third tolerance space distinct from the two the
project already tracks (solver-scaled, `PRIMAL_TOL`/`DUAL_TOL` ≈ 1e-7; checker-original,
caller-supplied `tol`). D-04 places presolve in the model's own unscaled units, before scaling
exists at all, so any epsilon a reduction needs (bound-tightening improvement threshold,
duplicate-row/column detection tolerance, dominated-column comparison tolerance) is a new constant
in unscaled space and needs its own measurement on both sides per `fp-numerics` and per this
project's own rule that every number needs a measurement on both sides — none of these constants
exist in the tree today and none should be invented without a sweep.

**Primary recommendation:** build the D-01 scaffolding first (reduced model + postsolve arena +
one reduction — fixed variables is the simplest correctness argument, see Architecture Patterns),
prove it against all 139 instances with the presolve-off digest identity as the negative control,
then add reductions one at a time in an order the D-13 counters can justify, each with its own
round-trip test built to fail first (D-10) and reviewed by `numerics-reviewer` before any campaign
that measures it.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Reduced-model construction (reading `jaos_model`'s CSC arrays, building index maps) | Solver core (`src/presolve.c`) | — | Reads `jaos_model` (src/model.c) but must not mutate it (D-06); lives beside `sx_init`'s scaled-copy pattern, not inside it |
| Reduction rules (the 8 families) | Solver core (`src/presolve.c`) | — | Pure transformations of the reduced model's arrays; no I/O, no solve-loop coupling (out of scope per phase boundary) |
| Postsolve arena (append-only LIFO record) | Solver core (`src/presolve.c`) | Solve-local state (`jm_dual_simplex`'s stack, D-08) | Built and consumed entirely inside one solve; never touches `jaos_model` directly except at the final write |
| Postsolve replay (index mapping back to original rows/cols) | Solver core (`src/presolve.c`), invoked from `publish()` in `src/simplex.c` | — | `publish()` (simplex.c:3641) is where scaled→original unit conversion already happens; postsolve's reduced→original index conversion is the same kind of boundary crossing, so it belongs at the same call site, before or folded into `publish` |
| Verification of the postsolved answer | `src/check.c` (unmodified) | — | D-11: the checker's independence is structural (no include chain to `presolve.c`, `scale.c`, `lu.c` or `simplex.c` — code note in ROADMAP.md); postsolve's only new obligation is filling `sol_dual`/`sol_redcost` correctly, not changing the checker |
| Work-unit billing for presolve's own passes | Solver core (`jm_work`, D-14) | `docs/work-units.md` (documentation) | Same counter every kernel bills (`jm_work_add`); a presolve pass that goes unbilled understates every future work-limit budget |
| Build-time on/off switch | Build system (`Makefile`'s `EXTRA_CFLAGS`, D-03) | `src/presolve.c` (`#ifdef`/`#if` guard) | Method, not contract (D64) — never a `jm_config` field, never a public setter |
| Round-trip and negative-case tests | `tests/` (new or extended file, Claude's discretion) | — | Same tier as `tests/test_check.c`'s existing "build the case it must reject" tests (`test_t1_flags_wrong_dual_sign`, etc.) — precedent already in the tree |
| Campaign measurement (D-15 geometric mean, negative control) | `bench/run.c` + `.claude/skills/jaos-measure/scripts/` | `.planning/phases/02-.../<plan>-MEASUREMENT/` (raw readings) | Development-machine measurement; a plan whose deliverable is this verdict must commit its raw readings per `bench/README.md`'s "a verdict commits its readings" |

## Standard Stack

### No external packages

JAOS has no external dependencies by project rule (D2, D11; CLAUDE.md). This phase adds no
library, no build tool, and no dev-time tool beyond what is already in the tree. There is nothing
for the **Package Legitimacy Audit** to check — the section is included below only to state that
explicitly, per the output contract.

## Package Legitimacy Audit

**Not applicable.** No packages are installed, referenced, or proposed by this phase. `src/presolve.c`
is pure C23 against the existing internal headers. If a future task in this phase's plans proposes
any external tool, it contradicts D2/D11 and CLAUDE.md and should be rejected at plan-check rather
than executed.

## Architecture Patterns

### System Architecture Diagram

```
jaos_solve(m)
   │
   └─> jm_dual_simplex(m)                                    [src/simplex.c]
          │
          ├─(1) jm_presolve(m, &pre)  ── NEW, src/presolve.c ────────────────┐
          │      reads m->a_start/a_index/a_value, m->col_*, m->row_*        │
          │      (model as loaded, UNSCALED — D-04)                         │
          │      builds:                                                     │
          │        - a reduced problem  (own col/row arrays, own dims)       │
          │        - an index map: reduced idx -> original idx               │
          │        - a postsolve arena: append-only, tagged records (D-07)   │
          │      loop to fixed point, capped at a measured round count (D-02)│
          │      may prove infeasibility directly -> skip straight to        │
          │      publish(INFEASIBLE) with no simplex run at all              │
          │      #if !defined(JAOS_NO_PRESOLVE) guards the whole call (D-03) │
          │                                                                   │
          ├─(2) sx_init(&s, reduced_model_view)  [existing, unchanged shape] │
          │      computes scaling on the REDUCED problem (D-04: after (1))   │
          │      builds the scaled working copy exactly as it does today     │
          │                                                                   │
          ├─(3) build_warm_basis(&s) / build_initial_basis(&s)  [existing]   │
          │      NEW: start_col_status/start_row_status are in ORIGINAL      │
          │      indices (D-08) -> must be mapped into reduced indices here, │
          │      mapped BACK on the way out                                  │
          │                                                                   │
          ├─(4) run(&s, &outcome)   [existing dual simplex loop, untouched]  │
          │      operates entirely on the reduced, scaled problem            │
          │                                                                   │
          └─(5) publish(&s, outcome)  [existing, src/simplex.c:3641]         │
                 existing: scaled -> original units (rho/gamma)               │
                 NEW: reduced -> original INDICES, via postsolve replay ──────┘
                      pops the postsolve arena strictly LIFO (D-07),
                      restoring sol_col/sol_row/sol_dual/sol_redcost/
                      sol_col_status/sol_row_status for every row/column
                      the reduction removed
                 writes m->sol_* in ORIGINAL indices, as always

Caller reads jaos_solution(m, ...) / jaos_basis(m, ...) / jaos_check_solution(m, ...)
   -> jaos_check_solution has NO include chain to presolve.c (D-11, D18) and
      reads m->a_start/a_index/a_value as loaded — the ORIGINAL, unreduced
      matrix — so it verifies the postsolved answer independently, structurally.
```

### Recommended Project Structure

```
src/
├── presolve.c        # NEW — reduced-model builder, the 8 reductions, postsolve replay
├── jaos_internal.h    # +prototypes for jm_presolve/jm_postsolve, +per-family counter struct (D-13)
├── simplex.c          # jm_dual_simplex gains the presolve call before sx_init;
│                       # publish() gains the postsolve replay before/around unit conversion
├── model.c            # UNCHANGED — presolve reads jaos_model, never writes it (D-06)
└── check.c            # UNCHANGED (D-11) — verifies against m->a_start/a_index/a_value as loaded

tests/
└── test_presolve.c    # NEW (or extend test_simplex.c — Claude's discretion) — one round-trip
                        # test per family, each validated to fail on a broken index map first (D-10)

bench/
└── run.c              # D-12: confirm existing iters==/work==/digest== chain covers presolve;
                        # extend only if a presolve-specific path escapes it (see Code Insights)
```

### Pattern 1: Reduced model as a parallel structure, never a mutation

**What:** `sx_init` already builds a scaled *working copy* of the model from `jaos_model`'s
arrays without touching `m->a_start`/`a_index`/`a_value` — that is the exact shape D-06 asks
presolve to repeat one layer up: a reduced-model struct (own CSC-like arrays, own `num_row`/`num_col`,
plus an `orig_row[]`/`orig_col[]` index map back to the caller's indices) built fresh from `m`'s
arrays, read-only against `m`.

**When to use:** Every reduction. Never write to `jaos_model`'s `a_start`/`a_index`/`a_value`,
`col_lower`/`col_upper`/`row_lower`/`row_upper` from presolve — those stay the authority
`src/check.c` judges against (D18, D-06's rationale).

**Example (structure, not literal code — no such code exists in the tree yet):**
```c
/* Source: pattern generalized from sx_init, src/simplex.c:556 (read this session) */
static jaos_status sx_init(sx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    ...
    s->av  = jm_alloc_array(m->num_nz, sizeof(double));   /* OWN copy */
    ...
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];
    /* m->a_value is read, never written */
    return JAOS_OK;
}
```
A presolve reduced-model builder is the same shape: allocate fresh arrays sized by the *reduced*
dimensions, copy/transform from `m`'s arrays, never write back into `m` until `publish()`'s
postsolve replay writes the final `sol_*` arrays.

### Pattern 2: LIFO postsolve arena (D-07)

**What:** Every reduction that removes a row or column pushes one or more tagged records onto an
append-only arena as it fires. Postsolve replays the arena in reverse (LIFO) order after the
reduced problem is solved. LIFO is not a style choice — it is what makes replay order the *only*
order there is, so two runs of the same presolve produce the same replay sequence by construction
rather than by discipline (matches the project's determinism rule, D8/D34).

**When to use:** Every reduction, without exception — including the "trivially correct" first one
D-01 asks for. A reduction with no postsolve record cannot be undone, which is exactly the defect
shape D-01's rationale calls out ("the correctness machinery is the part that can be wrong in a way
nothing catches").

**Precedent for the allocation strategy:** `jm_alloc_array`/`jm_calloc_array` are the only
allocation paths in the tree (`jaos_internal.h:328-329`, confirmed this session); the arena should
grow through `JM_GROW`/`jm_grow` (`jaos_internal.h:343-345`), the same growable-array mechanism
`jm_svec` and the LU factorization's eta lists already use.

### Pattern 3: Presolve-provable infeasibility short-circuits the simplex entirely

**What:** Several reductions (empty row with an infeasible implied range, forcing constraint
forced both directions at once, singleton-row bound tightening producing `cl_j > cu_j` beyond
tolerance) can prove the model has no feasible point without ever building a basis. When this
happens, `jm_dual_simplex` should call `publish(&s_or_stub, JAOS_SOLVE_INFEASIBLE)` directly,
skipping `sx_init`'s scaling and the simplex loop.

**When to use:** Any reduction whose fixed-point loop detects a structural infeasibility. This is
the direct interaction with the 29-instance infeasible reference set the research priorities flag:
`bench/run.c`'s `run_one_infeasible` (read this session, lines ~380-459) only requires
`expected=infeasible verdict=ok det=ok` — it does not require the simplex to have run any
iterations to reach that verdict, and it does not compute a digest for the infeasible set at all
(`bench/README.md`: "the 29 infeasible ones produce a refusal verdict... and carry no `digest=`
field"). A presolve-only infeasibility detection is therefore compatible with the existing gate
predicate as read; `iters=0`, `work=<presolve billing only>` on those instances is not a
regression by the gate's own 2× work-growth rule (it is a *reduction*, and reductions are not
flagged) but **is** a large, deliberate change to what those 29 lines report, and the plan should
say so explicitly rather than let it surface as a surprise in the campaign diff.

### Anti-Patterns to Avoid

- **Thresholding a dropped dual term by magnitude instead of computing what it certifies.**
  `src/check.c`'s own `implied_bounds`/`certified_step` machinery (read this session,
  `check.c:145-170`, `219-245`) is the in-tree cautionary tale: D47 measured that no local
  magnitude threshold on a multiplier separates a harmful dropped term from a harmless one — what
  matters is the distance the variable can actually travel. If bound-tightening or dominated-column
  detection needs an "is this improvement worth taking" decision, do not invent a bare epsilon;
  measure what a false tightening costs (a cut-off optimum) against what refusing to tighten costs
  (a missed reduction), the same two-sided-measurement discipline the rest of this project already
  applies to every tolerance in `docs/tolerances.md`.
- **Giving the checker any awareness of the reduced model.** D-11's rejected alternative
  ("adding checks to `src/check.c`") is explicitly named as destroying the structural independence
  that makes the checker an oracle (D18). If a reduction's correctness seems to need a
  reduced-model-aware check to validate, that need belongs in a round-trip test (D-10), not in
  `check.c`.
- **A tolerance without a stated space.** Per `fp-numerics`: before adding any numerical test or
  threshold inside `src/presolve.c`, say which space it lives in (unscaled model space is the only
  space presolve runs in per D-04) and why. Do not reuse `PRIMAL_TOL`/`DUAL_TOL` (scaled-space,
  1e-7) or the checker's caller-supplied `tol` (original space, but a *diagnostic* tolerance chosen
  by the caller) for a presolve decision — neither was measured for this purpose.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Growable arrays for the reduced model and the postsolve arena | A bespoke realloc wrapper | `jm_alloc_array`/`jm_calloc_array`/`JM_GROW` (`jaos_internal.h:325-345`) | Already the only allocation path in the tree; overflow-checked, already used by `jm_svec` and the LU factorization for the same "arena that grows as reductions fire" shape |
| Bitmap membership tracking (e.g. "which rows/columns are still live in the reduced model") | A fresh ad-hoc bitmap type | `jm_nonbasic_build`/`insert`/`remove`/`expand` pattern (`jaos_internal.h:287-323`) as a *pattern* to imitate, not to call directly — those functions are typed to `jm_var_status`, not to a presolve liveness concept | The existing bitmap idiom (persistent, ascending-order expand, `(n+63)/64` words) is the established style in this codebase for exactly this kind of "is member v still active" tracking; a second, differently-shaped bitmap type invented from scratch is inconsistent with `CONVENTIONS.md`'s established patterns |
| Constraint-propagation bound tightening | A new fixed-point loop written without reference to the one already in the tree | `src/check.c`'s `implied_bounds` (`check.c:304-420`, read this session) is the **existing, working, swept implementation of the same algorithm** — activity-range propagation over column boxes, iterated to a measured cap, monotone-only-tightens | It is not reachable from presolve (checker independence, D18) so it cannot be called directly, but its *shape* — the double loop over rows then columns, the `moved` flag, the round cap doubling as canary target — is the direct precedent for D-02's presolve fixed-point loop and should be read before writing a new one from memory |

**Key insight:** This codebase already has two working instances of "iterate a monotone reduction
to a measured fixed point" (`IMPLIED_ROUNDS` in `check.c`, and the not-yet-written presolve loop)
and one of "append-only tagged arena consumed once" (the LU factorization's Forrest-Tomlin eta
list, `jm_lu.ft`/`ft_source`). Presolve's scaffolding is a recombination of patterns already proven
in this tree rather than a new kind of machinery — which is exactly what D-01's "prove the
machinery while the model is nearly unchanged" argues for: the risk is in getting the *new*
combination right, not in inventing new mechanism.

## Reduction Families (REQ-presolve's eight, from the published literature)

All eight are LP presolve reductions from **Andersen, E.D. and Andersen, K.D. (1995), "Presolving
in linear programming," Mathematical Programming 71, 221–245** [CITED: springer link
`10.1007/BF01586000`, verified against publisher this session]. **Gondzio, J. (1997), "Presolve
analysis of linear programs prior to applying an interior point method," INFORMS Journal on
Computing 9(1), 73–91** [CITED: `pubsonline.informs.org/doi/abs/10.1287/ijoc.9.1.73`, verified
this session] independently describes the same first four families (empty/singleton rows and
columns, forcing/dominated constraints, bound tightening) for an interior-point front end, which is
useful corroboration that these reductions are solver-method-agnostic rather than dual-simplex-
specific. **Brearley, A.L., Mitra, G. and Williams, H.P. (1975), "Analysis of mathematical
programming problems prior to applying the simplex algorithm," Mathematical Programming 8, 54–83**
[CITED: springer link `10.1007/BF01580428`, verified this session] is the earliest of the three and
is the origin of row/column bound-tightening-to-fixed-point as a preprocessing pass ahead of the
simplex specifically.

**None of the eight is MIP-only.** REQ-presolve's list maps onto A&A 1995's survey almost verbatim
— there is no probing, clique merging, GCD/coefficient tightening, or other integer-specific rule
in it. The planner does not need to triage the list for LP-applicability; every item is a
core LP reduction and the phase boundary's "anything that needs a solved relaxation... belongs to a
later decision" (Deferred Ideas) is what correctly excludes the MIP-flavoured dual-fixing rules
that *would* need triage.

| Family | What it is | What it removes | Netlib-class payoff (literature, general) |
|--------|-------------|------------------|--------------------------------------------|
| Empty row | A row with no nonzero coefficients (`a_start[j+1]==a_start[j]` for every column touching row `i`... i.e. row `i` appears in no column's entries) | The row entirely; either proves infeasibility (if `0 ∉ [row_lower_i, row_upper_i]`) or is simply dropped | [ASSUMED] Rare in a well-formed MPS-sourced Netlib instance as originally authored; more likely to arise from `jaos_set_coefficient` zeroing out the last entry of a row, or from a caller-built model. Not expected to be a large contributor on the standard 94, but cheap and structurally necessary as a base case |
| Empty column | A column with no nonzero coefficients | The column; fixes `x_j` at whichever finite bound does not increase the objective (min-sense: lower bound if `c_j ≥ 0`, upper if `c_j ≤ 0`); if the favorable side is `±∞`, the model is unbounded and this can be detected without a simplex ray | [ASSUMED] Same rarity argument as empty rows on this instance set |
| Singleton row | `a_ij x_j` is the row's only nonzero term | The row; folds into a tightened bound on `x_j` (`x_j ∈ [row_lower_i/a_ij, row_upper_i/a_ij]` or the flipped interval if `a_ij<0`), intersected with `x_j`'s existing bounds; an empty resulting interval (beyond tolerance) proves infeasibility | [CITED, general] A&A 1995 report this as one of the reductions with the largest per-round trigger count on Netlib-scale instances generally — it commonly cascades (removing a singleton row can make another row a singleton). **The specific percentage for JAOS's 94-instance standard set is not stated anywhere in the source literature located this session and must come from D-13's own counters, not from this citation** |
| Singleton column | `x_j` appears in exactly one row `i` | The column; `x_j` becomes a slack-like variable for row `i` (a "free column singleton" if `x_j` is otherwise free — its row can be eliminated entirely by substitution) | [CITED, general] A&A 1995's "free column singleton" case is explicitly one where the column can be substituted out and the *row* eliminated too — this is a bigger structural win than a plain singleton column and worth distinguishing in implementation, since it removes both a row and a column for one reduction |
| Forcing constraint | A row whose activity range (min or max possible value given the column boxes) exactly equals one of its own bounds | Every column touching the row is fixed at the bound that makes it attain that extreme; the row itself becomes redundant and is removed. If the row's min activity exceeds its upper bound (or max activity is below its lower bound) the model is directly infeasible — no fixing needed, refuse immediately | [CITED, general] Named explicitly in Gondzio 1997 as "primal forcing... constraints" and in Brearley/Mitra/Williams 1975 as the origin of the technique |
| Redundant constraint | A row whose activity range lies entirely inside `[row_lower_i, row_upper_i]` regardless of the columns' values | The row; it can never bind, so it is dropped with no fixing of any column | [CITED, general] Same activity-range computation as forcing constraints and bound tightening — implementation-wise this is "the third outcome" of one shared activity-range routine (row forced / row redundant / row bounds a variable), not a separate pass |
| Bound tightening | Using each row's activity range (same computation as forcing/redundant), derive a tighter bound on a variable that appears in it, when the rest of the row is bounded on the relevant side | Nothing structural by itself — narrows `col_lower`/`col_upper` in the reduced model, which then *enables* fixed-variable, singleton-row/column and dominated-column reductions to fire on the next round | [VERIFIED: src/check.c:264-331, quoted below] JAOS already has a working, swept implementation of exactly this activity-range propagation as a checker diagnostic (`implied_bounds`), proving the technique converges and is cheap on this instance set in a closely related context (see Fixed-Point Iteration section) |
| Fixed variables | `col_lower_j == col_upper_j` (either as loaded, or produced by a prior round's bound tightening) | The column; `x_j` is fixed at that value, its cost contributes a constant to the objective (`c_j * col_lower_j`), and its contribution to every row it appears in shifts that row's bounds by `-a_ij * col_lower_j` | [CITED, general] The natural fixed point of bound tightening — every implementation of the cited papers treats "bounds have converged to equality" as its own reduction rather than a side effect, because folding the constant into the objective and row bounds is a distinct bookkeeping step |
| Duplicate rows | Two rows `i`, `k` whose nonzero patterns and coefficients are identical up to a positive scalar multiple `λ` (`a_kj = λ·a_ij` for every `j`) | One of the two rows; the surviving row's bounds become the intersection of `[row_lower_i, row_upper_i]` and `λ·[row_lower_k, row_upper_k]` (flipped if `λ<0`) | [CITED, general] A&A 1995 |
| Duplicate columns | Two columns `j`, `k` whose nonzero patterns and coefficients are identical up to a positive scalar multiple `λ`, **and** whose costs are in the same ratio (`c_k = λ·c_j`) | One of the two columns; the surviving column's bound range is adjusted to represent the combined range of both, and postsolve must split the recovered value back proportionally between `x_j` and `x_k` | [CITED, general] A&A 1995. **Note:** the cost-ratio condition is what keeps this an *equivalence* rather than a *dominance* — without matching costs this degenerates into the dominated-column case below, and getting the boundary between the two wrong is a plausible implementation bug worth a dedicated test |
| Dominated columns | Column `j` "dominates" column `k` when, for every row, `j`'s coefficient is at least as favorable as `k`'s (same sign structure, `a_ij` at least as good as `a_ik` for every row `i`) and `j`'s cost is no worse (`c_j ≤ c_k` in min-sense) — informally, `j` is never a worse choice than `k` | Fixes the dominated column `k` at whichever of its bounds is consistent with the domination (it can be pushed to its least-favorable bound with no loss, because `j` can always substitute for any improvement `k` might have offered) | [CITED, general] A&A 1995 names this the most delicate of the eight to implement correctly — the domination condition must hold strictly across *every* row simultaneously, and a partial-domination check that misses one row's sign is a silent correctness bug, not a missed opportunity. **Recommend this be the last of the eight added**, both because D-13 will have the most data on the other seven's actual value by then and because it is the reduction most literature-flagged as easy to get subtly wrong |

**On MIP-vs-LP payoff, directly answering the research priority:** there is no split to report. All
eight are reported in the LP-specific literature (A&A 1995 is explicitly about LP; Gondzio 1997 is
LP-relaxation-for-interior-point, not MIP) as paying on LP instances, Netlib-class ones included in
A&A's own computational section. Statements more specific than "the literature reports these as
generally effective on LP" (e.g., "family X removes Y% on average") were not located from a source
this session could verify against the publisher and would be **[ASSUMED]** folklore if stated — the
honest position, consistent with the phase's own D-15 framing ("a measured number *for JAOS* rather
than one carried over"), is that the per-family payoff on the standard 94 is exactly what D-13's
counters and D-15's geometric mean exist to produce, and no number should be quoted in a plan as if
borrowed from A&A 1995 or Gondzio 1997.

## Postsolve Correctness — where the risk actually lives

**General recovery obligation, common to every family** [CITED, general shape from A&A 1995's
restoration-procedure discussion, located this session but not re-derived against JAOS's sign
convention]:

1. **Primal.** Once the reduced problem is solved, every removed variable's value is recoverable
   directly from the equation the reduction folded away (a singleton row's `x_j`, a fixed
   variable's constant, a duplicate column's split). This is the "no primal postsolving is
   necessary, `x_j` is fixed at its optimal value" case the A&A survey states for a singleton row,
   and it generalizes: primal recovery is *arithmetic*, not a search, for every one of the eight.
2. **Row activity.** For a removed row, the activity is `sum_j a_ij * x_j` over the row's original
   (unreduced) coefficients, evaluated after every column's value is known — this is exactly the
   computation `src/check.c`'s own scatter-add already performs (`check.c:503-526`, read this
   session) for every row of the *original* model, so a removed row's activity needs no special
   case at verification time; the checker will compute it correctly as long as postsolve has
   correctly filled in every column's value.
3. **Dual value / reduced cost.** This is the risk. For a removed row, its dual `y_i` (or for a
   removed column, its reduced cost `d_j`) is **not** determined by the reduced problem's solve —
   it must be *chosen* by postsolve so that the full-model dual feasibility and complementary-
   slackness conditions hold. The general shape (paraphrased from the literature, **not verified
   against JAOS's exact minimize-canonical sign convention this session — [ASSUMED]**): the
   removed row's/column's multiplier is set to whichever value makes the reduced cost of the
   variable(s) it was folded into come out consistent with that variable's basis status in the
   *reduced* solve. Concretely, for a singleton row `i` folded into column `j`'s bound: the reduced
   solve produces `d_j' = c_j - sum_{k≠i} a_kj·y_k` (row `i` absent from the sum). If `x_j` sits
   nonbasic at the bound the singleton row induced, postsolve should set `y_i` so that the *full*
   reduced cost `d_j = d_j' - a_ij·y_i` reads as the checker's `sign_condition`
   (`check.c:422-481`, read this session) expects for `x_j`'s recovered basis status — which
   generally means solving `d_j = 0` for `y_i` (making `x_j` effectively basic in the original
   problem, since its bound was an artifact of the removed row) when the row was what pinned it,
   or `y_i = 0` when it was not. **This formula is presented at the conceptual level only — its
   precise form must be worked out and checked against `check.c`'s actual sign conventions (row
   dual `w>0 ⇒ at lower`, `w<0 ⇒ at upper`, quoted verbatim below) during implementation, and is
   exactly the class of defect D-10's "hand postsolve a deliberately broken index map" test and the
   `numerics-reviewer` task exist to catch.**
4. **Basis status.** For a removed row/column, its `jaos_basis_status` (`JAOS_BASIS_BASIC` /
   `AT_LOWER` / `AT_UPPER` / `JAOS_BASIS_FREE` — [VERIFIED: include/jaos.h:459-462] `JAOS_BASIS_BASIC
   = 0, JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_UPPER, JAOS_BASIS_FREE, /* nonbasic at zero, both bounds
   infinite */`) must be assigned consistently with steps 1–3: a variable whose bound was an
   artifact of a removed row is typically restored as basic; a fixed variable's status can be
   either `AT_LOWER` or `AT_UPPER` since both are the same point (`check.c`'s own comment: `fixed
   -> anything`, confirmed in `jaos_internal.h`'s `sign_condition` documentation, quoted below).
   **The row-count invariant `jaos_set_basis` enforces — exactly `num_row` basic entries — [VERIFIED:
   src/model.c:537, `if (basic != m->num_row)`] — must hold on the postsolved basis in original
   indices, which is a structural check the plan can assert directly without needing the checker at
   all.**

**What the checker actually requires, quoted verbatim so the recovery obligation above is checkable
against it** [VERIFIED: src/jaos_internal.h:147-155 for the status enum, src/check.c:422-481 for
the sign condition — both read this session]:

```c
/* jm_var_status, jaos_internal.h:150-155 */
typedef enum {
    JM_BASIC = 0,
    JM_AT_LOWER,
    JM_AT_UPPER,
    JM_FREE,        /* nonbasic at zero, both bounds infinite */
} jm_var_status;
```
```c
/* sign_condition's documented rule, jaos_internal.h comment above jm_dual_simplex
   and check.c:422-481 implementation — minimize-canonical:
   at lower  -> w >= 0
   at upper  -> w <= 0
   interior  -> w == 0   (complementary slackness)
   fixed     -> anything                                                    */
```

This is the exact target every postsolved dual/reduced-cost value must satisfy in the *original*
problem's rows and columns, since `jaos_check_solution` (D18) reads only `m->a_start`/`a_index`/
`a_value`/`col_lower`/`col_upper`/`row_lower`/`row_upper` — the model as loaded — and has no
awareness that presolve ever ran.

## Infeasibility and Unboundedness Paths

- **Infeasibility.** Three of the eight families can prove infeasibility directly (empty row with
  `0 ∉ [row_lower_i, row_upper_i]`; singleton row whose implied bound conflicts with `x_j`'s
  existing bound beyond tolerance; a forcing-constraint row whose min activity already exceeds its
  upper bound or max activity already falls below its lower bound). When any of these fires,
  `jm_dual_simplex` should report `JAOS_SOLVE_INFEASIBLE` without ever calling `sx_init` or `run`.
  This interacts with the 29-instance infeasible reference set as follows: `bench/run.c`'s
  `run_one_infeasible` (read this session, `bench/run.c:380-459`) checks
  `expected=infeasible verdict=ok det=ok` and **never computes or requires a digest** — confirmed
  by `bench/README.md`'s explicit statement that the infeasible set "carries no `digest=` field at
  all." A presolve-only infeasibility verdict is compatible with this predicate as written. It
  does change `iters` (likely to 0) and `work` (to whatever presolve itself billed) on any of the
  29 where presolve alone suffices to prove infeasibility — the plan should call this out explicitly
  as an expected, deliberate change in the campaign diff rather than let it look like a regression.
- **Unboundedness.** REQ-presolve's eight families do not include a dual-fixing or ray-detection
  rule (those are the "reductions that need the dual" the CONTEXT explicitly defers). The one case
  where presolve alone can prove unboundedness without a ray is an empty column whose favorable
  bound is `±∞` (see the Empty column row in the table above) — this is a genuine, sound
  unboundedness proof and should report `JAOS_SOLVE_UNBOUNDED` directly. Every other unboundedness
  case remains the simplex's job (`jm_harris_pick`/the ray-based verdict D19 already implements) —
  presolve should not attempt to detect unboundedness from any of the other seven families; doing
  so is out of scope and not asked for by REQ-presolve.

## Fixed-Point Iteration (D-02)

**In-tree precedent, read this session and quoted verbatim** [VERIFIED: src/check.c:246-264]:
```c
/* The cap on propagation rounds. Not a quality knob: the loop exits as soon
 * as a round bounds nothing new, so this is the safety stop, and it is set
 * where the propagation reaches its fixed point rather than anywhere useful
 * work is still being cut off.
 *
 * Swept over the standard set, counting certified answers — the canary that
 * had to move, and did:
 *
 *   rounds    1    2    4    8   16   32   64  128
 *   certified 17   23   32   38   46   47   48   48
 *
 * 64 is where it stops changing. The cost is flat across the whole sweep,
 * 119 s to 128 s against a gate that takes about 120 s, so the passes are
 * indistinguishable from noise and there is nothing to trade against.
 *
 * 8 was here first, chosen by nothing, and it left ten answers uncertified
 * (D91). */
constexpr int64_t IMPLIED_ROUNDS = 64;
```
This is a **checker diagnostic's** round cap (how many rounds of activity-range propagation the
checker runs to certify a dropped dual term), not a presolve reduction's round cap — the two loops
are structurally the same algorithm (row activity ranges propagating into tighter column bounds,
iterated to a fixed point) applied for different purposes. D-02 explicitly asks presolve to follow
this *precedent* — the sweep methodology and the canary discipline — not to reuse the constant
itself; presolve's own cap is a new sweep over a new canary (rows/columns removed, or total work
reduced, rather than "certified answers").

**What the literature says about round counts, and what it does not** [ASSUMED — general knowledge,
not independently verified against a specific paper's numeric table this session]: cascading
reductions (a singleton-row elimination creating a new singleton row, or a fixed variable enabling
a duplicate-row match) typically converge within a small number of rounds on well-structured
instances, but the *worst case* is a chain where each round resolves exactly one row or column —
bounded only by `num_row + num_col`, not by a small constant. A cap chosen without a sweep risks
leaving value uncollected on precisely the instances (long chains) where presolve would otherwise
pay the most. The `IMPLIED_ROUNDS` sweep (1, 2, 4, 8, 16, 32, 64, 128) is a directly reusable sweep
grid for the new cap; the canary discipline `jaos-measure` demands (a result too clean is a broken
instrument) applies here specifically: **the sweep must include a model constructed to cascade
through many rounds** (e.g., a chain of `n` singleton rows each fixing the variable the next row's
singleton depends on) so the sweep's curve is forced to move at low round counts and confirms the
instrument is measuring cascading rather than reporting a flat line because nothing in the standard
set actually cascades past round 2.

## Numerical Hazards Specific to Presolve

**Presolve is a third tolerance space, and no existing constant belongs to it.** [VERIFIED:
src/simplex.c:569-570, quoted] `s->primal_tol = m->cfg.primal_tol > 0.0 ? m->cfg.primal_tol :
PRIMAL_TOL; s->dual_tol = m->cfg.dual_tol > 0.0 ? m->cfg.dual_tol : DUAL_TOL;` — these are resolved
inside `sx_init`, which per D-04 runs **after** presolve. Presolve cannot read `s->primal_tol`/
`s->dual_tol` because `sx` does not exist yet when presolve runs; even if it could, those are
scaled-space magnitudes (`docs/tolerances.md`: "the solver runs on a scaled copy... its tolerances
are magnitudes in scaled space") and D-04's whole rationale is that presolve must not depend on a
scaling computed from a matrix presolve has not yet finished changing. The checker's `tol`
(`jaos_check_solution`'s caller-supplied parameter) is equally unusable — it is a caller's
diagnostic choice for judging a *finished* solution, not a constant available inside a solve, and
it was never measured for deciding whether to fold a bound.

**Concretely, presolve needs its own new constants, and none currently exist in
`docs/tolerances.md`:**
- A "is this bound-tightening improvement worth taking" epsilon (guards against tightening on
  rounding noise, which — per `fp-numerics`'s cancellation discussion — is a real risk when an
  activity range is a sum of many terms of differing magnitude, the same `finnis`-shaped hazard
  `docs/tolerances.md` documents for the checker's row-scale argument).
- A duplicate-row/duplicate-column detection tolerance (comparing `a_kj` against `λ·a_ij` for a
  candidate scalar `λ` derived from floating-point coefficients cannot require bit-exact equality;
  needs a relative tolerance, and per `fp-numerics`'s "prefer a quantity that has no space at all"
  guidance, the ratio `a_kj / a_ij` compared across a row's entries is scale-invariant in a way an
  absolute difference is not).
- A dominated-column comparison tolerance (comparing `c_j ≤ c_k` and `a_ij ≥ a_ik` for every row —
  each comparison needs the same kind of relative-tolerance treatment, and a partial-domination
  check that a numerical near-tie flips the wrong way is silent and answer-changing, not merely
  a missed opportunity).

**Bound tightening can cut off the optimum if it accumulates rounding — this is the specific hazard
the research priorities name, and the mitigation pattern already exists in the tree as a diagnostic
(not yet as a corrective action):** `src/check.c`'s `implied_bounds` (read this session,
`check.c:304-420`) only overwrites a bound when the new value is *strictly* tighter (`if (want_up
&& (double)lim < cu[j])`) and is monotone (only ever tightens, never loosens, `moved` flag). A
presolve reduction that *actually fixes the feasible region* (unlike the checker's read-only
diagnostic) carries higher stakes than the checker's use of the same math: a checker bound that is
slightly too tight only under-certifies a dual term (D87's finding — over-tightening there rejected
a *correct* answer, `pilot`, at some propagation intervals, which is why D91 restricted the
verdict-relevant gap to bounds the model declared). A presolve bound that is too tight **changes
the feasible region the simplex searches**, and can silently exclude the true optimum. Recommend
the plan require: (a) any bound tightening be applied with a conservative rounding direction
(round the new bound *away* from tightening by at least one measured epsilon, never toward it), and
(b) the D-10 round-trip test for bound tightening specifically construct a case where the true
optimum sits exactly on the boundary the tightening computes, to confirm it is not excluded.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Unity (vendored, `tests/vendor/`) — [VERIFIED: CLAUDE.md, "Unity for the test suite" is one of the two closed exceptions to the no-dependencies rule] |
| Config file | `Makefile` (test target), no separate test framework config file |
| Quick run command | `wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && make test"` |
| Full suite command | Same as quick run — the unit suite is fast; the *campaigns* (`make netlib*`) are the slow, separate layer |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| REQ-presolve (criterion 1: same verdict/objective with reductions on or off) | Each of the 139 reference instances, solved twice (presolve on, presolve off), agree on verdict and are each within Koch tolerance of the reference | integration / campaign | `wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/... && make netlib J=12 && make netlib-infeas J=12 && make netlib-kennington J=12"` | ✅ exists (`bench/run.c`, `bench/netlib*.manifest`) — needs a presolve-off build variant to compare against, not a new file |
| REQ-presolve (criterion 2: postsolve returns values/activities/duals/reduced-costs in original indices, checker accepts all 139) | `jaos_check_solution` (unmodified, D-11) accepts every postsolved answer | integration / campaign | Same `make netlib*` runs above — `checker=ok` column in the record | ✅ exists |
| REQ-presolve (criterion 2, unit-level) | Postsolve maps a broken index correctly-shaped-but-wrong and the checker rejects it | unit, negative case (D-10) | `wsl ... make test` (new test file/cases) | ❌ Wave 0 — one round-trip test per family plus its "must fail first" companion |
| REQ-presolve (criterion 3: each reduction reports what it removed) | Per-family counters populate and are readable in a test | unit, white-box (D-13) | `wsl ... make test` | ❌ Wave 0 — depends on the counter struct existing in `jaos_internal.h` |
| REQ-presolve (criterion 4: determinism across two solves, basis cleared between) | Status, iteration count, work units and every published value's bits agree across two solves of the same (post-presolve) instance | integration / campaign, already largely covered — see finding below | Same `make netlib*` runs; `det=ok` column | ✅ **already checks iters/work/digest as one precondition, not three separate ones — see Code Insights**; extend only if presolve introduces a path (e.g. presolve-only infeasibility) not already exercised by the existing `run_one`/`run_one_infeasible` double-solve |
| D-09 negative control | Presolve compiled off leaves all 139 digests bit-identical to today's committed baselines | integration / campaign | `make netlib*` built with `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` (or whatever guard name the plan picks) against `bench/netlib*.baseline` | ❌ Wave 0 — the guard macro itself is new; the comparison mechanism (`-b`/baseline diff) already exists |
| D-15 deliverable (geometric mean, negative control instances read 1.00x) | Presolve-on vs presolve-off ratio over the standard set, `J=1`, with instances presolve removes nothing from reading 1.00x | measurement, not a pass/fail test | `.claude/skills/jaos-measure/scripts/geomean.py --metric work old.txt new.txt` (existing script) plus a `.planning/phases/02-.../<plan>-MEASUREMENT/` raw-reading directory (`bench/README.md`'s "a verdict commits its readings" rule) | ✅ script exists; the raw-reading directory is a new artifact per plan |

### Sampling Rate
- **Per task commit:** `wsl ... make test` (unit suite; seconds)
- **Per wave merge / reduction added:** `make netlib J=12` (~85 s) at minimum; the phase's own D-09
  discipline means **all three sets** (`netlib`, `netlib-infeas`, `netlib-kennington`) should run
  before any reduction is considered landed, per `jaos-measure`'s "run all three, let the largest
  decide" rule — a change that looks perfect on the standard and infeasible sets has previously
  been caught costing 3.2x work only on Kennington-scale instances.
- **Phase gate:** All three sets green (presolve on) **and** all three sets bit-identical to the
  current baselines (presolve off, D-09) before `numerics-reviewer` runs on the finished diff,
  before `jaos-measurer` runs as the verdict step, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `tests/test_presolve.c` (or an extension of an existing file — Claude's discretion, D-10) —
      one round-trip test per reduction family, each validated to fail on a deliberately broken
      postsolve index map before its passing counts, following the exact pattern already
      established in `tests/test_check.c`'s `test_t1_flags_wrong_dual_sign` /
      `test_t1_flags_complementarity_break` / `test_t1_flags_primal_violation` (read this session,
      `tests/test_check.c:69-117`) — that precedent is the in-tree template for "build the case it
      must reject."
- [ ] Per-family counter struct in `src/jaos_internal.h` (D-13) — needed before any white-box test
      can read it.
- [ ] `JAOS_NO_PRESOLVE`-style build guard (name at Claude's discretion) wired into `Makefile` via
      the existing `EXTRA_CFLAGS` hook (`Makefile:94-101`, confirmed this session) — needed before
      D-09's negative control can be built.
- [ ] `.planning/phases/02-presolve-and-postsolve/<plan>-MEASUREMENT/` directory convention for the
      plan whose deliverable is the D-15 geometric mean — not a test, but a Wave 0 process gap: the
      raw timing logs and the analysis script must be committed per `bench/README.md`'s rule, and
      phase 1's own history (12 timing logs and 2 callgrind annotations never committed, recovered
      only because a scratchpad happened to still have them) is the receipt for why this cannot be
      skipped.

## Common Pitfalls

### Pitfall 1: A postsolve that recovers the primal correctly and the dual wrongly
**What goes wrong:** The solution "looks right" — correct objective, correct primal values, passes
a casual glance — but a dual value or reduced cost for a postsolved row/column violates a sign
condition or breaks complementary slackness. This is the exact defect shape the phase boundary
names as the thing D-11's checker *would* catch but only because the checker verifies dual sign
conditions and complementary slackness independently.
**Why it happens:** Primal recovery is arithmetic (step 1 in Postsolve Correctness above); dual
recovery is a *choice* among values that are all consistent with the reduced problem's solve but
not all consistent with the original problem's optimality conditions (step 3). It is easy to
implement "any dual value that doesn't crash" and have it silently be the wrong one.
**How to avoid:** Never trust a postsolve formula from research or memory without the D-10
round-trip test exercising it, and never let a round-trip test pass without first confirming it can
fail (feed it a deliberately wrong index map or a deliberately wrong dual-recovery formula and
confirm the checker rejects it, per `jaos-testing`'s "validate the instrument before believing it").
**Warning signs:** A round-trip test that passes on the first attempt with no failing case ever
constructed; a postsolve formula that was written by pattern-matching against a reduction's *primal*
recovery rather than derived from the checker's actual `sign_condition` rule.

### Pitfall 2: Reusing an existing tolerance for a presolve decision
**What goes wrong:** `PRIMAL_TOL`, `DUAL_TOL`, or the checker's caller-supplied `tol` get reused
inside `src/presolve.c` for a bound-tightening or duplicate-detection decision, because they are
"already there" and superficially the right order of magnitude.
**Why it happens:** Under time pressure, an existing constant looks safer than a new, unmeasured
one — but per the Numerical Hazards section above, none of the existing constants live in
presolve's tolerance space (unscaled, pre-scaling, model-as-loaded), and D-04's entire rationale is
that this space does not convert cleanly to or from the other two.
**How to avoid:** Every new presolve constant gets its own sweep and its own `docs/tolerances.md`
entry, following the `IMPLIED_ROUNDS` precedent's format (value, what it decides, the sweep that
set it).
**Warning signs:** A presolve constant with no comment citing a sweep; a presolve constant whose
value exactly matches `PRIMAL_TOL`/`DUAL_TOL`'s `1e-7` with no independent justification.

### Pitfall 3: Treating bench/run.c's determinism check as needing a new field when it may already suffice
**What goes wrong:** A plan task is written to "add iteration-count and work-unit checks to the
double-solve determinism logic in `bench/run.c`," duplicating logic that (as read this session)
already exists.
**Why it happens:** CONTEXT.md's D-12 description ("it currently requires bit-identical digests...
it must also require equal iterations and equal work units") reads as though the check does not
exist yet.
**How to avoid:** Re-read `bench/run.c:437-438` (infeasible path) and `:574-580` (optimal path)
before writing the D-12 task; confirm whether presolve introduces any *new* path through
`jm_dual_simplex` (e.g., the presolve-only-infeasibility short-circuit in Pattern 3 above) that
escapes the existing double-solve wrapper, and scope the task to that gap specifically rather than
re-implementing existing logic.
**Warning signs:** A diff to `bench/run.c` that duplicates an `iters == iters && work == work`
condition already present a few lines away.

## Code Examples

### The postsolve replay's natural insertion point
```c
/* Source: src/simplex.c:3641-3760, publish(), read this session.
 * The existing scaled -> original UNIT conversion, which is the direct
 * precedent for where a reduced -> original INDEX conversion (postsolve
 * replay) belongs -- either just before this function's body, or folded
 * into it as an additional pass over the (still-reduced) sol_* arrays
 * before the scaled-to-original unit conversion loops run. */
static jaos_status publish(sx *s, jaos_solve_status status)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    m->solve_status = status;
    m->solve_iters = s->iters;
    ...
    /* existing: scaled -> original units */
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col[j] = published(gamma[j] * var_value(s, j));
    ...
}
```
A presolve-aware `publish()` needs the reduced model's `sol_col`/`sol_row`/`sol_dual`/`sol_redcost`
arrays (sized by the *reduced* dimensions) as an intermediate step, then the postsolve arena replay
(D-07, LIFO) expands them into arrays sized by `m->num_col`/`m->num_row` (the *original*
dimensions) before this function's existing scaled-to-original conversion (which, unchanged, still
runs on whichever entries came from the reduced solve — postsolve-restored entries are already in
original units and must not be re-scaled).

### The row-count basis invariant a postsolved basis must satisfy
```c
/* Source: src/model.c:507-544, jaos_set_basis, read this session.
 * Not called by presolve directly (it validates a CALLER-supplied basis),
 * but the invariant it enforces -- exactly num_row basic entries -- is
 * exactly the invariant a postsolved basis must also satisfy, and can be
 * asserted directly in a round-trip test without needing the checker. */
    int64_t basic = 0;
    for (int64_t j = 0; j < m->num_col; j++) {
        ...
        basic += col_status[j] == JAOS_BASIS_BASIC;
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        ...
        basic += row_status[i] == JAOS_BASIS_BASIC;
    }
    if (basic != m->num_row) {
        jm_set_err(m, "a model with %lld rows needs %lld basic variables, "
                      "not %lld", ...);
        return JAOS_ERR_INVALID_INPUT;
    }
```

### The existing double-solve determinism check (D-12's actual current state)
```c
/* Source: bench/run.c:568-581, read this session. Already requires
 * iteration-count and work-unit equality as a PRECONDITION of computing
 * det, before the digest comparison even runs. */
    jaos_clear_basis(m);
    st = jaos_solve(m);
    double obj2 = 0.0;
    (void)jaos_objective(m, &obj2);
    uint64_t d2 = 0;
    bool det = false;
    if (st == JAOS_OK && jaos_status_of(m) == ss &&
        jaos_iterations(m) == iters && jaos_work_units(m) == work &&
        memcmp(&obj, &obj2, sizeof obj) == 0 &&
        jaos_solution(m, x, nullptr, y, nullptr) == JAOS_OK) {
        d2 = digest(x, nc, 1469598103934665603u);
        d2 = digest(y, nr, d2);
        det = (d1 == d2);
    }
```
The infeasible-set path has the analogous check at `bench/run.c:437-438`:
```c
    jaos_clear_basis(m);
    st = jaos_solve(m);
    bool det = (st == JAOS_OK && jaos_status_of(m) == ss &&
                jaos_iterations(m) == iters && jaos_work_units(m) == work);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|---------------|--------|
| No presolve — every instance goes to the simplex at its full size | A presolve/postsolve pair, switched by build flag, measured against presolve-off as the negative control | This phase | Changes what T2 in `bench/compare`'s ladder measures for JAOS going forward (`bench/compare/README.md:67-72`: "When JAOS gains a presolve, presolve moves into T0 for everyone and a new rung appears above" — recalibration itself is phase 5's work, D-16) |

**Not deprecated by this phase, stated because the roadmap and STATE.md currently say otherwise and
should be corrected (per CONTEXT.md's "Specific Ideas"):** the open question "how is the comparison
ladder recalibrated once JAOS has a presolve" is **already answered on disk** at
`bench/compare/README.md:67-72` (read this session, quoted above) — it is not an open research
question for this phase to resolve, only a stale cross-reference in `ROADMAP.md`/`STATE.md` for
someone to fix (out of this phase's scope per D-16, but worth flagging so the planner does not
accidentally schedule work to "answer" a question that already has a documented answer).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The exact dual/reduced-cost recovery formula for each of the 8 reduction families (e.g., "set `y_i` by solving `d_j = 0`" for a singleton row) | Postsolve Correctness | HIGH — this is precisely the defect shape the phase boundary calls the correctness risk. Must be independently derived against `check.c`'s actual `sign_condition` and validated by a round-trip test built to fail first (D-10), and reviewed by `numerics-reviewer` before any campaign |
| A2 | Empty rows/columns are rare on the standard 94 Netlib instances as originally authored | Reduction Families table | LOW — affects only expected payoff estimation, not correctness; if wrong, the reduction still fires correctly, it is just more valuable than expected |
| A3 | Presolve round-count convergence behavior on Netlib-class instances ("typically converges within a small number of rounds; worst case bounded by num_row+num_col") | Fixed-Point Iteration | MEDIUM — if the real convergence behavior is slower than assumed, D-02's round cap sweep might need a wider grid than `IMPLIED_ROUNDS`'s 1..128; if faster, no harm, the sweep will show it |
| A4 | The literature's general claim that all 8 reductions "pay" on LP instances generally (not a specific number for JAOS or Netlib) | Reduction Families | LOW — this is explicitly framed as not-a-target already; the risk is only that a plan quotes a literature number as if it were a JAOS measurement, which the phase's own D-15 framing already guards against |
| A5 | A presolve-only infeasibility short-circuit (bypassing `sx_init`/`run` entirely) is compatible with `bench/run.c`'s existing infeasible-path predicate as written | Infeasibility and Unboundedness Paths | MEDIUM — verified this session that the predicate does not require a digest or a minimum iteration count, but not verified that no *other* part of the harness (e.g. `jaos_iterations`/`jaos_work_units` being read before a solve completed normally) assumes a non-zero iteration count somewhere unread this session |

**None of these are compliance, retention, or security-standard claims** — they are LP-numerics and
codebase-behavior claims, and each is routed to the phase's own review agents (`numerics-reviewer`,
D-10's round-trip tests) rather than presented as settled.

## Open Questions

1. **Which family is D-01's "trivially correct" first reduction?**
   - What we know: fixed variables has the simplest postsolve (primal recovery is a constant; the
     only dual obligation is that `sign_condition`'s `fixed -> anything` rule means *no* sign
     constraint applies, which is the smallest possible surface for a first proof). Empty
     row/column removal is even simpler mechanically but is expected (A2) to fire on almost none of
     the 139 instances, which would make D-01's "proved end to end over all 139 instances" a weak
     proof if nothing actually reduces.
   - What's unclear: whether "fires on nothing" is acceptable for D-01's first reduction (the
     CONTEXT text says "proved end to end over all 139 instances with almost nothing removed,"
     which reads as *expecting* a low-yield first reduction) or whether the scaffolding proof needs
     at least one instance where the reduction visibly fires to be a real proof.
   - Recommendation: pick fixed variables (already-fixed columns, i.e. `col_lower_j==col_upper_j`
     as loaded, with no bound tightening needed to produce them) — it very likely fires on at least
     a few of the 139 real-world instances (fixed variables are common in practical LP models) while
     keeping the postsolve obligation minimal, satisfying both readings.

2. **Does the postsolve replay belong inside `publish()` or as a distinct function called from
   `jm_dual_simplex` around `publish()`?**
   - What we know: `publish()` already does scaled→original unit conversion; D-08 says the postsolve
     stack is consumed "before `publish` returns."
   - What's unclear: whether postsolve replay should run *before* `publish()`'s existing loops
     (expanding reduced-indexed arrays to original-indexed ones first, then letting the existing
     unit-conversion loops run over the now-original-sized arrays unchanged) or *interleaved* with
     them (each loop iteration checks whether index `j`/`i` was reduced-away and looks it up in the
     postsolve arena instead).
   - Recommendation: prefer "before, as a distinct pass" — it keeps `publish()`'s existing,
     already-tested unit-conversion logic untouched in shape, which is lower-risk than interleaving
     new index-mapping logic into loops the rest of the solver already depends on being simple.

3. **What is the actual measured yield of each reduction family on the standard 94, and in what
   order should D-13's counters justify adding families 2 through 8?**
   - What we know: D-01 ships the scaffolding plus one reduction first; the remaining order is
     Claude's discretion "subject to D-13 making that order derivable from measurement."
   - What's unclear: this cannot be answered by research — it requires running the built
     scaffolding against the standard set with each candidate reduction added in turn, which is
     execution-phase work, not planning-phase research.
   - Recommendation: the plan should build reductions in small batches (e.g., empty+singleton
     rows/columns together as the next logical group after fixed variables, since they share the
     row-activity-range machinery with bound tightening/forcing/redundant), measure after each
     batch, and let D-15's geometric mean at each stage inform whether the next batch is worth the
     postsolve risk it adds.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| WSL (Ubuntu-24.04) + GCC 14+ | All builds and tests (CLAUDE.md: "The Windows side has no compiler") | Assumed ✓ (project's documented, working build path; `test_command` in `.planning/config.json` targets it) | GCC 14 minimum per CLAUDE.md | None — build and test are WSL-only by project rule, no fallback exists or is wanted |
| `make` (`netlib`, `netlib-infeas`, `netlib-kennington` targets) | The campaigns this phase's D-15/D-09 measurements depend on | Assumed ✓ (existing, working `bench/` infrastructure, unmodified by this phase's scope) | — | None needed |
| `.claude/skills/jaos-measure/scripts/{preflight,record_diff,geomean}.py` | Measurement protocol (`jaos-measure` skill) | ✓ confirmed present via skill load this session | Python 3, dev-time only | None needed — these are the required instruments, not optional tooling |
| Unity test framework (vendored) | Unit tests, including the new round-trip tests | ✓ confirmed present, `tests/vendor/` | Vendored, pinned per CLAUDE.md's closed exception | None needed |

No missing dependencies. This phase adds no new external dependency of any kind.

## Sources

### Primary (HIGH confidence — read directly this session)
- `C:/Users/vall-/Desktop/projectes/jaos/.planning/phases/02-presolve-and-postsolve/02-CONTEXT.md` — locked decisions D-01 through D-16
- `C:/Users/vall-/Desktop/projectes/jaos/.planning/REQUIREMENTS.md` — REQ-presolve's full text and acceptance state
- `C:/Users/vall-/Desktop/projectes/jaos/.planning/STATE.md` — project history, current phase state
- `C:/Users/vall-/Desktop/projectes/jaos/src/jaos_internal.h` — `jm_var_status`, `jm_config`, `struct jaos_model`, work-unit machinery, allocation primitives
- `C:/Users/vall-/Desktop/projectes/jaos/src/model.c` — `jaos_load_lp`, `jaos_set_basis`, `model_matrix_is_stale`, CSR mirror
- `C:/Users/vall-/Desktop/projectes/jaos/src/check.c` — the independent checker, `implied_bounds`, `sign_condition`, `IMPLIED_ROUNDS`
- `C:/Users/vall-/Desktop/projectes/jaos/src/simplex.c` — `sx_init`, `build_warm_basis`, `publish`, `jm_dual_simplex` (lines 500-1000, 3620-3830 read directly)
- `C:/Users/vall-/Desktop/projectes/jaos/bench/run.c` — the double-solve determinism check, record format (lines 380-620 read directly)
- `C:/Users/vall-/Desktop/projectes/jaos/bench/README.md`, `bench/compare/README.md` — the gate, the ladder, the recalibration rule
- `C:/Users/vall-/Desktop/projectes/jaos/docs/tolerances.md`, `docs/work-units.md` — the frozen constants and billing rules
- `C:/Users/vall-/Desktop/projectes/jaos/include/jaos.h` — `jaos_solve_status`, `jaos_basis_status`, `jaos_check_report` (lines 560-640 read directly)
- `C:/Users/vall-/Desktop/projectes/jaos/DECISIONS.md` — D8, D16, D17, D18, D64, D81 read in full this session
- `C:/Users/vall-/Desktop/projectes/jaos/Makefile` — `EXTRA_CFLAGS` hook (lines 85-109)
- `C:/Users/vall-/Desktop/projectes/jaos/tests/test_check.c`, `tests/test_simplex.c` — existing test patterns, "build the case it must reject" precedent
- `~/.claude/skills/sparse-simplex-perf/SKILL.md`, `jaos-measure/SKILL.md`, `fp-numerics/SKILL.md`, `jaos-testing/SKILL.md` — loaded in full this session

### Secondary (MEDIUM confidence — WebSearch, verified against publisher/archive this session)
- Andersen, E.D. and Andersen, K.D. (1995), "Presolving in linear programming," Mathematical
  Programming 71, 221–245. [springer link `10.1007/BF01586000`, confirmed]
- Gondzio, J. (1997), "Presolve analysis of linear programs prior to applying an interior point
  method," INFORMS Journal on Computing 9(1), 73–91. [`pubsonline.informs.org/doi/abs/10.1287/ijoc.9.1.73`, confirmed]
- Brearley, A.L., Mitra, G. and Williams, H.P. (1975), "Analysis of mathematical programming
  problems prior to applying the simplex algorithm," Mathematical Programming 8, 54–83. [springer
  link `10.1007/BF01580428`, confirmed]

### Tertiary (LOW confidence — general/folklore, flagged inline as [ASSUMED])
- The exact per-family dual/reduced-cost recovery formulas (Assumptions Log A1)
- General claims about round-count convergence behavior on Netlib-class instances (A3)
- The relative rarity of empty rows/columns on the standard 94 as originally authored (A2)

## Metadata

**Confidence breakdown:**
- Standard stack: N/A — no external packages, project rule (D2/D11)
- Reduction family identification and literature sourcing: HIGH — three independent, publisher-
  verified sources agree on the same eight-family taxonomy
- Postsolve dual/reduced-cost recovery formulas: LOW — conceptual only, explicitly flagged for
  independent derivation and the two mandatory review agents
- Architecture (where presolve/postsolve hooks into `jm_dual_simplex`/`publish`): HIGH — based on
  source read directly this session, not inference
- Fixed-point round cap: MEDIUM — the sweep methodology is HIGH-confidence precedent
  (`IMPLIED_ROUNDS`, read directly), the expected convergence behavior on Netlib-class instances is
  LOW-confidence (A3)
- Validation architecture / D-12 finding: HIGH — `bench/run.c`'s existing determinism check was
  read directly and quoted verbatim

**Research date:** 2026-08-12
**Valid until:** No expiry driven by external ecosystem change (no external dependencies); revalidate
if `src/simplex.c`'s `sx_init`/`build_warm_basis`/`publish` or `bench/run.c`'s double-solve logic
change materially before this phase is planned or executed, since several findings here (especially
the D-12 finding) are line-anchored to the current tree.
