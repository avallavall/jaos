# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## Where the last session stopped — 2026-08-19

### The state of the tree, first, because everything below assumes it

**Nothing is in flight and no worktree is registered.** The tree is clean
apart from one untracked directory that is not this session's (see
`bench/measurements/02-31/` below). **`make configs` exits 0**, which is all
five build configurations, and the three gate sets read `gate: PASS` with
`0 regressed, 0 improved, 0 new`.

**Say `make configs`, not "make test and the reference build both pass".**
That sentence stood here while three of the five configurations did not
compile (D154). `make` decides from timestamps and does not track a change in
`EXTRA_CFLAGS`, so `make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` run after a
plain `make test` re-runs the plain binaries and exits 0 without building
anything. `make configs` puts `make clean` between the runs.

**`main` and both tags are pushed, 2026-08-20**, on the maintainer's explicit
say-so. `v0.1.0` had been waiting since 2026-08-19 and went with `v0.1.1`.

**Push from the WINDOWS side, not from WSL.** The remote is
`git@github-personal:...`, and `github-personal` is an SSH alias that lives in
the Windows `~/.ssh/config` only — WSL has no `~/.ssh/config` at all, so a
push from there dies on `Could not resolve hostname github-personal` and looks
like a network fault. It is not: `github.com` resolves fine from WSL. Every
other step in the loop runs under WSL, so this is the one command that does
not.

**The gate campaign at HEAD is valid.** The last two source commits (D153 and
its correction) are debug-only: the release objects keep the same md5 as the
commit the gate ran on, measured rather than assumed, so the campaign carries
over. `bench/results/netlib.txt` and `warm*.txt` were rewritten deliberately
on their landed trees and are current; `netlib-infeas`, `plato-fome` and
`plato-pds` are behind by several `src/` commits and `preflight.sh` says so
on every run.

**Two things this session added that a later one should USE rather than
rediscover:**

- **An assert-enabled build now runs every instance** (`-UNDEBUG`, D152).
  Before it, eleven of the 94 aborted, so every assert in the solve was
  untested on them. Reach for it when diagnosing anything.
- **Every debug build verifies published row activities against the
  published columns** (D153), for rows whose logical is basic, to
  `(n-1)·eps` times the row's traffic. It is validated by fault injection
  and clean on all 139.

**This session (2026-08-18/19, unattended) landed D140 through D154 —
records 02-49 through 02-65, five source changes, one bench widening (the
gate sees the basis now, D150), three kept candidates, and the repository's
first tag, `v0.1.0`.**

**The session continued to D161 — records 02-65 through 02-71.** Eight items,
of which **four were wrong answers rather than latent risks**: presolve
refusing models `-DJAOS_NO_PRESOLVE` solves (D159, D160, D161) and publishing
a value outside a declared bound (D158). The gate is bit-identical on all 139
for every one of them, so **no campaign could have found any of them** — each
needed a constructed model, and `numerics-reviewer` built three.

**Read this before trusting your own tolerance work here.** The reviews
returned eleven findings across five reviews and every one was real. Four were
defects in the repair being reviewed, including one that **published `optimal`
on a model infeasible by 1e-3** and was one commit from landing. Two more were
justifications that sounded right and were false. One was a test that could
not fail. The pattern is not carelessness: it is that a tolerance argument
reads as sound in the source and is only settled by a model.

**And the probes were counting wrong.** `-j N` forks children onto one stderr;
a single `fprintf` with many conversions tears, `grep` still counts the line,
and the SUMS come out low. Three readings of one counter gave 4858, 4844 and
4798 at 188 lines every time. Every probe writes one `write(2)` per record now,
and D156's and D158's readings were re-taken because both had landed reporting
zeros — a count that can only be undercounted is the wrong shape for a finding
that IS a zero. Both stand.

**D154 is the first of them, and it is the one to read before trusting any
"all builds pass" sentence.** It started as §5's comment edit and found that
the reference build and both fault builds had not compiled since D151. Two
more failures were hiding behind the compile error. `make configs` is the
guard, and `bench/measurements/02-65/` also records the three separate ways an
object-md5 comparison reports a difference that is not one — `-flto` being the
default is the surprising one, because it makes two builds of ONE unedited
tree differ on 12 of 12 objects.

**D153 is the last of them, and the entry's value is what it got WRONG.**
The row-activity check it built reported `pilotnov` as a defect; the finding
was committed and then withdrawn the same day. Four versions of that one
predicate each accused someone innocent — no gate on an optimal solve, a
fixed multiple of eps where the quantity was a sum of n terms, comparing rows
whose logical rests on a bound, and once the harness rather than the check.
What settled it was splitting the firing population by basis status, and the
split was total: 18 firings on one side, 0 on the other.
`bench/measurements/02-62/` carries all four with what each falsely said.
**Read it before widening or narrowing any predicate over published values.**

**D152 closed the last of the two standing debts that were worth more than
they looked.** The singleton-column replay clamps its published value into
the column's own box, `bnl1` and `finnis` stop publishing up to 1.3e-15
outside a declared bound, and **all 94 standard instances run under
`-UNDEBUG` where eleven aborted** — a whole build configuration back. Its
record (`bench/measurements/02-61/`) corrects a number this file had carried
since 2026-08-14 and refuses two tolerance designs with the measurement that
refutes each.

**D151 closed D149's condition**: the warm count
repair landed behind a shortfall cap of 4, netlib warm work 0.2553 → 0.1916
and Kennington 0.0572 → 0.0070, the three gate sets unmoved. Its record
(`bench/measurements/02-60/`) carries three things worth reading before any
similar work: a sweep whose per-instance-choice arithmetic was **checked
against a real campaign on 103 of 103 instances**; the obvious better-shaped
constant (a cap relative to the model's rows) **swept and refuted**, because
the worst instance in the set has the smallest relative shortfall; and a test
whose observable had to be the log line rather than an iteration count,
because warm and cold counts coincide by accident on unit-test-sized models.

**A previous session's cap=1 job died mid-run and its two output files were
recovered** from `build/diag/cap-sweep/`. They are preserved at
`bench/measurements/02-60/dead-session-cap1/` and match this sweep's cap=1
prediction on every instance, which is what turned the arithmetic into a
measurement. Nothing else of that job survived — no diff, no record, no entry.

The earlier arc, in one line each: D140 (the 80 are an exact tie)
plus the swap's value guard, bit-identical everywhere; D141 (within-row
demotion refused); D142 (remember-basis count guard refused on its
measured warm cost); D143 (warm re-measure: Kennington improved, netlib
regressed 3.7x through the mapping; records rewritten); D144 (the balance
is multi-family; repair selected at the consumer); D145 (that repair
refused by its own judge — eight wrong optima through the termination
hole); D146 (the hole reproduces at HEAD through the public API alone);
D147 (located: the settling loop publishes the violation it measured);
**D148 (the certificate guard landed — 0 wrong of 80, gate bit-identical,
`jaos.h`'s promise enforced)**; and D149 (the warm repair retried behind
the guard: correct now, refused on `dfl001`'s 172x; the shortfall-cap
sweep has its material named). **Both subagents are live**:
`numerics-reviewer` delivered six times and `jaos-measurer` returned
ACCEPT on D151 from its own campaign. **They deliver late**, though —
on D151 both reports arrived after the main context had finished the
same work itself, so the loop's steps 3 and the verdict were done twice.
Neither disagreed, and each found something the other did not; D151's
entry says which. The three source changes are
`ps_singleton_col_swap`'s value guard, D148's certificate guard with its
cold restart, and D151's capped warm repair; everything else was
measurement or a kept candidate.

**Read this before judging any basis work.** The gate cannot see a basis.
`bench/run.c` says so: the digest covers x and y and not the statuses, so *"a
change that moves only the basis is invisible to all three sets"*. D138 and
D139 were judged by `bench/measurements/02-47/` and `02-48/`, which count the
published basic variables directly. A green gate on a basis change means
"nothing else broke", not "it worked".

**D127 left the working tree dirty and it was reverted deliberately.** A gate
run writes `bench/results/*.txt`, which are committed files, so a refused
candidate dirties the record as well as `src/`. Both were restored with
`git checkout --`. Check `git status` before believing a record.

Three things about this working session that a later one needs and cannot
infer:

- **Another Claude session was committing and pushing to this repository at
  the same time.** Its commit was documentation only (`docs/diagrams/`,
  `docs/architecture.html`) and could not affect a solve, so no measurement was
  invalidated. If two sessions are live again, `CHANGELOG.md`, `DECISIONS.md`
  and `TODO.md` are the files that will collide.
- **`bench/measurements/02-31/run-gaps.sh` is untracked and is not this
  session's.** It is an unfinished probe of the `want_lo <= want_hi` gaps, for
  the standing debt at the end of this file. It was left alone deliberately.
  Do not commit it without knowing whose it is.
- **A campaign cannot be run in the main tree while it is dirty**, so a
  candidate is measured in a git worktree with `bench/instances*` symlinked in.
  `preflight.sh` follows symlinks now. See the memory note
  `measure-a-candidate-in-a-worktree`.

### What closed today, in one line each

`fome`'s candidates all go to D95 before D106 sees them (**D117**) → giving
D106 first refusal is **refused**, `pilotnov` publishes 29% wrong (**D118**) →
that was not presolve, the same reduced model is right at a shorter
refactorization interval (**D119**) → five explanations closed, one
contradiction left (**D120**) → the contradiction is a cost that never comes
back (**D121**) → **repaired and landed** (**D122**) → no loan is outstanding
when the duals are published, on any instance that answers (**D123**) → and
the 186 missing loans were never missing, the tally added them one way and the
repayments another (**D124**) → no loan swamps a real cost anywhere in the
gate, while one lend in six sets `d` to zero on a cost that never moved
(**D125**) → that zero turns out to be what stops the breach compounding, so
removing it is **refused** (**D126**) → and the wrong-signed dual step next to
it is holding `pilot87` up, so clamping it is **refused** too (**D127**) → and
the record now says what the cost moved by, which closes §5a (**D128**).

**Then a maintenance task turned into the largest correctness item in the
file, and it is now half closed.** Refreshing two stale `warm` records
(**D129**) showed 25% of netlib losing its warm start; the losses split into
short and over (**D130**); presolve's mapping turned out exact and the basis
handed to it already wrong (**D131**); the drift was traced to two families
(**D132**, **D133**) and then to four named branches (**D134**); the exchange
was measured available and valid (**D135**); the singleton row's rule fell out
of its own dual (**D136**); the published literature confirmed all three rules
(**D137**); and both repairs landed (**D138**, **D139**).

**Kennington publishes a valid basis on every solve now.** netlib went from
132 wrong solves of 188 to 48, and its worst error from 596 basic variables
too many to 23.

The repair: a repayment restores from a write-once `cost0` instead of
subtracting the recorded loan, because `x += d; x -= d` does not restore `x`.
Costs 1.0001x on netlib, 0.9975x on Kennington, 29 of 29 infeasible instances
bit-identical, iterations moving on one instance of 139. `jaos-measurer`
ACCEPT.

D123 closed §5a's first item and left an assert in `publish` behind it. D124
closed the second and left nothing behind but a corrected number: 67, not 186,
is the count of columns whose cost moved while the record read zero, and two
source comments had borrowed the wrong line of the same file. D125 measured
the third and refused half of it; that half's repair was never needed and the
other half turned out to fire on one lend in six. **D126 and D127 then built
the two repairs that were left and the measurement refused both**, on
neighbouring lines of the same machinery — see the warning at the head of §5a.
All of D123's, D124's and D125's are no-ops on the
sets, 94, 29 and 16 instances bit-identical to the committed record.

## §5a is closed. Nothing in the shift machinery is open.

Six entries, D123 to D128, and the shape of the section changed twice under
them. What it looked like at the start and what it turned out to be:

| §5a asked for | what the measurement said |
|---|---|
| is a loan outstanding when the answer is published? | no, on all 128 instances that answer (**D123**), and the assert stays |
| 186 loans go missing | they never went missing; the tally added them one way and the repayments another (**D124**) |
| bound a loan against the cost it lands on | no loan swamps a real cost anywhere in the gate — **refused** (**D125**) |
| `d[v] = 0.0` is a fabrication | it is not; removing it lets the breach compound — **refused** (**D126**) |
| the dual step is computed from an unclamped `d` | true, and that step is what keeps `pilot87` moving — **refused** (**D127**) |
| `shift[v]` records a loan that was never made | true and repaired; one instance moves at 0.9921x (**D128**) |

