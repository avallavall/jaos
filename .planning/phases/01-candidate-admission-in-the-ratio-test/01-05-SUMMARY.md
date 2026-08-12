---
phase: 01-candidate-admission-in-the-ratio-test
plan: 05
subsystem: the record — DECISIONS.md, CHANGELOG.md, SPECS.md
status: complete
tags: [d93, decisions, changelog, specs, refusal, record, d81, d16, d46, audit-correction]

requires:
  - "01-01 — the bitmap, the eight sites, and the two injected faults"
  - "01-02 — the charge, the D16 definition change, and the four files that cite D93 forward"
  - "01-03 — the campaigns, the 139-instance null result, and the derived dense-call counts"
  - "01-04 — the J=1 time ratio, the callgrind re-read, and the INCONCLUSIVE verdict"
provides:
  - "D93, closed and indexed, with the measurement on both sides and the four required parts"
  - "the 4.2% derivation traced to D81 and reconciled against D60's 1.3%"
  - "the null result stated accurately: 110 digests + 29 infeasibility verdicts over 139 instances"
  - "a CHANGELOG entry that points at D93 rather than arguing"
  - "SPECS.md's warm-against-cold work figure re-read from the records 01-03 produced: 0.0162 -> 0.0164"
  - "a correction appendix on 01-04-SUMMARY.md, attributed to the independent audit"
affects:
  - "the four files citing D93 forward — src/simplex.c, docs/work-units.md, tests/test_simplex.c x2 — now resolve"
  - "Phase 2 onward — every time ratio on this host inherits the negative control D93 records"

tech-stack:
  added: []
  patterns:
    - "the correction to a committed summary is appended and marked, never merged: a document quietly corrected cannot be told from one that was right"
    - "the anchor is derived from the heading by the same slug rule the neighbouring index lines use, and checked by deriving it rather than by eye"

key-files:
  created:
    - .planning/phases/01-candidate-admission-in-the-ratio-test/01-05-SUMMARY.md
  modified:
    - DECISIONS.md
    - CHANGELOG.md
    - SPECS.md
    - .planning/phases/01-candidate-admission-in-the-ratio-test/01-04-SUMMARY.md

decisions:
  - "D93's heading is the decision and it names the finding rather than the verdict: the bar cannot be measured on this host, which is what the negative control establishes and is not the same claim as the candidate missing the bar."
  - "The plan's must_have sentence 'all 139 digests unmoved' was NOT written, because it is false. 110 instances publish a digest and 29 publish an infeasibility verdict instead. The repository's own record already says 110 in four places."
  - "The audit's three corrections went into an appendix on 01-04-SUMMARY.md as well as into D93, so the committed document is not left wrong."
  - "SPECS.md's competitive-gap figures were checked and deliberately NOT touched: bench/compare did not run in this phase, and a figure nobody re-measured must keep the citation that dates it."

metrics:
  duration: "~35 min, no machine time — this plan builds nothing and runs no campaign"
  completed: 2026-08-12

actuals:
  tokens: 16400
  tasks: 2
  commits: 4
---

# Phase 01 Plan 05: D93, and the record it closes on Summary

The phase's first deliverable was a closed decision and it is now its last one:
D93 says the ratio test's dense scan walks the nonbasic set, that it changes no
answer anywhere in the record, that it costs 1.60% more instructions, and that
the 4.2% bar it was to be judged against **cannot be measured on this host** —
which is a different and better-supported statement than the candidate having
missed it.

## D93, and the sentence it is built around

**`## D93 — The ratio test's dense scan walks the nonbasic set, and the bar it
was to be judged against cannot be measured on this host`**

The heading is the decision, not the topic, and the second clause carries the
thing a reader would otherwise get wrong. `01-04` returned INCONCLUSIVE against
4.2% at 0.9709x, and the natural way to write that up is "measured at 2.91%,
below the bar". **The negative control makes that framing indefensible**: eight
instances that provably cannot be sped up by this change read 0.9699x paired
and 0.9356x pooled — a 3.0% to 6.4% "improvement" where the truth is exactly
zero, the same size as the headline. What the phase measured is the host.

Indexed at the top of the file, anchor derived from the heading by the same
slug rule the neighbouring lines use and checked by deriving it rather than
reading it:

```
#d93-the-ratio-tests-dense-scan-walks-the-nonbasic-set-and-the-bar-it-was-to-be-judged-against-cannot-be-measured-on-this-host
```

