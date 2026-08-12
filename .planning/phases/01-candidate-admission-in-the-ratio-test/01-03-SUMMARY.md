---
phase: 01-candidate-admission-in-the-ratio-test
plan: 03
subsystem: bench — the three instance sets, the work baselines
status: complete
tags: [campaign, netlib, kennington, infeasible, warm-start, baseline, digests, d46, d-06]

requires:
  - "01-01 — the bitmap walk whose trajectory this proves unchanged"
  - "01-02 — the accounting change whose effect these baselines now describe"
provides:
  - "139 digests confirmed identical to the committed record, dated before any rewrite (44c0ef6)"
  - "the three rewritten work baselines (e8c2f58)"
  - "per-set geometric means of the work ratio: 0.9779x / 0.9857x / 0.9860x"
  - "the count of dense-branch calls per instance, recovered from the work diff without instrumenting anything"
  - "a standard-set record that carries a real baseline comparison — the committed one did not"
affects:
  - "01-04 — the J=1 time ratio now has a settled record to be read against, and knows which instances the change actually bites"
  - "01-05 — D93's measurement on both sides is here; so is one fact it has to account for (galenet)"

tech-stack:
  added: []
  patterns:
    - "the derived quantity carries its own falsification test: work_saved must be a whole multiple of the row count, and the check is worth running because it can fail — it did, on one instance"

key-files:
  created: []
  modified:
    - bench/results/netlib.txt
    - bench/results/netlib-infeas.txt
    - bench/results/netlib-kennington.txt
    - bench/results/warm.txt
    - bench/results/warm-kennington.txt
    - bench/netlib.baseline
    - bench/netlib-infeas.baseline
    - bench/netlib-kennington.baseline

decisions:
  - "The digest confirmation got its own commit before the rewrite ran, so D-06's ordering is a fact in the history rather than a claim in a summary."
  - "Task 2's commit carries three files rather than the six its criterion names: the confirming run reproduced the committed records byte for byte, so the records had nothing to contribute to a diff. See Deviations."
  - "The saving is an exact multiple of the row count on all 139 instances, which is D-09's accounting identity confirmed on real data. The quotient counts dense-branch calls and NOT iterations — galenet refutes the iteration reading and the summary says so rather than rounding it away."
  - "The pre-existing `NOT COMPARED` standard-set record is fixed by the confirming run, but the missing automated check for it was recorded and NOT added: a tooling edit mid-plan invalidates the campaign this plan exists to run."

metrics:
  duration: "~55 min, of which 34.4 min was campaign time under WSL"
  completed: 2026-08-12

actuals:
  tokens: 32200
  tasks: 2
  commits: 2
---

# Phase 01 Plan 03: The digests are unmoved, and only then the baselines Summary

All 139 published answers came back bit-identical, every iteration count with
them, and the only column that moved anywhere in the record is the one the
accounting deliberately changed — so the three baselines were rewritten, in
their own commit, authorised by a verdict that was already in the history.

## The headline, and what it rests on

| | standard | infeasible | Kennington |
|---|---|---|---|
| instances | 94 | 29 | 16 |
| gate | PASS | PASS | PASS |
| against the old baseline | 0 regressed, 0 improved, 0 new | same | same |
| **digest changes** | **0** | **0** | **0** |
| bit-identical instances | 9 | 9 | 3 |
| instances that moved | 85 | 20 | 13 |

139 instances compared, **0 digest changes**, and every one of the 118 that
moved moved in `work` and in nothing else. `record_diff.py` printed no
`ITERS`, no `DIGEST`, no `STATUS`, no `PREDICATE` and no `FIGURE` line on any
of the three sets.