**Four refusals to one repair, and every refusal came from measuring rather
than from reading.** This machinery reads as careless and is not. Two
candidates that looked correct in the source were refuted by the numbers, one
of them only by the gate. **Do not land a change here on an argument from the
code alone.**

What is still true and unrepaired, both with their size on the record:

- The dual step is computed from an unclamped `d`, on 248 of netlib's 477562
  picks and 170 of Kennington's 435418, worst step 8.37e-09
  (`bench/measurements/02-35/`). Every answer is `optimal` and `checker=ok`,
  so it costs nothing today. D127's reopen condition is in the refusals table.
- D121's loan of 1e32 on a cost of one is real and stays reachable through
  D118's refused presolve candidate. No instance in the gate reaches it.

## → START HERE — what is actually next, 2026-08-19

**D146's hostile-basis item is CLOSED and is no longer the start.** All four
of its steps landed: the defect was located (D147), the certificate guard
shipped (D148), the warm repair was refused blanket (D149) and landed capped
(D151). Its detail is kept below under "the hostile basis, for the record".

Nothing open today makes JAOS publish a wrong answer. What is open is listed
in the order it should happen, with what each already has.

### 1. ~~The collapsed fold~~ — CLOSED 2026-08-20 (D158), BOTH halves

**The unbounded error is gone**, and it was one repair for both halves
(`bench/measurements/02-68/`). The midpoint is clamped into the column's own
box, which is D152's repair on the other family, and it keeps the symmetry the
midpoint was chosen for because the clamp reads the box rather than which end
was tightened.

- **The value half**: the published value can no longer sit outside a bound
  the caller declared. The reproducing model goes from `1e9 + 2.4e-7` to `1e9`
  and still solves `OPTIMAL` rather than becoming `INFEASIBLE`.
- **The dual half**, which was expected to need its own decision and did not.
  Two singleton rows folding into one column leave the second fold's midpoint
  strictly inside the box the first one left, so no record's bound equals the
  published value and the cost goes unpaid. The clamp puts the value back on
  the first fold's bound and restores ownership: `max_dual_violation` 1 → 0.

**0 collapses in 100018 singleton-row folds** over the three sets, so the
branch never runs there and the gate is bit-identical on all 139.

**Two things the entry corrects, both found by `numerics-reviewer`.** The
first version aborted on an inverted column box, which `jaos.h` says is legal
input; and the clamp doubles the row residual, which on a gap near the top of
the window crosses an absolute `CHECK_TOL` the midpoint's split stayed under.
The second is kept deliberately and D158 says why.

### 2. 48 netlib solves still publish a wrong basis count

Measured, attributed to two families, and every LOCAL repair is refused with
its measurement (D140, D141). What is left is a design wider than the firing
row carrying a rank argument, or accepting the residue — which is what the
published state of the art does (Galabova 2023). Its whole price is 48 solves
losing their warm start. §"`jaos_basis` publishes something that is not a
basis" below has all of it.

### 3. Two instances still lose real work behind D151's cap

`scsd1` 4.65x and `degen2` 4.09x, both with warm iterations exactly equal to
cold — D148's guard rejecting the repaired trajectory and charging the
attempt plus the whole cold solve. **The shortfall cannot be the rule that
separates them and this is measured**: both are short by 1, the same
shortfall as the sixteen instances that win. Needs a predictor of a doomed
trajectory. Refusals table, D151.

### 4. Three smaller, each measured and each needing its own campaign

- ~~**`row_traffic[i]` saturates to `+inf` and never recovers.**~~ **CLOSED
  2026-08-20 (D155, `bench/measurements/02-66/`).** It accumulates only what a
  still-finite end absorbed now, and the three sets are bit-identical on all
  139. Two things the entry corrects: it was **110 of 117** rows at zero
  margin, not all 117; and fixing the accumulation does NOT change the
  frozen-row test, because that test reads `ps_bound_scale` and never reads
  the traffic. What WOULD change it is the item below, which is new.
- ~~**The frozen-row test's scale, now that the traffic is a live
  quantity.**~~ **CLOSED 2026-08-20 (D159, `bench/measurements/02-69/`)**, and
  it was a wrong answer rather than a tolerance improvement: presolve reported
  INFEASIBLE on a model `-DJAOS_NO_PRESOLVE` solves. 0 verdicts flip on any of
  the 139, so the campaign could not see it and only a constructed pair
  separates the two windows.

- ~~**`ps_bound_scale` in the FROZEN-ROW window is a live wrong answer, and it
  predates D159.**~~ **CLOSED 2026-08-20 (D161,
  `bench/measurements/02-71/`).** The term is dropped, the model reads
  INFEASIBLE with the oracle, and every one of D159's own frozen-row tests
  still passes. Found by `numerics-reviewer` while reviewing D160.

  ```
  -1e12 <= x0 + x1 <= 0,  x0 and x1 both cost 0 in [1e-4, 1]
  ```

  Both are cost-0 bounded singletons, so both relax and freeze the row, which
  empties. Infeasible by 2e-4. Reads **optimal** with `x = {1e-4, 1e-4}` both
  before D159 and with D159 landed; INFEASIBLE on the reference build and
  with the term dropped; INFEASIBLE on all four with `rl = -INFINITY`, which
  is the control confirming it is the irrelevant end of the row supplying the
  window.

  **It is the same term D160 dropped from the activity pass**, for the same
  reason: the window comes from the row's LOWER bound for a test on the UPPER
  side, and `ps_bound_scale`'s own comment says it is for a comparison between
  two BOUNDS. D159's safety argument does not reach it — an emptied frozen row
  is deleted with everything else, so nothing re-tests it.

  The repair is the one line D160 already validated at the sibling site. It
  needs its own campaign because D160's was not run for it.

- **The activity pass has the SAME defect, in mirror image, and it is live
  today.** ~~Found by `numerics-reviewer` while reviewing D159.~~ **CLOSED
  2026-08-20 (D160, `bench/measurements/02-70/`).**
  `ps_row_tol(&rg)` is `8*eps*rg.traffic` alone, so the BOUND side's error is
  uncovered there exactly as the activity side was uncovered at the frozen-row
  test. The model, which is D159's with one cost changed from 0 to 1 so the
  row never freezes:

  ```
  min x1 + x2  s.t.  R: 1e9*x0 + x1 + x2 <= 1e9
  x0 in [1,1]  fixed -> cur_ru = 0, row_traffic = 1e9
  x1 in [1e-10, 10] cost 1
  x2 in [0, 1] cost 1
  ```

  Shipping build **INFEASIBLE**, reference build **OPTIMAL**. A second
  feasible model refused.

  **The repair is NOT a copy of D159's.** That `rtol` is shared with FORCING
  and REDUNDANT, and widening the forcing window is the change that pinned
  `pilot` column 3554 and cost 02-04 a campaign — the comment beside the
  constant says so. Clause 1 needs its own window, as its own change with its
  own measurement.

- ~~**`PRESOLVE_ROUND_ULPS` is missing its term count on a bound side.**~~
  **CLOSED 2026-08-20 by TWO entries, and the first one only reached three of
  the four sites.** D162 (`bench/measurements/02-72/`) took the emptied row,
  clause 1 of the activity pass and the frozen-row test; **D163
  (`bench/measurements/02-73/`) took the singleton row's fold**, which judges
  `cur_rl[i] / a` and was left on a flat eight ulps for one commit. The class
  is closed at all four now, and `docs/tolerances.md` owns the list — it had
  the wrong three in it, which is how the fold was missed
  (`numerics-reviewer`).
  It was a live wrong answer rather than a coherence repair, at both entries: a
  constructed model with an exactly representable feasible point reads
  INFEASIBLE at 256 removals and is accepted at 128, and D163's fold model is
  refused where the reference build solves it to the last bit. The largest
  count on the three sets is 325, 804 rows carry more than eight, and the gate
  is bit-identical on all 139 for both.
  **Two shapes were built before the right one and both are in the entry.**
  The count times the traffic alone is short, because the per-step rounding is
  half an ulp of the PARTIAL SUM and near the boundary that is the activity
  rather than zero. The count times `ps_bound_scale` brings D161's defect back
  through the count and D161's own test refuses it. Read that before touching
  any window here: **the three sets cannot separate the two shapes** — they are
  the same number on every row of all 139.
- ~~**Two smaller things D155 left, both cheap.**~~ **CLOSED 2026-08-20
  (D157).** They were the same predicate, so `ps_traffic_usable` is asserted
  at all three places now. The measurement adds something the sweep alone did
  not: with the accumulation reverted, netlib aborts on 45 of 94 **at the
  sweep and not at either read**, so the "unreachable" claim those two
  fallbacks carry is true from the assert side too.
- ~~**A row's own width can be destroyed by the shift that removes a
  column.**~~ **REFUSED 2026-08-20 (D156, `bench/measurements/02-67/`).** The
  collapse is real and reproducible, and this file's own claim that "the
  answer is wrong by up to that width" overstates it. **The width that dies
  was already below one ulp of the activity it constrains**, because a shift
  close enough to leave `rl - t` small subtracts exactly. 0 of 8942 shift
  events lose any width on the three sets, and seven forced shapes are
  bit-identical to the reference build — including a surviving singleton at
  `a = 1e-12`, which is the amplification that makes §1 unbounded and does not
  apply here. **Do not confuse this with §1.** §1's error is relative to the
  row's TRAFFIC, which cancellation puts above the activity; this one is
  relative to the activity, and the division by a small `a` scales both.
- **A fold fixes a column at a value carrying another row's error, and no
  window can see it.** Confirmed as a wrong answer with the oracle disagreeing
  (D163, `bench/measurements/02-73/`), and the repair is refused as a window:

  ```
  row S:  x1 + (256 y_s fixed at 2^-25) == 1e9      x1 in [1e9-1, 1e9+1]
  row R:  x1 + w1 + w2 == 1e9 - 63*2^-23            w1, w2 in [0, 2^-23]
  ```

  Feasible exactly at `x1 = 1e9 - 2^-17`, `w1 = 2^-23`, `w2 = 0`. Round 2 folds
  row S and fixes x1 at 1e9, wrong by 7.6294e-6, and the fold's own test does
  not fire because `new_lo == new_hi` there. Row R is charged **one** shift at
  its own traffic and clause 1 refuses it. Shipping INFEASIBLE, oracle optimal
  at 1.1920928955078125e-07.
  **CLOSED 2026-08-20 (D165, `bench/measurements/02-75/`)**, by the first of
  the two directions below: the row bounds keep their residue now, so the fold
  fixes the column at the value the model actually has and there is nothing to
  inherit. The model publishes the oracle's answer bit for bit with both rows
  at residual zero. **It is also the first change in this class that moves the
  gate** — netlib 15 moved and 14 digests, work geomean 1.0000x, iterations and
  reduction counts identical on every one.
  ~~**What is left from it: the shift counts are now redundant and still
  ship.**~~ **CLOSED 2026-08-20 (D166, `bench/measurements/02-76/`).**
  `row_shifts`, `ps_shift_excess` and `ps_end_scale` are gone, 196 lines of
  `src/presolve.c` with them, and all five tests they were built for still
  pass — which is the evidence, because those tests are the models. 139 of 139
  bit-identical. **The whole class D159 opened is closed at all four sites**,
  by compensation rather than by tolerance.
  The history below is kept because the refusal in it is what stopped a worse
  repair landing.
  **The error weight was built and is REFUSED (D164,
  `bench/measurements/02-74/`).** It stops the refusal and then publishes
  `optimal` with **both rows violated by 7.5 times `CHECK_TOL`** and an
  objective of 0 against a true 1.1920928955078125e-07. A wider window decides
  whether to refuse; it cannot correct a value that is already wrong, so it
  only converts a loud failure into a silent one. The route itself is real and
  large — 324826 window reads on Kennington see a non-zero inheritance and the
  worst is 1.143e+05 times the shipped window — and it flips 0 verdicts on the
  139.
  **Two directions are left, neither built nor measured:**
  - **Compensate `cur_rl`/`cur_ru`.** They are the only uncompensated running
    sums in `src/presolve.c`, while `ps_row_range` has used Neumaier for
    activities since 02-04. If the bounds carry no error the fold's value is
    right and nothing inherits anything — and **D162's and D163's shift counts
    stop being needed**, so this subsumes the class rather than adding to it.
    `long double` is out (D34); Neumaier is portable and already in the file.
    This is the one to do first.
  - **Widen the folded BOX instead of the window**, so the column stays a range
    and the simplex judges it. Reduces less, needs its own campaign, and is
    §11b's deliberate-slack direction reached from another question.
  `test_a_folds_value_carries_its_rows_error_into_the_next` pins the wrong
  answer and the reference build's right one in the same test, so whichever
  lands announces itself there.
