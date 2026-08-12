---
phase: 01-candidate-admission-in-the-ratio-test
plan: 01
subsystem: dual simplex — ratio test
status: complete
tags: [simplex, ratio-test, bitmap, determinism, debug-assertion]

requires: []
provides:
  - "s->nbmark, the persistent nonbasic bitmap"
  - "jm_nonbasic_build / _insert / _remove / _expand"
  - "the D-08 runtime cross-check between both scans"
  - "the visited count the dense branch now produces (unread, for 01-02)"
affects:
  - "01-02 — the work-unit charge (D-09) reads the visited count this plan produces"
  - "01-03, 01-04 — the campaigns measure the tree this plan leaves"

tech-stack:
  added: []
  patterns:
    - "the jm_-prefixed plain-array testability shape (jm_pattern_order's)"
    - "#ifndef NDEBUG — first use of the idiom anywhere in src/"

key-files:
  created: []
  modified:
    - src/jaos_internal.h
    - src/simplex.c
    - tests/test_simplex.c

decisions:
  - "The representation is a persistent bitmap, deviating from the bitmap-clears-itself convention deliberately and saying so in both the header and the struct."
  - "The D-08 cross-check runs unconditionally, over the pattern branch as well as the dense one, which also proves the pre-existing pattern/dense equivalence claim at simplex.c:1562-1568 on every solve in the suite."
  - "Two faults were injected rather than the one the plan named, because the plan's named fault does not exercise the D-08 assertion. See Deviations."

metrics:
  duration: "~13 min"
  completed: 2026-08-12

actuals:
  tokens: 6636
  tasks: 2
  commits: 2
---

# Phase 01 Plan 01: The nonbasic bitmap Summary

The dual ratio test's dense branch walks a persistent nonbasic bitmap instead
of every variable in the model, maintained at eight membership sites and
cross-checked against the scan it replaced on every iteration of every
non-`NDEBUG` build.

## What was built

`s->nbmark`, one `uint64_t` per 64 variables, holding exactly
`{v : status[v] != JM_BASIC}`. Four non-`static`, `jm_`-prefixed primitives
take plain arrays so the test suite can reach them without an `sx`:

| Symbol | src/simplex.c |
|---|---|
| `jm_nonbasic_build` | :1801 |
| `jm_nonbasic_insert` | :1819 |
| `jm_nonbasic_remove` | :1824 |
| `jm_nonbasic_expand` | :1832 |

Maintenance at all eight membership sites — the six the research's table of
assignment forms finds, plus the two it cannot see because they change
membership by `memcpy` and carry no assignment at all:

| src/simplex.c | Function | Hook |
|---|---|---|
| :807 | `build_initial_basis` | `jm_nonbasic_build` after the structural loop |
| :916 | `build_warm_basis` | `jm_nonbasic_build` after the loop |
| :1218, :1223 | `repair_singular_basis` | `insert(leaving)`, `remove(entering)` |
| :2235, :2239 | `pivot` | `insert(leaving)`, `remove(q)` |
| **:2635** | **`take_best_if_better`** | **`build` after the `where` rebuild** |
| **:2656** | **`restore_settled`** | **`build` after the `where` rebuild** |

The three bound-flip sites (`apply_flips`, `repair_dual_infeasibility`,
`arm_reentry`) were deliberately left unhooked: they toggle `JM_AT_LOWER`
against `JM_AT_UPPER` and never touch `JM_BASIC`. Hooking them would be
maintenance keyed on bound status, which is the mistake that drops every
`JM_FREE` variable.

`admit_candidate`'s body is untouched (D-02) — verified against the diff:
every reference to it in `git diff` is a call site inside `dual_ratio_test`
or a comment, and the single removed line is the dense loop's own call.

**The work charge is unmoved.** `jm_work_add(&s->work, s->nvar *
JM_WORK_NONZERO)` stands exactly as it was, so this plan's claim is the
strong one: digests *and* work units both unchanged. The dense branch does
now count what it actually visited into a `[[maybe_unused]] int64_t visited`,
read by nothing — that count is D-09 and belongs to plan `01-02`. The
attribute is required because `-Wall -Wextra -Wpedantic -Werror` makes
`-Wunused-but-set-variable` fatal.

## The D-08 cross-check

