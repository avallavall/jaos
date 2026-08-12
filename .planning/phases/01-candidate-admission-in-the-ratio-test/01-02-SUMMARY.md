---
phase: 01-candidate-admission-in-the-ratio-test
plan: 02
subsystem: dual simplex — ratio test, work accounting
status: complete
tags: [simplex, ratio-test, work-units, d16-contract, pinned-test, documentation]

requires:
  - "01-01 — the visited count the bitmap walk produces"
provides:
  - "the dense branch's work charge, counted from what the scan visited (D-09/D93)"
  - "docs/work-units.md stating the rule both ratio-test branches now obey"
  - "WORK_PINNED = 8536, re-pinned with the derivation"
  - "the <62000 ceiling with both its figures re-measured on this tree"
affects:
  - "01-03 — the three baselines it rewrites are now stale by a known cause"
  - "01-05 — D93 must exist; four files cite it as a forward reference today"

tech-stack:
  added: []
  patterns:
    - "throwaway diagnostic via a deliberately-failing TEST_ASSERT_EQUAL_INT64, to read a value Unity would not otherwise print"

key-files:
  created: []
  modified:
    - src/simplex.c
    - docs/work-units.md
    - tests/test_simplex.c

decisions:
  - "The charge is the visited count; the bitmap words are billed nothing, because a rate for them would have no measurement on either side."
  - "The plan's acceptance criterion `grep -c 'nvar * JM_WORK_NONZERO' == 0` was NOT met, deliberately: satisfying it would have required changing pivot()'s dual-update sweep, which genuinely reads every variable. See Deviations."
  - "The <62000 ceiling's failure side was re-measured rather than carried, because the correct side had drifted to within 2.1% of the bound and a stale pair could no longer say whether the ceiling still discriminates."
  - "The ceiling stays at 62000: it sits between two current measurements, and moving it would be headroom chosen by eye."

metrics:
  duration: "~22 min"
  completed: 2026-08-12

actuals:
  tokens: 2372
  tasks: 1
  commits: 1
---

# Phase 01 Plan 02: The charge counts what the scan visited Summary

The dual ratio test's dense branch bills the nonbasic variables its bitmap
walk actually handed to `admit_candidate` instead of the model's dimension, so
the saving plan `01-01` made is now expressible in the only currency this
project measures in — and re-measuring the one test that bounds it turned up a
2800-unit drift that predates this phase entirely.

## What changed

One argument, in one call, at `src/simplex.c:1667`:

```c
jm_work_add(&s->work, visited * JM_WORK_NONZERO);   /* was s->nvar * ... */
```

`visited` is the count plan `01-01` produced and left unread; the
`[[maybe_unused]]` attribute came off with it. Both branches of
`dual_ratio_test` now bill one unit per variable they looked at — the pattern
branch over `s->anpat`, unchanged, and this one over the set bits of
`s->nbmark`.

**The bitmap words are billed nothing.** A word is not a variable, the rule
the document states is one per variable looked at, and a rate for the words
would be a number with no measurement on either side of it. The floor that
leaves — `nvar/64` reads no unit counts — is now recorded in
`docs/work-units.md` under *What is outside the budget*, beside the unbilled
`alpha` sweep it resembles.

### The charge in `pivot` was not touched

`src/simplex.c:2174` still reads `s->nvar * JM_WORK_NONZERO`, and correctly:
that loop walks `[0, nvar)` and reads every variable's status through
`update_dual`, so the dimension *is* what it looked at. The enclosing function
was confirmed by inspection (`pivot` opens at `:2103`) rather than from a line
number.

This makes the two dense charges differ, which a reader comparing the two
lines would otherwise read as a defect. `docs/work-units.md` now says so
outright: same rule, two loops, one of which was taught to skip.

## The pinned constant moved, and the arithmetic closes

`WORK_PINNED`: **8545 → 8536**, re-pinned with a comment in the file's
established style naming both figures and what moved them.

Nine units, and the derivation is exact rather than plausible:

| Quantity | Value | How it was obtained |
|---|---|---|
| iterations on the pinned model | 3 | measured (throwaway diagnostic) |
| variables | 6 | 3 columns + 3 rows |
| nonbasic variables | 3 | `nvar - nrow`, constant across the solve |
| saving | 3 × (6 − 3) = **9** | 8545 − 8536 = 9 |

That it closes exactly is *also* the evidence that all three iterations take
the dense branch, which is what the file's existing D40/D41 note predicts: the
sparse path needs a pattern no larger than a quarter of the variables, and a
quarter of six is one.

## The ceiling: a drift nobody was going to notice