- **The solver's own row activity loses terms the way presolve's bounds did**,
  and it is new (D162, `bench/measurements/02-72/`). It sums a row's columns in
  index order, so a row holding a large magnitude before many small ones loses
  every one of the small ones. On D162's model the reference build
  `-DJAOS_NO_PRESOLVE` reads INFEASIBLE where the feasible point is exactly
  representable, at every removal count — including the counts where presolve
  accepts. It is a defect in the feasibility test and not in any presolve
  window, and it is why D162 has no reference-build disagreement to show.
  Nothing on the three sets is affected: 139 of 139 bit-identical. What it
  needs is a compensated activity, and `ps_range`'s Neumaier accumulator is the
  shape that already ships in presolve; `src/check.c` uses `long double` and
  cannot be the model, because D34 forbids it outside that file.
- **The unclamped dual step**, 248 netlib picks and 170 Kennington picks,
  worst 8.37e-09. Costs nothing today; clamping it was refused (D127) because
  the perturbation is what keeps `pilot87` moving.

### 5. ~~A stale claim in `src/presolve.c`~~ — CLOSED 2026-08-19 (D154)

The claim was stale. `git log -S` puts it in `541f7dd` and its repair one
commit later in `7587ecd`, both 2026-08-14: the frozen-row feasibility test at
the end of `jm_presolve_run` is what revisits the row, and four tests pin it.
The comment is corrected rather than deleted, because the replay site still
cannot detect an infeasible model and naming the site that now does is worth
more than saying nothing.

**Checking it cost three broken build configurations**, which is the whole
value of the entry: the reference build and both fault builds had not compiled
since D151, and `make` re-running the previous binary is why nothing said so.
`make configs` now builds all five. Read D154 before writing "all
configurations pass" anywhere.

### If all of the above is dropped

`D97` is the largest prize in the file and is behind one precondition: a dual
postsolve for an imposed bound. `docs/research/dual-postsolve-imposed-bound.md`
is the design and nothing is built. It unlocks two families, not one — §3's
doubleton equalities need the same machinery and are 8.55% of netlib's live
rows and 29.36% of Kennington's.

## The hostile basis, for the record — CLOSED (D146 to D151)

**Nothing here is open.** It is kept because the four steps and their
measurements are what a similar investigation should copy, and because the
numbers below are what "before" looked like. What follows describes the tree
as it was on 2026-08-19 BEFORE D148, not the tree today.

**This WAS the largest open correctness item in the file and it is public
API.** `jaos_read_mps` + `jaos_set_basis` + `jaos_solve`: `degen2`
publishes a wrong objective as `JAOS_SOLVE_OPTIMAL` on **16 of 16**
deterministic hostile bases (worst −1352.64 against a true −1435.178,
5.7%), `scsd1` on 10 of 16 with the point checker-refused on 15 of 16;
`cycle`, `modszk1` and `woodw` hold. 26 wrong optima in 80 trials, no
candidate code involved (`bench/measurements/02-54/`, D146). The caller
sees no signal. `jaos.h` states the defect beside its broken promise.

The mechanism is this file's own, carried since D119: **the termination
never re-reads dual feasibility, so a solve that starts badly can end
wrongly.** It now has seconds-cheap reproductions — `run-hostile.sh`
rebuilds them all — where before it had a 278003-iteration warm campaign.

In order:

1. ~~Diagnose on `degen2`, shift 1.~~ **Done 2026-08-19 (D147,
   `bench/measurements/02-55/`), and the defect is located to the line
   class**: the settling loop measures its dual violation every round,
   keeps the best (`bstdv = 35.34` here, fixed at round 1, never beaten
   through all 32 rounds), and `take_best_if_better` publishes it as
   `OPTIMAL` with nothing comparing that number to a tolerance.
   `classify_optimum` passes trivially — every lend repaid, and invented
   bounds are all it asks. D89's "publish the best instead of failing" is
   where the laundering happens; it was benign on `pilot87`'s 8.37e-09.
2. ~~Measure the guard's two sides.~~ **Done (02-56)**: all 220 legitimate
   settling exits read exactly zero excess; the hostile one reads 35.34.
3. ~~The guard plus cold restart.~~ **Landed 2026-08-19 (D148,
   `bench/measurements/02-57/`)**: 02-54 reads 0 of 80 on the candidate
   (was 26 wrong + 5 refused), the gate is bit-identical on all 139, and
   `jaos.h`'s promise is enforced rather than assumed. The review's HIGH
   finding was load-bearing: the driver settles once more before reading,
   or a repair-singular restore would hand the guard `d[v] = 0.0` on
   exactly the breached columns.
4. ~~The warm retry.~~ **Refused blanket (D149), landed capped (D151,
   `bench/measurements/02-60/`).** D149: the correctness bar was met —
   `disagreed=0, rejected=0` where 02-53 read 8 and 2 — and `dfl001` went
   from a cold fallback at 1.0 to **172x work**, its 596-short repaired
   basis buying a ~2e6-iteration doomed trajectory the guard then threw
   away, with the netlib work geomean at 0.2605 against the repair-less
   0.2553. D151 swept the shortfall cap on both sides and landed it at
   **4**: netlib work **0.2553 → 0.1916**, Kennington **0.0572 →
   0.0070**, worst per-instance ratio 4.65 instead of 172.03,
   `disagreed=0, rejected=0` on both sets, and the three gate sets
   unmoved because the gate never reaches `build_warm_basis`. The value
   is chosen at the end of a plateau, not at the minimum — the mean is
   flat across caps 1..7 and the worst case steps to 15.48 at 7
   (`greenbea`). **The relative cap `S/nrow` was swept too and is
   worse**, because `greenbea` is 7 short of 1954 rows and is one of the
   two worst outcomes. What is still open is the two instances that lose
   real work behind the cap, in the refusals table.

## `jaos_basis` publishes something that is not a basis

**This is the largest open correctness item in the file and it was a standing
debt an hour ago.** Refreshing the stale `warm` records (D129) led to it in
four steps: 25% of netlib loses its warm start, the losses split into short
and over (D130), presolve's mapping turns out to be exact and the basis handed
to it already wrong (D131), and the gate then says so directly.

| set | optimal solves | basic count exact | **wrong** | worst over | worst under |
|---|---|---|---|---|---|
| netlib, at D131 | 188 | 56 | 132 (70%) | 596 | 169 |
| netlib, after D138 | 188 | 88 | 100 | 596 | 0 |
| netlib, **now (D139)** | 188 | **140** | **48 (26%)** | **23 — stale, see below** | **0** |
| Kennington, at D131 | 32 | 8 | 24 (75%) | 12104 | 406 |
| Kennington, after D138 | 32 | 24 | 8 | 119 | 0 |
| Kennington, **now (D139)** | 32 | **32 — all** | **0** | **0** | **0** |

**Both halves landed. Kennington publishes a valid basis on every solve.**
D138 decided the singleton row's status after the replay; D139 gave the
singleton column its exchange. Both pinned change-detector tests fired in the
direction their own comments predicted and are re-pinned at the reference
build's answer.

**Do not use the summed error as the target.** netlib's sum went from +3904 to
+5942 under D138 while every other measure improved, because the under-count
was cancelling part of the over-count. **The measure is the count of solves
publishing a wrong basis.**

**The "worst over" cell is stale as of D165 and is annotated rather than
replaced.** That 23 is `bandm`, and `bandm` now publishes 18 too many:
compensating the row bounds moved four instances' bases without changing how
many solves are wrong (`bandm` +23 → +18, `czprob` +3 → +1, `finnis` +1 → +2,
`capri` unchanged at +6). **The count of wrong solves — the measure this table
says to use — did not move**, and no other instance can have, because `basis=`
hashes exactly those statuses and only four moved. Counting the other 47 was
not done, so the new worst is unknown and inventing one from four instances
would be worse than saying the old one expired. Measured by `jaos-measurer`
while judging D165, with the reference build holding the contract on both
sources as the control.

`src/model.c` states the rule twice — *"a model with n rows needs n basic
variables … it is structural"* — and enforces it twice: `jaos_set_basis`
refuses a basis whose count is wrong, `basis_survives_or_goes` clears one that
becomes wrong. **`jm_model_remember_basis` checks nothing.** So the solver
publishes through `jaos_basis`, and stores for its own next solve, something
it would refuse from a caller.

**No answer is wrong and the gate is green.** Nothing in the solve reads the
basis back except `build_warm_basis`, which refuses it — which is why 21
`src/` commits passed with nobody noticing.

**Two families are the whole of it, and the sum closes exactly (D133).** The
writer is recorded at the moment of each write, so the attribution is not an
inference:

| last writer | netlib | Kennington |
|---|---|---|
| `REDUNDANT_ROW`, `FORCING_ROW`, `EMPTY_ROW`, `IMPLIED_FREE_COL` | 0 | 0 |
| `FIXED_COL`, `EMPTY_COL` | **0** | **0** |
| survivors | 0 | 0 |
| **`SINGLETON_COL`** | **+5902** | **+482** |
| **`SINGLETON_ROW`** | **−1998** | **+25172** |
| sum = published error | **+3904** | **+25654** |

Nothing is unaccounted for and the survivors balance, so the reduced solve's
basis is a basis and nothing upstream of the replay is wrong.

**`SINGLETON_COL` is one line** (`src/presolve.c:2131`):

```c
orig->sol_col_status[j] =
    (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;
```

**Its row survives** — the record's `index` names a row that stays, relaxed —
so it restores a column, marks it `BASIC` whenever the value is not at `hi`,
and no row comes back to pay for the basis position. +1 per firing.

**`SINGLETON_ROW`'s branches are counted (D134)**, and the sign flip is which
wrong combination dominates. It restores one row and writes two statuses by
two tests that never look at each other, so `drift = (row BASIC) + (column set
BASIC) − 1`:

| combination | drift | netlib | Kennington |
|---|---|---|---|
| row at a bound, column not set | **−1** | 2524 | 3886 |
| row at a bound, column set BASIC | 0 | 4200 | 48586 |
| row BASIC, column not set | 0 | 1372 | 116 |
| row BASIC, column set BASIC | **+1** | 526 | **29058** |

**A repair that handles one direction fixes one set and worsens the other.**

### The obvious repair does not work, and that is measured

"Stop writing BASIC" fails: `AT_LOWER` and `AT_UPPER` are claims that the
variable rests on that bound, and `SINGLETON_COL`'s `BASIC` branch is reached
precisely when it rests on neither. **A strictly interior variable cannot be
nonbasic.** The status is not the error. The basis has one member too many,
and correcting it means taking a different variable *out*.

**So it is a swap**: mark the interior column basic and move some row's
logical out of the basis, chosen so the result is still a basis. Nothing in
postsolve does that. `SINGLETON_ROW`'s "row at a bound, column not set"
combination is the opposite problem — one member too few — and needs a
variable brought *in*.

**Do these in order.**

