---
phase: 01-candidate-admission-in-the-ratio-test
verified: 2026-08-12T17:08:41Z
status: passed
score: 27/29 must-haves verified
behavior_unverified: 2
overrides_applied: 0
deferred:

  - truth: "Whether the change buys wall-clock time is unresolved — the host cannot resolve the 4.2% bar"
    addressed_in: "Phase 5"
    evidence: "ROADMAP Phase 5 'Known blocker: this phase needs a controlled host and the ingest set names none', and success criterion 3 'taken on a host that satisfies D17: a number taken under WSL is a development number and cannot close this gate'. D93's 'What is left open' hands it there explicitly."

  - truth: "The ratio test's O(nvar) cost attacked at its root rather than at its scan"
    addressed_in: "Phase 3"
    evidence: "REQ-hyper-sparse-downstream-results — 'Widening the hyper-sparse path so the pricing row has a pattern more often, which attacks the same cost at its root', handed forward by D93."
behavior_unverified_items:

  - truth: "Roadmap SC 3 / 01-04 T4 — admit_candidate's share on truss re-read under callgrind against the 14.98% it stood at, reported beside the J=1 time ratio"
    test: "Re-run callgrind on truss against both binaries in one session, or accept the committed reading"
    expected: "admit_candidate 14.79% -> 14.20%, ftran_prefix 6.59% -> 6.64%, PROGRAM TOTALS +1.60%"
    why_human: "The callgrind annotations were never committed. The figures live only in 01-04-SUMMARY.md and D93. Re-deriving them costs a valgrind run on truss; grep cannot see them, and the raw annotation files are gone."

  - truth: "01-04 T1 — the J=1 same-instance time ratio was taken on two correctly-built binaries, alternated, with iteration counts agreeing instance for instance"
    test: "Confirm the timing run's control 2 — that iteration counts agreed between run-parent and run-candidate on every instance across all 12 passes, before any timing number was read"
    expected: "Iteration counts identical instance for instance; a disagreement would mean the wrong binary was timed and every ratio in 01-04 and D93 would be void"
    why_human: "The 12 raw timing logs were never committed and no longer exist. The control is self-reported in 01-04-SUMMARY.md deviation 4 and was recomputed by the independent jaos-measurer audit from those logs while they existed, but it cannot be re-derived from the tree. build/bench/run-parent and run-candidate survive as distinct binaries (464096 vs 472232 bytes), which corroborates that two binaries were built but says nothing about the iteration counts."
human_verification:

  - test: "Read the per-instance timing table and the geometric mean in 01-04-SUMMARY.md (harvested deferred human-check from 01-04-PLAN.md)"
    expected: "The figure quoted as the result is the geometric mean of per-instance ratios; iteration counts agreed between both binaries on every instance; the verdict follows the 4.2% rule"
    why_human: "The verdict decides whether D93 is written as a gain or a refusal, and no automated check can confirm that the right binary was timed"

  - test: "Decide whether the raw timing logs and callgrind annotations should have been preserved, and whether a future phase's measurement plan must commit them"
    expected: "A rule either way. Today the only measurement in this phase that cannot be re-derived from the repository is the one that produced the verdict."
    why_human: "A process decision about what the record must carry, not a fact about the code"
---

# Phase 1: Candidate admission in the ratio test — Verification Report

**Phase Goal:** How the ratio test admits candidates is settled by a measured comparison, and the settled rule is in the solver without weakening what Harris's two passes guarantee.
**Verified:** 2026-08-12T17:08:41Z
**Status:** human_needed
**Re-verification:** No — initial verification

## The verdict this phase reports, and why it is not a failure

The phase closed **INCONCLUSIVE**. That is a valid outcome of this goal and it was
pre-authorised in writing before execution began: the goal was to *settle* candidate
admission by measurement, not to achieve a speedup. `01-CONTEXT.md` item **D-04**,
committed 2026-08-12 13:45:03 — two hours before the first implementation commit
`f2ed4bc` at 15:46:55 — reads: *"If the measurement says it does not pay, the phase
closes with a refusal written up like D82 and D84… A closed refusal is a valid
outcome."*