`git diff --stat DECISIONS.md` reads **428 insertions, 0 deletions**. Nothing
was renumbered and no heading moved, which matters because 79 citations across
29 decisions point at headings in this file.

### The four parts, and where each is

| part | what it carries |
|---|---|
| **the question, with the expectation** | D84's profile — `admit_candidate` at 14.98% on `truss` against `ftran_prefix` at 6.68% — and the expectation stated plainly: a tenth of the calls removed from a function that is a seventh of the program should be visible on a clock. It is not, and the reason is not that the effect is smaller than expected |
| **the measurement, per instance, named** | the null result, the work figures on both sides of the definition change, the time ratio with every instance that moves against the mean named, the threshold derivation, the negative control, and callgrind on both binaries |
| **what was refuted** | the bound-status invariant, the incomplete site table, the silent-`remove` calibration trap, `pivot`'s charge, and the phase's own estimator claim |
| **what is handed onward** | seven items, to `.planning/ROADMAP.md`, since `PLAN.md` is archived |

### The five things the entry was told not to get wrong, and did not

1. **Numbered D93 specifically**, not one past whatever the file ended at.
   It was cited forward from **three files and nine lines**, counted rather
   than carried: `src/simplex.c` 1, `tests/test_simplex.c` 4,
   `docs/work-units.md` 4. All resolve now.
2. **The 4.2% is three times D81's 1.4%**, shown as a derivation rather than
   asserted, with D81 quoted for what its 1.4% actually measures. D83's 1.4% —
   Clp within 1.4% of HiGHS on total time — is named as the different quantity
   it is, because it is what anyone grepping the figure lands on. D60's 1.3% is
   reconciled: three times it is 3.9%, and no reading of the data lands between
   3.9% and 4.2%, so nothing turns on it.
3. **The null result is stated accurately.** See below — the plan's own
   `must_have` asked for a sentence that is false.
4. **The entry says plainly that the change costs instructions.** Program total
   up 1.60% on `truss`, `admit_candidate` shedding 198.8M while the caller it
   inlines into gains 994.9M. It also records that this is the finding the
   "relocation, not a saving" test was built to catch, and that any future
   reading quoting 14.79% → 14.20% alone is reporting the flattering half.
5. **The decision preceded the code, and the entry says where.**
   `01-CONTEXT.md`, 2026-08-12, thirteen numbered items D-01 through D-13,
   committed before `f2ed4bc` existed. D-04 pre-authorised the refusal shape.

### Harris's two passes

Recorded as the roadmap asks: the guarantee is a function of the candidate set
and the arrival order and nothing else, both are preserved by construction
because `admit_candidate`'s body is untouched and the bitmap is walked ascending
in `v`, and **the case the instrument had to refuse was built and confirmed
refused** — `test_nonbasic_notices_a_missed_hook` asserting the bitmap does
*not* match with a hook omitted, and two injected faults with what caught each
named. Including the one whose silence is the finding: a no-op
`jm_nonbasic_remove` leaves a superset, changes no candidate, and the run-time
cross-check is correct to say nothing. The fault the assertion exists for is a
no-op `jm_nonbasic_insert`, which aborts at `src/simplex.c:1687`.

## The changelog entry

Six lines under `## [Unreleased]` → `### Changed`, newest first, with a `(D93)`
pointer and no argument. What it does, the one thing a reader would get wrong,
and the cost:

> the work counts fell because the charge was redefined underneath it, not
> because the solver took a different path

That is the sentence the entry exists for. A reader comparing a work figure
across `b65d9f2` otherwise reads a redefinition as a speedup.

## Every SPECS.md figure that was checked, not only the ones that moved

The instruction was to have looked, so here is the whole sweep.

### Moved, and updated

| figure | before | after | read from |
|---|---|---|---|
| warm/cold **work**, standard set, geometric mean | **0.0162** | **0.0164** | `bench/results/warm.txt:100` |

The row now says why it moved and what it read before, so the third decimal is
not left looking like a regression: a cold solve makes thousands of dense calls
and a warm re-solve makes a handful, so the saving comes mostly off the
denominator. It is the only measured figure in `SPECS.md` this phase moved.

### Checked and confirmed unmoved