`tests/test_simplex.c` bounds
`test_a_clean_up_pass_dispatches_every_column_it_identified` with
`work_units < 62000`, described in the file as "a ceiling with a measurement
on each side of it". Both sides were re-measured on this tree:

| Figure | Old comment | Before this change | After this change |
|---|---|---|---|
| correct | 58141 | **60941** | **60701** |
| one pivot per call | 67416 | not measured | **64633** |
| margin below 62000 | 3859 | 1059 | 1299 |
| correct → defect gap | 16% | — | **6.5%** |

**Only 240 units of the move is this change.** The other ~2800 had already
accumulated between whenever 58141 was measured and HEAD — unnoticed, because
a ceiling is consulted only when it trips and nothing re-measures it on the
way up. A stale pair on a bound that had quietly closed to 1.7% of its
headroom is a test one commit away from failing for a reason unrelated to what
it guards.

`62000` still sits between two current measurements, so it was left where it
is. Moving it would be headroom chosen by eye, which is the thing CLAUDE.md
forbids.

### How the figures were read

Unity's `TEST_ASSERT_TRUE` prints no value, so each figure was read by
temporarily replacing the assertion with `TEST_ASSERT_EQUAL_INT64(-1, ...)`
and letting it fail — "Expected -1 Was 60701". The pre-change correct-side
figure came from temporarily restoring `s->nvar` in the charge, and the
failure side from re-injecting the defect the test exists for (a `break` after
`(*pivots)++` in `primal_cleanup`, reproducing "one pivot per call").

All four temporary edits were reverted. `git diff` of the committed tree
against the intent carries no trace of them: the only executable line changed
in `tests/test_simplex.c` is `WORK_PINNED`, and `src/simplex.c`'s diff is the
one charge plus its comment.

## Verification

All under WSL (Ubuntu-24.04, gcc-14), on the tree as committed:

| Gate | Result |
|---|---|
| `make test` | exit 0 — 78 + 4 + 12 tests, 0 failures |
| `make sanitize` | exit 0 — ASan+UBSan clean, D-08 cross-check live |
| `make all` | exit 0 — `-Werror -O3 -flto -DNDEBUG` |

The test count is unchanged from `01-01`, which is correct: this plan adds no
test, it re-pins one and re-measures another's bound.

**No campaign was launched.** `make netlib`, `make warm` and the rest belong
to `01-03`.

**The tree is clean and every source edit of this phase is finished**, which
is this plan's `must_have` and the precondition for `01-03`'s ~27 minutes of
WSL time being valid for the tree that produced it.

## Deviations from Plan

### 1. [Rule 4 boundary — plan criterion refused on correctness] `grep -c 'nvar * JM_WORK_NONZERO'` is 1, not 0