So this report does not mark the phase failed for reporting INCONCLUSIVE. Nor does
it mark it passed because five SUMMARY files exist. What follows is what the
codebase and the committed records actually show.

## Goal Achievement

### ROADMAP Success Criteria (the contract)

| # | Success criterion | Status | Evidence |
|---|---|---|---|
| 1 | A closed decision states how candidates are admitted, carries the measurement on both sides, and exists before any implementation commit | ✓ VERIFIED | `DECISIONS.md:6474` — **D93**, numbered one past D92, indexed at `DECISIONS.md:103` with a working anchor, heading is the decision not the topic. 428 lines added, none changed. Pre-code record `01-CONTEXT.md` committed `11ac530` at **13:45:03**, first implementation commit `f2ed4bc` at **15:46:55**; all thirteen items D-01…D-13 present and D-04/D-13 say what D93 cites them as saying |
| 2 | The decision says what the chosen rule does to Harris's two-pass guarantee, and the case it must refuse is built and confirmed refused | ✓ VERIFIED | D93 §"Harris's two passes". **Re-verified behaviorally by this verifier** — both faults re-injected in a scratch copy and D93's table reproduced exactly (see Behavioral Spot-Checks) |
| 3 | `admit_candidate`'s cost re-read on `truss` under callgrind against 14.98%, reported beside a `J=1` same-instance time ratio | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | Reported and substantive: 14.79% → 14.20%, `ftran_prefix` 6.59% → 6.64%, PROGRAM TOTALS **+1.60%**, beside `0.9709x`. But the callgrind annotations and the 12 timing logs were never committed and no longer exist — the figures cannot be re-derived from the tree |
| 4 | All three netlib campaigns PASS and the per-instance baseline diff shows no regression on any of the 139 instances, on any of the four predicates or the work count | ✓ VERIFIED | Recomputed from the committed records: 110 digests byte-identical, 29 infeasibility verdicts byte-identical, **139 iteration counts identical**, three `gate: PASS`, `0 regressed, 0 improved, 0 new` on all three |

### Plan must-haves

**01-01 — the bitmap, its maintenance, and the cross-check**

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | Dense branch walks a persistent nonbasic bitmap, hands `admit_candidate` only `status[v] != JM_BASIC` (D-01) | ✓ VERIFIED | `src/simplex.c:1656-1667` — word loop + `__builtin_ctzll`, `bits &= bits - 1`. Bitmap invariant established by `jm_nonbasic_build` (`:1810-1827`), which keys on `JM_BASIC` and nothing else |
| 2 | `admit_candidate`'s body byte-identical to pre-phase | ✓ VERIFIED | `diff` of the function extracted from `2b07de1` against `HEAD`: **identical**, 29 lines both sides. The `JM_BASIC` test, `PIVOT_MIN`, the per-status sign test and the clamped numerator are all present and unmoved |
| 3 | Emitted order identical to `for (v = 0; v < nvar; v++)` — ascending by construction, not restored by sorting (D-03) | ✓ VERIFIED | Bit position *is* the variable index; words ascending, `ctzll` ascending within a word. Corroborated by 139 identical iteration counts on real instances |
| 4 | Exactly-equal ratios keep the relative order the dense scan produced, so `bfrt_walk`, `jm_harris_pick` and `apply_flips` break ties the same way | ✓ VERIFIED | Follows from truth 3; the strong evidence is behavioral — 110 identical digests **and** 139 identical iteration counts. A tie broken differently on any instance would move a trajectory |
| 5 | Zero candidates returns -1 as before; degenerate bitmap states covered by unit tests | ✓ VERIFIED | `src/simplex.c:1708-1709` unchanged. `test_nonbasic_expand_handles_the_degenerate_counts` covers no bits / one bit (in the last word) / every bit — PASSES |
| 6 | The bitmap holds exactly `{v : status[v] != JM_BASIC}` after every membership-changing site, including all three that create `JM_FREE` — membership, never "has a finite bound" | ✓ VERIFIED | All eight sites audited by hand against every `s->status` write in the file. See the site table below. The three `JM_FREE` sites are `:800`, `:910`, `:1217` — covered by `build` at `:807`, `build` at `:916`, and `insert` at `:1218` |
| 7 | Cross-check present in a non-`NDEBUG` build over count/`cand`/`rnum`/`rden`/`rrange`; absent from the binary under `NDEBUG` (D-08) | ✓ VERIFIED | `src/simplex.c:1670-1706`. **Verified on the object, not assumed from the guard**: assertion text present in `build/dev/simplex.o`, absent from `build/release/libjaos.a`; both configurations compile clean under `gcc-14 … -Werror` |
| 8 | The unit test was shown to fail against a deliberately broken maintenance sequence before its passing was treated as evidence (D-07) | ✓ VERIFIED | `test_nonbasic_notices_a_missed_hook` embeds the negative case. **Re-verified behaviorally** — see Behavioral Spot-Checks |