| figure | value | read from |
|---|---|---|
| warm/cold **iterations**, standard set | 0.0052 | `warm.txt:99` |
| warm/cold **iterations**, Kennington | 0.0006 | `warm-kennington.txt:21` |
| warm/cold **work**, Kennington | 0.0041 | `warm-kennington.txt:22` |
| 92 of 94 measured, 11 of 16 measured; 0 disagreed, 0 rejected | as stated | both records |
| Netlib standard 94 / Kennington 16 / infeasible 29 | **pass** ×3 | `01-03`, three campaigns |
| Determinism across two solves, all 139 | **pass** | `01-03` |
| ASan+UBSan clean; reader fuzzing | **pass** ×2 | `01-01`, `01-02` |

Kennington's work ratio is the near miss worth naming: its worst instance moved
`pds-06` 0.0326 → 0.0329, and the set's geometric mean still reads 0.0041 to
four places. Checked rather than assumed from the standard set having moved.

### Checked and deliberately left alone

- **Every competitive-gap figure** — T0 against HiGHS 3.72x, SoPlex 1.34x, Clp
  3.77x, the decomposition table (iterations 1.47x / 0.70x / 1.67x, per
  iteration 2.54x / 1.92x / 2.26x), the T1–T3 rungs, the two-targets table
  (`maros-r7` 25.6x, `pilot` 13.4x, `pilot87` 13.2x, `greenbea` 8.1x) and the
  `fit2p` 17.5x → 5.0x / `maros-r7` 16.5x → 11.0x pair. **`bench/compare` did
  not run in this phase**, so none of these was re-measured, and each carries
  the decision citation that dates it. **This is a real caveat and it is worth
  stating**: `01-04` puts the change at up to ~3% on the clock, which is above
  the comparison harness's own 1.4% floor, so these figures are pre-phase
  rather than confirmed current. Inventing a correction for them would be the
  thing CLAUDE.md forbids.
- **The 80–93% steepest-edge weight-discard figure** (D63) — nothing in this
  phase touches pricing weights.
- **The partial-and-multiple-pricing row** (D82, D84) — a different feature,
  and its "measured and refused" status is unchanged by a third measurement in
  adjacent territory.
- **The hyper-sparsity row** (`partial`) — the triangular solves were not
  touched; widening the pattern is Phase 3.

### Changed as a description rather than as a figure

The **dual simplex** row gained a clause: the dense branch enumerates its
candidates from a maintained nonbasic set rather than the whole model, the same
candidates in the same order, which is what keeps it out of what Harris's two
passes guarantee — **and that it is not established that it buys time.** The
clause was added because the verdict left the change in the tree; the last part
was added because a feature row that describes a change without its verdict is
how a refusal quietly becomes a claim.

## Deviations from Plan

### 1. [Rule 1 — the plan's own `must_have` states something false] "All 139 digests unmoved" was not written

- **Found during:** Task 1, checking the claim before repeating it
- **Issue:** The plan's `must_haves.truths` and its Task 1 action both require
  the sentence *"all 139 digests unmoved"*, and the acceptance criteria require
  the entry to "state the count of unmoved digests". **139 is the instance
  count, not the digest count.** Verified directly against the record files:
  `bench/results/netlib.txt` and `netlib-kennington.txt` carry `digest=` on 94
  and 16 lines; `bench/results/netlib-infeas.txt` carries it on **zero** — the
  infeasible set publishes `expected=infeasible verdict=ok det=ok` instead.
- **Corroboration that this is the record's own convention, not a nicety:**
  `DECISIONS.md` already says **110** in four separate entries — "0 of 110
  solution digests", "all 110 digests are unchanged" (twice), "110 digests
  unchanged". Writing 139 would have contradicted four existing entries in the
  file it was being added to.
- **Fix:** D93 states it as *"110 solution digests unmoved and 29
  infeasibility verdicts unmoved, over 139 instances, zero answers changed"*,
  and says outright that 139 is the instance count. The null result loses
  nothing: it gains the per-instance iteration identity on all 139 and the
  masked byte-identical diff across all five records.
- **Files modified:** `DECISIONS.md`
- **Commit:** `f21d15b`

### 2. [Rule 1 — an acceptance criterion whose arithmetic is wrong] The D9x heading count is 5, not 4

- **Found during:** Task 1, running the plan's own check
- **Issue:** The criterion is `grep -c '^## D9' DECISIONS.md` is 4 (D90, D91,
  D92, D93). It returns **5**, and always would have: `## D9 — Primal
  heuristics inside, metaheuristics outside` matches the same pattern.