1. **48 netlib solves still publish a wrong count, and every local repair is
   now measured unavailable.** The re-measure this item ordered is done
   (**D140**, `bench/measurements/02-49/`); the local candidates are closed
   by **D141** (`bench/measurements/02-50/`), whose reopen condition is in
   the refusals table. The three shapes, for the record:
   - **80 firings whose row logical the REDUCED solve left nonbasic**, all
     surviving, resting exactly on the widened row bound (58 lower, 22
     upper). Not a second defect: an exact degenerate tie the replay's
     division rounds ulps interior. D140's snap candidate is **refuted**
     (D141): 74 of the 80 rows land exactly ON their original bound with
     the interior `xv`, and snapping perturbs that activity by ulps —
     it trades a column-status defect for a row-status one.
   - **152 firings whose row is not on a bound.** D135 said 108: its probe
     read the activity before the final carry fold, and 44 of its "tight"
     rows are loose on the folded value the shipped swap reads (D140).
   - **40 singleton rows** whose final activity misses its bound by about
     1e-16 of the row's own traffic (D136). They fall through to `BASIC`. No
     tolerance for them has been measured, and D8 means any such window needs
     a sweep on both sides.
   The shared alternative, demoting another basic column of the row resting
   on its own bound, is **refused** (D141): 66 of the 80 and 86 of the 152
   have no such column, so no within-row rule can close the residue. What
   remains is a wider-than-the-row design with a rank argument (refusals
   table), or accepting the residue — whose whole price is 48 solves losing
   their warm start, which item 4 below measures.
   The guard hardening D140 named is **landed**: the swap decides from the
   recovery value, not the rewritable status, with two asserts enforcing the
   contract. Bit-identical on all three sets and on the published counts
   (`bench/measurements/02-49/guard-count.txt`).
   `boeing1` is the control for a second mechanism: its stored count is right
   and its reduced count still is not. Presolve's *mapping* is exact (0
   identity failures of 88), so nothing there needs touching.
   **The published rules agree with all of the above (D137)**, in
   `docs/research/postsolve-basis-recovery.md`: Galabova 2023 states the
   counting rule ("at each step of postsolve where a new row is introduced, a
   variable must be identified as basic"), the status rule, and
   interior-implies-basic. Two things it adds. HiGHS **attempts** an assignment
   and falls back rather than deriving it. And postsolve is followed by
   re-optimisation, so **the bar is a valid starting basis, not the optimal
   one** — `num_row` members and nonsingular, without reproducing the duals.
   `build_warm_basis` is the only consumer here and it re-optimises anyway.
   **HiGHS disabled the zero-cost column singleton rule by default**, the
   family costing +5902 here. Not an argument to disable it — D95 and D106 are
   this project's own measurements of its value — but it belongs beside them.
   **A row's activity is not final until every record touching it has
   replayed AND the carry is folded** (`ps_row_add` accumulates, and the fold
   `sol_row[i] = ps_published(sol_row[i] + rowc[i])` runs after the whole
   replay), so anything reading it has to read it after both. **This has
   produced a wrong number four times in five probes.** It cost D135 a pass
   and would have killed that design — 0 valid swaps where there are
   thousands — then its corrected pass still read before the fold and called
   44 loose rows tight (D140); and it cost D136 two, the second of which
   would have claimed the published point violates complementary slackness.
   **And the solver itself does it**, in the `SINGLETON_ROW` case: that is
   the +1 combination.
2. **The termination defect now blocks the warm prize, and it has eight
   reproductions (D145).** The D144 count repair was built, reviewed,
   gated bit-identical on all 139 instances — and refused by its own
   judge: from the repaired (structurally valid) warm basis, **8 netlib
   solves publish `optimal` with a wrong objective** (`dfl001` 3.099e8
   against the true 1.127e7, `modszk1` 1135456 against 321, `cycle`,
   `d2q06c`, `degen2`, `greenbea`, `maros`, `woodw`), 2 more have their
   warm point checker-refused, while Kennington recovers cleanly
   (0.0572 → 0.0070, `osa-60` 7061 → 1 iterations). The mechanism is
   §5a's own: the termination never re-reads dual feasibility (D119), so
   a solve that starts badly can end wrongly. **The probe was run the same
   night and it reproduces at HEAD through the public API alone (D146,
   26 wrong optima in 80 trials)** — the defect is now this file's START
   HERE, above, and the warm retry queues as its step 3.
   `jm_model_remember_basis` checking the count was refused here — D142 —
   and D143's single-family un-swap by D144; both in the refusals table.

3. ~~Widen the solution digest to cover the basis.~~ **Done 2026-08-19
   (D150, `bench/measurements/02-59/`), on all three sets**: every optimal
   line carries `basis=`, `det` covers the published basis across the cold
   re-solve — a claim that now holds measured on all 110 optimal solves —
   and the instrument was validated against both cases it must reject
   before the campaign. The 48-solve netlib residue is pinned deliberately
   so its future repair moves the record visibly. The records were
   rewritten; the `*.baseline` files did not change.

4. ~~Re-measure the `warm` records.~~ **Done 2026-08-19 (D143), and the
   records are rewritten.** Kennington improved 0.0873 → 0.0572; netlib
   regressed 0.0696 → 0.2553 through the mapping, which is item 2 above.

The standing debt below names one postsolve family and a minimum case of one
status. **132 solves and a worst error of 12104** make that case a corner of
this rather than a description of it. This session refused two repairs in §5a
that looked correct in the source; measure before proposing.

### The unclamped dual step, for the record

`admit_candidate` clamps the ratio-test numerator, and `bfrt_walk`,
`jm_harris_pick` and `jm_bland_pick` read only that clamped number. Both exits
of `dual_ratio_test` then compute `*theta_out = s->d[best] / s->alpha[best]`
from the raw `d`, with `|alpha|` required only above `PIVOT_MIN = 1e-9`. At
HEAD this fires on 248 netlib picks and 170 Kennington picks, worst step
8.37e-09 (`bench/measurements/02-35/`).

D127 clamped it and **the gate refused**: `pilot87` 3.228x work, 108973
iterations against 40246, entirely from the Harris exit. The tiny wrong-signed
step perturbs the whole dual vector through `update_dual`, and that
perturbation is what keeps `pilot87` moving. A correctly-signed step small
enough to keep it would be a new constant with a sweep; nothing proposes one.

**Two candidates in a row here were refuted by measurement after looking
correct in the source.** This machinery reads as careless and is not. Do not
land a change to it on an argument from the code alone.

**`numerics-reviewer` did not deliver on D126, D127 or D128** — two instances,
four requests, no content, both stopped with `TaskStop`. Every review was done
in the main context and each entry says so. It is still the loop's step 3; if
it fails again, record that rather than skipping the read.

**`REFACTOR_EVERY` is not a proposal in any of this.** It is what D119 swept to
prove the failure was numerical, on one instance, and one instance is not a
population — `bench/measurements/02-28/` says so in as many words.

### What else is open, if §5a is dropped or finished

§5a is first because it is correctness. It is not the only open thing, and the
rest of this file is not all background:

| where | what |
|---|---|
| §4, end | how often `plato` should run — `pds` alone is 6.4 hours of wall clock. And `nug20`/`nug30` are unmeasured rather than unsolvable |
| §4, end | **`nug` has no row removed by any family**, all three instances. Nobody has asked why |
| §3 | doubleton equalities — 8.55% of netlib's live rows and 29.36% of Kennington's, and **99.7% of it is behind D97** |
| §5 | the rest of M2: factorization fill, Devex pricing, and closing the competitive gate |
| §6 | feature expansion, decided but not started |
| standing debts | about a dozen at the end of this file, each small and real. The two that were worth more than they looked are **both closed**: the `assert(want_lo <= want_hi)` clamp landed as D152 and all 94 instances run under asserts now, and the `warm` records were rewritten by D129/D143/D151 and are current |

D97 is the largest prize in the file and it is behind one precondition:
a dual postsolve for an imposed bound. Its first precondition was met by D114.
`docs/research/dual-postsolve-imposed-bound.md` is the design and nothing is
built.

Everything below this line is that detail, in order.

---

**The fourth set exists and has been run (§4, `bench/measurements/02-23/`).**
`fome` 4/4 and `pds` 8/8, both `gate: PASS`, every instance `shape=ok
checker=ok det=ok`. `nug08-3rd` solved; `nug20` and `nug30` are unmeasured, not
unsolvable. Nothing is in flight and `baseline-header` is merged.

**The number §4 was built to get.** The pds ladder is now twelve points over a
52.9x range in rows, four of which were already in the tree. Split it:

| | iteration exponent | work exponent |
|---|---|---|
| `pds-02` … `pds-20`, the range netlib and Kennington live in | **1.27** | **2.61** |
| `pds-20` … `pds-90`, above it | **2.08** | — |
| whole 52.9x range | 1.69 | **2.77** |

**Small models understate iteration growth by a factor of 1.6, and understate
work-unit growth by 6%.** So this repository's chosen unit of cost holds its
shape across a 53x change in model size and the iteration count does not —
CLAUDE.md's "work units are the unit of cost", measured rather than assumed.

**And the set is expensive.** `pds` alone is 23016 s of solve time, 6.4 hours of
wall clock at `J=4`, and `pds-100` costs 6.425e11 work units — twenty times the
whole netlib standard set. The three `netlib*` targets stay the gate. How often
`plato` should run is open and is not decided in 02-23.

The tree is clean and **§1 is finished**: the P0 comparison was re-taken
after D106 (3.15x HiGHS, **0.95x SoPlex — faster per solve for the first
time** — 2.57x Clp; worst instance now `stocfor3` at 30.0x, which is
presolve's, not the factorization's — its fill is 1.036), and all five
questions D106 opened closed in one day, four by measurement alone and one
with a source change: D107 (inequality rows are a tenth, refused), D109
(the window's floor declines nothing, refused), D108 (two overcost
mechanisms, no refuse rule), D110 (`maros-r7`'s factor collapsed 7.9x),
D111 (the postsolve recovery is compensated; `jaos-measurer` ACCEPT; 9
netlib digests moved where the rounding lived; netlib baseline rewritten
deliberately and confirmed). The reference build debt is repaired in
spirit by D111's test discipline but item four below still stands as
written.

**§2 closed too (D112), and `stocfor3` is counted (D113): the aggregator
alone owns its iteration half, so D97's reopen now guards three prizes.**

**D97's derivation is DONE (D114, `bench/measurements/02-21/`)**: the
over-tightening was never the implied bounds' values — a forcing window
scaled by the activity's magnitude (941.58 on a row whose bounds are 0)
certified 5.86 of real slack as binding and pinned the vertex `pilot`
cannot have. The shipped forcing family already windows by the row
bounds, which is why it is green.

**D97's second precondition is designed, 2026-08-17:
`docs/research/dual-postsolve-imposed-bound.md`.** Thirteen sections, the
scout run folded in as §11. Nothing built, nothing measured, no source file
touched, and no `DECISIONS.md` entry — a design is not a decision. What it
establishes:

- The transfer `y_i += d_j / a_ij` is legal in exact arithmetic in all four
  sign cases, because `x_j` at the imposed bound forces the implication's
  premises tight by feasibility alone.
- The cascade is acyclic and strictly decreasing in derivation time, so the
  arena's existing LIFO replay is the whole of the ordering it needs.
- **The arithmetic and the ownership test already ship**, in
  `JM_PS_SINGLETON_ROW` (`src/presolve.c:2035`, `1977-2021`). What is new is
  only the implying row's *other* columns, which a singleton row does not
  have. Smaller work than D97 implies.
- The basis, not the reduced cost, is where the risk is. §8c proves the
  postsolved point is still a vertex for one active imposed bound, with
  exactly one local basic/nonbasic swap. §8d finds where that breaks: **an
  equality row imposing bounds on two of its own columns**, which needs a
  refusal at the firing site. D112 measured 94% of the widening family's
  firings on equality rows, so the refusal is not free.
- `sol_redcost` must be recomputed from the duals, not patched — the copy at
  `src/presolve.c:2529` goes stale for every surviving column of the implying
  row. The literature recomputes (Cederberg & Boyd 2026, §2.1).
- **Two halves of it are in print after all** (§11a, §11b — `poppler-utils`
  installed in WSL 2026-08-17, which is what made the PDFs readable). Galabova
  2023 states the status rule and the record design, and names §8a's
  multiple-optima hazard. But the published state of the art *attempts* the
  basis assignment and falls back; **§8c's rank argument is stronger than
  anything found in print**, which is a claim that has to survive review before
  it is relied on. HiGHS also checks the KKT conditions after **each individual
  postsolve rule**, which JAOS does not do and should, under a diagnostic build.
- **The rule itself is published, and the scout's "folklore" headline is wrong
  (§11c).** Gould & Toint 2004, Math. Prog. 100, 95-132, section **6.2**, titled
  "Tightening a bound on the variables", is D97's second precondition with its
  own two numbered equations. (6.1) is `y_i += z_k/a_ik`, this design's §2
  unchanged; (6.2) is the implying row's other columns, which §9 had identified
  as the only genuinely new part. The scout missed it because it could not read
  a PDF. **One discrepancy to settle before copying it**: (6.2) is written as an
  assignment where §3's derivation gives an increment, and the two agree only
  where the other columns' reduced costs are already zero.
- **What is still nobody's**: the basis. Gould & Toint's solver is
  interior-point, so §8 cannot arise in their treatment either. §8c and §8d
  remain this repository's own.
- **A third design nobody here had considered** (§11b): publish the imposed
  bound deliberately loose so it can never be tight, and §2 through §9 have
  nothing to do. PaPILO ships it for previously-unbounded variables, crediting
  Fourer & Gay 1994. **Gould & Toint measured it over 160 problems (§11d) and
  the loosest mode wins**: 12% average gain against tightest's 11%, and 14%
  against 10% on linear problems specifically, with the fewest failures. Their
  own caveat is the whole caveat — an interior-point solver cannot see §8.
- **Directed rounding, a design D97 never considered (§11f).** Fourer & Gay
  1994 hit exactly D97's failure class — presolve declaring a solvable model
  infeasible — on **`maros`** (one of D97's four) and on `greenbea`,
  **`greenbeb`** (one of D108's three), `perold` and `woodw`. Their fix was not
  a tolerance. They computed the activity bounds with **IEEE directed
  rounding**, so the deduced bound is valid by construction and the judgement
  window is not needed for validity at all. D114 derived why JAOS's window
  failed; this says the window may not have to exist. Their measured cost is "a
  few percent" of presolve time and under 1% of the combined total. `fesetround`
  is C99 and deterministic, so D8 is not at risk — but that gets measured here,
  not assumed. Their same section reports a fused-multiply-add reviving one
  false infeasibility, and their fix is a compiler option forbidding it: JAOS's
  `-ffp-contract=off`, arrived at independently thirty years apart.
- **The slack question has a simplex answer too, and it is 1994's default
  (§11f).** AMPL keeps two bound sets and **passes the looser one by default**,
  because "if AMPL passes the strongest variable bounds it can deduce to a
  simplex-based solver, the solver often takes more iterations". And
  "degeneracy is much less of an issue for interior-point than for simplex
  algorithms", so the §11d effect is **larger** for JAOS, not smaller. Their own
  qualification is in the record too: simplex sometimes runs better with the
  tighter bounds, because it picks a different pivot order.
- No constants to inherit. Both windows get measured here from zero.

**Directed rounding was tried first and is REFUSED
(`bench/measurements/02-24/`, 2026-08-17).** Built in a worktree while the
`plato-pds` campaign held the main tree; nothing landed. Two designs, both
refuted, and the second one is the useful entry:

- **Widening both ends outward and dropping every window** dies on `make test`
  in under a minute. The FORCING reading detects an **equality**, not an
  inequality, and outward widening destroys an equality detection instead of
  making it safer. `x0 + x1 <= 0` with both columns in `[0, 10]` — the test
  suite's own `make_forcing_row_model` — has minimum activity exactly 0 against
  an upper bound of exactly 0, and one ulp declines it. That shape is why the
  family exists.
- **Outward only for INFEASIBLE and REDUNDANT**, which do prove inequalities,
  passes `make test` and `make sanitize` clean and then fails the gate:
  `pilotnov` goes 86587427 → 2378158900 work units, **27.5x** against a 2.0x
  bar. Mechanism named, not inferred: 32 rows survive instead of being dropped
  (101 → 69 removed, columns identical at 1811 both sides) and cost 60866
  iterations. The answer is bit-identical and the residuals are *better*, so it
  is a cost question, not a correctness one.

So the residual unsoundness stays and is now written down: the redundant test
can drop a row whose minimum activity is within 8 ulps of traffic below `rl`.
Bounded, never observed to produce a wrong answer here, and 27.5x to remove.
The reopen conditions are in `02-24`. **Fourer & Gay's `maros` fix was real and
JAOS is simply not in that position**, because D103 already replaced the
judgement constant with the error bound.

Next, therefore, is unchanged from before the detour: the deliberate-slack
design against §8d's refusal, measured here because both published directions
came from solvers that could not see the basis. Then the deliberate-slack design against §8d's refusal, measured here
because both published directions came from solvers that could not see the
basis. Alternatives if this is dropped: §4's fourth set, §5's Devex.

Three things this session left deliberately unmeasured, so nobody re-derives
them by accident: `greenbeb`'s 1.5126x, `maros-r7`'s 15.7x per-iteration drop,
and what a zero margin admits. Each has its own subsection with its numbers.

## 1. Implied free column singletons — what its own measurements left open

**The equality-row half landed 2026-08-15 (D106).** It removes 1041 rows,
2040 columns and 47043 nonzeros over 17 of the 94 standard instances, and
**980 rows from `maros-r7`**, the figure stated before the code existed.
Kennington is bit-identical. Work over the standard set is a geometric mean
of 0.9527x. `maros-r7` alone goes from 21010708013 work units to 328053926
and from 10479 iterations to 2576.

**Every question this section opened is closed**: §1a (D107), §1b (D109),
§1c (D111), §1d (D108), §1e (D110). The subsections below are the closed
record; nothing here is open work.

### 1a. Inequality rows — closed 2026-08-17 by D107: a tenth of the count, refused

The sign-respecting count is **341 rows of the 3315**, 10% and not two
thirds: 2980 of the as-loaded hits are equality rows, so the gap between
3315 and the shipped 1041 is margin (§1b owns 1353 of it) and presolve-time
interaction, not row sense. 304 of the 341 sit on the six `ship*` instances,
all below the comparison's 0.05 s floor; `stocfor3` carries zero; Kennington
carries zero. The sign condition declines nothing a feasible bounded model
can carry, and that is derived in the entry. Refused at this population; the
reopen condition is in the refusals table. Readings in
`bench/measurements/02-13/`.

### 1b. The margin's floor — closed 2026-08-17 by D109: it declines nothing, and the window ships as it is

Two measurements closed it in one day. `d2q06c`'s 2.2163x at margin zero is
the D108 trajectory class, with the extra iterations on degraded pricing —
no correctness or relaxation defect behind the number
(`bench/measurements/02-15/`). And a floor-less window at the shipping 8 is
a bit-identical no-op over all 94 standard instances, digests included: the
1353 rows between margin 8 and margin 0 are declined by any nonzero window,
because their bounds are zero or too small to absorb a margin of any scale
(`bench/measurements/02-16/`, with the instrument's self-proof and the
prediction stated before the run). The constant stays `ULPS = 8` with both
floors; the reopen condition is in the refusals table.

### 1c. The recovery's error — closed 2026-08-17 by D111: the accumulation is compensated

The 02-18 case (11.4x the margin's promise, predicted bit for bit) settled
the choice: compensate rather than degree-scale, because the arithmetic can
simply not make the error. Every replay accumulation goes through
`ps_row_add`, readers read sum plus carry, and the walkers fold once.
`numerics-reviewer` read the diff first and its findings carry dispositions
in D111; `jaos-measurer` returned ACCEPT from its own campaign: verdicts,
iterations and work units identical everywhere, 9 netlib digests moved
where the replay's rounding lived, infeas and Kennington 45 of 45
bit-identical. The pinned test fails on the unrepaired tree and is green
now. The netlib baseline was rewritten deliberately after the verdict and
confirmed by a following run.

### 1d. `greenbeb`, `scfxm3` and `forplan` — closed 2026-08-17 by D108: two mechanisms, no refuse rule

The record split and a calibrated callgrind attribution say the three do not
share a mechanism: `greenbeb` pays in iterations (1.3779x, per-iteration
1.03x in instructions — trajectory), `scfxm3` pays per iteration in the
ratio-test path (`update_dual` 1.71x, `admit_candidate` 1.57x, LU side
1.05–1.11x, iterations 1.054x), `forplan` is small and diffuse. Both are
downstream of an exact substitution, nothing at the reduction site separates
them from the 14 instances the family made cheaper, and a refuse rule on
predicted trajectory is refused — the reopen condition is in the refusals
table. §2's relaxing family and its candidate rule are untouched. Readings
in `bench/measurements/02-14/`.

### 1e. `maros-r7`'s cheaper iteration — closed 2026-08-17 by D110: the fill collapsed

Measured with a per-refactorization fill print on both trees, calibrated
until the pre side reproduced D46's committed 4.801x on its own: L falls
28.5x (90523 to 3172 nonzeros), the whole factor 7.9x, the fill ratio 4.801
to 1.457, on a model that shrank 31%. The refactorization cadence is
unchanged. That is where the 15.7x per-iteration drop lives, and the fact
now sits in §5's factorization item. Readings in
`bench/measurements/02-17/`.

## 2. Presolve makes `grow22` and `grow7` far worse — closed by D112 (opened by D103)

Not a regression from D103 — the records either side of it are bit-identical —
so this arrived with presolve and has never been asked about.

```
grow22   4.4e7 -> 4.9e8 work   11.16x    2179 -> 16381 iterations   7.51x
grow7    6.4e6 -> 5.5e7 work    8.56x     544 ->  4804 iterations   8.82x
```

They are the only two instances of 94 past 2x, and they set the standard set's
worst case while its geometric mean is 0.810x.

**Measured, in `bench/measurements/02-11/`.** One family fires on them and
nothing else: `JM_PS_SINGLETON_COL`, the cost-0 bounded singleton column
(D95), exactly 20 times on each of `grow7`, `grow15` and `grow22`. So it is
the whole of the difference, and `-DJAOS_NO_PRESOLVE` already measured the
other side.

What the 20 firings do is not visible in the `presolve=` field, which reports
20 columns and 20 nonzeros of 8252 and no rows at all. Every one of the 20
rows is an **equality `== 0`** and every one becomes a **range of up to
5e5**. Twenty exact pins become twenty things that constrain nothing in
practice, and a dual simplex steers by those pins. None of the 60 records
leaves a row unconstrained on both sides, so a check for "did this row become
free" would miss it: the damage is the width.

**Closed 2026-08-17 by D112, on the candidate rule's own counter
(`bench/measurements/02-19/`).** The widening distribution over the
standard set: 8617 firings on 60 instances, 94% on equality rows, 98.6%
past the row's own scale — the "unbounded relative widening" the rule would
refuse is the family's normal act, so the rule is the family's off switch.
And the discriminator does not discriminate: `grow7`, `grow15` and
`grow22` carry the same maximum widening, 5.524e5, and one is helped while
two are hurt. Refused; the reopen condition (a mechanism that predicts
trajectory direction from the firing site) is shared with D108 and is in
the refusals table. `grow22` and `grow7` stand as the set's worst cases
against the reference build, below the gate's own bar.

## 3. Doubleton equalities: a family nobody has counted (opened by D104)

An `==` row with exactly two entries. One variable is substituted out, the row
goes, and every nonzero the substituted column had goes with it. It is not one
of the five live families and not one of the three D101 deferred, so no
measurement in this repository has ever counted it. On the model as loaded:

| set | doubleton equality rows | of all rows | instances carrying one |
|---|---|---|---|
| netlib | 6504 | **7.53%** | 67 of 94 |
| Kennington | 72459 | **28.15%** | 12 of 16 |

`ken-18` alone carries 48276, and it is the slowest instance in that set.
Against D101's 0.15% for the three deferred families this is fifty to a
hundred and ninety times larger, which makes it a different proposition rather
than a fourth deferral.

**Measured at presolve's exit, and that is where it stops being simple.**
The five live families barely touch these rows — 6504 to 6153 on netlib,
72459 to 60382 on Kennington — so the population survives to where a sixth
family would run. But a doubleton is substituted by eliminating one endpoint,
and unless that endpoint is free its bounds must be transferred onto the
survivor, which is bound tightening.

| set | surviving | share of live rows | with a free endpoint |
|---|---|---|---|
| netlib | 6153 | 8.55% | **19** |
| Kennington | 60382 | 29.36% | **0** |

**99.7% of the family is behind D97**, which refused bound tightening in six
designs, every one returning INFEASIBLE on a model that has an optimum. What
is buildable today is 19 rows across six netlib instances.

So this is not "build doubletons next". It is that **the prize behind D97 is
much larger than D97 knew**: it weighed bound tightening on its own, and this
is a second family that cannot exist without it. D97's reopen condition
already says what would settle it — derive the over-tightening on
`pilot`/`pilot87`/`agg`/`maros`, and have a dual postsolve for an imposed
bound. That work now buys two families instead of one.

**And a third, larger still (D113, 2026-08-17):** `stocfor3`'s rule
ablation says HiGHS's aggregator — equality substitution at any degree,
which needs the same bound transfer — alone owns the iteration half of the
worst instance in the comparison (6404 vs 14788 iterations with it off,
against JAOS's 18431). D97's condition is unchanged; its prize now
includes `stocfor3` whole.

The remaining cost question is unchanged and still unmeasured: a substitution
adds the eliminated column's terms into the survivor's rows, so it creates
fill. `subnz` says a non-interacting pass would remove 2.65% of netlib's
nonzeros and 5.13% of Kennington's, against row shares three times larger,
and neither figure accounts for fill.

## 4. A fourth instance set — the conclusions are population-dependent and say so

Every verdict in this repository is taken on 139 models: 94 netlib standard,
16 Kennington, 29 netlib infeasible. Netlib is a 1980s collection and it is
small. Three entries already in the record say the population is doing more
of the deciding than it should:

- **D46**: two instances are **74%** of the standard set's total work. A set
  total is a statement about `maros-r7` and `dfl001`.
- **D101** deferred three families because they remove 0.15% *on these 139
  models*, and its reopen condition is written as a model population, not as
  an opinion. A fourth set is the executable form of that condition.
- **§1's own counter** reads 3315 rows on netlib and **0 on Kennington**. The
  two sets already disagree about what is worth building.
- **HiGHS says it in print, with its own numbers** (Galabova 2023, §3.7, read
  2026-08-17): "Most problems in the classic Netlib test set are too small to
  be of interest", and presolve's geometric-mean speed-up is **1.10 on netlib
  against 1.67** on a 74-problem set built from Mittelmann's benchmarks plus
  four industrial models. Same code, same measure, the population alone moves
  the verdict by 52%. That is the strongest argument in this section and it
  did not come from here.

**What blocks it, and how Kennington already solved it.** netlib has
published exact rational optima (Koch), which is what lets the gate say
`objective=ok`. A modern set has no such reference. Kennington answers that
by entering at a lower tier — `bench/README.md` calls it "the same, for
correctness only" — and a fourth set enters the same way: checker verdict,
solution digest, determinism and work units, no reference objective.

**The blocker expired and nobody noticed.** This section said "Not before §1
closes". §1 closed 2026-08-17 (D107, D108, D109, D110, D111) and §2 closed
with D112. Four refusals in the table below name a model population as their
reopen condition, and **three of them already have their script written**:
D101 (`bench/measurements/02-07/`), D107
(`02-13/run-sign-count.sh`), D109 (`02-16/run-floorless.sh`). D107's entry even
names this section as the standing candidate. That is D24's pattern for the
fourth time.

### Sizes read 2026-08-17, before fetching anything

| where JAOS is now | rows | cols |
|---|---|---|
| `stocfor3`, biggest of netlib standard | 16675 | 15695 |
| `ken-18`, biggest of Kennington | 105127 | 154699 |
| `pds-20`, largest pds JAOS carries | 33874 | 105728 |

**Mittelmann's LPopt is the right set to aim at and the wrong one to adopt
today** (`plato.asu.edu/ftp/lpopt.html`, 1 Jul 2026, read at HEAD). Its
smallest instance, `qap15`, is 6331 × 22275 with 110700 nonzeros — already
3x `dfl001`'s nonzero count. The median is around 1.5M nonzeros and `dlr2` is
7.1M rows × 38.9M cols × 78M nonzeros. On that set HiGHS solves 54 with a
shifted geometric-mean runtime of 494 s, and **SoPlex solves 31**. JAOS reads
0.95x SoPlex and 3.15x HiGHS per solve at P0, so it would time out on most of
it. Sixteen of the instances are undisclosed in any case.

**The step that is actually available**, from the same host, all
`emps`-packed the way Kennington already is:

| family | instances | compressed | note |
|---|---|---|---|
| `pds/` | pds-30 … pds-100 (9) | 848K–4.4M | **the same family JAOS already carries** at pds-02…pds-20; `pds-100` is 156244 × 505360 |
| `fome/` | fome11, 12, 13, 21 | 310K–1.6M | `fome13` is 48569 × 97840 |
| `nug/` | nug08-3rd, nug20, nug30 | 325K–3.3M | QAP lower bounds |
| `rail/` | rail507, 516, 582, 2586, 4284 | 285K–8.0M | set covering; `rail4284` is 4284 × 1092610 |
| `fctp/` | 30 instances | 2.2K–111K | **too small — netlib's own problem again** |
| `network/` | 10 instances | 3.4M–164M | too big for now |

So the first fourth set is `pds-30…pds-100` plus `fome` plus `nug`, which
walks the size up by a factor along a family already in the tree instead of
leaping. Twenty-odd instances.

**Licence, settled.** No licence statement exists on
`plato.asu.edu/ftp/lptestset/`; its `00README` gives origins and citations
only. The position is the one this repository already takes for netlib and
Kennington, unchanged: fetch at build time, pin by sha256, **never
redistribute**. The three instance directories are in `.gitignore` beside the
other three.

**The fetch path exists and is proved, 2026-08-17.** `bench/fetch.sh` gained a
`bz2-emps` mode (plato serves netlib's own emps packing, bzip2'd instead of
gzip'd — three lines, not a second pipeline). Run against
`bench/plato-fome.manifest`: `verified 4, already present 0, failed 0`, exit 0,
and the cached re-run reads `already present 4`. The three manifests are
pinned and their dimensions cross-check against Mittelmann's published size
table, agreeing on every column count with the same one-row objective offset
the Kennington manifest already documents — except the three `nug` instances,
which show no offset and are recorded unexplained.

| manifest | instances | largest |
|---|---|---|
| `bench/plato-pds.manifest` | 8 | `pds-100`, 156243 × 505360, 54.5 MB expanded |
| `bench/plato-fome.manifest` | 4 | `fome21`, 67748 × 211456 |
| `bench/plato-nug.manifest` | 3 | `nug30`, 52260 × 379350, 58.6 MB expanded |

`fome11 → fome12 → fome13` doubles exactly in both dimensions, which is the
one family here that can say whether a cost grows linearly or worse with
nothing else about the model changing. `nug` is the shape the tree does not
have: every model JAOS reads today is economic, transport or stochastic, and a
QAP relaxation is none of those.

**The runner can read the set, 2026-08-17.** `bench/run.c` gained a third
expectation, `EXPECT_OPTIMAL_NOREF` (`-e noref`), and six `make plato-*`
targets exist. Shape, checker verdict, determinism, digest and work units are
asked exactly as they are for the other sets; only the comparison against a
published optimum is gone, and it prints `objective=none` rather than `ok` so
the two cannot be read as the same thing.

Built with the case it has to reject, in **both** directions
(`bench/measurements/02-22/reject-case.sh`, all four as expected): a `none`
manifest under `-e optimal` exits 2, and — the direction that matters, because
it is the silent one — `netlib.manifest` under `-e noref` also exits 2 rather
than quietly ceasing to check Koch's optima. `make test` and `make sanitize`
both exit 0.

**`plato` is not part of the gate**, and the three `netlib*` targets still are.

**The set is measured (D115).** Every figure is in
`bench/measurements/02-23/`, and `ladder.py` there derives them from the
manifests and the baselines rather than restating them.

### 4a. The first thing it found — closed 2026-08-18 by D117: another family takes them first

`bench/measurements/02-25/` asked why D106 fires on none of `fome`'s 166 / 332
/ 664 candidates and ruled out the margin with a canary that moves.
`bench/measurements/02-26/` answered it with a decline reader compiled into a
copy of `src/presolve.c`: **`JM_PS_SINGLETON_COL` takes 100% of them in round
0**, from a branch above D106's in the same column pass. The frozen row, this
file's leading suspect, declines 2 candidates over the whole standard set.

Three calibrations passed before any new number was read — `maros-r7` at 984
candidates and 980 firings, netlib at 3321 and 02-12's 8639 rows removed, and
zero candidates left disagreeing with the code. The 1353 the margin declines
reproduces D109's own figure, which nobody asked it to.

That closes §4a and opens §4b, which is the header of this file.

### 4b. Should D106 be preferred over D95 — closed 2026-08-18 by D118: refused

Built and measured, `bench/measurements/02-27/`. One branch moved, nothing
else. The footprint was exactly what D117's read-only counter predicted, nine
instances for nine: `ganges` 12, `czprob` 11, `dfl001` 9, `pilotnov` 7,
`pilot-ja` 7, `perold` 6, `seba` 1, `scrs8` 1, `d2q06c` 1.

It buys `ganges` 0.8429x, `dfl001` 0.8951x and `czprob` 0.9227x, and it makes
`pilotnov` publish an objective 29% wrong as `optimal`. Refused on that alone.
Geometric mean 1.0358x over 94; `netlib-infeas` and `netlib-kennington` both
`0 regressed, 0 improved, 0 new`.

The reopen condition is a fifth restriction on D106 that declines `pilotnov`'s
seven, which is §4c above.

### 4c. Why D106 answers `pilotnov` wrong — closed 2026-08-18 by D119: it does not

Opened by D118 and answered the same day, `bench/measurements/02-28/`. The
wrong answer is **numerical, not structural**. The same reduced model reaches
Koch's published optimum to the last bit at `REFACTOR_EVERY = 16`, and costs
1.032x HEAD at 8 against 30.2x at the shipping 64. D106's substitution on those
seven columns is sound and presolve cut off nothing.

What the candidate found instead is in §5a, at the top of this file: the
termination test never re-reads dual feasibility, so a numerically damaged
solve publishes `optimal`. That is HEAD's, not the candidate's.

**Also open:** how often `plato` should run — `pds` alone is 6.4 hours of wall
clock — and `nug20`/`nug30`, which are unmeasured rather than unsolvable.
`nug` also turns out to have **no row removed by any family** on all three
instances (`bench/measurements/02-26/counts/nug.txt`), which is its own
question and is not asked anywhere yet.

## 5. After presolve — the rest of M2, in order

### 5a. 186 loans go missing, and nothing bounds one — OPEN

Opened by D119, narrowed by D120, located by D121 and half-repaired by D122,
all on 2026-08-18 (`bench/measurements/02-29/` and `02-30/`). The repaired half
and the two that are left are in the header of this file. It sits first in this
section because it is a correctness question and everything below it is a speed
one.

Five explanations are closed by measurement and should not be re-derived: the
solver not re-reading dual feasibility (it does, six re-entry rounds, its own
violation zero), the basics' assumed-zero reduced costs (recomputed, worst
3.82e-14), a column resting on a bound phase 1 lent (72 lent, 0 resting), the
reduced model differing between refactorization intervals (identical, family by
family), and the carried `x_B` (drifts 7.22e-10, four orders too small).


- **Factorization** (REQ-lu-fill-and-markowitz, REQ-hyper-sparse-downstream):
  the stale live counts Markowitz chooses on, and the fill — factors carry
  2.673x the basis nonzeros set-wide (D46, which predates D106; `maros-r7`
  read 4.801x before D106 and 1.457x after it, D110). Measured at HEAD with
  D110's instrument: `stocfor3` reads **1.036** — no fill at all, so its
  30.0x against HiGHS belongs entirely to the presolve item below — and the
  live fill cases are `pilot87` at 3.610 and `pilot` at 3.261, the same two
  instances the fewer-iterations half of the split names
  (`bench/measurements/02-17/`); keep sparse triangular
  results sparse downstream (`stocfor3`: 6.79x per iteration, solves 43%,
  memset/memcpy/malloc 18.8% against 11.3% on `dfl001`). Left-looking
  elimination is a rewrite and needs its own decision first. Struck off by
  measurement, do not re-cost: `compact_pivot_row`'s row-to-position lookup
  (<0.5% on `maros-r7`); per-column arrays vs one arena (0.73% + 0.30%; the
  locality argument needs a cache simulation before it is costed or dropped).
- **Search path** (REQ-devex-pricing — acceptance stated: full gate with
  iteration count and per-iteration cost reported separately;
  REQ-reentry-oscillation — investigative first: 0.24% on `pilot87` at
  interval 24, D51 names the mechanism, D74 closed the only proposed cure,
  D89 removed the consequence).
- **Close M2** (REQ-m2-competitive-gate): needs a controlled host — D17 says
  a WSL number cannot close a gate, and this machine is Windows/WSL with a
  measured repeatability of 6.27% (D93). The per-instance guard factor is
  unset and is measured, not guessed. The ladder is recalibrated and the
  question is closed: **P0** is the rung, presolve on both sides, and T0 keeps
  its definition and its record as a historical rung
  (`bench/compare/README.md`). Standing at P0, re-taken 2026-08-17 after
  D106: **3.15x HiGHS, 0.95x SoPlex, 2.57x Clp**, on 2.04x / 1.51x / 1.95x
  the cost of an iteration. The per-iteration figure is what M2 is aimed at:
  three independently written dual simplexes put JAOS's iteration between
  1.5x and 2.0x theirs.

- **`stocfor3`'s presolve gap is counted, and it is the aggregator (D113,
  `bench/measurements/02-20/`).** Rule ablation on HiGHS at the P0 options:
  turning off its Aggregator collapses the 8416-row reduction to 2859 and
  lifts it from 6404 to 14788 iterations (2.31x) — against JAOS's 18431,
  near parity. Eleven of twelve other rules change nothing; the doubleton
  rule is subsumed by the aggregator. So the iteration half of `stocfor3`'s
  30.0x is equality substitution at any degree, and that machinery is
  behind D97 (§3), which now carries three prizes. The `maros-r7` half of
  this item closed with D106 (1.33x HiGHS).

## 6. After M2 — feature expansion (decided 2026-08-13)

Two decisions are locked: the two premises are absolute (no external code,
bit-identical everywhere; a feature that cannot be built under them is not
built), and the goal is the best open solver that is deterministic across
machines and ships its own checker — not matching Gurobi.
`docs/feature-matrix.md` is the scoreboard; read it at every close. Whether
M2 finishes as scoped is answered when presolve closes.

Proposed order: cheap breadth first (write MPS, write LP, write a solution
file, Python bindings, sensitivity and ranging, infeasibility certificates),
then primal simplex, then barrier with crossover, then MILP, then
QP/conic/NLP/MINLP. VIPR-format certificates are a cheap differentiator —
only SCIP 10.0 emits them and JAOS already ships a checker. For exact
rational verification, GMP is excluded (D11); the methods to weigh are
iterative refinement, interval arithmetic in double, or hand-rolled
rationals for the final basis only.

## 7. Presolve is closed — what that means

**REQ-presolve is done.** Six families live, three deferred on a count with
an executable reopen condition (D101), the postsolve defect closed in both
halves (D99, D100), an infeasible model no longer published OPTIMAL (D102),
the sense and window defects repaired (D103), and a removed column now paying
every row it touches (D106). `jaos-measurer` returned **ACCEPT** on D103 from
its own binaries; D106 was judged on its own campaign and its own sweep, with
`numerics-reviewer` on the diff. The three baselines have been rewritten
deliberately after each and confirmed by a following gate run: all three read
`0 regressed, 0 improved, 0 new` and exit 0.

Nothing in the sections above is presolve being unfinished. Every one is a
question presolve's own measurements raised, and every one has its number
already.

## Refusals and deferrals — what would reopen each

A refusal is a measurement, and a measurement is valid while its premises
hold. D24's reason expired when presolve landed, and it was caught by an
accident rather than a checklist (D94). This table is the checklist: when a
change satisfies a condition in the right column, re-ask that question. Until
then, do not — a refusal whose premise has not changed just fails again.

| decision | what was refused or deferred | reopens when |
|---|---|---|
| D149 | the blanket warm count repair, retried behind the certificate guard — correct now (`disagreed=0, rejected=0`) and refused on cost: `dfl001` at 172x work for a doomed 596-short repair, netlib geomean 0.2605 vs 0.2553 | **condition MET by D151**: the cap was swept on both sides and the capped repair landed at 4. This row stays as the record of the refusal and its expiry |
| D151 | the two instances that still lose real work behind the cap — `scsd1` 4.65x and `degen2` 4.09x, both with warm iterations exactly equal to cold, which is D148's guard rejecting the repaired trajectory and charging the attempt plus the whole cold solve | a rule that predicts a doomed trajectory before paying for it. **The shortfall cannot be that rule and this is measured**: both are short by 1, the same shortfall as the sixteen instances that win. Raising the cap is separately refused — the sweep in `bench/measurements/02-60/` reads 15.48x on `greenbea` at 7 |
| D145 | the warm count repair in `build_warm_basis` — refused because 8 netlib solves published a wrong objective through the termination hole | **condition MET by D148** (the certificate guard landed), retried as D149 and refused again on cost. This row stays as the record of the refusal and its expiry |
| D142 | a count guard in `jm_model_remember_basis` — the premise "build_warm_basis already rejects it" is false: it counts the MAPPED basis, and clearing the stored publish costs `capri` and `fffff800` their warm starts (1→273 and 7→945 iterations) for nothing any consumer reads | a consumer of `start_*` appears that reads the orig-space count as a claim, or warm starting stops going through presolve's mapping. The candidate and its validated test are at `bench/measurements/02-51/remember-guard-candidate.diff` |
| D141 | a within-row demotion for the published-basis residue — 152 of the 232 declines have no basic column of the row at a bound, and the snap for the 80 breaks the row-bound exactness 02-49 measured (74 of 80 exact) | a demotion design whose candidate set is wider than the firing row AND that carries a rank argument for the demoted member; the fallback in the published shape (Galabova 2023) is accepting the residue |
| D101 | duplicate rows, duplicate columns, dominated columns — 0.15% left to remove on these 139 models | a model population where `bench/measurements/02-07/`'s counter reports a non-trivial share. The condition is executable, not a matter of opinion. Three pieces of the work have no published source and would have to be derived with their own tests |
| D97 | bound tightening — INFEASIBLE on models with an optimum, six designs | **first precondition met 2026-08-17 (D114)**: the over-tightening is derived — a forcing window scaled by the activity certified 5.86 of slack as zero, and the design requirements for a retry are in `bench/measurements/02-21/`. What remains: a dual postsolve for an imposed bound; then only under a campaign. **The condition is unchanged and the prize is not**: doubleton substitution needs the same machinery, and it is 8.55% of netlib's live rows and 29.36% of Kennington's, of which 19 rows in total can be built without it (§3). D97 weighed this feature alone; it now unlocks two |
| SPECS §3 | crash basis — destroys the exact slack-basis steepest-edge weights | pricing stops starting from exact steepest-edge weights; REQ-devex-pricing landing is the trigger |
| D74 | removing the re-entry loan — 2.372x `pilot87` iterations for 0.980x `pilot` | the oscillation mechanism itself changes (phase 4's investigation) |
| D63 | restarting weights to exact instead of 1.0 | the pricing rule changes; Devex would replace the question |
| D107 | the inequality implied free column singleton — 341 sign-ok rows, 10% of the count, 304 of them on `ship*` instances below the harness floor, zero on `stocfor3` and Kennington | a model population where `bench/measurements/02-13/run-sign-count.sh` reports a non-trivial sign-ok share. **Asked of §4's fourth set 2026-08-18 and NOT satisfied** (`bench/measurements/02-25/`): zero inequality candidates across all fifteen instances, on 02-13's own instrument with both its calibrations reproduced. The refusal now stands on 154 models across four sets and a 53x range in rows, so the population is no longer the objection to it |
| D108 | a refuse rule for the implied free column singleton on trajectory grounds — `greenbeb` and `scfxm3` pay through different machinery, both downstream of an exact reduction, and no site-local predictor exists | an instance crosses the gate's 2.0x work bar from this family's firings, or a measured mechanism predicts trajectory direction from the reduction site |
| D109 | removing the implied-free window's `max(1, scale)` floor — a bit-identical no-op over all 94 standard instances, digests included | a model population where `bench/measurements/02-16/run-floorless.sh` reports a moved instance line; or the D106 sweep's own reopen |
| D112 | the unbounded-relative-widening refusal for the cost-0 singleton column — 98.6% of firings would be refused, and the helped and hurt `grow*` instances carry the same widening | D108's condition: a measured mechanism that predicts trajectory direction from the firing site; or an instance crossing the gate's 2.0x work bar from this family |
| D95 | eliminating nonzero-cost singleton columns | a dual-informed elimination design exists (the lift condition is in the entry). **Checked against D106 and NOT reopened, deliberately.** D106 eliminates nonzero-cost singleton columns, so the question was re-asked. It does not satisfy D95's condition and does not need to: D95 refused *choosing which bound is optimal*, and an implied free column has no bound to choose — it is interior, so `d_j = 0` is forced and the dual falls out of one division. The columns D95 still refuses are the ones whose own bounds can bind, and D106 declines exactly those |
| D118 | giving the implied free column singleton first refusal over D95's bounded cost-0 singleton column — `pilotnov` publishes an objective 29% wrong as `optimal`, `checker=REJECTED`, `dual=0.89`, 30.2x work | **the condition was rewritten the same day by D119, because the first one looked in the wrong place.** It is not a fifth restriction on D106: the substitution is sound, and the same reduced model reaches Koch's optimum to the last bit at `REFACTOR_EVERY = 16`. It reopens when the solve stops publishing `optimal` without re-reading dual feasibility, or when the refactorization interval stops collapsing on `pilotnov` — §5a, both. The prize is real and stated: `ganges` 0.8429x, `dfl001` 0.8951x, `czprob` 0.9227x |
| D127 | clamping the dual step to the same number the pick was made on — `pilot87` 3.228x work and 108973 iterations against 40246, entirely from the Harris exit | a correctly-signed step that keeps the perturbation `update_dual` spreads, which is a new constant and needs its own sweep; or `pilot87` stops oscillating. The candidate is at `bench/measurements/02-36/candidate.diff` |
| D126 | removing `shift_to_feasible`'s `d[v] = 0.0` when the cost cannot move — the breach then compounds across iterations, worst dual step 8.37e-09 → 2.21e-03 and 49% more ratio-test picks | the dual step stops being computed from an unclamped `d` (§5a item 2), or `update_dual` stops being the thing that pushes the same variable further every iteration. The candidate is kept at `bench/measurements/02-35/candidate.diff`, so a retry starts from the measured version rather than from scratch |
| D93 | the 4.2% time bar — unmeasurable on this host | a controlled host that satisfies D17 |
| D92/backlog | `pilot87`'s suboptimality bound, not understood | it blocks a gate (trigger already recorded) |
| D82, D84 | partial and multiple pricing | nothing scheduled — refused on wrong answers, not on a trade; a new scheme is a new decision, not a retry |
| D34, D11, D2 | `long double`, GMP, any external code | never, while the two absolute premises stand (locked 2026-08-13) |

## `pilotnov` was NOT a defect — opened and withdrawn the same day (D153)

The row-activity check reported `pilotnov` publishing a row activity its own
columns do not make, worst 1.93e-07 on an equality row at zero with three
nonzeros. The arithmetic in that sentence was right and the conclusion was
wrong.

**Why it is not a defect, written here so it is not re-opened from the same
evidence.** All 18 of `pilotnov`'s disagreeing rows have a **nonbasic**
logical and none is basic. A nonbasic logical means the basis asserts the
constraint is tight, so the published activity is that tight value; the
column sum is a different quantity, carrying the basis solve's primal
residual, which is bounded by the basis conditioning and by nothing available
at that site. The row trace (`bench/measurements/02-63/`) confirms it from the
other side: only one producer writes row 931, the copy from the reduced
solve — so the two assignments the debt suspected are exonerated on the very
row that looked worst.

The check skips rows whose logical rests on a bound and **passes on all 139
instances**. It is an invariant of every debug build now.
`bench/measurements/02-62/` carries the four wrong versions of it and what
each falsely reported; anyone tempted to widen or narrow it should read that
table first.

## Standing debts — small, real, none blocks the sections above

- ~~`preflight.sh` does not check committed records for `baseline: NOT
  COMPARED`.~~ **Done 2026-08-18.** It reads the line D93 said nobody read:
  STOP when a committed record carries it, WARN when only the working-tree
  copy does. Validated against the case it must reject, in a worktree — clean
  tree clear, working-tree copy WARN and exit 0, committed copy STOP and exit
  1.
- The `REFACTOR_EVERY` 16..256 trajectory sweep is manual; three of M1's four
  defect closures came from it and no target automates it. **D119 is the
  fourth** — `pilotnov` under D118's candidate is right at 16 and 29% wrong at
  the shipping 64, on the same reduced model
  (`bench/measurements/02-28/sweep-refactor.txt`).
- Test ceilings drift silently — the `<62000` one drifted 2800 units with
  nothing watching. Re-measure a ceiling's both sides when touching its
  subject.
- `pilot87`'s suboptimality bound is not understood (`gap_positive` moves
  0.0068–26.7 across D92's variants while every answer is inside tolerance).
  Deferred with a trigger: it re-enters the plan if it blocks a gate, and it
  already refused two of D92's three candidate repairs.
- Restricting the candidate set ahead of `bfrt_walk`/`jm_harris_pick` is open
  and not refused (D93); it puts Harris's guarantees at stake and needs its
  own decision before any code.
- `galenet` makes two `dual_ratio_test` calls in a one-iteration solve —
  calls are not iterations in any work-saved arithmetic (D93).
- ~~**The `warm` record predates presolve.**~~ **Rewritten 2026-08-18 (D129),
  and what it now says is a finding rather than a refresh.** The work ratio
  went from 0.0164 to **0.0696** on netlib and 0.0041 to **0.0873** on
  Kennington, and 26 netlib instances plus 6 Kennington ones read **exactly
  1.0000** — a warm re-solve doing bit-identical work to the cold one. That is
  the basis-count defect below, and the attribution is exact rather than
  inferred: 23 count mismatches against 23 non-trivial 1.0000 instances on
  netlib, 6 against 6 on Kennington — see D130, which corrects D129's split of
  those 23 into short and over. `scrs8`'s old "regression" was a
  different branch, `x8<=0` becoming `x14<=0` because the anchor optimum moved
  in its last digits, and that branch is infeasible.
  **The staleness is watched now, and it was not.** `preflight.sh` asks every
  record in `bench/results/` how many `src/` commits it was written before. It
  only ever asked the three netlib ones, which is why 21 commits passed —
  asking all of them turned up two more straight away. The count is not a
  verdict: a record written before N commits is still valid if those commits
  were no-ops on that set, which is why the baselines being behind is correct
  rather than stale. Found by `jaos-measurer` while judging D122.
- ~~**A collapsed fold leaves a bound no record owns.**~~ **CLOSED 2026-08-20
  (D158, `bench/measurements/02-68/`).** When a singleton row's intersection
  collapsed inside the fold's rounding window, `src/presolve.c` put the
  midpoint of the two ends into both folded bounds, and that midpoint was no
  row's implied bound, so when the reduced cost's sign pointed at the other
  side no record paid and the cost was left on a column strictly inside its
  own box. The midpoint is clamped into the column's box now, which puts the
  value back ON the earlier fold's bound and restores that record's ownership:
  `max_dual_violation` 1 → 0. Found by `numerics-reviewer` 2026-08-14, closed
  by the same repair as the containment half.
  **The model this debt recorded is stale and was replaced.** It used
  `x0 <= 5 - 1e-13`, and the fold's window at scale 5 is
  `8 * DBL_EPSILON * 5 = 8.88e-15` — eleven times narrower, so both builds
  refuse that model outright and it reaches no collapse at all. `1e-13` → a
  gap inside the window, `4e-15`, is what reproduces. A reproducing model that
  has stopped reproducing is worse than none.
  ~~**D103 gave this a stated size, and it is not bounded.**~~ **The VALUE
  half is CLOSED (D158, `bench/measurements/02-68/`).** The midpoint was
  unclamped, so the published value could sit up to half the window outside a
  bound the caller stated: `4 * DBL_EPSILON * row_traffic[i] / |a|`, with the
  traffic term uncapped and a shape reading 0.89. It is clamped into the
  column's own box now, the branch runs 0 times in 100018 folds over the three
  sets, and `test_a_fold_onto_the_box_at_scale_still_collapses` asserts
  containment where it used to document the overshoot.
  **The dual half above is what remains**, and D158 is neutral on it: that
  model's midpoint is well inside its column's box, so no clamp reaches it.
- ~~**The other half of `assert(want_lo <= want_hi)`: an empty intersection of
  an ulp.**~~ **Closed 2026-08-19 (D152, `bench/measurements/02-61/`).** The
  clamp landed, all 94 standard instances run under `-UNDEBUG` where eleven
  aborted, and the gate moves on exactly `bnl1` and `finnis` with the other
  137 of 139 bit-identical.
  **One number in this debt was wrong and the probe corrected it.** Eleven
  instances tripped the assert, which is right; but of the 138 empty
  intersections behind them, only **10 records on 2 instances** ever published
  outside the column's box. On the other 128 `want_lo` equals `rec->lo` with
  the box open above it, so the intersection was empty while the published
  value was inside all along. The worked example was one of the real ten:
  `bnl1` row 581 wanted 2.1850000000000005 from a column whose upper bound is
  2.1850000000000001.
  **The emptiness assert is removed, not widened, and two windows are refused
  on the record** so nobody builds a third: eps times the division's own
  inputs collapses onto the residue it exists to bound (the surviving rows are
  equalities at zero whose activity cancelled), and eps times the row's
  accumulated traffic reads **exactly zero on 86 of the 138** because
  `sol_row[i]` is copied from the reduced solve rather than accumulated — the
  residue is the simplex's and its error budget is not in the replay at all.
  **What stays open is what was always separate**: presolve does not detect a
  genuinely infeasible frozen row — the `x0 + x1 = 100` case whose gap is 93
  rather than 1e-14. It belongs where the row is frozen. This site cannot tell
  the two apart without an error budget it has no access to, and the old
  assert only appeared to because it was never enabled.
- ~~**`make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` is RED.**~~ **Repaired
  2026-08-18 and green for the first time.** The two tests were
  `test_singleton_col_between_two_removals_solved_path` (expected 3 basic, got
  2) and `test_a_fold_onto_the_box_at_scale_still_collapses` (expected
  OPTIMAL, got INFEASIBLE). Both now assert the reference build's **own**
  answer instead of being guarded out, because in both cases that answer is
  the right one and skipping would have thrown it away: 2 basic is what the
  model has, and `x0 >= 1e9 + 5e-7` against `x0 <= 1e9` really has no common
  point. Both tests run and pass in both builds; `make test`, the reference
  build and `make sanitize` all exit 0.
  **A second pin was added beside it the same day (D118).** The basis-count
  pin above only holds while D95 wins a race against the implied free family:
  under D118's refused candidate that model read the correct 2 and stopped
  detecting anything at all. `test_the_basis_count_promise_breaks_on_a_declined_column`
  pins the same defect on a column the implied free family declines by
  **margin** rather than by order, so no change of order can retire it
  quietly. Found by `numerics-reviewer`.

- **Two positive tests had no fault-build guard.** `make test
  EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE` did not compile at all until
  2026-08-15 (`make_frozen_row_infeasible_model`, fixed), and once it did,
  `test_a_maximised_singleton_row_is_owed_its_multiplier` and
  `test_a_maximised_empty_column_takes_its_upper_bound` failed because a
  fault build is meant to break exactly what they assert. Both now carry the
  guard every other positive test in the file has. **All seven off-by-one
  negative tests pass**, which is the first time that has been true since the
  compile broke.

- **A row's own width can be destroyed by the shift that removes a column,
  and no family is involved.** `cur_rl[i]` and `cur_ru[i]` are running
  differences, so a row the caller wrote as `[1, 2]` reads as a single number
  once a fixed column takes 1e17 off both: `ulp(1e17)` is 16, and `1 - 1e17`
  and `2 - 1e17` are the same double. The row handed to the simplex has lost
  its own width, and the answer is wrong by up to that width whatever
  presolve does next. `row_traffic[i]` is exactly the quantity that would say
  when this has happened and no site compares against it for this purpose.
  The implied free column singleton now declines such a row
  (`test_a_range_row_that_shifted_into_an_equality_is_declined`), which keeps
  that family inside its measured scope and does not repair this. Found by
  `numerics-reviewer`, 2026-08-15.

- ~~**Two assignments in the replay are correct by an argument no assert
  states.**~~ **The check is built and it found something else (D153,
  `bench/measurements/02-62/`).** `ps_verify_row_activities` recomputes every
  row's activity from `sol_col` and compares, behind
  `-DJAOS_VERIFY_ACTIVITY`. Validated against two injected faults (45 of 94
  and 86 of 94, both silent with asserts off) before being believed.
  **138 of the 139 pass.** The two assignments are not the problem, so this
  debt is answered. What the check turned up instead is below.

- **`row_traffic[i]` saturates to `+inf` and never recovers.** The relaxation
  at `src/presolve.c` adds `max(|cmax|, |cmin|)` to it, and a column with a
  half-infinite box makes that infinite. Measured: all 117 standard-set rows
  that reach the frozen-row test at exactly zero margin carry
  `row_traffic == inf`. What should accumulate is the finite part actually
  subtracted from `cur_rl`/`cur_ru`. Fixing it changes the frozen-row test's
  behaviour on existing reductions, so it needs its own measurement rather
  than riding along. Found by `numerics-reviewer`, refused as out of scope for
  D102 with that reason.
  **D103 did not change its severity and the source now says why.** The two
  sites that read the traffic guard against an infinite value, and both guards
  are unreachable: the only site that can saturate it sets `row_frozen[i]`
  four lines later, `row_frozen` is never cleared, and the round loop's row
  pass skips a frozen row. They are guards against a sixth family that relaxes
  a row without freezing it, and they are labelled as that in the file.
- **The basis the singleton-column family publishes breaks the count
  promise.** `jaos.h` promises exactly `num_row` of the `num_col + num_row`
  statuses are basic. It does not hold when the replay recovers the column
  strictly inside its own box: the column is published basic, and so is the
  row it was relaxed out of, which is one basic too many. Minimum case, on
  the `jm_postsolve_expand` path: `min x0 s.t. x0 + x1 = 7, x0 in [0,20],
  x1 in [0,100] cost 0` publishes 2 basic against `num_row = 1`, where
  `-DJAOS_NO_PRESOLVE` publishes row0 `AT_LOWER` and 1. **That model is
  `test_the_basis_count_promise_breaks_on_a_declined_column`, added 2026-08-18,
  and the repair announces itself there** — expect its 2 to become 1. The
  two-row `jm_postsolve_solved` model in
  `test_singleton_col_between_two_removals_solved_path` publishes 3 against 2
  and pins that too, but it holds only while D95 wins a race against the
  implied free family: D118's refused candidate gave it to the implied free
  family and the test stopped detecting anything. The minimum case cannot lose
  that race, because the implied free family declines it on the **margin** (its
  implied box is `[-13, 7]`, below x1's own lower bound of 0). When the
  column lands on its own bound instead, the count is right —
  `make_singleton_col_model` is that case. So the discriminator is which
  bound determined the value, which `ps_replay_one` has already computed
  when it picks `want_lo`; `JM_PS_FREE_COL_SINGLETON` derives its row's
  status as the mirror of its column's, with the row-count argument written
  beside it (`src/presolve.c` 1619-1634), and that is the pattern. Cost is a
  lost warm start, not a wrong answer — `build_warm_basis` falls back to cold
  when the count does not hold, and no checker or digest reads a status — so
  the repair is measured on `make warm` and `make warm-kennington`, which is
  what it changes.
  **The price is measured now (D129): 23 of netlib's 92 measured instances and
  6 of Kennington's 11 lose the warm start outright, 25% and 55%.** Those
  instances read a work ratio of exactly 1.0000, and taking them out of the
  geometric mean returns Kennington to 0.0047 against the pre-presolve
  record's 0.0041.
  **The shape, named (D130), and a repair aimed at the minimum case answers
  sixteen of the twenty-three.** Of netlib's 92 warm re-solves: 66 accepted,
  **17 short** — sixteen by exactly one, `maros` by five — and **6 over**,
  which is `nbasic` exceeding `nrow`: `80bau3b` by 21, `finnis` by 12,
  `standmps` by 11, `standata` by 10, `vtp-base` by 2, `boeing1` by 1. The
  minimum case describes a status published NONBASIC that should be BASIC, so
  it does not describe any of the six. `80bau3b` over by 21 on 2022 rows is
  not a small-model artefact.
  A further **3 instances store no basis at all** — `pilotnov`, `scrs8`,
  `share1b`, where the anchor solve stored nothing, so their 1.0000 is the
  absence of a warm start rather than the loss of one. They are outside the 23.
  **This debt is no longer the right size for what it describes (D131).** The
  published basic count is wrong on 132 of netlib's 188 optimal solves and 24
  of Kennington's 32, worst error 12104, and presolve's mapping is exact. It is
  a defect in published output, it has moved to the head of this file, and the
  minimum case named above is a corner of it.