**01-02 — the work charge**

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | The dense branch charges one unit per variable actually handed to `admit_candidate`, not `s->nvar` (D-09) | ✓ VERIFIED | The phase's only deleted line in this region is `jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);`, replaced by `visited * JM_WORK_NONZERO` at `:1667` |
| 2 | `docs/work-units.md` describes the charge the code now makes; the unqualified "every variable when the pricing row is read densely" no longer stands | ✓ VERIFIED | Now reads "the nonbasic ones when the pricing row is read densely, because that scan walks the nonbasic set and never reaches a basic variable at all (D93)". A new paragraph also states the unbilled bitmap-word reads and the floor they leave |
| 3 | The pinned work constant was re-pinned deliberately, with the figures on both sides | ✓ VERIFIED | `WORK_PINNED` 8545 → 8536 with the arithmetic stated and closing exactly (3 iterations × (6 vars − 3 nonbasic) = 9). The `< 62000` ceiling test's **both sides** were re-measured on this tree (58141/67416 → 60701/64633) with the ~2800-unit unnoticed drift called out and the narrowed margin quantified |
| 4 | Every source edit finished when this plan closes, so 01-03's campaigns are valid for the tree that produced them | ✓ VERIFIED | `git log b65d9f2..HEAD -- src/ tests/ include/` is **empty** |

**01-03 — the campaigns and the baselines**

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1–2 | *(subsumed by ROADMAP SC 4 above)* | ✓ VERIFIED | |
| 3 | The warm campaigns agree warm against cold on every measured instance and the independent checker refuses neither answer | ✓ VERIFIED | `warm.txt:98` — measured 92, skipped 2, **disagreed 0, rejected 0, errors 0**. `warm-kennington.txt:20` — measured 11, skipped 5, **0/0/0**. This is the only cover `build_warm_basis` gets; the gate never loads a basis |
| 4 | The baselines were rewritten only after the digests were confirmed identical, never before, never as a side effect of a gate run (D-06, D-10) | ✓ VERIFIED | Commit separation is structural: `44c0ef6` touches **five results files and no baseline**; `e8c2f58` touches **three baselines and no results file** |
| 5 | A gate run against the rewritten baselines diffs clean — record and baseline come from the same build | ✓ VERIFIED | Verified statically, which tests the same property without a 30-minute campaign: for **all 139 instances across all three sets**, the committed baseline agrees with the committed record on name, status, iters and work. A diff against these baselines cannot report a regression |