That last claim needed a second instrument, because `record_diff.py` has a
reporting gap on exactly one column: a `drop` change below the D88 threshold
sets its `moved` flag but prints nothing, so "118 moved, 118 `WORK` lines"
does not by itself rule out a sub-threshold `drop` change riding along. So the
whole record was masked at `work=` and diffed against `64efcc6` end to end.
**Every instance line in all three sets is byte-identical once the work field
is masked** — with a canary confirming the mask does not hide a real change
(altering `25fv47`'s iteration count by one digit is caught).

D-05 is met, and met in its strong form. It asked for identical digests; what
came back is identical *everything* except the charge.

## Iterations are 1.0000x on every instance individually

Not in the mean — per instance, all 139:

```
netlib             GEOMETRIC MEAN 1.0000x   best 25fv47 1.0000x   worst woodw 1.0000x
netlib-infeas      GEOMETRIC MEAN 1.0000x
netlib-kennington  GEOMETRIC MEAN 1.0000x
```

This is the claim `01-01` was built to earn and could not prove from a unit
suite. The candidate set and the array positions are unchanged by
construction, so every tie-break resolves the same way and the solver walks
the same trajectory; the bitmap only changed how it is reached. A digest is
evidence about the endpoint. An iteration count identical on `pilot87`'s
50,850 and `ken-18`'s 113,652 is evidence about the whole path.

## What the accounting change is worth, per set

Geometric mean of per-instance work ratios (D46):

| set | **geometric mean** | ratio of totals | best | worst |
|---|---|---|---|---|
| standard | **0.9779x** | 0.9933x | gfrd-pnc 0.9431x | 9 instances at 1.0000x |
| infeasible | **0.9857x** | 0.9669x | ceria3d 0.9407x | 9 at 1.0000x |
| Kennington | **0.9860x** | 0.9891x | cre-c 0.9476x | 3 at 1.0000x |

Down on 118 instances, **up on none**, which is what `01-02` predicted from
`visited <= nvar` holding always.

**The middle column is why D46 is a rule and not a preference**, and this set
of runs makes the point better than the usual example. On the standard set the
ratio of totals reads 0.9933x — it *understates* the change, because `pilot87`
and `maros-r7` carry 74% of the set's work and both barely move. On the
infeasible set it reads 0.9669x and *overstates* it, because `gosh` alone is
most of that set. Same instrument, same change, wrong in both directions. Only
the per-instance mean says the same thing about both.

## A count nobody had to instrument for

Before D-09 the dense branch billed `nvar` per call; after it, `visited`, which
is the nonbasic count `nvar - nrow`. So every dense call got exactly `nrow`
units cheaper and the identity inverts:

    work_saved = dense_branch_calls * rows          (JM_WORK_NONZERO = 1)

Worth running because it can fail. **On all 139 instances `work_saved` is an
exact whole multiple of that instance's row count** — a saving that was not
this quantity would land on a multiple by chance roughly one time in `nrow`,
so 139 hits is a confirmation of `01-02`'s edit stronger than anything in the
unit suite, on the real instances rather than on a six-variable model.

| set | dense-branch calls | reported iterations | share |
|---|---|---|---|
| standard | 193,422 | 280,970 | 68.8% |
| infeasible | 24,245 | 32,430 | 74.8% |
| Kennington | 31,077 | 285,270 | 10.9% |

Kennington is the interesting row: `ken-18` takes the dense branch 45 times in
113,652 iterations and `pds-06` 4 times in 9,251. D40/D41's sparse path already
covers those models almost completely, which is exactly why their work barely
moved — and it is the reason a change that looks worth 2.2% on the standard set
is worth 1.4% here.

**21 instances never take the dense branch at all** — 9 standard (`fit1p`,
`fit2p`, `ganges`, `recipe`, `sctap2`, `sctap3`, `seba`, `sierra`,
`stocfor3`), 9 infeasible, 3 Kennington (`ken-07`, `ken-11`, `pds-02`). Their
work is unchanged, and unchanged work *means* zero dense calls: a single one
would have to reduce the charge, since `visited < nvar` strictly for any model
with rows.

### The check fired, on one instance, and it is not being rounded away

The test also required the quotient not to exceed the reported iteration count.
**`galenet` failed it: 8447 → 8431 is 16 units, its rows are 8, and 16/8 = 2
dense calls in a solve that reports `iters=1`.** Its neighbours are exact —
`itest2` 27/9 = 3 calls in 3 iterations, `bgprtr` 120/20 = 6 in 25.

So the quotient counts **calls to `dual_ratio_test`, not iterations**, and at
least one of them happens somewhere that is not a counted iteration. That is a
small unrecorded fact about the solver, not a defect and not this plan's to
chase; it is handed to `01-05`, which has to state D93's measurement and would
otherwise be free to write "iterations" and be wrong on one instance in 139.

## The warm campaigns, which the gate cannot stand in for

`build_warm_basis` is one of the eight membership sites `01-01` hooked, and the
gate never loads a basis — it solves each instance once from a fresh load. So
these two runs are the only cover that site gets:

| | measured | skipped | disagreed | rejected | errors |
|---|---|---|---|---|---|
| `make warm` | 92 | 2 | **0** | **0** | **0** |
| `make warm-kennington` | 11 | 5 | **0** | **0** | **0** |

The skip counts are the ones `bench/README.md` documents — models whose optimal
values land on integers, so there is no fractional column to branch on. Warm
and cold agree on every verdict and every objective they were asked about, and
the independent checker refused neither side of any pair, which is the check
D92 added after the perturbed model's cold solve turned out to be the one
published answer here that nothing judged.

The same masked diff was run on these two records, with the mask fitted to
their `warm=iters/work` format: **every per-instance line is byte-identical
once the work half of each pair is masked.** Branch choices, iteration counts
both sides, objectives, `moved=`, and both checker verdicts are unchanged
across all 103 measured instances.

Two aggregate lines did move, and they are worth a sentence because they are
the one place in this plan where a ratio changed rather than a count:

| | before | after |
|---|---|---|
| work units warm/cold, geometric mean (standard) | 0.0162 | **0.0164** |
| work ratio, worst — `afiro` | 0.5688 | **0.5768** |
| work ratio, worst — `pds-06` (Kennington) | 0.0326 | **0.0329** |

Warm re-solve got very slightly *worse* relative to cold. That is the expected
direction and not a regression: the saving is per dense call, a cold solve
makes thousands of them and a warm re-solve makes a handful, so the change
takes more off the denominator than the numerator. It moves the third decimal
of a ratio that is already 0.016.

## The ordering, put where it can be checked

D-06 says confirm first and rewrite second. That is easy to *claim* in a
summary written after both have happened, so it was made checkable instead:

| commit | what it contains | what it establishes |
|---|---|---|
| `44c0ef6` | the five records, no baseline | the digests were unmoved **at this point in the history** |
| `e8c2f58` | the three baselines, no record | the rewrite, consuming that verdict |

`git log` now carries the ordering. The Task 1 script also asserted directly
that no `bench/*.baseline` was modified by it, and the assertion is in the
transcript above the diff it authorised.

### The confirming run, and the negative control it needed

A `*-baseline` run overwrites the results file with a record that compared
against nothing. Committing that record beside the baseline it produced makes
the pair unfalsifiable, so the three plain gate targets were re-run afterwards.
Both directions were checked rather than one:

- the **rewrite** copies must say `baseline: NOT COMPARED` — all three do, which
  is what proves the `-w` run actually happened
- the **committed** records must not — none of the three does

Then the confirming run's records were diffed against the rewrite run's, per
instance: **94, 29 and 16 bit-identical, 0 moved.** The two runs agree
completely, so the baseline was not written from a run its successor disagrees
with.

### The threat the register describes was already in the tree

Checking the `NOT COMPARED` control turned up that the reference record this
plan compared against was itself one:

| record at `64efcc6` | its baseline line |
|---|---|
| `netlib.txt` | **`baseline: NOT COMPARED (no baseline given)`** |
| `netlib-infeas.txt` | `baseline: 0 regressed, 0 improved, 0 new` |
| `netlib-kennington.txt` | `baseline: 0 regressed, 0 improved, 0 new` |

The committed standard-set record — the largest of the three, and the one the
project quotes from most — was produced by `make netlib-baseline` and had
therefore been checked against nothing. This is T-01-07 exactly, and
`bench/README.md` describes it as something that "went wrong immediately". It
was still true for the standard set at HEAD when this plan started.

**It does not weaken anything above.** A `-w` run still solves all 94 instances
and writes every per-instance line with its digest; `record_diff.py` parsed 94
instances out of it and compared them. What was missing was the comparison, not
the data. All three records now carry a real one.

What is worth handing forward is that the `NOT COMPARED` line exists to make
this detectable and **nothing in the repository reads it**. It was caught here
because Task 2's script checked it as a control, not because any gate, hook or
script does. `preflight.sh` is the natural home for the check — it already
refuses a campaign over an unsettled tree — and it does not have it.

### The baseline diff carries exactly one column

Checked field by field against `HEAD:bench/*.baseline` rather than assumed from
the line count: **column 9 — work — is the only column that differs on any of
the 139 lines** (85, 20, 13). Status, all four predicates, the iteration counts
and the dropped term are byte-identical. 118 insertions, 118 deletions, and 118
is the same number the record diff reported.

### No wall-clock figure anywhere

Grepped for it rather than trusted: no `elapsed`, no `time:`, no `N.NNNs` in
any of the three baselines or the five records. The seconds this plan produced
(66 / 9 / 491 / 118 / 257, and 63 / 11 / 487 / 66 / 9 / 489) are in this
document and in the commit messages' absence, nowhere else. They are also
`J=12` numbers and therefore inflated by contention; the wall-clock verdict is
`01-04`'s job and needs `J=1`.

## Deviations from Plan

### 1. [Rule 3 — a criterion that could not apply] Task 2's commit carries three files, not six

- **Found during:** Task 2, after the confirming gate run
- **Issue:** The acceptance criterion is `git diff HEAD~1 --stat` on the commit
  showing "exactly the three baselines and the three results files". It shows
  the three baselines only. The three results files were **not modified**: the
  confirming run rewrote them and produced content byte-identical to what
  `44c0ef6` had already committed.
- **Why they are identical, which is the part worth knowing:** the record is a
  function of the solver and the instances. The only baseline-dependent line in
  it is `baseline: N regressed, M improved, K new`, and that reads `0, 0, 0`
  against *both* baselines — against the stale one because falling work is not
  a regression the runner prints, and against the new one because nothing
  moved at all.
- **Checked rather than assumed**, because "file unchanged" and "nothing wrote
  the file" look identical from outside and that has printed a false
  confirmation here before: the files' mtimes are 16:43–16:52, after the commit;
  they carry a real comparison rather than `NOT COMPARED`; and they were diffed
  against the rewrite run's copies instance by instance.
- **Resolution:** the criterion's intent — that what is committed comes from a
  checked run against the new baselines — is satisfied more strongly than it
  asks. Staging an unmodified file to satisfy the letter of a `--stat` would
  have produced an empty commit entry and nothing else.
- **Files modified:** none — this deviation is the absence of three
- **Commit:** `e8c2f58`

### 2. [Process] Task 1 got its own commit, which the plan does not mention

- **Found during:** Task 1, at the commit step
- **Detail:** The plan writes a commit instruction into Task 2 only. Committing
  Task 1's five records separately is what dates the digest confirmation ahead
  of the rewrite, turning T-01-06's "mitigated structurally" from a statement
  about how the tasks were arranged into something `git log` can be asked. It
  is also where the two warm records land: they are in the plan's
  `files_modified` but appear in neither task's file list, and Task 2 does not
  touch them.

### 3. [Scope — a derived measurement the plan does not ask for] The dense-call counts

- **Found during:** writing this summary
- **Detail:** The `work_saved / rows` inversion is arithmetic over two files
  this plan already produced — no campaign, no instrumentation, no source edit.
  It was computed because `01-05` needs D93's measurement on both sides and
  this is one of the two sides, and because it doubles as a 139-instance
  validation of `01-02`'s edit. It is reported with the falsification it
  survived and the one it did not.

### 4. [Scope] The phase requirement is still NOT marked complete

`REQ-ratio-test-candidate-admission` covers the whole phase. The decision it
turns on — D93 — does not exist yet and is `01-05`'s, and `01-04` still owes
the time ratio that criterion 3 names. It stays `Pending` in
`REQUIREMENTS.md`, on the same reasoning `01-01` recorded. What this plan does
close is roadmap criterion 4, and that is annotated in `ROADMAP.md` with the
two commits that closed it.

## Known Stubs

None. This plan writes no code.

## Threat Flags

None. No new network endpoint, auth path, file access pattern or schema at a
trust boundary. `bench/fetch.sh` ran as it always does and verified every
instance against its pinned sha256; no instance file entered the repository.

The register's four entries, and where each was actually enforced:

| | |
|---|---|
| **T-01-06** — a baseline rewritten before the digest check | Enforced twice. The Task 1 script asserts no `bench/*.baseline` was modified, and the two-commit split puts the ordering in the history. |
| **T-01-07** — a record from a run that compared against nothing | Enforced with a control in each direction: the rewrite copies must say `NOT COMPARED` and do; the committed records must not and do not. **The control also found a live instance of this already committed** — `64efcc6:bench/results/netlib.txt` was a `-w` record. Fixed by the confirming run; see above. |
| **T-01-08** — a second session's runner writing `bench/results/` | `preflight.sh` ran before anything and reported no bench process in this repository, plus a settled tree and a green suite. |
| **T-01-SC** — supply chain | Accepted and unchanged. No package installed, no fetch path touched. |

## Estimate vs actuals

The plan estimated `tokens: 60000` at `confidence: low`. The realized diff is
128,798 characters — **32,200** on the chars/4 scale ADR-2629 specifies. Unlike
`01-01` and `01-02`, where the estimate overshot the realized diff by an order
of magnitude, this one is within a factor of two, and for a reason that says
something about estimating: this plan's diff is mostly *data*. 236 rewritten
record and baseline lines are close to predictable from the instance count,
whereas a source diff's size is not predictable from a task count at all.

What the estimate still does not model is the 34.4 minutes of WSL campaign time,
which is what the plan actually cost and is uncorrelated with its diff.

## Commits

| Hash | Message |
|---|---|
| `44c0ef6` | bench(01-03): all 139 digests are unmoved and every iteration count with them, so the only column that moved is the one the accounting changed |
| `e8c2f58` | bench(01-03): the three baselines are rewritten on purpose, and the unmoved digests of the commit before are what authorised it |

## Self-Check: PASSED

Run after the summary was written, against disk rather than against the summary:

- All eight files in `key-files.modified` present, plus `01-03-SUMMARY.md`,
  `STATE.md` and `ROADMAP.md`.
- Both commit hashes found in `git log`: `44c0ef6`, `e8c2f58`.
- `record_diff.py --against 64efcc6` re-run on the three records: **`0 digest
  change(s)` on all three**, reproducing the verdict this plan turns on.
- `bench/netlib.baseline` confirmed to differ from `64efcc6` — the rewrite is
  really in the tree and not merely described here.
- The three baselines differ from `64efcc6` in column 9 alone, and the three
  records carry a real baseline comparison rather than `NOT COMPARED`.
- Working tree carries only the three `.planning/` files, committed next.

## What plan 01-04 inherits

- **A settled record and a settled baseline, from one build.** A gate run now
  diffs clean; the next per-instance diff will show only what `01-04` does.
- **Roadmap criterion 4 is met.** All three campaigns PASS and the per-instance
  diff shows no regression on any of the 139 instances, on any of the four
  predicates or the work count.
- **Where the change actually bites**, which matters for choosing instances for
  a `J=1` time ratio. `truss` — the instance criterion 3 names — takes the
  dense branch and its work fell 1.43% (0.9857x). The two instances that carry
  74% of the standard set barely move: `pilot87` 0.9956x, `maros-r7` 0.9987x.
  So a time ratio taken on those two will see almost nothing, and picking them
  because D46 names them would be reading D46 backwards.
- **The seconds recorded here are useless to it.** Every figure above is `J=12`
  and inflated by contention. D45's time ratio needs `-j 1`, two binaries in one
  session, alternated, minimum of three or more rounds.
- **The work counter's optimism is unmeasured for this change.** 2.2% fewer
  units on the standard set says nothing yet about seconds: D45 puts the real
  cost of a billed unit across a 14.7x span, and the whole point of `01-04` is
  that the other two instruments cannot see it.
- **One open item, for whoever wants it and not for `01-04`:** nothing in the
  repository reads the `baseline: NOT COMPARED` line, which is why a `-w`
  record sat committed as the standard set's record until this plan. The check
  is two lines and `preflight.sh` is where it belongs. Not done here — this
  plan was under instruction to make no edit outside `bench/results/` and
  `bench/*.baseline`, and a campaign is only valid for the tree that produced
  it.