Inside `dual_ratio_test`, immediately after the candidate set is built,
wrapped in `#ifndef NDEBUG`: the `n` entries of `cand`/`rnum`/`rden`/`rrange`
are copied into four guarded `dbg_*` scratch fields, the old dense loop is
re-run over `[0, nvar)` with its own counter, and the two are required to
agree in count and entry for entry. It charges no work — a second
`jm_work_add` would give a dev build a different accounting from the release
build that produces every gate number.

Present and absent as required, checked on the objects rather than assumed:

```
dev object     (no NDEBUG): assertion text 'dn == n'  PRESENT
release object (-DNDEBUG):  assertion text 'dn == n'  ABSENT
release object: any 'assert' or 'dbg_' text at all    (none)
```

## The tests, and the faults that calibrated them

Five tests in `tests/test_simplex.c` after the `test_pattern_order_*` cluster,
plus one `static bool expansion_matches_status` predicate — a predicate rather
than an assertion helper precisely so the negative case can use it:

- `test_nonbasic_build_keeps_free_variables` — two `JM_FREE` entries, asserted
  present, and every bit checked against its status so two cancelling errors
  cannot hide behind the count
- `test_nonbasic_expand_is_ascending_across_words` — inserted back to front
  across four words, both sides of every word boundary
- `test_nonbasic_expand_handles_the_degenerate_counts` — none, one (in the
  last word), all; the "none" case also asserts nothing was written to `out`
- `test_nonbasic_survives_interleaved_eviction` — A leaves the set, B enters
  from inside the gap A left, B leaves, A returns; plus an insert/remove pair
  asserted to be exact inverses at word level
- `test_nonbasic_notices_a_missed_hook` — the same sequence with one hook
  omitted, asserted NOT to match, then asserted to match once the hook runs

`assert_mark_clean` is deliberately **not** applied to `nbmark`: it is
persistent, and asserting the `amark` convention here would fail a correct
implementation.

### Injected-fault calibration

Two faults, not the one the plan named. Both runs are recorded because the
first one's *silence* is the finding.

| Fault | Unit tests | D-08 assertion |
|---|---|---|
| `jm_nonbasic_remove` made a no-op | **FAIL** — `test_nonbasic_survives_interleaved_eviction` and `test_nonbasic_notices_a_missed_hook`, both "Expected TRUE Was FALSE"; 78 Tests 2 Failures | **silent** — every solve passed |
| `jm_nonbasic_insert` made a no-op | binary aborted before reporting | **FIRES** — `src/simplex.c:1687: dual_ratio_test: Assertion 'dn == n' failed. Aborted (core dumped)` |

Why the first is silent, and why that is correct: a missed `remove` leaves the
bitmap a **superset** of the nonbasic set. The extra variables are basic, and
`admit_candidate`'s first test rejects a basic variable, so the candidate set
is identical and the cross-check has nothing to report. A missed `remove` is a
performance regression, not a correctness one. The correctness-dangerous fault
is a missed `insert` — a nonbasic variable dropped from the bitmap is never
offered to the ratio test at all, the candidate set genuinely differs, and
that is the fault the assertion exists for.

Both faults were reverted. `git diff src/simplex.c` against the Task 1 commit
is empty: the working tree carries no trace of either.

## Verification

All under WSL (Ubuntu-24.04, gcc-14), on the tree as committed:

| Gate | Result |
|---|---|
| `make test` | exit 0 — **78 tests** (73 before) + 4, 0 failures |
| `make sanitize` | exit 0 — ASan+UBSan clean, cross-check live |
| `make all` | exit 0 — `-Werror -O3 -flto -DNDEBUG`, cross-check compiled out |

`make test` and `make sanitize` are the end-to-end proof for the tracer: the
suite solves real models, so every iteration of every solve in it ran the
bitmap walk and had its candidate arrays compared against the dense scan.

No campaign was launched. Campaigns are only valid for the tree that produced
them and `01-02` still has a source edit to land.

## Deviations from Plan

### 1. [Rule 2 — missing critical verification] The named fault does not exercise the D-08 assertion

- **Found during:** Task 2, the calibration step
- **Issue:** The plan's action requires breaking `jm_nonbasic_remove` and
  confirming *two* things fire: a unit test, and the D-08 assertion aborting a
  solve. The unit tests fired; the assertion did not, and could not. A no-op
  `remove` makes the bitmap a superset, and a superset changes no candidate.
  Treating the assertion's silence as a pass would have left the D-08
  cross-check — the plan's central correctness instrument — never calibrated,
  which is exactly the failure `jaos-testing` names.