- **Fix:** The criterion's intent — that no entry was renumbered — is checked
  the way that actually establishes it: `git diff --stat` shows **428
  insertions and 0 deletions**, so no existing line changed at all. The five
  matching headings are D9, D90, D91, D92, D93, at lines 201, 6093, 6170, 6292
  and 6474.
- **Files modified:** none — this deviation is a check replaced by a stronger one

### 3. [Rule 2 — three inherited grounds do not hold] An appendix on `01-04-SUMMARY.md`

- **Found during:** before Task 1, reading the verdict
- **Issue:** An independent `jaos-measurer` audit recomputed `01-04`'s result
  from the 12 raw timing logs and both callgrind annotations. It **accepts the
  INCONCLUSIVE verdict** and reproduces every figure, but three of the stated
  grounds do not hold: the estimator-independence claim is false on the plan's
  literal three-round protocol (5.12% on rounds 4–6 crosses the bar); the
  negative control was in the data and was never run; and the callgrind profile
  contains two solves, so every per-call figure is out by a factor of two and
  the "two instruments agree to the digit" showpiece is two errors cancelling.
- **Fix, in two places.** D93 carries the corrected versions and reproduces
  none of the three claims — the estimator finding is written as *the verdict
  depends on having run six rounds, not on the arithmetic being robust*, the
  per-call figures read 6.0 and 3.41, and the negative control is the section
  the entry's verdict rests on. And **a marked correction section was appended
  to `01-04-SUMMARY.md`**, attributed to the audit, rather than leaving a
  committed document wrong. Appended, not merged: a summary quietly corrected
  cannot be told from one that was right.
- **What survived unchanged:** the +1.60% program total, the 5:1 relocation
  ratio, all six pooled readings, `truss` 1.496 → 1.460, control 4, the
  running-order split and the 24 identical functions. The audit reproduced them
  exactly.
- **Files modified:** `01-04-SUMMARY.md` (appended), `DECISIONS.md`
- **Commits:** `f21d15b`, and this plan's final commit

### 4. [Scope] `01-04-SUMMARY.md` is not in the plan's `files_modified`

The plan lists `DECISIONS.md`, `CHANGELOG.md` and `SPECS.md`. The correction
appendix is a fourth file and a `.planning/` one. It is recorded here rather
than silently included: leaving a committed summary carrying three grounds that
do not hold, while writing a decision entry that carefully avoids them, would
have put the two documents in contradiction with nothing saying so.

### 5. [Rule 2 — a figure the plan did not ask for] The dose-response

The audit produced one reading in the change's favour that `01-04` did not
report: r = +0.684 with a permutation p of 0.0003 over the 25 clock-visible
instances, +0.848 on the 10 largest, against a slope collapsing to +0.15 with a
−2.24% intercept over all 86. **Both halves are in D93.** An entry that carries
only the refuting half of a measurement is the same defect as one that carries
only the flattering half, pointed the other way.

## The broken-windows ledger closes

`.planning/WINDOWS.md` carried exactly one open entry, recorded by `01-04`: its
Task 3 required spawning the `jaos-measurer` subagent for an independent read of
the verdict, that executor's context had no agent-spawning tool, and the
checklist was run inline by the same context that produced the numbers — so an
independent read was still owed.

**It is owed no longer, and the entry is marked fixed.** The audit ran, accepted
the verdict, and refuted three of the grounds first given for it. That the
independent read found real errors is the argument for the ledger: the inline
checklist reproduced every figure and still missed a negative control that was
sitting in the data.

The ledger gains nothing from this plan. `open_count` is 0.

## Known Stubs

None. This plan writes no code. Every claim in D93 traces to a figure in
`01-01` through `01-04`, to a committed baseline, or to a file read during
execution — the digest counts, the row counts, the three `JM_FREE` assignment
sites and the four D93 citations were each read from disk rather than carried.

## Threat Flags

None. Three markdown documents at the repository root and one `.planning/` file.
No code path, no input surface, no build output, nothing installed.

The register's four entries and where each was enforced:

| | |
|---|---|
| **T-01-12** — a rewritten or renumbered heading breaking live references | Enforced by construction and checked twice: every edit was a scoped replacement, `git diff --stat` reads 428 insertions and **0 deletions**, and the index anchor was checked by *deriving* the slug from the heading and comparing, not by reading it. The four forward citations of D93 in `src/`, `tests/` and `docs/` now resolve |
| **T-01-13** — an entry missing its refuted section | Four refutations, not the two the register requires: the bound-status invariant, the eight-site table, the silent-`remove` calibration trap, and `pivot`'s charge. Plus the phase's own estimator claim, refuted by the audit |
| **T-01-14** — a `SPECS.md` figure left stale | Every measured figure in the file was enumerated above, one moved and was updated with both sides recorded, and the ones deliberately not re-measured are named with the reason |
| **T-01-SC** — supply chain | Accepted and unchanged. Nothing installed, no fetch path touched |

## Estimate vs actuals

The plan estimated `tokens: 45000` at `confidence: low`. The realized diff is
65,466 characters — **16,400** on the chars/4 scale ADR-2629 specifies — under
by a factor of 2.7.

**That figure is 4,100 higher than the one this summary first carried.** 12,300
was computed before this summary and the four state files were written, which
is to say it was an estimate of the plan's own tail wearing the label of a
measurement. Corrected against `git diff dc7f881..HEAD` after the final commit
and recorded unadjusted, because a flattered actual corrupts every later
projection.

This is the first plan of the phase where the estimate was in the right
neighbourhood, and it is worth saying why: the deliverable is prose whose length
is set by the number of things that have to be recorded, and this plan knew that
number in advance. `01-01`, `01-02` and `01-04` all overshot by an order of
magnitude because their cost was machine time or measure-revert cycles that no
token field models. A documentation plan is the one shape a token estimate can
actually see.

## Commits

| Hash | Message |
|---|---|
| `f21d15b` | docs(01-05): D93 closes, and what it closes on is that the bar cannot be measured on this host |
| `ca55c40` | docs(01-05): the changelog says the charge moved under the counts, and the warm ratio's third decimal moved with it |

Plus this plan's final commit, carrying this summary, the `01-04` correction
appendix and the state files.

## Self-Check: PASSED

Nine checks run against disk after the summary was written, reading the sources
rather than this document:

1. All five files present at the paths named — the three root documents, this
   summary, and `01-04-SUMMARY.md`.
2. Both commit hashes found in `git log`: `f21d15b`, `ca55c40`.
3. Exactly **1** line matching `^## D93 — ` and exactly **1** matching
   `^- \*\*\[D93\](#d93-`.
4. The forward citations resolve: `D93` found in `src/simplex.c`,
   `tests/test_simplex.c` and `docs/work-units.md` — the three files carrying
   the seven references `01-02` left dangling.
5. **The digest counts re-read from the record files, not carried:**
   `netlib.txt` 94 lines with `digest=`, `netlib-kennington.txt` 16, and
   `netlib-infeas.txt` **0**. 94 + 16 = 110, which is what D93 says.
6. Both warm records re-read: standard `0.0164`, Kennington `0.0041`.
7. `SPECS.md` carries `0.0164` as the figure and `0.0162` only inside the
   clause explaining what it read before — `grep -c '0.0162 of the work'` is 0.
8. The changelog entry is **exactly 6 lines** under `### Changed`.
9. `git diff --shortstat` on the D93 commit: **428 insertions, 0 deletions.**
   No existing line of `DECISIONS.md` changed.

## What the phase closes with

- **D93 exists and is closed.** Roadmap criterion 1 is met — a closed decision
  states how candidates are admitted, carries the measurement on both sides,
  and the pre-code record it was taken from is committed ahead of the first
  implementation commit. Criterion 2 is met — the entry says what the rule does
  to Harris's two passes, and names the case that was built and confirmed
  refused. Criteria 3 and 4 were met by `01-04` and `01-03`.
- **The verdict stays INCONCLUSIVE and the code stays in the tree**, under the
  developer's pre-authorisation of 2026-08-12. D93 says what justifies keeping
  it — a proven observable no-op, work down on 118 instances and up on none,
  determinism-safe, and a run-time cross-check that is now permanent cover for
  an equivalence claim the solver had been asserting in a comment. And it says
  what is unproven: that any of it buys wall-clock time.
- **What the next phase inherits is the negative control.** Eight instances
  that cannot be sped up read a 3.0–6.4% improvement. Any future time ratio on
  this host that does not carry a control of that shape is not measuring what
  it thinks it is, and D93 is where that is written down.
