# Phase 1: Candidate admission in the ratio test - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-08-12
**Phase:** 1-Candidate admission in the ratio test
**Areas discussed:** What is being attacked, Trajectory and digests, What evidence closes D93, The case Harris must reject

---

## What is being attacked

| Option | Description | Selected |
|--------|-------------|----------|
| The dense O(`nvar`) scan | Nonbasic list in ascending order gives the same set in the same positions — trajectory intact, Harris untouched | ✓ |
| Restrict the candidate set | Filter harder ahead of `bfrt_walk`/`jm_harris_pick`; the path PLAN.md flags as risking Harris's guarantees | |
| Widen the hyper-sparse path | Make `s->anpat >= 0` hold more often; overlaps Phase 3 | |
| Measure all three | Start the phase as a comparison rather than a chosen path | |

**User's choice:** The dense O(`nvar`) scan
**Notes:** Chosen over the higher-ceiling option deliberately. The two adjacent
pricing schemes were already refused on correctness (D82, D84), and this is the
half neither touched. Restricting the candidate set is recorded as deferred
rather than refused — it stays available as its own decision later.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Close it as refused | A `DECISIONS.md` entry saying it does not pay and what it cost to learn that, like D82/D84 | ✓ |
| Try the next path | The phase does not end until something pays | |

**User's choice:** Close it as refused
**Notes:** Prevents Phase 1 becoming an open-ended funnel in front of the other four.

---

## Trajectory and digests

| Option | Description | Selected |
|--------|-------------|----------|
| Identical or it is wrong | The change is designed as an observable no-op; a moved digest means it touched something it should not have | ✓ |
| Allow a different trajectory | Room for variants that reorder the visit, at the cost of the cheapest detector available | |

**User's choice:** Identical or it is wrong
**Notes:** Makes verification total and free.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Charge what is visited, rewrite the baseline | The counter tells the truth again; baselines rewritten deliberately as a step of the phase | ✓ |
| Keep charging `nvar` | Baselines stay put, but the improvement becomes invisible in the project's own currency | |
| Decide by measurement | Measure both accountings and let D93 reason it out | |

**User's choice:** Charge what is visited, rewrite the baseline
**Notes:** Surfaced from `src/simplex.c:1576`, which charges `s->nvar *
JM_WORK_NONZERO` — per variable visited, not per candidate admitted. The
ordering safeguard (digests authorise the rewrite, never the reverse) was
derived during the discussion and recorded as D-06.

---

## What evidence closes D93

| Option | Description | Selected |
|--------|-------------|----------|
| Time at `J=1` decides, callgrind explains | D45 as written; callgrind cannot see locality, which is where this is won or lost | ✓ |
| Callgrind only | Perfectly reproducible, but fewer instructions is not faster | |
| Both, and they must agree in sign | Stricter; a divergence becomes a finding to explain | |

**User's choice:** Time at `J=1` decides, callgrind explains

---

| Option | Description | Selected |
|--------|-------------|----------|
| Standard set, geometric mean | D46 — never a sum; `truss` reported separately but does not decide alone | ✓ |
| `truss` and a few others | Faster to iterate; the fit-to-one-instance risk this project names repeatedly | |
| Standard set and Kennington | Adds the large instances, ~8 min more per run | |

**User's choice:** Standard set, geometric mean

---

| Option | Description | Selected |
|--------|-------------|----------|
| 3x repeatability — 4.2% | Derived from an existing measurement rather than chosen by eye | ✓ |
| Anything above 1.4% | Merely clearing the noise floor | |
| No threshold up front | Measure, then let D93 reason about what is enough | |

**User's choice:** 3x repeatability — 4.2%

---

## The case Harris must reject

**Framing note:** choosing the dense scan changed this area's nature mid-discussion.
With the candidate set and its order preserved, Harris's rule is not touched — so
the adversarial case is no longer about Harris but about the *equivalence of the
two scans*.

| Option | Description | Selected |
|--------|-------------|----------|
| Differential equivalence of both scans | Same set AND same array positions; the test is itself validated by handing it a list with a candidate missing | ✓ |
| Unit test with a fabricated row | Precise on boundary cases, covers only the ones imagined | |
| A known Netlib instance | Realistic, and the kind of green this repo has watched fail before | |

**User's choice:** Differential equivalence of both scans

---

| Option | Description | Selected |
|--------|-------------|----------|
| Assert in debug builds | Per iteration, free in release — the D30 lesson | ✓ |
| Test only | Simpler, but equivalence is checked only in states the test reaches | |

**User's choice:** Assert in debug builds

---

| Option | Description | Selected |
|--------|-------------|----------|
| Incremental, maintained at the pivot | One in, one out per iteration; O(1) amortised | ✓ |
| Rebuilt when needed | Harder to desynchronise, returns part of the O(`nvar`) cost | |
| Let the measurement decide | Implement both, let D93 reason | |

**User's choice:** Incremental, maintained at the pivot

---

## Claude's Discretion

- Where the nonbasic list lives in `sx` and how it is represented.
- The exact form of the debug assertion, provided it compares both scans and
  compiles out of release builds.
- Whether the differential test lives in `tests/test_simplex.c` or its own file.

## Deferred Ideas

- Widening the hyper-sparse path — Phase 3, `REQ-hyper-sparse-downstream-results`.
- Restricting the candidate set ahead of the window — available as its own
  decision later; not refused here.
- A deliberate `REFACTOR_EVERY` sweep over 16..256 — roadmap Open Question 5.