**01-04 — the measurement**

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | A `J=1` same-instance time ratio on two binaries built in one session from candidate and parent, alternated over ≥3 rounds, minimum taken (D-11, D45) | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | Documented in detail; six rounds run in two orders, not three. `build/bench/run-parent` (464096 B) and `run-candidate` (472232 B) survive as **distinct** binaries. But the 12 raw logs are gone and control 2 — iteration counts agreeing instance for instance — cannot be re-derived from the tree |
| 2 | A geometric mean of per-instance ratios, never a ratio of totals (D46, D-12) | ✓ VERIFIED | `0.9709x` quoted as the result with `0.9847x` beside it and explicitly labelled *not* the answer, in both `01-04-SUMMARY.md` and D93. `geomean.py --pairs` exists at `.claude/skills/jaos-measure/scripts/geomean.py:10` and supports the flag |
| 3 | `truss` reported on its own line, and it does not decide alone (D-12) | ✓ VERIFIED | `1.496s → 1.460s, 0.9759x` on its own line in both documents, beside nine other named instances on both sides of the mean |
| 4 | *(subsumed by ROADMAP SC 3 above)* | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | |
| 5 | ACCEPT only at 4.2% or better; below that INCONCLUSIVE and no yes (D-13) | ✓ VERIFIED | 2.91% < 4.2% → `VERDICT: INCONCLUSIVE`. The REJECT branch, which was **not** pre-authorised, was correctly not taken |
| 6 | No wall-clock figure enters `bench/results/*.txt` or any baseline | ✓ VERIFIED | No time field exists in any record or baseline; grep for a seconds pattern over all eight files returns nothing. `1.496`/`1.460` appear only inside untouched `.mps` instance data |

**01-05 — the record**

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1–2, 4 | *(subsumed by ROADMAP SC 1 above)* | ✓ VERIFIED | D93 carries all four required parts: the question with what was expected; the measurement per instance with instances named; what was refuted, in five sections; and what is left open, seven items handed forward |
| 3 | D93 states what the rule does to Harris's guarantee and that the case it must refuse was built and confirmed refused | ✓ VERIFIED | Re-verified behaviorally — see below |
| 5 | D93 shows the derivation of 4.2% and reconciles the two repeatability figures rather than asserting the threshold | ✓ VERIFIED | 4.2% = 3 × 1.4%, **attributed to D81** with D83's different 1.4% explicitly refused as a different quantity, and D60's 1.3% reconciled — "no reading of the data lands between 3.9% and 4.2%". Note this **corrects the phase's own plan**, which attributed it to D83 twice (`01-04-PLAN.md:266`) |
| 6 | The null result stated as a claim | ✓ VERIFIED | "110 solution digests unmoved and 29 infeasibility verdicts unmoved, over 139 instances" — and the entry says why the counts are not interchangeable: "139 is the instance count, not the digest count". All three figures independently recomputed by this verifier |
| 7 | The changelog entry is two to six lines, says what changed and what it cost, and points at D93 rather than carrying the argument | ✓ VERIFIED | Exactly 6 lines. Carries the cost — "It costs 1.60% more instructions on `truss` and buys no time this host can resolve (D93)" |
| 8 | Any measured figure in `SPECS.md` this phase moved was re-read and updated | ✓ VERIFIED | Exactly one figure moved and it was updated: the warm-against-cold standard-set work ratio **0.0162 → 0.0164**, matching `warm.txt:100`, with the reason recorded. Kennington's 0.0041 and 0.0006 did **not** move and were correctly left alone |