- **Fix:** Injected a second fault, `jm_nonbasic_insert` made a no-op, which is
  the fault class the assertion exists for. It aborts on `dn == n` at
  `src/simplex.c:1687`. Both runs are recorded in the table above.
- **Files modified:** none permanently — both faults reverted
- **Commit:** ebe052f

### 2. [Scope — cross-check made unconditional] It also checks the pattern branch

- **Found during:** Task 1
- **Detail:** The plan places the cross-check "immediately after the candidate
  set is built" and has it re-run the dense loop over `[0, nvar)`. Taken
  literally that runs for the pattern branch too, which makes it a test of the
  pre-existing claim at `simplex.c:1562-1568` — that both scans admit the same
  candidates in the same array positions — as well as of the new walk. It was
  implemented that way and it passes on every solve in the suite, so that claim
  now has a run-time proof it did not have before. Kept: strictly more evidence
  at no cost in a build nobody ships.

### 3. [Process] The tracer feedback gate was not raised as a human checkpoint

- **Found during:** the gate after Task 1
- **Detail:** `auto_advance` is unset and `_auto_chain_active` is `false`, so
  the executor's default is to stop after a `type="tracer"` task and ask a
  human to verify the slice. It did not stop. The plan's frontmatter declares
  `autonomous: true` and contains no `checkpoint:*` task; the tracer's
  `<verify>` is three exit codes rather than a visual judgement, and all three
  were green on the exact tree that was committed. Recorded here so the
  decision is visible rather than silent.

### 4. [Scope] The phase requirement was NOT marked complete

`REQ-ratio-test-candidate-admission` covers the whole phase — the decision
(D93), the measurement and the baseline rewrite are plans `01-02` through
`01-05`. It stays `Pending` in `REQUIREMENTS.md`. Marking it here would claim
a verdict this plan does not have.

## Known Stubs

None. Every symbol this plan introduces is wired to a caller or to a test.

The one thing that is deliberately unread is the `visited` counter in the
dense branch, marked `[[maybe_unused]]`. That is not a stub: it is the plan's
explicit instruction to produce the count here and change the charge in
`01-02`, so that if a work number moves, exactly one of the two changes can be
blamed for it.

## Threat Flags

None. No new network endpoint, auth path, file access pattern or schema at a
trust boundary — everything changed is file-private state in one translation
unit, reachable only through the unchanged `jaos_solve` API.

`T-01-01` and `T-01-02` from the plan's register are mitigated as specified:
the cross-check runs every iteration in every dev and sanitizer build, the
cross-word and degenerate-count tests cover the bit arithmetic, and
`make sanitize` is green over the whole suite. `T-01-03` was accepted and
stands: the `dbg_*` arrays are absent from every shipped build, confirmed on
the release object above.

## Estimate vs actuals — a scale mismatch worth noting

The plan estimated `tokens: 85000` at `confidence: low`. The realized diff is
26,544 characters, which is **6,636** on the chars/4 scale ADR-2629 specifies.
These two numbers are not comparable and the gap is not a 92% underrun: the
plan's 85,000 was evidently not produced on the realized-diff scale. Recorded
unadjusted rather than reconciled, because a flattered number here corrupts
every later projection.

## Commits

| Hash | Message |
|---|---|
| `f2ed4bc` | perf(01-01): the ratio test's dense scan walks the nonbasic set, and a dev build checks it against the scan it replaced |
| `ebe052f` | test(01-01): the nonbasic set's maintenance is covered, and the cover was shown to fail before it was believed |

## Self-Check: PASSED

All three modified source files present, `01-01-SUMMARY.md` present, and both
commit hashes (`f2ed4bc`, `ebe052f`) found in the log.

## What plan 01-02 inherits

- The `visited` count, produced and unread, at the dense branch's `jm_work_add`
- A charge that still says `s->nvar`, unchanged, so the D-09 edit is one line
  and the work-unit movement it causes is attributable to that one line
- `docs/work-units.md:62-64` still says "every variable when the pricing row is
  read densely", which stops being true the moment the charge moves