- **Found during:** Task 2, acceptance-criteria check
- **Issue:** The plan's acceptance criteria require
  `grep -c 'nvar \* JM_WORK_NONZERO' src/simplex.c` to be **0**. There are two
  such call sites. Only `:1656` (`dual_ratio_test`'s dense branch) is D-09's.
  The other, now `:2174`, is `pivot`'s dual-update sweep over `[0, nvar)`.
  Satisfying the criterion literally would have changed a charge that is
  already correct, silently altering work units on every instance in every
  campaign — and no test would have caught it, because nothing pins that
  figure.
- **Resolution:** The criterion is left unmet and the charge untouched. Read as
  intent rather than as a grep, the criterion says "no charge for a count the
  scan no longer walks", and that is satisfied: the only site that stopped
  walking `nvar` stopped billing it. The count is 1 by design.
- **Files modified:** none — this deviation is the absence of an edit
- **Commit:** b65d9f2

### 2. [Rule 2 — missing critical measurement] The failure-side figure was re-measured, though the plan allowed carrying it

- **Found during:** Task 2, re-measuring the ceiling's correct side
- **Issue:** The plan permits leaving the 67416 failure-side figure
  un-re-measured provided the comment says it is from the earlier accounting.
  That was written on the expectation that the correct side would stay far
  below the bound. It did not: it came in at 60701 against a ceiling of 62000.
  With the margin at 2.1%, a stale failure-side figure can no longer answer
  the only question the pair exists to answer — whether 62000 still separates
  correct from broken. CLAUDE.md's "every number needs a measurement on both
  sides" applies directly.
- **Fix:** Re-injected the defect (one pivot per call) and measured 64633, then
  reverted. `60701 < 62000 < 64633` — the bound still discriminates, now on
  evidence rather than on inference from a superseded pair.
- **Files modified:** `tests/test_simplex.c` (comment only)
- **Commit:** b65d9f2

### 3. [Rule 2 — the document contradicted the code] The dual-update clause had to be rewritten too

- **Found during:** Task 2, editing `docs/work-units.md`
- **Issue:** The plan scopes the doc edit to the stale *example* in the
  ratio-test sentence. But the same sentence continues "and the dual update
  charges the same way" — true before this change and false after it, since
  the ratio test now bills the nonbasic count where the dual update still
  bills `nvar`. Rewriting only the example would have left the paragraph
  asserting an equality the code had just broken, which is precisely the
  `key_links` obligation the plan states.
- **Fix:** The clause now says the rule is shared and the sets are not, and why
  `pivot`'s loop legitimately reaches every variable. The rule itself — one per
  variable it looked at — is unreworded, as instructed.
- **Files modified:** `docs/work-units.md`
- **Commit:** b65d9f2

### 4. [Scope — two adjacent doc updates] Two further `docs/work-units.md` paragraphs went stale

- **Found during:** Task 2, `jaos-record`'s "check the status tables, not just
  the prose"
- **Detail:** Two paragraphs beyond the one the plan names would have been left
  wrong rather than merely incomplete:
  - *What is outside the budget* gained the unbilled bitmap words. The section
    exists to record exactly this — real work no unit counts — and omitting a
    new instance while adding it to the code is how the document stops being
    the design record.
  - The D32 attribution shares said they "predate D40 and D41, which between
    them took the ratio test's candidate scan ... off the first two rows
    wherever the pricing row is sparse". D93 takes the same scan down on the
    dense iterations, which are exactly the ones D40/D41 do not reach, so the
    sentence now names all three.
  - Both are inside `files_modified`. No file outside it was touched.

### 5. [Forward reference] D93 does not exist yet

`DECISIONS.md` ends at D92. Four files now cite D93 — `src/simplex.c`,
`docs/work-units.md` and two places in `tests/test_simplex.c` — because the
plan's acceptance criteria require the citation. `ROADMAP.md:58` assigns D93 to
plan `01-05`. This is a dangling reference until `01-05` lands, and
`jaos-record` warns that source comments cite decision headings, so `01-05`
must number its entry **D93** and not one past whatever `DECISIONS.md` shows at
that time.

## Known Stubs

None. The `visited` counter that `01-01` left deliberately unread is now read
by the charge it was produced for, and its `[[maybe_unused]]` attribute is
gone.

## Threat Flags

None. No new network endpoint, auth path, file access pattern or schema at a
trust boundary. The change is one integer argument inside a file-private
function.

`T-01-04` (the D16 work-unit contract across the commit boundary) is mitigated
as the register specifies: the accounting change, the document that defines the
unit and the re-pinned detector landed in **one commit**, and the pinned
constant carries both figures with the derivation. The `WINDOWS.md` entry for
the D93 forward reference is recorded under Deviations 5 rather than the ledger,
which this repository does not carry.

`T-01-05` (baseline tampering) is untouched by this plan and enforced
structurally in `01-03`: no baseline was read, written or rewritten here.

## Estimate vs actuals

The plan estimated `tokens: 40000` at `confidence: low`. The realized diff is
9,489 characters — **2,372** on the chars/4 scale ADR-2629 specifies. As in
`01-01`, the two numbers are not on the same scale and the gap is not a 94%
underrun; recorded unadjusted rather than reconciled.

The work that dominated this plan was not diff size. It was four
measure-revert cycles under WSL to obtain figures Unity does not print, which
a token estimate does not model at all.

## Commits

| Hash | Message |
|---|---|
| `b65d9f2` | perf(01-02): the dense ratio test bills what it visits, and the ceiling it was measured against had drifted 2800 units while nobody looked |

## Self-Check: PASSED

All three modified source files present, `01-02-SUMMARY.md` present, commit
`b65d9f2` found in the log, and the two constants confirmed as committed:
`WORK_PINNED = 8536` at `tests/test_simplex.c:628`, the ceiling still
`< 62000` at `:1190`.

## What plan 01-03 inherits

- A settled tree. Every source edit of this phase is committed; the working
  tree is clean. A campaign launched now is valid for the tree that produced it.
- **All three baselines are stale, by a known and attributable cause.** Every
  dense-ratio-test work figure in `bench/netlib.baseline`,
  `bench/netlib-kennington.baseline` and `bench/netlib-infeas.baseline` comes
  from the previous definition. D-06's ordering is unaffected and still binding:
  confirm the 139 digests unmoved *first*, then rewrite.
- The expected direction of the work diff is **down, never up**, on every
  instance. `visited <= nvar` always, and equality holds only where no variable
  is basic. An instance whose work rises is a defect, not a re-definition.
- Instances taking the sparse path throughout will show **no work change at
  all** — their charge never went through the edited branch.