**Score:** 27/29 truths verified (2 present, behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|---|---|---|---|
| `src/jaos_internal.h` | Declarations for the four `jm_nonbasic_*` primitives | ✓ VERIFIED | `:319-323`, with a 32-line contract at `:287-318` stating the membership rule and the persistence property. +38 lines |
| `src/simplex.c` | The bitmap, maintenance at all eight sites, the list-driven scan, the cross-check | ✓ VERIFIED | +200/−3. `nbmark` declared `:373`, allocated `:606`, freed `:524`, checked `:625` |
| `tests/test_simplex.c` | Plain-array unit tests including the `JM_FREE` case and the missed-hook negative case | ✓ VERIFIED | +245. Five tests at `:1437-1601`, all five registered at `:2736-2740`, all five run and pass |
| `docs/work-units.md` | The ratio-test charge rule, matching the code | ✓ VERIFIED | +37/−7 |
| `bench/results/*.txt` (5) | The per-instance record on the new tree | ✓ VERIFIED | 139 instances across the three gate sets, plus both warm campaigns |
| `bench/*.baseline` (3) | The rewritten baselines carrying the new work counts | ✓ VERIFIED | Agree with the records on all 139 instances |
| `DECISIONS.md` | D93, indexed | ✓ VERIFIED | +428 lines, 0 changed |
| `CHANGELOG.md` | The unreleased entry, pointing at D93 | ✓ VERIFIED | +7 (6 content lines + blank) |
| `SPECS.md` | Feature status and measured figures, current | ✓ VERIFIED | 2 lines changed |
| `01-04-SUMMARY.md` | Timing table, geometric mean, truss, callgrind, verdict against 4.2% | ✓ VERIFIED (as a document) | 709 lines, 67 table rows, with the independent audit's CORRECTIONS appended unmerged at `:610` |

### Key Link Verification — the eight membership sites

Every write to `s->status` in `src/simplex.c` was enumerated and classified.
Membership-changing writes must be paired with bitmap maintenance; bound flips
must **not** be, and hooking them would be the same mistake wearing the opposite
sign.

| # | Site | Routine | Maintenance | Status |
|---|---|---|---|---|
| 1 | `:768`, `:788-800` | `build_initial_basis` | `jm_nonbasic_build` `:807` | ✓ WIRED |
| 2 | `:893-910` | warm-basis install | `jm_nonbasic_build` `:916` (every earlier return precedes any status write — checked) | ✓ WIRED |
| 3 | `:1213-1217` (leaving) | crash/repair pivot loop | `jm_nonbasic_insert` `:1218` | ✓ WIRED |
| 4 | `:1222` (entering) | same | `jm_nonbasic_remove` `:1223` | ✓ WIRED |
| 5 | `:2245` (leaving) | `pivot` — the every-iteration site | `jm_nonbasic_insert` `:2246` | ✓ WIRED |
| 6 | `:2249` (entering) | same | `jm_nonbasic_remove` `:2250` | ✓ WIRED |
| 7 | `:2631` `memcpy` | `take_best_if_better` | `jm_nonbasic_build` `:2646` | ✓ WIRED |
| 8 | `:2656` `memcpy` | `restore_settled` | `jm_nonbasic_build` `:2667` | ✓ WIRED |
| — | `:1530`, `:2350`, `:2780` | `apply_flips`, `repair_dual_infeasibility`, `arm_reentry` | **none, correctly** | ✓ CORRECT — all three toggle `JM_AT_LOWER` ↔ `JM_AT_UPPER` and never touch `JM_BASIC` |

The two `memcpy` sites are the ones a grep-built site table misses, exactly as
D93 says. No unhooked membership-changing write was found.

| From | To | Via | Status |
|---|---|---|---|
| `pivot()` | `dual_ratio_test()` dense branch | `s->nbmark` — insert on leave, remove on enter, read by the word walk | ✓ WIRED |
| `take_best_if_better()`, `restore_settled()` | `s->nbmark` | `jm_nonbasic_build` after the wholesale `memcpy`, beside the `s->where` rebuild | ✓ WIRED |
| dense branch charge | `docs/work-units.md` | both state one unit per variable looked at | ✓ WIRED |
| `CHANGELOG.md` | `DECISIONS.md` D93 | a `(D93)` pointer, not the argument | ✓ WIRED |
| `SPECS.md` warm row | `bench/results/warm.txt` | 0.0164 quoted, 0.0164 recorded | ✓ WIRED |
| forward `D93` citations | `DECISIONS.md` D93 | **nine** citations in three files, all resolving | ✓ WIRED |

### Data-Flow Trace (Level 4)

| Value | Source | Flows | Status |
|---|---|---|---|
| The dense branch's candidate set | `s->nbmark`, maintained at eight sites from `s->status` | Yes — into `admit_candidate` and thence `s->cand`/`rnum`/`rden`/`rrange` | ✓ FLOWING |
| The dense branch's work charge | `visited`, counted by the walk itself | Yes — into `jm_work_add`, and out through `jaos_work_units` into every record and baseline | ✓ FLOWING |
| `SPECS.md` warm work ratio 0.0164 | `bench/results/warm.txt:100` | Yes — re-read from the campaign 01-03 ran | ✓ FLOWING |
| D93's before/after work table | `64efcc6:bench/*.baseline` and `e8c2f58:bench/*.baseline` | Yes — recomputed by this verifier, reproduces to the digit | ✓ FLOWING |
| D93's callgrind figures | the annotation files | **No** — the annotations were never committed | ⚠️ STATIC (documented value only) |
| D93's / 01-04's time ratios | the 12 raw timing logs | **No** — the logs were never committed and no longer exist | ⚠️ STATIC (documented value only) |

### Behavioral Spot-Checks

Presence checks cannot see a state-transition invariant. These were run.

| Behavior | Command | Result | Status |
|---|---|---|---|
| The suite passes on the pristine tree (dev build — the cross-check runs on every ratio test of every solve test) | `make -j12 test` | 78 tests, **0 failures**, exit 0; plus `test_lu`, `test_check`, `test_model`, `test_mps`, `test_scale`, `test_lp`, `test_version` all OK | ✓ PASS |
| All five nonbasic tests actually run, not merely exist | `make test \| grep nonbasic` | five `:PASS` lines at `tests/test_simplex.c:2736-2740` | ✓ PASS |
| **Negative control A** — `jm_nonbasic_remove` made a no-op | scratch copy, rebuilt, run | `test_nonbasic_survives_interleaved_eviction:FAIL`, `test_nonbasic_notices_a_missed_hook:FAIL`; **run-time cross-check silent, 0 assertions** | ✓ PASS — reproduces D93's table exactly, including the silence |
| **Negative control B** — `jm_nonbasic_insert` made a no-op | scratch copy, rebuilt, run | exit **134**, `test_simplex: src/simplex.c:1698: dual_ratio_test: Assertion 'dn == n' failed.` | ✓ PASS — the cross-check catches the correctness-dangerous fault |
| Main tree untouched by the injection | `git status --short src/ tests/ include/` | empty | ✓ PASS |
| Cross-check absent under `NDEBUG`, present without it | `strings` on both objects | dev: assertion text present. Release `libjaos.a`: **absent**. Both compile clean under `gcc-14 -Werror` | ✓ PASS |
| The cross-check charges no work | inspect `#ifndef NDEBUG` block for `jm_work_add` | the only occurrence in the block is inside a comment; corroborated by the dev build's pinned test reading 8536, the release figure | ✓ PASS |
| 110 solution digests unmoved | recomputed across `44c0ef6^..44c0ef6` | netlib 94 identical, kennington 16 identical | ✓ PASS |
| 29 infeasibility verdicts unmoved | same | identical | ✓ PASS |
| 139 iteration counts unmoved | same | 94 + 29 + 16, all identical | ✓ PASS |
| Work moved on 118 instances and on nothing else | same | 85 + 20 + 13 = **118** | ✓ PASS |
| Baselines agree with records on all 139 | join over name/status/iters/work | 94, 29, 16 — all agree | ✓ PASS |
| D93's before/after work table | `git show` on both cited baselines | `truss` 1154610114 → 1138043114; `gfrd-pnc` 3485629 → 3287277; `stocfor3` unchanged — all to the digit | ✓ PASS |
| **D93's strongest structural claim** — "the saving is an exact whole multiple of the row count on all 139 instances" | recomputed by this verifier from both baselines and the records' `rows=` | **139 of 139, zero violations** | ✓ PASS |
| The pre-code record predates the code | `git log` timestamps | `11ac530` 13:45:03 vs `f2ed4bc` 15:46:55 | ✓ PASS |
| Nine forward D93 citations resolve | `grep -rn D93 src/ tests/ docs/ include/` | 9 citations, 3 files, D93 exists and is indexed | ✓ PASS |
| No source edit after the tree was settled | `git log b65d9f2..HEAD -- src/ tests/ include/` | empty | ✓ PASS |
| The `J=1` timing run's iteration-count control | — | raw logs not committed | ? SKIP — routed to human verification |
| The callgrind re-read | — | annotations not committed | ? SKIP — routed to human verification |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exist in this repository and no plan declares a
probe. The equivalent runnable checks for this project are `make test` and the
three campaigns; `make test` was run (above), and the campaigns were not re-run
by instruction — their evidence is committed and was recomputed statically
instead.

### Requirements Coverage

| Requirement | Source plans | Status | Evidence |
|---|---|---|---|
| `REQ-ratio-test-candidate-admission` | 01-01, 01-02, 01-03, 01-04, 01-05 (all five declare it) | ✓ SATISFIED | The requirement's acceptance was "**absent** — and a decision is required before implementation". That decision exists as D93, dated, pre-recorded, and carrying the measurement on both sides. `REQUIREMENTS.md:26` is `[x]`, `:36-43` records the closure with the INCONCLUSIVE verdict stated rather than hidden, and the traceability table at `:236` reads **Complete**. The requirement asked for a decision, not a speedup |

**Orphaned requirements:** none. `grep -E "Phase 1" .planning/REQUIREMENTS.md` maps
exactly one requirement to this phase, and all five plans declare it.

**Note on how this requirement closed.** `01-04-SUMMARY.md` deviation 7 correctly
recorded the requirement as still Pending at the end of 01-04, because it turns on
D93 which did not yet exist. 01-05 created D93 and the requirement was then marked
complete. The sequencing is correct.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| `src/simplex.c`, `src/jaos_internal.h`, `tests/test_simplex.c`, `docs/work-units.md` | — | `TBD` / `FIXME` / `XXX` / `TODO` / `HACK` / `PLACEHOLDER` | — | **None found.** No debt markers in any file this phase modified |
| `.planning/ROADMAP.md` | 53 | "139 digests unmoved" | ⚠️ Warning | The phrase D93 was written to stop. D93 says in as many words: *"139 is the instance count, not the digest count"* — it is 110 digests + 29 verdicts. The annotation at line 53 was written by 01-05, **after** D93 drew the distinction, so this is a slip introduced by the closing plan rather than inherited. Line 67 of the same file has it right. Substance unaffected; the wording contradicts the record it cites. Lines 61 and 66 carry it too but predate D93 |
| `DECISIONS.md` | 6827 | `src/simplex.c:1687` cited as the assertion line | ℹ️ Info | The assertion is at `:1698`. Confirmed by re-injecting the fault: the abort reads `src/simplex.c:1698`. The line moved when the block's comment was written after the injection run |
| `DECISIONS.md` | 6776 | `take_best_if_better (src/simplex.c:2635)` | ℹ️ Info | The status `memcpy` is at `:2631`; `:2635` is the `fake` memcpy. Off by four |
| `DECISIONS.md` | 6815 | "the contract at `src/simplex.c:1562-1568`" | ℹ️ Info | That range is the ratio test's opening comment. The tie-break sentence the citation is reaching for is at `:1626-1628` |

No stub, no hollow prop, no orphaned artifact, no unwired link was found anywhere
in this phase.

### Human Verification Required

#### 1. The timing run's iteration-count control (harvested from `01-04-PLAN.md`, and independently the one thing this verifier could not close)

**Test:** Read the per-instance timing table and the geometric mean in
`01-04-SUMMARY.md`. Confirm the figure quoted as the result is the geometric mean
of per-instance ratios, and that iteration counts agreed between `run-parent` and
`run-candidate` on every instance across all 12 passes.
**Expected:** The geometric mean is the answer and the ratio of totals is labelled
as not the answer (this half is verified — both figures are quoted and labelled).
Iteration counts identical instance for instance.
**Why human:** The 12 raw timing logs were never committed and no longer exist. A
disagreement in iteration counts would mean the wrong binary was timed and every
ratio in 01-04 and D93 would be void. This is the control that catches D82's and
D80's failure modes, it is self-reported in the summary, and it was recomputed by
the independent `jaos-measurer` audit while the logs existed — but it cannot be
re-derived from the tree today.

**Mitigating evidence found:** `build/bench/run-parent` (464096 B) and
`run-candidate` (472232 B) survive as genuinely distinct binaries, the candidate
being the larger, which is the shape the bitmap machinery predicts. That closes
the "same binary timed twice" failure mode but not the iteration-count control.

#### 2. Whether the raw measurement evidence should have been preserved

**Test:** Decide whether a future measurement plan in this project must commit its
raw logs and annotations.
**Expected:** A rule either way.
**Why human:** A process decision. It is worth surfacing because of what this
verification found: **every claim in this phase is re-derivable from the repository
except the two that produced the verdict.** The code, the tests, the eight sites,
the 139 digests and verdicts, the 139 iteration counts, the work table, the
baselines and D93's divisibility claim were all recomputed here from committed
artifacts. The time ratio and the callgrind share were not, because their inputs
are gone.

### Deferred Items

Not gaps — explicitly addressed in later milestone phases.

| # | Item | Addressed in | Evidence |
|---|---|---|---|
| 1 | Whether the change buys wall-clock time is unresolved, because the host cannot resolve the 4.2% bar | Phase 5 | Phase 5's stated **Known blocker**: "this phase needs a controlled host and the ingest set names none", and its SC 3: "a number taken under WSL is a development number and cannot close this gate". D93 hands it there by name |
| 2 | Attacking the same O(`nvar`) cost at its root rather than at its scan | Phase 3 | `REQ-hyper-sparse-downstream-results` — "Widening the hyper-sparse path so the pricing row has a pattern more often" |
| 3 | Restricting the candidate set ahead of `bfrt_walk` and `jm_harris_pick` | Not scheduled — **and not refused** | D93 leaves it open as its own decision requiring its own record. Correctly not attempted here (D-04) |

### Gaps Summary

**No gaps.** No must-have failed. No artifact is missing, stubbed or orphaned. No
key link is broken. No debt marker was introduced. The phase goal — *settle
candidate admission by measurement, and put the settled rule in the solver without
weakening Harris's guarantee* — is achieved on the evidence in the tree.

Two things kept this out of `passed`, and neither is a defect in the work:

1. **Two measurement truths are present but not behaviorally re-verifiable.** The
   `J=1` time ratio and the callgrind re-read are documented in full, were
   independently audited, and produced the verdict — but their raw inputs were
   never committed, so this verifier cannot re-derive them. They are *present*, not
   *proven*, and the honest status for that is not VERIFIED.

2. **`01-04-PLAN.md` deliberately deferred one human-check to end of phase.** It is
   harvested above rather than dropped.

Three things are worth recording in this phase's favour, because a verifier that
only reports doubts is as misleading as a summary that only reports gains:

- **The phase caught its own errors and said so.** `01-04-SUMMARY.md` records seven
  deviations including that the `jaos-measurer` subagent could not be spawned and
  that "what is genuinely lost is an independent reader" — and 01-05 then closed
  exactly that gap, appending an independent audit that **refuted three of the
  stated grounds and two factual claims** while accepting the verdict. The audit is
  appended unmerged, above an explicit note that a quietly corrected summary cannot
  be told from one that was right.

- **D93 does not repeat the refuted grounds.** It states the opposite of the
  estimator claim its own plan first made, and corrects its own plan's D83
  attribution to D81.

- **The strongest evidence in the phase was independently reproduced here.** D93's
  claim that the work saving is an exact whole multiple of the row count on every
  instance holds **139 out of 139** when recomputed from the two committed
  baselines. So does every digest, verdict, iteration count and work figure in the
  entry.

---

_Verified: 2026-08-12T17:08:41Z_
_Verifier: Claude (gsd-verifier)_
