# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## Where the last session stopped — 2026-09-01

### → FRESH CONTEXT: READ THIS FIRST. THE PACE CHANGED.

**The maintainer's call, 2026-09-01: features, not hardening, and a lighter
loop.** The assert debt was eating whole sessions for no visible product.
It is PAUSED. Do not add asserts. Do not run control campaigns.

**The light loop, per feature:**

1. Write it, with tests.
2. `make configs` — five configurations. **Not optional and not
   substitutable**: on D239 the three suites a shortcut ran were green and
   `test_write` was red, because it asserted the contract the feature
   changed.
3. `python3 tools/record-check.py`.
4. Commit. **`CHANGELOG.md` in 5 lines**, and a `DECISIONS.md` entry only
   when a measurement settled something — 15 lines, not 80.
5. `make netlib netlib-infeas netlib-kennington J=12`, read
   `git status bench/results/`. **This is the one step never to skip**: 12
   minutes of machine time, and it is what catches a wrong answer.
6. Push from the Windows side.

**What is next, in order.** Each closes a `missing` or `partial` row in
`SPECS.md`:

| | why it is next |
|---|---|
| **presolve: duplicate rows/columns, dominated columns** | already has its reopen condition written (D101) |
| **sensitivity and ranging** | real user value, isolated from the solver |
| **Python bindings** | the most user value, and touches no solver code |

Not next, and each says why in `SPECS.md`: barrier and crossover (the
starting point is undecided), MILP (a whole subsystem), D97 (needs
crossover), the 48 wrong basis counts (needs a rank argument, a design).

**D239 is the shape to copy** — one feature, light loop, and it still found a
wrong claim in `SPECS.md` on the way.

**`.gz` input landed 2026-09-01.** `src/inflate.c` is a gzip and DEFLATE
decoder written here; both readers detect it from the first two bytes.
`record-check` refused the commit until `docs/claims.txt` and the `SPECS.md`
row moved, which is the mechanism working. The evidence is 369 comparisons
against the real `gzip` over 123 instances plus 31 large ones, all
byte-identical, and 15 unit tests (D240, `bench/measurements/02-152/`).

**Stage 7, the unboundedness verdict, landed 2026-09-01** (D241,
`bench/measurements/02-153/`). It is smaller than this file said it was:
`classify_optimum` already answered UNBOUNDED for every model whose ray is
one column leaving a lent bound, and the forced primal already shared it.
What was missing was the branch inside phase 2, and it fires zero times on
the standard set.

**Devex pricing is the next line of §0 and it is BLOCKED.** Its weight-update
recurrence and reset threshold are in Harris (1973), which is paywalled and
appears in none of the nine free sources read. The copy in
`docs/research/primal-simplex.md` §3 is unverified and must not be coded
from. Getting a citable source is what unblocks it; nothing else in the tree
does.

**Two new open items, both from D241:**

- **A ray that moves several columns at once is not decided.**
  `improves_without_limit` moves one column, so a model unbounded only along
  a combined direction reaches the refusal instead of a verdict. `min -p - q`
  over `p - q = 0, p + q >= 2` with neither column capped is one, and the cap
  ladder in `tests/test_simplex.c` proves it unbounded. The refusal is safe —
  a missing answer, not a wrong one — and its message no longer claims the
  optimum is finite. Deciding these needs a ray test over a direction rather
  than over a column.
- **The forced primal reads 61 ok, 30 DISAGREE, 3 overrun on the standard
  94.** The campaign reports it every run and it is not new, but nothing in
  this file tracks it. Most disagreements are `the settled point is not dual
  feasible` from a cold start; the 3 overruns are Dantzig pricing, which is
  stage 5. Read from a run on 2026-09-01, `make primal J=12`.

**Nothing is half-done. Start at presolve duplicates, or unblock Devex.**

### → the earlier handover, for the assert work that is now paused

**Five decisions landed on 2026-09-01, D232 to D236**, all pushed. Twenty of
the comment purge's 137 prose contracts are checks now, and the two leads
D232 handed forward are both closed.

| | |
|---|---|
| **D232** | ten contracts in `simplex.c` and `presolve.c`, `02-145/` |
| **D233** | `s->verified` has one writer and one reader, `02-146/` |
| **D234** | four scratch contracts, and `nbmark` on the primal paths, `02-147/` |
| **D235** | four presolve contracts, `02-148/` |
| **D236** | two more, and five debt items already done, `02-149/` |
| **D237** | scaling never moves a bound's finiteness, `02-150/` |
| **D238** | two singleton-row replay contracts, `02-151/` |

**Three things that session learned, in the order they cost time:**

1. **A proposed assert on the debt list can be wrong in three ways.** Already
   implemented — five of them were, and the table is in the assert-debt
   section below (D236). **False** — D235's read true and fired 58 times.
   **Unfalsifiable** — D238's restates the equality that defines the branch it
   sits in. Grep for the symbol, then read what the branch already
   guarantees, and only then write it.
2. **Run the gate population BEFORE landing an assert.** D235 wrote one that
   read true, passed every unit suite, and fired 58 times on the population.
   The proposed assert D233 was handed was false the same way.
3. **A quiet assert needs a canary, and an unreachability claim needs the
   assert INVERTED.** One that is never evaluated is never violated.
   `bench/measurements/02-148/` and `02-149/` do it both ways, and in
   `02-150/` the canary caught the arm stopping one step before the assert it
   was measuring — a result that would otherwise have been written up as
   "holds on 139 instances".

**A presolve assert costs three minutes to judge, not fifty.** Patch an early
return after `jm_presolve_run` and all 139 instances run in under a minute
(`02-148/run-presolve-population.sh`). A `simplex.c` assert needs the real
thing, and Kennington under `-UNDEBUG` is about 50 minutes of it.

**What is next in that debt**: `simplex.c` and `presolve.c` still carry most
of the 137, and the two `lu.c` checks that are not asserts. Those two need a
stamp array in the LU struct, which is the most delicate file here and wants
`numerics-reviewer` on the diff.

**The three file writers landed on 2026-08-31** — D226,
`bench/measurements/02-138/`. `jaos_write_mps`, `jaos_write_lp` and
`jaos_write_solution` are in `src/write.c`, `SPECS.md` section 6 reads done /
partial / done, and the three `absent` lines for them are out of
`docs/claims.txt`. What is next is the assert debt's TEST half, listed below.

**The assert halves are all done.** `lu.c` at D216, `model.c` at D219,
`check.c` at D221, `jaos_internal.h` at D223, and the first four tests at
D224. Ask the remote for the pushed count rather than trusting one written
here, and push from the WINDOWS side: the remote is an SSH alias that lives
in the Windows `~/.ssh/config` only.

**The phase-1 stop rule and the whole skills debt landed on 2026-08-29** —
D218, `bench/measurements/02-133/`.

**Section 0 stage 2 landed on 2026-08-27** — D212, `da16a20`, the Harris
two-pass ratio test in primal form.

**02-126's trajectory for `pilot87` is stale and reads as current.** It says
the objective turns at iteration 341234. That is the tree before D212. At
HEAD it rises 633x at iteration 19532 and never recovers (D218).

**Stage 8 and all three of its follow-ups landed on 2026-08-27** — seven
commits, `9ef21ef` through `92e767d`.

`PIVOT_MARGIN = 1.0`: a primal pivot must
stand above one ulp of its own FTRAN column's largest entry. The campaign goes
to **56 of 94 agreeing** from 55, `pilot4` DISAGREE → ok, and **the one
instance that refused calling itself defective is gone**. All three gate sets
came back byte-identical, records and work units both. `jaos-measurer`
returned **ACCEPT** and caught three errors in the record, all now fixed
(D207, `bench/measurements/02-122/`).

**Stages 8a, 8b and 8c are all closed too, on 2026-08-27** — D208, D209, D210.
8a landed (the pricing row's pivot is judged against its own terms, and
`PIVOT_MIN` there turned out to be a **stability** floor that
`docs/tolerances.md` had described as a noise floor). 8b closed on a
measurement: the floor does **not** weaken Bland's rule, twelve of thirteen
movers never stall. 8c is **refused** with a re-test in `bench/refusals.txt`:
0 of 139 instances reach the site, and the only direction it could move
declares a ray from the absence of a blocker, which D19 refuses.

**8b turned up something bigger than itself, and it is now stage 8d.**
`pilot87`'s phase-1 objective — a sum of bound violations, which must never
rise — falls to 1.24365e+12 at iteration 341000, **turns at 342000**, reaches
1.88282e+24 by 351000, and ends alternating between two values. `dfl001` runs
136695 phase-1 iterations at both settings and never rises, so this is not
what long runs do. That is a phase-1 defect, not a floor defect.

**If you were told "continue", this is the order:**

1. ~~**The assert debt's TEST half**~~ — **CLOSED 2026-08-31**, D227 through
   D230. Start at item 2, software prefetching. The paragraph below is kept
   because it records what each group covered and what four of its items
   turned into.

   **The assert debt's TEST half** (`bench/measurements/02-121/`). **Every
   assert half has landed**: `lu.c` at D216, `model.c` at D219, `check.c` at
   D221, `jaos_internal.h` at D223, with `jaos_set_coefficient`'s inline-flags
   defect fixed on the way and `-ffast-math` now a build error. **Four tests
   landed at D224** — the one-ulp tie-break, `jm_nonbasic_build` at
   `nvar <= 0`, `jm_alloc_array(0)`, and `jm_two_product_residue` past 2^996.

   **THE DEBT IS CLOSED**, all four groups on 2026-08-31: `check.c` at D227,
   `lu.c` at D228, `model.c` at D229, `jaos_internal.h` at D230. Thirteen
   tests landed and four items turned out not to be tests at all — an
   injected allocator, a census, a three-build record comparison, and one
   finding about which build carries which guard. The list below is kept for
   what each group covered. Every one needs the arm that
   makes it go red; `bench/measurements/02-137/run-test-control.sh` is the
   shape to copy, and its lesson is that a test can pass through a mechanism
   it does not test.

   - ~~`check.c`, and these are the ones that guard a published verdict~~ —
     **all five landed 2026-08-31, D227, `bench/measurements/02-139/`.** The
     sixth was `certified_step` returning 0 and not a negative, and it turned
     into the campaign's finding instead of a test: the clamp, D219's assert
     and the caller's running maximum are three guards on one property, and
     **`-DNDEBUG` leaves only the maximum**. The test pins the reported value;
     the assert is what would catch the clamp going away, in a debug build.
     Running the same breaker twice, once under `-DNDEBUG`, is what turned
     that from an assumption into a measurement, and no earlier control
     campaign here did it.
   - ~~`lu.c`~~ — **all four closed 2026-08-31, D228,
     `bench/measurements/02-140/`**, and only one of them turned out to be a
     test. The failed update's ftran/btran silence is
     `test_a_wrecked_factorization_writes_nothing`. `grow_pair`'s second-array
     failure needed an injected allocator, because both arrays hold
     eight-byte elements and no input makes only the second grow fail. The
     `piv_n == 0` shortcut needed three builds, because one binary runs one
     path. And **`find_pivot`'s zero-count bucket is dead on this
     population**: 0 of 23,103,784 accepted pivots came from it, skipping it
     leaves every record byte-identical, and the whole unit suite is green
     with the loop starting at one. The bound stays; the measurement is
     beside it in `src/lu.c` because nothing else guards it.
   - ~~`model.c`~~ — **closed 2026-08-31, D229, `bench/measurements/02-141/`.**
     Four tests. The campaign's own finding is about controls, not about
     `model.c`: two arms came back GREEN where a red test was expected, once
     because the test changed a cost from one non-zero value to another and
     the break was keyed on zero, and once because the break was placed at
     `m->scale_valid = true`, which runs BEFORE the factors are computed.
   - ~~`jaos_internal.h`~~ — **closed 2026-08-31, D230,
     `bench/measurements/02-142/`.** Three tests, and one finding: letting
     `jm_pattern_order` keep a position equal to `limit` is a **segfault**
     in a release build, because `limit` sizes the bitmap and that position
     indexes one word past it. With asserts it aborts; with `-DNDEBUG` the
     hardware stops it. The defect cannot be silent, which is the opposite of
     what D227 found about `certified_step`'s clamp.
2. ~~**Software prefetching on the indirect loads**~~ — **REFUSED 2026-08-31,
   D231**, `bench/measurements/02-143/` and `02-144/`. Built, bit-identical on
   all three sets, and unmeasurable: **`tools/icount.sh -m` cannot see a
   prefetch at all**, proved by a canary whose eight scattered prefetches per
   iteration moved the miss count 0.061%. The only reading outside the 6.27%
   noise floor was a slowdown (`pilot`, 1.0709x). The reopen condition is in
   `bench/refusals.txt`. What the census did establish is kept: 54.1% of
   inner-loop iterations are in loops longer than 64, so the technique
   applies and it is the instrument that is missing. The paragraph below is
   the original item.

   **Software prefetching on the indirect loads**, the first performance
   candidate to survive a literature pass against the bit-identical
   constraint. Ainsworth & Jones, CGO 2017 and ACM TOCS 36(3) 2019, DOI
   10.1145/3319393: one prefetch per level of indirection, at
   `offset = c(t - l) / t` — `t` loads in the chain, `l` a load's position,
   `c` one microarchitecture constant. Their figure 6 puts `c = 64` close to
   optimal across five microarchitectures, so the sweep is around 64 and not
   from scratch. Determinism-safe: no value, order or control flow changes,
   and GCC documents `__builtin_prefetch` as fault-free on a bad address.
   Sites: `x[row[k]] -= val[k] * p` in FTRAN, and the row-wise pricing loop.

   **Judge it on `tools/icount.sh -m`, not on instructions** — every prefetch
   is a retired instruction, so the instruction count reports a working
   change as worse (D225). Read the paper before writing the code: the PDF
   is at `https://www.cl.cam.ac.uk/~tmj32/papers/docs/ainsworth19-tocs.pdf`
   and `pdftotext` handles it; section 4.4 is the scheduling formula.

   The same pass produced four refusals, all cited, none of which needs
   re-deriving: structure-of-arrays versus array-of-structures has **no
   peer-reviewed study for sparse linear algebra** and the cache-conscious
   layout papers target pointer-chasing rather than flat indexed arrays;
   cache blocking targets capacity-bound kernels and a hyper-sparse
   triangular solve is latency-bound on dependent indirect loads; Sympiler's
   symbolic analysis needs a pattern that does not change and the basis
   changes every iteration; DCSC compresses out empty columns and presolve
   already removes those.
3. ~~**Ten of `simplex.c`'s and `presolve.c`'s prose contracts**~~ —
   **LANDED 2026-09-01**, D232, `bench/measurements/02-145/`. Eleven asserts,
   fourteen arms, and a census for the one assert no arm can fire. The rest
   of that debt is in the OPEN section below and it is not next: §0 is.
4. Section 0's headline decision.

**The phase-1 stop rule closed on 2026-08-29** — D218,
`bench/measurements/02-133/`. `PHASE1_RISE_MAX = 1.0`: phase 1 publishes
`NUMERICAL_ERROR` when its total infeasibility doubles above its own running
minimum. Censused over all 110 forced-primal solves, every threshold from
1e-5 to 1e+2 stops `pilot87` at the same iteration and nothing else at all,
so the value sits on a plateau eight decades wide. What settled it is not the
window: `pilot87`'s running minimum last improved at phase-1 iteration 19532
of 381886, so the 362354 iterations the rule removes lowered it by nothing.

**The skills debt is closed** (D206's two scripts, plus the `geomean.py` half
found later): `.claude/skills/jaos-measure/scripts/time_ratio.sh` is the `-j 1`
alternating-minimum time ratio with the 6.27% host floor printed beside it,
`.claude/skills/jaos-testing/scripts/reference_diff.sh` is the
`-DJAOS_NO_PRESOLVE` comparison with the `presolve=` canary as a hard STOP,
and `geomean.py --side dual|primal` reads either half of
`bench/results/primal.txt`'s pair.

Item 1 is the full loop for solver internals, `numerics-reviewer` included.
`make refusals` after it, because the assert debt touches the LU kernels.

**Section 0 stage 6 landed on 2026-08-28** — D214,
`bench/measurements/02-129/`. `can_move` reads `breached(s, v)`, a rate
against a rate in both spaces. `netlib` 94 of 94 and `netlib-infeas` 29 of 29
bit-identical; on Kennington `pds-20` publishes the same objective from a
different vertex for a fifth of the work and `pds-06` for 0.8297x, a work
geometric mean of 0.8930x over the 16. **D184's refusal is spent** and its
line has left `bench/refusals.txt`. The two arms measured — the scaled rate
alone and the union of both spaces — are byte-identical on all three sets, so
the union was chosen by argument: `wants_a_pivot` already filters with
`breached` over the complementary case, and the gap the scaled arm leaves is
reachable through the public `jaos_set_dual_tolerance`.

**The work in flight is the primal simplex, and §0 is the item.** Stages 0, 1,
3 and 4 have landed. **What is next is a DECISION, not code.** D194 measured
that 60.5% of the primal campaign's iterations are the dual's and that the
primal's phase 2 runs 97 iterations across the whole set, so §0's headline
number does not mean what it reads as. That block is at the top of §0, and
`make primal` now prints those three figures itself (D197).

**§0's remainder list is empty** (D192, D193, D195, D196, D199, D200, D201).
Nothing in §0 waits on anything except that decision.

**A second `/code-review max` ran on 2026-08-26 and found twenty items. All
twenty are closed** (D202 to D205), and two rounds of `numerics-reviewer` on
the fixes found nine more, including one the fixes themselves introduced. The
one that mattered: moving the iteration split onto the model left `solve_iters`
behind `publish()`'s gate, so an abandoned solve published the previous solve's
total and `pilot87` reported a dual re-entry of 20835 that never ran. Stage 2
and its width have landed since; what is left is stage 5 (Devex, blocked on a
paywalled source) — and D195 says stage 5's pricing question belongs to phase
1, which is where every budget is spent.

**The tree is clean and everything is committed and pushed** — ask the
remote for the count rather than trusting one written here, because it is stale
the moment anything lands. **Push from the WINDOWS side**: the remote is an SSH
alias that lives in the Windows `~/.ssh/config` only. `git fetch` first,
because another Claude session commits here.

**Twenty-nine decisions landed between 2026-08-24 and 2026-08-26, D177 to
D205**, and two of the five open items closed. What each one did is below,
newest first.

| | |
|---|---|
| **D205** | 31 of 94 primal failures published a verdict with no sentence; 0 of 31 record lines carried one, 31 of 31 do |
| **D204** | "phase 1 is 39.5%" is two instances; the median instance is 57.3%, and the line names its carriers now |
| **D203** | D199's clear buys no seconds and costs none: movers 1.0007, controls 0.9853, host floor 6.27% |
| **D202** | an abandoned solve published the PREVIOUS solve's total; `pilot87` reported 20835 re-entry iterations that never ran |
| **D201** | the hand-over's zero margin is 55000x; `s->col`'s contract is an assert now |
| **D200** | phase 1 refreshes before refusing, reaches that branch 0 times, and can be stopped |
| **D199** | the phase-1 clear is O(nrow) now; 0.9452 on the campaign, and 0 answers moved |
| **D198** | phase 1 was under-billed by `nvar` an iteration; every instance moved and two left the budget |
| **D197** | the campaign says which method did the work: phase 1 38.8%, phase 2 0.0%, dual 61.2% |
| **D196** | the iteration cap is shared, phase 1 spends 1.68% of it, and the guard's message named the wrong phase |
| **D195** | the flip's 1e10 delta fires on nothing, and D194 counted phase 1 from a success-only log line |
| **D194** | 60.5% of the primal campaign is dual iterations; phase 2 runs 97 iterations in all |
| **D193** | `refresh` is the third place a cost is lent; 30 firings in phase 1, and 54 agreeing becomes 55 |
| **D192** | Bland's rule reaches the primal's leaving variable; 0 phase-1 arms in 94 |
| **D191** | the primal's "64 of 94" was 54; a guard was documented and never applied |
| **D190** | the primal phase 1 lands; a loan of exactly 1.0 found its defect |
| **D189** | the primal published a value outside a declared bound as OPTIMAL |
| **D188** | the primal simplex's first version, and both defects were invisible to a green suite |
| **D187** | the primal clean-up priced its row the expensive way |
| **D186** | 0 long mapped bases in 101 calls, so §2's cheapest route is closed. 35 of 90 netlib warm starts fall back to cold |
| **D185** | **item 5 CLOSED** — the gate has an absolute bar, `RSUB_CEILING = 1e-6` |
| **D184** | **item 1 CLOSED** — `DUAL_TOL` is 1e-9, all four wrong points gone |
| **D183** | `pilot87`'s bound follows a dual solution that is not unique |
| **D182** | `plato-nug` solves one of three; presolve reaches nothing on it |
| **D181** | the fourth set run for the first time; §3 is not reopened by it |
| **D180** | `REFACTOR_EVERY` swept for the first time; 64 stays, with a reason |
| **D179** | a wider rule covers 19 of 24 and two instances have no candidate at all |
| **D178** | `scsd1` and `degen2` do not lose the same way, and the record said they did |
| **D177** | the suboptimality predicate watched 4 solves of 110; it watches 84 |

**Seven mistakes these sessions made, all caught before anything was written
down, and all recorded where they happened.** They are listed because each one
produced output that read as a finished result:

- a probe read the published basis and concluded the MAPPED basis arrives long;
  it arrives short, and they are different objects (D181)
- `grep -vE` on a leading bracket threw away the runner's RECORD line, because
  the runner prefixes it with the timing bracket; the summary survived (D182)
- two vertices were called different from the digests alone, without the cost
  beside them; most of those columns price at nothing (D183)
- **`make test` does not compile `bench/run.c`** and reported
  `4 Tests 0 Failures OK` on a change that did not build (D185)
- an anchor checker stripped underscores the project keeps in its anchors
- a quoted heredoc collapsed a doubled backslash to a single one, four
  times, breaking C string literals; the fix is a placeholder token
  substituted in code rather than an escape written inside the heredoc
- a probe reporting 0 phase-1 Bland arms on all 94 read as a finished answer;
  0 is also what a probe that cannot see a phase-1 line prints. Forcing
  `STALL_FACTOR` to 0 in a worktree is what made the 0 mean something (D192)
- a fix for the empty DISAGREE note was written on the bench side, landed, and
  changed nothing: the record still read 0 of 31 afterwards, because the solver
  never wrote a message on that path. Checking the record rather than the diff
  is what caught it (D205)
- widening a buffer's LOCAL rather than its FIELD produced identical bytes,
  because the copy that follows truncates to the field's own size. A review
  caught it; the test suite could not, and neither could reading the diff

### 2026-08-24: D177, and item 5 is half closed

**One constant in `bench/run.c`, no change to `src/`.** `RSUB_FLOOR` from
`1e-9` to `1e-16`. The gate's suboptimality predicate was watching **4 solves
out of 110** and none of Kennington's 16, so an instance at `1e-15` could
degrade by six orders and the gate would report `0 regressed`. It watches 84
now. The floor's stated reason was refuted by numbers already committed:
D171 moved 88 of 94 digests and moved `rsub` by at most 1.688x, so nothing
would have fired at any floor at all (D177, `bench/measurements/02-89/`).

`make test` and `make sanitize` exit 0. All three sets `gate: PASS` with
`0 regressed, 0 improved, 0 new`, and `bench/results/*.txt` byte-identical to
the committed records. **The case the predicate has to catch was built and
confirmed**: a doctored baseline that halves `adlittle`'s value fires at
`1e-16` and is invisible at `1e-9`, on runners built from the same `gcc` line.

**`make configs` was NOT run and does not apply**: nothing in `src/` or
`tests/` changed, and no block behind a build flag was touched.

**Item 5's threshold half is still open and it is blocked behind item 1.**
Three candidate absolute bars are measured in 02-89 and every one turns the
gate red at HEAD. The strongest is not the certificate: tightening
`objective_accepted` from `1e-6` to `1e-9` catches `pilot` with zero false
positives on all 94, in a band that is empty for two and a half decades.

### 2026-08-24: D178, and §3's premise is refuted

**One comment in `src/simplex.c`, proved object-identical by
`comment_only.sh`, and the two warm records refreshed because they were 24
`src/` commits stale.** §3 said `scsd1` and `degen2` lose the same way. They
do not: only `degen2` is D148's guard, at a settled dual violation of 12.91,
and `scsd1`'s guard never fires — it reaches a dual feasible point and runs
314 iterations against cold's 89. So the item asked for a predictor of
something that happens once in twenty, and eleven quantities known before the
solve separate nothing (D178, `bench/measurements/02-90/`).

**The warm campaign at this tree**: netlib work geometric mean 0.1910 against
D151's predicted 0.1916, worst 3.7165x on `scsd1`, three instances costing
more warm than cold; Kennington 0.0071. The mean is where the sweep put it and
the two worst cases are not, which is why the comment quoting them aged and
the one quoting the mean did not.

**No gate campaign is owed.** `comment_only.sh` reports the release object
UNCHANGED at `d33fba7`, so D177's campaign still holds. `warm*` is not a gate.

### 2026-08-25: D186, the long-map refusal holds and §2's cheap route is closed

**No source change.** `build_warm_basis` refuses a long count because "no long
map has been measured". Nobody had counted. **0 long maps in 101 calls** across
both gate sets, so the premise holds.

**The hypothesis it killed was worth having.** §2's rank argument is needed at
POSTSOLVE, which has no factorization. `build_warm_basis` runs inside the
solver with `repair_singular_basis` downstream, so a demotion THERE would need
no new rank machinery, and D179 had already measured the supply. There is
nothing to demote.

**The published basis and the mapped basis move in opposite directions.** 24
netlib instances publish over-long (D179); 0 map long and 55 map short.
`fit1p` publishes **over by 21** and maps **short by 241**.

**And the census prices D151's cap per instance**, which nobody had: **35 of 90
netlib warm starts fall back to a cold solve** because the shortfall is past 4.
Kennington loses none. Worst: `sctap3` 596, `sctap2` 432, `dfl001` 343
(D186, `bench/measurements/02-98/`).

**One figure disagrees with D149** and is recorded rather than resolved: that
entry put the 596-short repair on `dfl001`, and today `dfl001` is 343 while
`sctap3` is 596.

### 2026-08-25: D185, item 5 is CLOSED — the gate has an absolute bar

**`bench/run.c`: `RSUB_CEILING = 1e-6`, a per-instance verdict that reads no
baseline.** Everything beside it compares against the baseline, so a bound
already bad when the baseline was written read as permanently fine. That is how
`pilot` published a point 2.31e-05 above the optimum with nothing here saying a
word.

**Placed on 123 solves across five sets**, not on netlib alone, which is what
this item said it needed: netlib 1.4e-07, Kennington 4.18e-14, `plato-pds`
9.91e-15, `plato-fome` 1.15e-13, `plato-nug` 4.14e-12. It clears the worst by
7.1x and the band it sits in is 494x wide.

**It fires on nothing today** — `gate: PASS`, `0 regressed` on all three,
records byte-identical — **and it rejects the case it exists for.** Built
against the solver as it stood at `bc398a5`: `pilot` 6.91e-05 OVER-CEILING,
`pilot87` 2.54e-06 OVER-CEILING, `wood1p` 7.4e-09 quiet, `gate: NOT MET`
(D185, `bench/measurements/02-97/`).

**Item 5 was blocked by item 1 and nothing else**, and that was not visible
until item 1 closed. Before D184 the bar would have failed `pilot` and
`pilot87`, which is a decision about those answers rather than about this
predicate.

**`make test` does not compile `bench/run.c`.** The first version of this did
not build and `make test` reported `4 Tests 0 Failures OK` regardless. Use
`make bench` for a change to the runner.

### 2026-08-25: D184, item 1 is CLOSED — `DUAL_TOL` is 1e-9

**The maintainer took the call.** `pilot` 2.312e-05 → **5.266e-09**;
`pilot87` and `scsd6` publish Koch's optimum **exactly**; `etamacro`
1.315e-08 → 1.137e-13. `make configs` exits 0 on all five configurations and
`gate: PASS` on all three sets, with every instance still `objective=ok
checker=ok det=ok` — the regressions are cost, and no answer got worse.

**The price, and half of it was new information put in front of them before
the baselines were rewritten.** netlib work geometric mean **1.0339x**, which
is D174's own prediction to four figures. **Kennington 1.0976x, which D174 did
not measure at all** — that sweep was netlib only, and `pds-20` goes from
6.15e9 to 2.96e10 work units. Past the 2.0x bar: `agg3` 2.28x, `d2q06c`
5.32x, `nesm` 2.17x, `perold` 2.80x, `pilot-ja` 2.21x, `pds-20` 4.81x
(D184, `bench/measurements/02-96/`).

**D177 is why two of the regressions are visible**: `bnl1` and `scsd1` report
their suboptimality bound moving at 1.57e-14 and 1.96e-16, both far under the
`RSUB_FLOOR = 1e-9` this project shipped the day before.

**A units conflation was found in review and then refuted by measuring it.**
`DUAL_TOL` bounds a rate everywhere except `can_move`, which compares a
rate-times-distance PRODUCT against it. Holding that site at 1e-7 while the
rest moves leaves **94 of 94 digests identical at 1.0000x**, so it decides
nothing today. Its likely reason is structural and unmeasured: it feeds a path
that needs the primal simplex `SPECS.md` has as missing. **That is its reopen
condition.**

**No independent verdict was taken.** `CLAUDE.md` asks for `jaos-measurer` on a
finished candidate and this session was instructed not to spawn subagents; the
per-instance evidence is in 02-96 so the judgement can still be made.

### 2026-08-25: D183, `pilot87`'s bound follows a dual solution that is not unique

**No source change.** D92's standing debt asked why `gap_positive` moves on
`pilot87` while every answer stays inside tolerance. D180 handed it a case the
record cannot resolve — the identical objective and two different digests at
`REFACTOR_EVERY` 8 and 256 — because `bench/run.c` hashes `x` and `y` into one.

Split apart: **the priced primal answer does not move.** 738 of the 987 columns
that move cost exactly zero and the other 249 move by at most 4.44e-15.
**1817 of 2030 duals move, 166 by more than 1e-9 relative**, largest relative
move 55.7%, none changing sign. `gap_positive` is built from the duals and
follows them; so does `unquantified_rays`, 10 against 14. **The bound moving is
a property of the model** (D183, `bench/measurements/02-95/`).

**Answered in mechanism, not in magnitude.** D92's variants spanned
0.0068–26.7, a factor of 3900; these two settings move it by 1.2%. Those
variants are not in this tree and the entry says so rather than claiming the
span.

**This session's own first reading was wrong and is recorded as such**: it read
"two different vertices with the same objective" from the digests alone. 987
columns moving while `c'x` holds to the last bit is a claim that needs the cost
beside it, and with the cost most of them price at nothing.

### 2026-08-25: D182, `nug` is measured and presolve reaches nothing on it

**No source change.** §4 had carried `plato-nug` as "unmeasured rather than
unsolvable" since D115 and nobody had checked the sentence. `nug08-3rd`
**solves** — 34424 iterations, 294654930775 work units, `checker=ok`,
`det=ok`. `nug20` does not finish in 3600 s and `nug30` not in 1800 s, and
neither is known unsolvable. **The family is not ordered by rows**: `nug20`
has 4488 FEWER rows than `nug08-3rd`. So the set is not practical and one
instance of it is usable, which is a third answer (D182,
`bench/measurements/02-94/`).

**Presolve removes NOTHING on it**, and across the committed records its reach
is a median of 9.04% of rows on netlib, 12.57% on Kennington, 2.93% on
`plato-pds`, 0.00% on `plato-fome` and 0.00% on `plato-nug`. That is §4's own
argument in JAOS's numbers. **It is not a matched comparison against
Galabova's 1.10-against-1.67** and §4 says why.

**The instrument was wrong first and its output read as finished.** A
`grep -vE` on a leading bracket, meant to drop the timing prefix, threw away
the RECORD line — the runner prefixes that with the same bracket — while the
summary and `gate: PASS` survived. Records come from `-o` now.

### 2026-08-24: D181, the fourth set run for the first time

**No source change.** The warm campaign had never been run on `plato-fome` or
`plato-pds`, which came in with D115. `plato-fome`: 4 instances, **0 repairs
fired**, so §3 is not reopened. The mapped basis arrives **short by a
constant 5.6% of rows** — 681, 1357, 2720 on a family that doubles exactly —
against a cap of 4, and neither cap shape reaches that (D181,
`bench/measurements/02-93/`).

**What the set does say is §2's price, and it is far higher than netlib's.**
The three instances publishing a wrong basic count are exactly the three whose
warm re-solve does bit-identical work to the cold one; `fome21`, which
publishes an exact count, saves **47%**. `fome13` is over by 53 against
netlib's worst of 21.

**Two things this cost and they are written into 02-93 so they do not cost it
again.** The first reading inferred the map arrives LONG from the published
over-count — the published basis and the mapped basis are different objects,
and it arrives short. And a running probe must be stopped before its patch
script is edited: the trap reverts with the anchors it finds at exit.

**`plato-pds` is 6.4 hours and was not attempted. `plato-nug` is unmeasured.**

### 2026-08-24: D180, the refactorization interval swept for the first time

**No behavioural change**: the sweep goes beside the constant in
`src/simplex.c` and into `docs/tolerances.md`, both proved object-identical by
`comment_only.sh`. This closes the standing debt `TODO.md` credited with three
of M1's four defect closures.

**No answer changes verdict at any of six intervals**, 94 netlib and 29
infeasible instances each time, so the interval hides no defect at HEAD.

**64 is not the work minimum and it stays anyway.** 32 reads 8.6% better on the
geometric mean and costs `grow15` 2.819x and `pilot87` three orders of accuracy
that no verdict reports — which is D177's open half seen from the other
side. The constant carries its sweep now.

**And `pilot` publishes Koch's optimum exactly at 8, 32 and 128**, at the
shipping tolerance, while 16 and 64 publish the 2.312e-05. At 256 it reads
5.266e-09, which is D174's own `dual_tol = 1e-9` value from a completely
different knob. Two knobs, one small set of vertices. That is item 1's material
and nothing was decided (D180, `bench/measurements/02-92/`).

**Two controls carry it**: the record at 64 is identical to
`bench/results/netlib.txt` on all 94 instance lines, and 0 of 94 instances
report identical work at every setting.

### 2026-08-24: D179, and §2 is down to the rank argument

**No source change.** A public-API probe counts what a wider-than-the-row rule
would have to demote: basic variables resting exactly on their own bound. The
model-wide supply covers the over-count on **19 of the 24 instances**, where
the within-row rule D141 refused had nothing on 66 of 80 firings. It does not
close the item — `fit1p` and `share1b` have zero candidates at every tier down
to 1e-9 relative — so **the rank argument is now the whole of the work**, and
accepting the residue has a measured floor of 3 of 24 (D179,
`bench/measurements/02-91/`).

The probe reaches 24 instances where 02-48 reaches 48 solves, on code the two
share none of. That is a second route to D171's number.

**D177, D178 and D179 were pushed on 2026-08-24**, fast-forward with no
divergence, on the maintainer's answer to a direct question: pushes are theirs
to delegate and they have. That sentence is about a past moment and cannot go
stale. **Ask the remote for the current state:**
`git rev-list --count origin/main..HEAD`. **Push from the WINDOWS side** — the
remote is an SSH alias that lives in the Windows `~/.ssh/config` only, and from
WSL it dies as "could not resolve hostname". `git fetch` first: another Claude
session commits to this repository.

### The state of the tree at 2026-08-21, which the sections below assume

**Nothing is in flight and no worktree is registered.** The tree is clean apart
from one untracked directory that is not this session's — `02-31/`, under the
measurements directory, see below. **`make configs` exits 0** — all five
build configurations.

**The gate campaign at HEAD is valid and it is D175's** (`c648f86`, the only
commit of the session that touched `src/`). `gate: PASS` on all three sets with
`0 regressed, 0 improved, 0 new`, and `bench/results/*.txt` came out
**byte-identical to the records already committed** — which is what 0 verdict
flips predicts and what 02-83's exact objectives confirm a second way. D173,
D174 and D176 changed nothing in `src/` at all, so no campaign is owed for
them.

**EVERYTHING WAS PUSHED AT THE END OF THE 2026-08-21 SESSION**, on the
maintainer's explicit say-so, fast-forward with no divergence. That sentence is
about a past moment and cannot go stale; a count can, and this paragraph has
said "nothing", "four" and "ten", each true when written and one of them wrong
within a minute because writing it was itself a commit. **Ask the remote:**
`git rev-list --count origin/main..HEAD`.
The commits carrying decision entries are `6061c38` (D173), `efe5884` (D174),
`c648f86` (D175) and `0a6d58f` (D176); the rest are records and documentation. **Push from the WINDOWS side**: the
remote is an SSH alias that lives in the Windows `~/.ssh/config` only, and from
WSL it dies as "could not resolve hostname". `git fetch` first — another Claude
session commits to this repository.

**Two tools settled "did this invalidate the campaign" three times today.**
`comment_only.sh <file> <ref>` reports the release object UNCHANGED for a
comment edit and for anything behind `NDEBUG`, which cleared `ab87c99`,
`4747f29`'s assert and the `resc` comment rewrite. And `$(B)/bench/run` links
the library built from `src/` alone, so a `tests/` edit cannot reach a campaign
at all.

### → IF YOU ARE A FRESH CONTEXT, THIS IS THE HANDOVER

**Everything below in this section is finished and committed.** Nothing is
half-done, no measurement is owed, and no agent is waiting. What follows is the
list of what to pick up, in order, and every item names what it needs before
any code.

### → THE NEXT SESSION BUILDS THE PRIMAL SIMPLEX. Section §0 below.

**Decided by the maintainer on 2026-08-25.** Items 1 and 5 closed that day,
and what is left cannot move without either a decision or a feature. The
primal simplex is the feature, and it is the only one anything here waits on.
**§0 is the item; read it before any code.**

**Items 1 and 5 are CLOSED (D184, D185).** Item 2 needs the rank argument at
postsolve and D186 closed the only cheaper location anyone had proposed. Item
3 needs a second instance of a mechanism that occurs once, and D181 showed the
fourth set does not have it. Item 4 is blocked behind §0. **None of them is
short of measurement.**

| # | item | what it needs | where |
|---|---|---|---|
| **0** | **BUILD THE PRIMAL SIMPLEX** | the literature first, then a design. **This is the next session's work and the maintainer chose it on 2026-08-25.** It is the one missing feature anything in this file waits on: it blocks crossover, crossover blocks D97, and D97 is item 4 | **§0 below** |
| 1 | ~~**`pilot` publishes a point 2.31e-05 above the optimum**~~ **CLOSED 2026-08-25 (D184)** — `DUAL_TOL` is 1e-9 and all four instances that published a point off the optimum no longer do; `pilot87` and `scsd6` publish Koch exactly. Work geometric mean 1.0339x on netlib and **1.0976x on Kennington, which D174 had not measured** (`pds-20` 4.815x, `d2q06c` 5.319x). `gate: PASS` on all three, no answer worse. Taken on the maintainer's decision | §4 below |
| 2 | **48 netlib solves publish an invalid basis** | **the rank argument, and now only that.** D179 measured the supply a wider rule would draw on: it covers 19 of the 24 instances outright, and `fit1p` and `share1b` have zero candidates at every tier, so such a rule improves the residue and cannot close it. Every local repair was already refused (D140, D141); D171 made it worse by 2. Accepting the residue now has a measured floor of 3 of 24 | §2 below |
| 3 | **`degen2` behind D151's cap — and `scsd1` is a SEPARATE question** | **the premise that they lose the same way is refuted** (D178). Only `degen2` is D148's guard, at a settled dual violation of 12.91; `scsd1`'s guard never fires and it genuinely runs 314 iterations against cold's 89. So a doomed trajectory happens ONCE in twenty, and eleven quantities known before the solve separate nothing. Needs a second instance, which means §4's fourth set | §3 below |
| 4 | **`D97`, the dual postsolve for an imposed bound** | nothing is built; `docs/research/dual-postsolve-imposed-bound.md` is the design with the literature verified. **The largest prize in the file** — it unlocks bound tightening AND doubleton equalities, 8.55% of netlib's live rows and 29.36% of Kennington's. **§8d is measured now and rewritten around it (02-87, 02-88)**: its refusal declines 50.2% of netlib's imposed bounds and 82.3% of Kennington's, and the hazard it prevents occurs **12 times in 98146 opportunities**. **The better design is postsolve detection and THIS TREE CANNOT HAVE IT**: the collision leaves the point one constraint short of a vertex, which needs a crossover, and `SPECS.md` has crossover and the primal simplex that blocks it both `missing`. So the first version is the refusal narrowed to equality rows — 35.5% and 20.3% — over-paying by three orders, and that price is the missing crossover rather than the reduction. §12 item 7 | "If all of the above is dropped" |
| 5 | ~~**the gate cannot see a suboptimal answer**~~ **CLOSED 2026-08-25 (D185)** — `RSUB_CEILING = 1e-6` is a per-instance verdict that reads no baseline. Placed on **123 solves across five sets**, worst 1.4e-07, so it clears by 7.1x and fires on nothing today. Validated against the case it must reject: on the pre-D184 solver it fires on `pilot` and `pilot87`, stays quiet on `wood1p`, and the verdict goes `gate: NOT MET`. **It was blocked by item 1 and nothing else** | §4 below |

**Three things are refused rather than open, so do not pick them up.**
`apply_flips` is the third uncompensated sum of D168's shape and loses terms
mid-solve only, because the final `refine = true` refresh rebuilds `x_B` from
scratch (D171). **presolve's `obj_offset` is measurably dead** — poisoned with
`1e300` and with `NaN`, all three sets stay bit-identical to a control that
reproduces the committed records exactly (D176). And **`finnis` is not a wrong
answer**: its gap is 1.24 half-ulps of its own largest term, against `pilot`'s
9.2e+09 (D173).

**The class D168 opened is closed at all four sites** — D168, D169/D172, D175
repaired, D176 refused with a measurement.

**Four traps this session paid for. Do not re-learn them.**

- **Read BOTH gate sets before judging a residual change.** D171 was nearly
  refused on netlib alone, where it moves 88 digests and no worst case.
  Kennington is where the worst case moved, and by four orders.
- **A control built with YOUR flags, or the reading is meaningless.** The first
  version of 02-81 read "all three sets differ" and that was the `-b` footer.
  The control also proved `-O2` and `-O3 -flto -march=native` give the same
  bits.
- **Re-read the published-basis count whenever a `basis=` hash moves.**
  `bench/measurements/02-78/run-basis-count.sh`. The gate reports a hash and
  never a count, so it said `0 regressed` while D171 cost two solves their
  basis.
- **A null intervention cannot discriminate between hypotheses.** D171's first
  entry argued the `gap` column was harmless because the symmetric change did
  not move it — and that change moved nothing at all, on any figure. The
  evidence that works is direct: `dual`, `cert`, `drop` and `rays` unchanged on
  all 110.

**And the one that cost an answer.** `numerics-reviewer` found a defect in D172
**after** it was committed with `make configs` green, the gate green on all
three sets, a constructed minimum model and a test that failed at the parent.
The overflow guard tested the two FACTORS while the split overflows on their
PRODUCT. **No campaign could have found it** — it needs a cost and a bound whose
product lands in the top 1.5e-8 of the double range, and no instance in 139 has
one. Run the loop AND get the review; a green loop does not cover the review's
job.


### 2026-08-21, third unattended session: D173 to D176 — an exact oracle, a diagnosis, a repair and a refusal

**Four entries and one source change.** `c648f86` is the only commit that
touched `src/`; the other three are measurements.

| commit | what it is |
|---|---|
| `6061c38` | **D173** — the exact objective oracle. `jaos_objective` is correctly rounded on 110 of 110; `finnis` refused; `pilot` named as the instance with a wrong point |
| `efe5884` | **D174** — `pilot`'s 2.31e-05 is `DUAL_TOL` and nothing else, and 1e-9 turns the gate red on six instances |
| `c648f86` | **D175** — the sum that ranks two rounds is compensated; gate byte-identical. Also folds in D173's corrections |
| `0a6d58f` | **D176** — presolve's `obj_offset` is measurably dead, so compensating it is refused |

**THE THING TO CARRY FORWARD IS NOT A FINDING, IT IS A FAILURE RATE.** Six
defects were found in this session's own evidence, and **every one produced
output that read as a clean result**. Two were caught by `numerics-reviewer`,
two by a control, two by re-reading. None announced itself.

- A probe passed `-m` without `-d`, so `bench/run` read the standard instance
  directory three times while the output printed three set names. **Kennington
  recorded zero comparisons** and the summary said "on the three gate sets".
- The same probe then copied `src/` from the working tree, where the repair
  already was, so it compared the compensated sum against itself and **every
  error column read exactly 0**.
- One figure was printed under one name with two definitions, disagreeing by
  five orders — `spread` and `flip` are named apart now.
- A poison harness omitted `-e infeasible`; the infeasible records then
  differed under both poisons **and under the control**, which without the
  control reads as "alive on one set, dead on two".
- D173's justification for its own threshold was false by seven orders. The
  threshold was right; the reason attached to it was not.
- A measurement record was committed while its background job was still
  writing it — 17 lines of 34.

**What actually caught them**: a control that must reproduce the committed
records before any treatment is believed; a second oracle sharing no code
(Python's `fractions`, and `gap_negative` against `priced`); and naming the
tree a probe measured. **Add a control to every probe, and make it fail if the
harness is blind.**

**Four habits worth keeping.**

- **Measure a gap against `eps * sum |c_j x_j|`, not against the objective.**
  `finnis` looks like the worst instance on the set at 7.62e-05 and is 0.107 of
  its own floor; `pilot` at 2.31e-05 is 1.87e+08 of its. The raw gaps say the
  opposite of the truth.
- **A caller-owned tolerance sweeps with no rebuild.** `DUAL_TOL` is settable
  through `jaos_set_dual_tolerance`, so seven settings over three sets cost one
  binary — and the control that passing the default explicitly reproduces the
  built-in one is what makes the sweep mean anything.

- **`jaos_objective` is finished. Do not open the published objective again.**
  It is the correctly rounded exact objective of the published point on **110
  of 110**, worst 0.493 ulp, measured against an oracle that rounds nowhere.
  The remaining disagreement with the checker is the CHECKER's — 790 ulps on
  `finnis` — because `long double` cannot hold a binary64 product's 106 bits.
- **A probe validates itself before it is believed.**
  `run-exact-objective.sh` refuses to take a reading when its self-test fails,
  and that self-test caught a wrong expected value typed in this session. Its
  own gap was that every case was non-negative, so the negation path was never
  exercised by the thing that gates the readings; there is a negative case in
  it now.

### 2026-08-20, second unattended session: D168 to D172 — four published numbers repaired, one refused, and a refusal overturned

**The eleven commits, in one line each**, so a fresh context does not have to
read `git log`:

| commit | what it is |
|---|---|
| `ba69a88` | **D168** — the simplex's row activity compensated; the reference build stopped calling a feasible model infeasible |
| `9114c94` | **D169** — the published objective, wrong two ways: a naive sum, and taken on the reduced model rather than the published point |
| `ab87c99` | D168's review folded in — its test asserted a status where a wrong objective would have passed |
| `4c972a2` | **D170** — the reduced costs are NOT a third wrong number; refuted, and §2 gains a second symptom |
| `34b6bdf` | D169's three review questions answered in the main context |
| `82b3b69` | the push recorded |
| `4747f29` | D169's review folded in — a guard that threw away D19's own verdict |
| `39a49f6` | **D171** — the refinement residual compensated; a refusal overturned by measurement |
| `311d73b` | **D172** — the per-term rounding recovered with Dekker's split |
| `e29198a` | **the hole D172 shipped with**: the guard tested the factors and the split overflows on the product, so it published `inf` |
| `a3f68c1` | the oracle D172 measured against is not exact either, and what `finnis` has left is the point |

**The defect D162 named and could not close.** `compute_primal` builds
`-N x_N` by walking the nonbasic columns in column order, so a row is a slot
many columns write into and the order it sees is the column order. A row that
met a large term before many small ones lost the small ones outright, and the
`-DJAOS_NO_PRESOLVE` build refused a model whose feasible point is exactly
representable. **That build is the oracle every presolve entry in this
repository is judged against.** The sum is compensated now, with the same
Neumaier accumulator D165 gave presolve.

**Three things to carry forward and not rediscover.**

- **The work counter cannot see this class of change, by construction.** The
  Neumaier step is arithmetic `jm_work_add` does not bill, so the units report
  the trajectory and say nothing about the cost. The seconds are the only
  evidence, and the way to make them mean anything is to time instances that
  came back BIT-IDENTICAL on the gate: their ratio is the arithmetic alone.
  Four of them span 0.9501x to 1.0302x across two runs of the same protocol,
  which is this host's 6.27% (D93) and not a cost.
- **A more accurate sum is not a smaller residual per instance.** Over netlib's
  94, `rsub` improves on 8 and worsens on 2, `row` improves on 7 and worsens on
  8. Two moves are large and right (`pilot-ja` 6.03e-12 → 1.62e-14, `pilotnov`
  8.16e-13 → 5.21e-14) and the rest is a few ulps either way. **Do not offer
  the residual table as the evidence.** The evidence is the constructed model
  and no verdict moving on the 139.
- **The published-basis count was re-read rather than assumed**, because six
  `basis=` hashes moved and the gate reports a hash and never a count. It read
  `exact=142 WRONG=46` on netlib and `WRONG=0` on Kennington, unchanged at
  D168; the sum fell +250 → +248. D167 is the entry that says why this must be
  run. **The habit paid twice: D171 moved nine more hashes and the same probe
  caught a regression the gate called `0 regressed`** — 46 → 48. Kennington
  stays at 0 throughout.

**D170 asked whether the reduced costs were the third wrong number and they
are not.** The published `col_dual` matches `c - a'y` in `long double` on every
column that fires; what is wrong is the status beside it, and every firing
instance is one of §2's 23. `price_entry`'s naive dot product is refused with
the measurement. **The detector it built is new and cheap** — three public
calls per instance, `02-80/run-redcost-signs.sh` — and it exists because
nothing else asks: the checker recomputes `d` from `y` and never reads
`col_dual`, and `basis=` is a hash compared only against a previous hash. It
also cross-checked the recorded 46 from an instrument that shares no code: 23
instances of 94, and the gate solves each twice.

**What D168 left open, and it is a candidate rather than a defect**:
`subtract_basis_times` is still an uncompensated sum. It is in the §4 list
below with what it needs.

**D169 asked the same question of the published objective and found two
defects, not one.** `jaos.h` promises "objective value of the solution held by
the model". The sum was naive — `+1e16`, then 256 costs of 1, then `-1e16` on
columns fixed at 1 publishes **0** where the answer is 256, while
`jaos_check_solution` reads 256 — and the two presolve paths reported the
REDUCED model's objective rather than a sum over the values the caller reads.
`jm_model_publish_objective` replaces all three producers.

**Two things from it to carry forward.**

- **The checker is the oracle for a published number, and it is already in the
  tree.** `jaos_check_solution` judges the model as loaded, in `long double`,
  independently of every solver bookkeeping, so `|jaos_objective −
  primal_objective|` IS the promise measured. 81 of netlib's 94 agree with it
  exactly now against 34. Reaching for the manifest reference instead is the
  weaker measure and it disagrees on `finnis`, because that reference is the
  true optimum and the published point is only near it.
- **A compensated sum does not make a `double` sum exact.** What is left on
  `finnis` is 2.65e-05, and every bit of it is each `c_j x_j` rounding to a
  double: one term of 6.5e11 rounds by up to 7.2e-05 on its own. The
  accumulation itself is exact to 6.3e-09 there, against 5.1e-05 naive.
  `bench/measurements/02-79/split-the-error.txt` separates the two, and any
  future "compensate this sum" should separate them the same way before
  claiming a result.

### 2026-08-20, unattended: D162 to D167, and the class D159 opened is closed

One defect ran through six entries, and the shape of it is the thing to carry
forward: **`cur_rl[i]` and `cur_ru[i]` were the only running sums in
`src/presolve.c` with no compensation.** Four windows judged them without
knowing how they were computed.

| | |
|---|---|
| **D162** | three of the four windows widened by a count of the removals. Two shapes built and refuted first: the traffic alone is short, and `ps_bound_scale` brings D161's defect back through the count |
| **D163** | a FOURTH window nobody had listed — the singleton row's fold. Found by `numerics-reviewer` after D162 had been committed |
| **D164** | **REFUSED**: carrying the error into the window stops a false INFEASIBLE and then publishes two rows violated by 7.5 times `CHECK_TOL`. A window decides whether to refuse; it cannot correct a value that is already wrong |
| **D165** | the row bounds keep their residue (Neumaier). The chained model now matches the oracle bit for bit. **15 netlib instances moved, 14 digests**, work geomean 1.0000x, iterations and reduction counts identical on every one |
| **D166** | the counts come out again — 196 lines. Their own five tests passing without them is the evidence, not the 139 |
| **D167** | the published-basis count re-measured, and it had been stale since the PREVIOUS session |

**Three things a later session should take from this and not rediscover.**

- **A window is the wrong instrument for an error in a value.** D162, D163 and
  D164 all widened or tried to widen; D165 removed the error and D166 removed
  the widening. Two entries of machinery existed because the first question
  asked was "how wide should this be" instead of "why is this number wrong".
- **Bit-identical on 139 is evidence of no COST, not of no HARM**, whenever a
  change NARROWS a window: nothing on the sets lands in the band given up, so
  the sets cannot see it. The safety evidence is the constructed tests
  (`jaos-measurer`, D166).
- **A number whose owner is a script nobody runs will drift.** `TODO.md`'s
  basis table was wrong for a day and five places cited it; `basis=` is a hash,
  so the gate detects a change and never reports a count (D167).

**Both subagents delivered LATE and both were right.** `numerics-reviewer`
returned two wrong answers after D162 was committed — D163 exists only because
of it. `jaos-measurer` returned ACCEPT on D165 after that commit too, having
run the parent itself, and found three things no campaign summary shows: an
objective that had been an EXACT match to Koch and is now one ulp off, a move
that is meaningless against a pre-existing gap of 203 million ulps, and the
stale basis figure above. **Plan the follow-up commit; do not write "the review
did not deliver" while the agent is alive.** That sentence was wrong twice and
needed correcting both times.

**Two process traps found by running into them**, both now in
`.claude/skills/jaos-measure/`:

- **`build/diag/wt-*` is inside what `make clean` deletes**, and `make configs`
  runs `make clean` five times. A `jaos-measurer` campaign lost its whole
  worktree to this session's build, silently. 44 scripts here use that
  location. `preflight.sh` warns about it now, validated against the case it
  must catch AND the control it must not fire on.
- **Re-running an old measurement script overwrites its record**, because they
  `tee` into their own directory — and it replaces only the working-tree
  column, leaving the parent and oracle columns from the old tree in the same
  file with nothing saying so. Check `git status` after running anything under
  `bench/measurements/`.

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

~~**The gate campaign at HEAD is valid.**~~ **Superseded — the paragraph at
the top of this section is the current one.** What it said about D153 was true
on 2026-08-19 and D162 to D166 have landed since.

~~**`bench/results/netlib.txt` was rewritten by D165**~~ — **D168 rewrote
it again** and the paragraph at the top of this section is the current one.
`netlib-infeas` and `netlib-kennington` were bit-identical throughout, so they
have not changed since 2026-08-19 and `preflight.sh` counts them as behind by
`src/` commits that were all no-ops on them — **which D167 shows is exactly how
a stale number survives**: no-ops leave a record looking current when nothing
re-derived it. `plato-fome` and `plato-pds` are behind and unmeasured.

**Two things this session added that a later one should USE rather than
rediscover:**

- **An assert-enabled build now runs every instance** (`-UNDEBUG`, D152).
  Before it, eleven of the 94 aborted, so every assert in the solve was
  untested on them. Reach for it when diagnosing anything.
- **Every debug build verifies published row activities against the
  published columns** (D153), for rows whose logical is basic, to
  `(n-1)·eps` times the row's traffic. It is validated by fault injection
  and clean on all 139.

**The PREVIOUS session (2026-08-18/19, unattended) landed D140 through D154 —
records 02-49 through 02-65, five source changes, one bench widening (the
gate sees the basis now, D150), three kept candidates, and the repository's
first tag, `v0.1.0`.**

**That session continued to D161 — records 02-65 through 02-71.** Eight items,
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
- **`run-gaps.sh`, in the untracked `02-31/` under the measurements directory,
  is not this session's.** It is an unfinished probe of the
  `want_lo <= want_hi` gaps, for the standing debt at the end of this file. It
  was left alone deliberately (`7ac820f`). **The path is written without its
  `bench/measurements/` prefix on purpose**: `record-check` reads that prefix
  as a claim that the directory is in the repository, and this one is not
  (D222).
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

## §0. BUILD THE PRIMAL SIMPLEX — the next session's work

**Chosen by the maintainer on 2026-08-25**, after items 1 and 5 closed and
everything else turned out to need either a decision or a feature. This is the
feature.

### What it is for, and what it is NOT for

**It is for crossover, and crossover is for D97.** `SPECS.md` has both
`missing`. The chain is `primal simplex → crossover → D97`, and D97 is the
largest prize in this file: it unlocks bound tightening AND doubleton
equalities, **8.55% of netlib's live rows and 29.36% of Kennington's**.
Without a crossover, D97's first version has to decline 50.2% of netlib's
imposed bounds and 82.3% of Kennington's to avoid a hazard measured at
**12 firings in 98146 opportunities** (02-87, 02-88).

**It is also for the warm starts the dual cannot serve.** `SPECS.md` says so
and nothing has measured what that is worth here.

**And it is for a refusal the solver ships today — a third dependent this
section did not name until 2026-08-25.** `classify_optimum`
(`src/simplex.c:3762`) returns `JAOS_SOLVE_NUMERICAL_ERROR` when a column is
held by a bound dual phase 1 lent it and a *real* constraint stops it short of
infinity. The optimum there is finite and this phase 1 cannot reach it.
Reaching it means lifting the loan and re-solving, and the degenerate case of
that — a basic already pressed against a real bound in the ray's direction —
is a primal pivot. D19 says so in those words and owns the population that
reaches it, from its sweep of 3000 generated LPs.

**Two facts about that population, and both belong in the plan.** It is a real
refusal with a counted rate, and it is **not a gate item**: every solve in
`bench/results/*.txt` ends `optimal` or `infeasible`, so nothing in the three
sets exercises this branch. Closing it therefore needs the generated-LP sweep
D19 used, not a campaign.

**It is NOT a speed argument, and that is measured.** Given free choice both
rivals ran the dual on every instance, with iteration counts identical to
being forced (D81). Do not open this expecting the gate to get faster.

**It is NOT needed for carried defect 4.** D85 closed that: the primal
clean-up already owns a ratio test and a basis change, and reading the reduced
cost's sign rather than the status is all a nonbasic free variable ever needed.

### The order in §6 is being jumped, deliberately

§6's proposed order puts cheap breadth first — write MPS, write LP, write a
solution file, Python bindings, sensitivity and ranging, infeasibility
certificates — and the primal simplex after them. The three writers landed
on 2026-08-31 (D226); the rest of the breadth is still after the primal
simplex. The maintainer chose the
primal simplex now because it is the only thing unblocking work already
analysed in this file. **That is a change of order and not a change of plan.**

### What it needs BEFORE any code

1. **`literature-scout`. DONE 2026-08-25 → `docs/research/primal-simplex.md`.**
   All five questions covered. Read that file's opening warning before trusting
   any formula in it: **no PDF was reached**, so the citations are checked
   against Crossref and several of the weight-update formulas are marked CHECK
   AT SOURCE. No solver source was opened (D12).
2. **The `sparse-simplex-perf` skill**, before planning any of the algorithm.
   **DONE 2026-08-25.**
3. **A design decision that is not obvious**: how much of `sx` the primal
   reuses. **This question is narrower than it looked, and reading the source
   on 2026-08-25 is what narrowed it — see the inventory below.**

### The reuse question, read off the source at 74be13e

**A primal iteration already runs in this solver.** `primal_cleanup`
(`src/simplex.c:3427`) picks a column, calls `primal_ratio_test`
(`src/simplex.c:3353`) for the leaving row, and hands both to the *same*
`pivot()` (`src/simplex.c:2512`) the dual loop uses. It bills its work as
iterations and it fires on real instances (`greenbea`, `pilot87`).

So sharing is not a choice to be weighed. It is what the tree does today, and
what is missing is a **pricing rule**, a **phase 1**, and a **better ratio
test**. Not a second engine.

**What is free.** The FTRAN of the entering column, the BTRAN, the pricing row
(`price_all:2288`, `price_entry:930`), and the whole basis change with every
update it carries. `pivot()` needs `alpha`, `rho` and `theta_dual` from its
caller, and for a primal iteration `theta_dual = d[q] / alpha[q]`, which is
what `primal_cleanup:3474` already passes. Eligibility for a primal entering
column is written three times over — `dual_breach:2832`, `breached:2903`,
`wants_a_pivot:3312` — and `jm_harris_pick:2246` and `jm_bland_pick:2131` are
already generic: they take plain `num`/`den` arrays and a tolerance and know
nothing about the dual.

**What is missing, in order of size.**

- **Primal pricing weights.** `gamma[nvar]`, one per variable, is new memory
  of a new length. `dse[nrow]` is the dual's and indexes basis *positions*; it
  is a different object and cannot be reused.

  **They start exact at the slack basis, and the formula is now read at a
  source.** The steepest-edge weight is `gamma_j = 1 + ||B^-1 a_j||^2`, derived
  from first principles in arXiv:1803.05167 §3.1 — the `1` is the entering
  variable's own movement and not a floor bolted on for safety.
  `var_column:905` writes `-1.0` for a logical, so the cold basis is `-I`, so
  `B^-1 M_j = -M_j` and `gamma_j = 1 + ||M_j||^2`: the model's own column norm,
  one pass over the matrix, no triangular solve. This is the same argument
  `build_initial_basis:952` already writes for the dual — "`B = -I`, so row i of
  `B^-1` is `-e_i` and its squared norm is exactly one" — and the reason given
  there for starting from the slack basis at all.

  **That argument holds for exact primal steepest edge and NOT for Devex, and
  the literature says start with Devex.** Devex weights are all 1 at a reset
  whatever the basis is, so a crash basis costs Devex nothing there. The
  crash-basis refusal (`SPECS` §3) reopens when "pricing stops starting from
  exact steepest-edge weights", and which way a primal simplex pushes that
  depends entirely on the rule chosen. Exact primal steepest edge would give the
  refusal a second reason; Devex would not touch it. **Decide the pricing rule
  before re-reading that row.**
- **Two gaps in the ratio test, and it was written as three.**
  `primal_ratio_test` is a single-pass minimum ratio. It never compares the
  blocking step against the entering column's own opposite bound, so that
  column can never bound-flip. It has no Harris window. **It is not missing a
  long step, and looking for one would waste
  time**: in the dual ratio test one row is scanned across many nonbasics, so
  there are many breakpoints to walk past, while in the primal **only the
  entering variable moves**. The only flip available is that variable reaching
  its own opposite bound, which is the correctness gap above rather than a
  separate technique. The primal long step exists only in phase 1, where the
  objective is a sum of infeasibilities and therefore piecewise linear
  (`docs/research/primal-simplex.md` §2).

  **The missing bound flip is NOT a defect shipping today, and the reason is
  the same shape as D184's.** `primal_cleanup` is the only caller, and it
  selects through `wants_a_pivot:3312`, which returns true only for a column
  with **no declared bound in the improving direction** — a free column, or one
  whose `real_upper`/`real_lower` is infinite. So the entering column's own
  opposite bound is always infinite there and the flip can never arise. It goes
  live the moment a pricing rule chooses entering columns, which is exactly
  when `can_move` goes live.

  **And when it does go live, the limit must be computed from `real_upper` and
  `real_lower`, never from `up` and `lo`.** `real_*` strips the bounds dual
  phase 1 invented (`real_lower:894`); the raw arrays do not. A column that
  `wants_a_pivot` selects may still carry an `ARTIFICIAL_BOUND` of 1e10 in
  `up[q]`, so a flip sized off the raw arrays would park a variable on a bound
  **the model never declared**. That is the case `repair_dual_infeasibility`
  refuses in as many words, and the evidence `classify_optimum` reads
  immediately afterwards.
- **A phase 1.** `build_initial_basis:952` makes the cold start dual feasible
  by construction and says so in its own comment. It is not primal feasible,
  so a primal simplex started cold **always** needs one. Crossover supplies its
  own basis, which is the motivating case, so phase 1 cannot be assumed away.
- **An unboundedness verdict.** `primal_ratio_test` returns -1 when nothing
  blocks and `primal_cleanup` deliberately declines to read that as a ray,
  citing D19's requirement of proof. A real primal simplex must decide, and
  D19 still applies.

**One scratch conflict, and it is the class `numerics-reviewer` hunts.**
`cand`, `rnum`, `rden` and `rrange` are `[nvar]` and belong to the dual ratio
test. A primal ratio test indexes rows, so it fits, but `primal_cleanup:3450`
already borrows `cand` on a written justification — "no dual iteration is in
flight here" — and a primal simplex *is* an iteration in flight. That
justification does not extend. Budget row-sized arrays of its own.

**One trade to record and defer.** `pivot()` always does a second FTRAN into
`tau` to keep the dual weights current, which a pure primal iteration does not
need. Skipping it saves one FTRAN per primal iteration; keeping it is what
lets the dual resume on the same basis without restarting its weights, and
`repair_singular_basis` already shows the cheap alternative (reset to 1.0).
**It does not matter yet and the record says why**: D30 measured
`primal_cleanup` closing `greenbea` in eight pivots and presenting `pilot87`
twelve candidates a round. Take the number when a primal loop runs thousands
of iterations instead of eight, on the harness below.

### What the literature settles, and the one thing it breaks

From `docs/research/primal-simplex.md`. Four choices come back with a clear
answer, and one premise of this section comes back broken.

**Pricing: Devex, not exact primal steepest edge.** This is the asymmetry that
decides it. Exact primal steepest edge costs **one extra BTRAN and one extra
full PRICE per iteration**; dual steepest edge costs one extra FTRAN and no
extra PRICE; Devex costs neither. Devex needs only the pivot row, which the
primal computes anyway for its reduced-cost update. That gap is part of why the
dual is the faster algorithm here, and it lines up with D81. Devex also solves
the warm-start problem for free — every weight is 1 at a reset, whatever basis
crossover hands over, where exact weights would cost `m` solves.

**D84 costs the primal nothing, and Maros's pricing report says so at a source
now read.** Of steepest edge: "It is a full pricing and does not adapt to the
multiple pricing scheme." Of Devex: "It is a full pricing and is not suitable
for multiple pricing." The two rules a primal simplex actually wants are
incompatible with multiple pricing anyway, so the refusal takes nothing away.
The same page states the asymmetry in Maros's own words — steepest edge "is
used in the dual simplex more frequently because it requires less extra
computations there", Devex "is considered a useful tool for the primal SSX".

**One thing that BLOCKS code, and it is the only one.** Devex's weight-update
recurrence and its reset threshold appear in **none of the nine free sources
read**; they were searched for in all of them. Harris (1973) is paywalled and is
where they live. The recurrence in `docs/research/primal-simplex.md` §3 is
written in its standard form and is **not verified**. Do not code it from that.

**Phase 1: the piecewise-linear composite (Maros 1986), and it must start from
a GIVEN basis.** No artificials. The phase-1 objective is the sum of the basics'
bound violations, piecewise linear in the step, so its ratio test walks sorted
breakpoints and several basics can become feasible in one iteration. **A phase 1
that builds its own basis out of artificials is useless for crossover**, which
is the motivating case, so design for a given basis from the start.

**Ratio test: Harris two-pass, in its primal form — which is the ORIGINAL
form.** Harris (1973) published it for the primal; the dual version JAOS has is
the transposition. EXPAND is available on top and is safe under D8, because its
tolerance depends on the iteration counter alone. It does not remove the need
for a cycle detector: Hall & McKinnon (2004) construct LPs where EXPAND still
cycles.

**One thing that changes the ANSWER and not the path.** Harris and EXPAND both
accept a point slightly outside its bounds — the step can leave a basic up to
`delta` past a bound. JAOS's checker bars that absolutely. **So the primal ratio
test must snap the leaving variable exactly onto its bound, and the answer must
be re-verified against the TRUE bounds and not the expanded ones.** Otherwise the
gate rejects solves that are correct.

**The broken premise: this section's chain assumes a starting point JAOS does
not have.** §0 states the chain as primal simplex → crossover → D97. **Crossover
as published starts from an interior point, and JAOS has no interior-point
method.** Where that point is to come from is written nowhere, and it has to be
decided before crossover is designed rather than after.

**And the requirement on it is harder than "some feasible point", which
Megiddo (1991) settles and which was read at the source.** His two theorems:
an optimal basis can be found in strongly polynomial time given optimal
solutions to **both** the primal and the dual (Thm 0.2); and doing it from
**either one alone** would give a strongly polynomial algorithm for general LP
(Thm 0.1), which is open. His introduction says the same thing in
implementation terms — a primal-optimal solution easily gives a *primal-optimal*
basis, and the dual solution attached to that basis can be infeasible. **So
crossing over the primal alone does not produce an optimal basis, however
carefully it is done. Whatever supplies the starting point must supply the
pair.**

Bixby & Saltzman (1994) is the paper to implement from. Andersen & Ye's variant
carries a conditional guarantee — it solves a perturbed problem, optimal for the
original only when the iterate is close enough to the optimal face — and is the
less safe start.

**There is no paper about carrying both algorithms in one implementation.** The
scout looked and found none; it is folklore that lives in implementations. Cite
Maros's design chapters for the shape and do not cite anything as if it settled
the one-struct-or-two question.

### What is already decided and constrains it

| | |
|---|---|
| **D8** | bit-identical results on every machine and every run. No clock, no address-ordered iteration, no unseeded randomness |
| **D11** | GMP is excluded |
| **D81** | the dual is what both rivals choose; this is not a speed play |
| **D85** | the primal is not needed for the free-nonbasic defect |
| **D127** | the unclamped dual step stays, because the perturbation is what keeps `pilot87` moving |
| **D148** | a warm trajectory that settles dual-infeasible is thrown away and restarted cold. A primal path has to say what it does here |
| **D82, D84** | partial and multiple pricing are both refused, on **wrong answers** and not on a trade. `pilot` published OPTIMAL on an objective outside tolerance and the checker passed it; `wood1p` was rejected at a dual infeasibility of 1.73e+05. Primal pricing starts full and stays full until something measured says otherwise |
| **D45** | the work counter cannot see the pricing-against-iterations trade. D82's 0.891x on Kennington is a loss in seconds, because a pricing sweep bills one cheap unit per row while the iterations it buys drag in two triangular solves each. A primal pricing verdict needs the time ratio, not units alone |

**Two rules D82 wrote down that any primal pricing scheme inherits.** Neither
is about the dual specifically. **Bland's rule cannot be given a slice**: it
promises the globally lowest-indexed candidate, and that promise is the only
thing between a degenerate solve and a cycle (D26). **The progress measure
cannot be fed a partial total**: a slice's total is smaller for a reason that
is not progress, so it would reset the stall counter and disable the one
detector that catches a cycle.

### One dead thing wakes up when this lands

`can_move` compares a rate-times-distance PRODUCT against `DUAL_TOL`, which is
a bound on a rate everywhere else. D184 measured that holding it at the old
value changes nothing — **94 of 94 identical digests** — and the likely reason
is structural: `can_move` feeds `anything_to_move`, and what those columns need
is a primal pivot that did not exist. **Building the primal simplex makes that
site live, so its units have to be settled before it decides anything.** That
is D184's stated reopen condition.

**It landed on 2026-08-25 (D188), the re-test on 2026-08-26 said the units were
LIVE (`bench/measurements/02-118/`, D206), and the question closed on
2026-08-28.** `can_move` reads `breached(s, v)`, a rate against a rate in both
spaces. No verdict moved on any of the three sets; two Kennington instances got
cheaper and `pds-20` publishes the same objective for a fifth of the work
(D214, `bench/measurements/02-129/`).

### What the gate will and will not say

The three `netlib*` sets are the gate and they solve each instance once from a
fresh load, so a primal path only reaches them if the solver chooses it. Decide
early how the choice is made, because a primal that never runs passes every
campaign in this repository while doing nothing.

**The harness this needs already has a template, found 2026-08-25:
`bench/warm.c`.** It is a second runner beside `bench/run.c` with its own
Makefile target, and it exists precisely to answer a question the gate
structurally cannot. Four properties of it transfer whole:

- it solves each instance more than once and compares the answers;
- **agreement is the gate and speed is the report** — same verdict, objectives
  within tolerance, and *both* answers put through the independent checker.
  Its own comment says "a warm start is a starting point and never a claim, so
  a disagreement here is a defect and not a trade-off", and the sentence holds
  word for word for a primal answer;
- it reports a ratio rather than a verdict, so it cannot make the gate red —
  `CLAUDE.md` already records the `warm*` targets that way;
- it never writes seconds into a file the gate reads (D17).

So a `bench/primal.c` follows the same shape: solve once with the dual, which
is the reference the committed records already hold, once with the primal
forced, require the two to agree, and report a work ratio per instance. **This
needs no new measurement method and no new baseline format.** It is the third
runner in a repository that already has two.

**The forcing must be a RUNTIME switch and not a build flag.** An earlier
version of this paragraph said to put a flag beside `JAOS_NO_PRESOLVE` in
`CONFIGS`; that is wrong for this job. The harness has to run *both* algorithms
on the *same* instance inside one process, exactly as `warm.c` runs warm and
cold, and a compile-time flag cannot do that — it would need two binaries and no
in-process comparison. It goes on `m->cfg` beside `work_limit`, `time_limit` and
`progress_cb`, which is where every other per-solve knob already lives
(`src/simplex.c:3835` and around).

**And it needs no public API, because the precedent is already in the tree.**
`bench/run.c:67` includes `src/jaos_internal.h`, and the Makefile's rule for it
says why in full: `-Isrc` is "the one deliberate exception (D-13)", for counters
that "are not, and must not become, public API (D64)", because "this runner is
in-tree tooling reading the solver it ships beside, the same relationship
`tests/` already has to it; it is not a caller judged by the same rule `jaos.h`
enforces on everyone else." A `bench/primal.c` built the same way sets
`m->cfg` directly. **So `jaos_set_algorithm` is a real question and this work
does not force it** — it can wait until the feature ships to callers.

### The build order, and what the Devex blocker really blocks

**Devex blocks the fast version, not the first one.** Dantzig pricing — the
eligible nonbasic with the largest `|d_j|`, ties on lowest index — is fully
specified, needs no paper, and is the right start. Correctness first, and the
primal's speed was never the argument for building it (D81).

**Anti-cycling is needed from stage 1, whatever the pricing rule**, and Hall &
McKinnon (2004) is read at source on this: "Cycling is shown to occur for both
the most negative reduced cost and steepest edge column selection criteria. In
addition it is shown that the expand anti-cycling procedure of Gill et al. is
not guaranteed to prevent cycling." `jm_bland_pick:2131` and the stall detector
in `price_row:1700` are the machinery, and D26 is the decision behind them.

| # | stage | blocked on |
|---|---|---|
| 0 | ~~the harness, `bench/primal.c` and the `cfg` switch~~ | **DONE 2026-08-25** — `bench/measurements/02-99/` |
| 1 | ~~phase-2 primal, Dantzig pricing, Bland fallback~~ | **DONE 2026-08-25** — D188, `bench/measurements/02-101/` |
| 2 | ~~Harris two-pass in primal form, and the snap~~ | **DONE 2026-08-27** — D212, `bench/measurements/02-127/`. Both tests build a candidate list and select with `jm_harris_pick`; D207's floor compacts that list, so a floored row neither pivots nor blocks. 60 of 94 agree against 56, overruns 8 → 4, work geomean 3.9470 → 3.8224, and `wood1p` publishes a different vertex of the same optimal face for **22% less work**. **The measurement that closed the story says no**: `pilot87` still overruns, 386392 phase-1 iterations against 387235 |
| 2a | ~~the Harris width~~ | **DONE 2026-08-27** — D213, `bench/measurements/02-127/`. `PRIMAL_HARRIS_DELTA = 0.5`, multiplied onto the per-model `s->primal_tol`. Seven widths swept. The forced-primal campaign is flat at **61 of 94** from 0.01 to 0.5, the same 61 instances name for name, and all three gate sets are byte-identical anywhere from 0 to 10; at 1e9 the gate breaks, which is the positive control that proves the probe reached the code. No reading chooses inside the plateau, so the value is argued: a factor of two under the phase-1 bound, the ratio MINOS and SNOPT start EXPAND at, and a power of two so the product is exact. **The 1351 that made `0.1` look special is one point**, between 8.27e+11 at 0.01 and 3.28e+16 at 0.3 — the width does not control `pilot87`'s divergence, and the earlier reading here said it did |
| 3 | ~~the entering column's bound flip~~ | **DONE 2026-08-25** — D189, it was a wrong answer |
| 4 | ~~phase 1 (Maros 1986) from a given basis~~ | **DONE 2026-08-25, short-step form** — 0 of 94 to 64 of 94 (D190) |
| 5 | **Devex** | **Harris (1973), paywalled** |
| 6 | ~~**`can_move`'s units** — D184's stated reopen~~ | **DONE 2026-08-28** — D214, `bench/measurements/02-129/`. `can_move` reads `breached(s, v)`: a rate against a rate, in both spaces. `netlib` 94 of 94 and `netlib-infeas` 29 of 29 bit-identical; on Kennington `pds-20` publishes the same objective from a different vertex for **a fifth of the work** (90938 iterations to 44790) and `pds-06` for 0.8297x, a work geometric mean of 0.8930x over the 16. **The two arms measured are byte-identical on all three sets** — the scaled rate alone and the union — so the union is chosen by argument: `wants_a_pivot` already filters with `breached` over the complementary case, and the gap the scaled arm leaves is reachable through the public `jaos_set_dual_tolerance`. **D27 chose the product to avoid choosing a space, and `pds-20`, the instance D27 chose it for, is the one that pays for it** |
| 7 | ~~the unboundedness verdict, and D19's refusal~~ | **DONE 2026-09-01** — D241, `bench/measurements/02-153/`. `run_primal`'s phase 2 returns `JAOS_SOLVE_UNBOUNDED` where it refused, on one added condition: no borrowed cost outstanding. The rest of D19's proof was already standing at that line, and a second opinion written for it restated the first and was deleted — `primal_apply_floor` returns the unfiltered list when its floor would empty one, so it can never remove the last candidate. **0 firings in 102 phase-2 ratio tests** on the standard set, because the dual re-entry takes the model after about one phase-2 iteration; two constructed models reach it, both where the loaded column is pulled into the basis. **A ray that moves two columns at once is still not decided**, and that is the new item below |
| 8 | ~~a relative pivot floor in the two primal ratio tests~~ | **DONE 2026-08-26** — `PIVOT_MARGIN = 1.0`, one ulp of the column's own largest entry, swept on both sides in `bench/measurements/02-122/` (D207). 56 of 94 agree against 55, and the one `ERROR` is gone |
| 8a | ~~the `alpha[q]` side is still absolute~~ | **DONE 2026-08-27** — the three sites apply `PIVOT_MARGIN` against `sum_i \|rho_i * a_iq\|` as well (D209, `bench/measurements/02-124/`). The census turned the question around: `PIVOT_MIN` there is a **stability** floor, and every call it rejects has `alpha[q]` equal to its own traffic to seventeen digits. The noise floor was the one missing, and `scsd1` was pivoting at a third of one ulp |
| 8b | ~~does the floor weaken Bland's finiteness argument?~~ | **CLOSED 2026-08-27 — it does not** (D208, `bench/measurements/02-123/`). Thirteen instances' phase-1 counts move and **twelve arm Bland's rule zero times**, as do all three controls. `pilot87` arms it once, at iteration 343682, *after* its own objective has already begun rising |
| 8d | ~~`pilot87`'s phase 1 diverges~~ | **CLOSED 2026-08-27, both questions** (D211, `bench/measurements/02-126/`). *What happens:* the turn is one pivot on an element of 3.26e-09 at 341234; the update refuses it, the rebuild recomputes `xb` from a basis that now contains it, and the objective jumps 3.4e+12. `pilot87` took 582 pivots below 1e-4 on the way, the control 3: the primal ratio test has no preference for a larger pivot, and stage 2 is that preference. *Whether to stop on a rise:* refused at the time, **and that refusal expired at D212** — `pilot-ja` rose 25.0449 then and rises 3.3348e-12 now, because a two-pass ratio test keeps the recomputation close to the carried values. The stop rule is open work again (D215, `bench/measurements/02-130/`) |
| 8c | ~~`improves_without_limit` kept the absolute floor~~ | **REFUSED 2026-08-27** (D210, `bench/measurements/02-125/`). **0 of 139 gate instances reach the function**, so the floor decides nothing and a swept constant would be fitted to nothing. The one direction it could move is the unsafe one: a skipped row is a row that does not block, so a relative floor would declare a ray from the *absence* of a blocker, which D19 refuses. Its re-test is in `bench/refusals.txt`; stage 7 is what would make it live |

**Validate the harness before there is anything to measure.** Run stage 0 with
the switch off, so both solves are the dual, and confirm it reports agreement
and a ratio of 1.0 — then doctor one answer and confirm it reports
disagreement. A harness that cannot detect a disagreement is not evidence, and
this project's own rule is to build the case a predicate must reject.

**Stages 1 to 5 must move ZERO digests on all three gate sets, and that is a
checkable property rather than a hope.** The primal path is unreachable from a
cold start — `build_initial_basis:952` is dual feasible and not primal feasible
— so nothing in the gate can enter it while the switch is off. Any instance that
moves is a defect in the shared code, not a property of the new feature. That
makes the ordinary campaign a strong test of these stages despite the gate being
unable to see the feature itself.

### → NEXT: the decision below, and then a stage

**§0's remainder list is empty.** D199 closed the phase-1 clear, D200 closed the
two refusals on carried numbers and phase 1's missing progress callback, and
D201 closed the last two:

- **The hand-over's "zero margin" is 55000x in practice.** Phase 1 declares
  feasibility from carried `xb` and `run_primal` re-checks refreshed `xb`
  against `primal_tol` exactly. Measured: 62 of the 86 instances that reach it
  land on exactly 0.0, and the worst is `ganges` at 1.81899e-12 against a bar of
  1e-07. Refused with reopen conditions in D201.
- **`s->col`'s contract is an assert now**, checked bit for bit against its own
  recomputation, and the injected-fault run proved the suite reaches the flip.

**And one count that came free** (D200): phase 1's tiny-pivot retry fires **13
times across 5 instances** — `dfl001` 7, `d6cube` 2, `greenbeb` 2, `pilot87` 1,
`tuff` 1. Nothing hangs on it; it is written down because it was never counted.

Stage 2 and its width both landed on 2026-08-27 (D212, D213). What is left is
stage 5 (Devex, blocked on a paywalled source). **D195 says stage 5's
pricing question belongs to phase 1**, which is where every budget is spent.
Stage 2 turned out to reach phase 1 too: both ratio tests changed, and phase 1
is where every one of its gains came from.

### → DECIDE THIS FIRST: what "55 of 94" means, and whether to keep it

**D194 measured the split and the number does not mean what it reads as, and
D195 corrected D194's own phase-1 counts.** `bench/primal.c` reports 55 of 94
agreeing with the dual. Over those 94 solves: phase 1 **336064 iterations
(39.5%)**, **phase 2 97 (0.0%)**, dual **515435 (60.5%)**. Phase 2 runs exactly one iteration on 80 of the 94, two to ten on 6,
zero on the rest, and **more than ten on none**.

The mechanism is measured, not argued. `update_dual` and the tail of `pivot()`
run `shift_to_feasible` once per iteration on every variable the pricing row
touches, guarded only while `in_phase1`. It sets `d[v] = 0.0` on every breached
nonbasic, and `primal_price` prices on `dual_breach`, which reads `d`. So after
the first phase-2 pivot there is nothing left to price. The primal declares
optimality, `settle_shifts` finds the point dual infeasible, and the dual's
re-entry solves the model.

**The 8 instances that never leave phase 1** are `d6cube`, `degen3`, `dfl001`,
`maros-r7`, `pilot87`, `scrs8`, `scsd8`, `wood1p`. Seven are `work limit
reached` **inside phase 1**; `pilot87` is phase 1's own refusal after 17165
iterations. **Phase 1, not phase 2, is where this method spends its budget**,
and that is where stage 5's pricing question actually applies (D195).

**Two numbers, both true, and only one of them is what the row is read as.**

| | optimal | phase-2 iterations = 1 | dual share |
|---|---|---|---|
| shipping | **56** | 80 of 94 | **60.5%** |
| phase 2 guarded too | **17** | 0 of 94 | **0.0%** |

Guarded, `truss` goes from 2802 phase-2 iterations to 422576, 44 instances lose
`optimal` and 5 gain it (`80bau3b`, `cycle`, `fit1p`, `ship08l`, `ship12l`).

**The choice is the maintainer's and nothing should move until it is made:**

- **Guard phase 2 too.** `SPECS.md` then reads 17 of 94, which is the primal
  solving models. Everything after it — Harris, Devex, the ratio test — is then
  measured on a method that runs. D191 refused this once, reading 54 → 20 as an
  over-correction; D194 refutes that reading.
- **Leave it and relabel.** `SPECS.md` says 55 of 94 *end* in agreement, and
  says in the same row that most of that is the dual finishing. Cheaper, and
  every later stage is then measured against a number that moves for reasons
  the primal does not control.

**`bench/primal.c` reports the split, and D197 closed that.** Every record line
carries `split=p1:N/p2:N/dual:N` and the summary leads with the campaign's
totals. Two instruments agree: phase 1 336660 (39.5%), phase 2 97 (0.0%), dual
re-entry 515522 (60.5%). **The decision above is still open** — what the
runner now does is state the case for it in its own output.

**And D195 moved where the next optimisation belongs.** The 8 instances that
never leave phase 1 spend every one of their budgets there — seven are `work
limit reached` INSIDE phase 1 and `pilot87` is phase 1's own refusal after
17165 iterations. Stage 5 is written as a phase-2 pricing question. On this set
it is a phase-1 one.

### OPEN: what `/code-review max` found, 2026-08-25

Fifteen findings. **One is closed by D191** — the `in_primal` guard was
documented at two sites and applied at one, which is why D190 published 64 of
94 when the honest figure is 54. Two more are closed with it (the harness's
dead `strstr`, and never reading the error for a `NUMERICAL_ERROR` primal).
**A fourth is closed by D192**: Bland's rule now reaches the primal's leaving
variable in both phases, and it arms zero times in phase 1 on all 94. **A fifth
is closed by D193**: `refresh`'s repair sweep was the third unguarded lending
path, it fires 30 times inside phase 1 on 11 instances, and guarding it takes
the set from 54 agreeing to 55. **A sixth is REFUSED by D195**: the bound
flip's `delta` really does reach 1e10 from an invented origin, 3974 times, and
moves neither phase's own measure once — no repair, with reopen conditions in
that entry.

**All four of D191's answer-changing findings are now disposed of.** D192 fixed
Bland's rule's missing half; D193 guarded `refresh`'s lending; D195 refused the
bound flip's 1e10 delta with reopen conditions; **D196 refuses the shared
iteration cap** — phase 1 spends at most 1.68% of it, on `pilot-ja`, against a
factor of 200 — and fixed the guard message that reported phase 1's count under
phase 2's label. **What is left below changes no answer.**

**The structural one is CLOSED.** `make test` now has `$(BENCH_TOOLS)` as a
prerequisite, so all three runners are compiled — and therefore `-Werror`-d —
by the loop's first step. Compiled and not run: running them needs instances
fetched from the network, which `make test` must never depend on. Verified
against the case it must catch: a deliberate break in `bench/primal.c` takes
`make test` to rc=2, and removing it back to 0.

**Two of the remainder are CLOSED by D198 and D199**: `primal_phase1_costs`
billed `nrow` for an `nvar` memset, and the clear is `O(nrow)` now; and the
four records that said "phase 2 only", "there is no primal phase 1 yet" and "a
cold start never gets here" say what the code does.

**Two more are CLOSED by D200**: phase 1 offers `progress_cb` now, and its two
refusals that are verdicts refresh before refusing. **The last two are CLOSED
by D201**: the hand-over's zero margin is 55000x in practice and is refused with
reopen conditions, and `s->col`'s contract is an assert. **Nothing from that
review is open.**

**A SECOND `/code-review max` ran on 2026-08-26 and found twenty more.** All
twenty are closed by D202 to D205, together with nine further findings from two
`numerics-reviewer` passes over the fixes. One of those nine was a defect the
fixes themselves introduced (D202), and it is the reason this paragraph names
the review rather than only the decisions: the review that finds twenty things
is also the one that catches what fixing them breaks.

### OPEN: contracts the comment purge kept as prose, each of which deserves an assert or a test

> **Stale below, 2026-08-31.** The `lu.c`, `model.c`, `check.c` and
> `jaos_internal.h` test lists in this section were closed by D227 through
> D230 — thirteen tests, `bench/measurements/02-139/` to `02-142/`. Four of
> the items turned out not to be tests at all and are recorded as such. Two
> further notes here are simply wrong now: `jaos_set_coefficient` does call
> `model_matrix_is_stale` (`src/model.c:315`), and `-ffast-math` is already
> a build error. **What is genuinely left in this section is `simplex.c`'s
> 65 contracts and `presolve.c`'s 72**, plus the two `lu.c` checks that are
> not asserts (`btran_u_pattern`'s order, `compact_pivot_row`'s duplicates).
> The per-file lines are kept below for what each one names.
>
> **2026-09-01: two of the four items this note called open were already
> closed, by D219 on 2026-08-29** (D233). `start_col_status` and
> `start_row_status` being both null or both set is the `JM_BASIS_PAIRED`
> macro in `src/model.c`, asserted at its three readers;
> `jm_model_publish_objective` opens with seven asserts, the OPTIMAL status
> and all six solution arrays. The note read
> "D227-D230 did not reach them", which was true and is not the same claim
> as "they are open". **Check the code before believing a list of what is
> left**; this is the same shape as the `pilot` row D233 found stale.
>
> **2026-09-01: ten of `simplex.c`'s and `presolve.c`'s contracts are checks
> now** — D232, `bench/measurements/02-145/`. Eleven asserts, because the
> forcing row's replay needed two. `simplex.c`: the cumulative iteration
> cap's floor (a `static_assert`), a retired bound-flip candidate's finite
> box, the phase-1 append count, `cost` not lent on entry to
> `primal_phase1_duals`, and `jm_pattern_order` returning no more than it was
> given. `presolve.c`: the compaction's two passes agreeing, a FORCING row
> pinning only at the caller's own bounds, `FREE_COL_SINGLETON` replaying
> only a free column, the backward arena walk's lower bound, a pinned
> column's non-zero coefficient, and a singleton column's exactly-zero cost.
>
> **2026-09-01: four more, all in `simplex.c`** — D234,
> `bench/measurements/02-147/`. `apat` names every slot where `alpha` can be
> nonzero and `rpat` every slot where `rho` is, both as a count rather than a
> marker array; `c1` is all zero after its incremental clear; and **`nbmark`
> matches `status` at every successful `refresh`**. That last one is the gap
> worth naming: D223's cross-check sits in `dual_ratio_test`, so the primal
> reached `pivot()` three ways with nothing checking the bitmap it maintains.
> Nothing was wrong, and that is measured on 33 instances in both methods.
>
> **2026-09-01: four more, all in `presolve.c`** — D235,
> `bench/measurements/02-148/`. The round count never passes the structural
> backstop; `row_traffic` is a magnitude; an already-infinite row end is not
> subtracted from; and the empty row's bound-scale fallback is unreachable,
> which is measured now rather than asserted in a comment.
>
> **One of those four was written wrong and the gate population caught it.**
> "A finite row end stays finite through the singleton-column shift" is
> false: `!free_col` means at least one column bound is finite, not both.
> It fired 58 times before anything was committed. **Run the population
> before landing an assert, not after.**
>
> **2026-09-01: two more in `presolve.c`** — D236,
> `bench/measurements/02-149/`. Boxes only narrow, and the reduced matrix
> never carries a row index of -1.
>
> **FIVE items on these lists were already implemented when this session
> reached them** (D236). Check the code before working from the list:
>
> | the item | where it actually landed |
> |---|---|
> | `start_*_status` are a pair | D219 |
> | `jm_model_publish_objective`'s precondition | D219 |
> | `amark` zero between iterations | D223, inside `jm_pattern_order` |
> | the reduced model aliases nothing of the caller's | D223 |
> | `pilot`'s 2.31e-05 | D184; the gap is 5.27e-09 |
>
> **Two things came out of the D232 campaign and are open.**
>
> - ~~`assert(!s->verified)` at `pivot()`'s entry~~ — **CLOSED 2026-09-01**,
>   D233, `bench/measurements/02-146/`. It fires, so the prose is wrong and
>   adding it would have aborted `etamacro`, `wood1p` and `pilot87` in every
>   debug build: 6 pivots out of 1033526 enter with the flag set, all through
>   `reenter_after_settling`. **No reader ever sees a spent verification**
>   (`stale_read` 0), so `primal_cleanup` is not a defect. The flag has one
>   writer and one reader now, and `verified_fresh` asserts the property that
>   is true. Three of the four clears turn out to be dominated.
> - The FORCING branch guards a column at a derived bound **twice**, and the
>   `col_pending_dual` test takes every rejection: 165 over 139 instances,
>   against 0 for the bound comparison that follows it, out of 98415 pinned
>   columns (`bench/measurements/02-145/census.txt`). The comparison stays,
>   but nothing goes red if a later reader deletes it.


The 2026-08-26 purge thinned six files to their contracts (D30's rule: an
invariant another piece of code depends on is an assert or a test, not a
sentence). These survived as sentences. Each line is one debt; the purge
reports name the exact surviving sentence. Add the assert or the test, then
delete the line. Code changes: the full loop, per file.

- `lu.c` — **the eight asserts landed on 2026-08-28** (D216, `bench/measurements/02-131/`): `grow_pair`'s capacity, `keep <= k` in the one-walk update, `mult_set` all false at each step top, both renumber maps total, `btran_u_pattern`'s stamp and its `top >= 0`, and the spike above the diagonal after `jm_lu_update`'s permutation. **What is left is the tests, plus two checks that are not asserts.** Tests: `grow_pair` leaves the first array freeable when the second grow fails (fault build, ASan); `find_pivot` visits a column whose live count reached zero (nonsingular matrix, `rank == dim`); the `piv_n == 0` path drops exactly what the general path drops (flag-forced comparison, L/U digests); a failed update leaves `rank < 0` and ftran/btran return without writing. Not asserts: `btran_u_pattern`'s order is dependency-respecting (a debug walk over the pattern, O(nnz)); `compact_pivot_row` leaves no duplicate column index — **the version the purge report suggested is a tautology** and the real check needs a stamp array the function does not own, so a debug build would write state the release build does not
- `model.c` — `start_col_status` and `start_row_status` are both null or both set (assert at the three readers; `basis_extend` holds it by luck today); `scale.c` reads no bound and no cost (test: change a bound and a cost, re-solve, scale factors byte-identical); every matrix modification invalidates `rowwise_valid` and `scale_valid` (one test per operation; `jaos_set_coefficient` bypasses `model_matrix_is_stale` and should call it); columns ascend by row with no duplicates and no explicit zeros after every mutation (debug checker); `jm_model_publish_objective` requires an OPTIMAL solve and all six arrays (assert); `jm_two_product_residue`'s overflow route returns 0.0 (test at 2^997); `-ffast-math` refused by the build (`#ifdef __FAST_MATH__ #error`); a column left empty by `jaos_delete_rows` is not an error (test)
- `check.c` — **the four asserts landed on 2026-08-29** (D221, `bench/measurements/02-135/`): `dual_acc.pos/neg` and their `_model` pair are magnitudes before the gap is formed, `certified_step` is only called where the opposite bound is infinite and never returns a negative distance, and `implied_bounds` only tightens. **What is left is the tests**: every multiplier contributes to the dual objective including the exempt ones (`w * b` moves it exactly); `note_dropped` counts a `1e-15` multiplier at an infinite bound; `certified_step` returns 0 and not a negative for a point sitting `tol` outside a row bound; `implied_bounds` contains every feasible point, counts infinite terms rather than summing them, and keeps an unreached bound infinite while still dropping the term
- `jaos_internal.h` — `a_*` sorted, no duplicates, no zeros after every load (debug walk); every scale factor is a power of two (`frexp` assert); `solve_primal_iters`/`solve_phase1_iters` written on an INTERRUPTED exit (test); `solve_time` read back by nothing (grep test); `jm_harris_pick`'s `num >= 0`, `den > 0`, non-empty set for `n > 0` (asserts); exact minimum in `jm_bland_pick`/`jm_primal_row_wins` (pinned one-ulp test); `jm_pattern_order`'s `mark` all zero on entry and return (debug assert), output ascending and once (test); `jm_nonbasic_*` equals `{v : status[v] != BASIC}` after every basis change (debug rebuild-and-compare); `jm_nonbasic_build` with `nvar <= 0` (test); `jm_alloc_array(0)` non-null (test); `jm_presolve_rec.index` is an original index at every push (assert); a forcing row's `index2` records are all `FIXED_COL` at replay (assert); `reduced` aliases nothing of the caller's (debug assert); `jm_postsolve_expand` entered only when `REDUCED` (assert); updates never touch L (debug checksum); a rank-deficient factorization leaves `x` untouched (test)
- `simplex.c` (65 contracts) and `presolve.c` (72) — too many for this page; the lists are in `bench/measurements/02-121/simplex.c.md` and `presolve.c.md`, the same section heading as above. Each report also names the false sentences the purge removed

**Skills debt, from the 2026-08-26 skill audit (D206).** Two procedures are prose with one right answer and should be scripts: the `-j 1` alternating time ratio (`.claude/skills/jaos-measure/scripts/time_ratio.sh -r <ref> [-n rounds] <instances>`, output piped into `geomean.py --pairs` with the 6.27% floor printed beside it; `tools/icount.sh` already has the worktree half) and the reference-build comparison in `jaos-testing` (`scripts/reference_diff.sh <instances>`, STOP when the `presolve=` canary shows a reduction). Three skills carry textbook a competent model already knows — about two thirds of `c-perf`, the residual/refinement opening of `fp-numerics`, the presolve and warm-start lists of `sparse-simplex-perf` — and should keep only the project facts; `jaos-testing:73-80`'s "none of the 139 executes `jm_postsolve_solved`" is a present-tense claim of absence `record-check` cannot see and needs a date and a record.

### OPEN: the instances the primal cannot solve and the dual can

Stage 4 took the reach to 54 of 94 agreeing (D190 published 64 and D191
corrects it), and D193 took it to **55**. The lists below are D193's run
(`bench/measurements/02-105/`):

**Those 31 now say why, which they did not until D205.** All of them end at
`NUMERICAL_ERROR` because the settled point is not dual feasible, and that site
wrote no message at all -- so `jaos_model_error` was empty on exactly the
instances this open item is about, and the record could say only "different
verdicts". Every record line carries the breach and the start it came from now,
which is the first evidence anyone picking this up has to work from.

- **31 disagree**, all of them the primal returning `NUMERICAL_ERROR` where the
  dual reaches an optimum: `25fv47`, `80bau3b`, `agg`, `bandm`, `bnl2`,
  `cycle`, `czprob`, `d2q06c`, `degen2`, `fit1d`, `fit1p`, `greenbea`,
  `greenbeb`, `maros`, `modszk1`, `pilot-we`, `pilot4`, `sc105`, `sc205`,
  `sc50a`, `sc50b`, `scsd1`, `scsd6`, `sctap3`, `ship08l`, `ship12l`,
  `stocfor2`, `stocfor3`, `truss`, `tuff`, `woodw`.
- **`pilot4` joined that list at D193 and D194 closed it as a non-regression.**
  Its phase 1 is bit-identical across the guard, it carries no loans at the
  refusal, and the one phase-2 primal iteration it lost is what sent the DUAL
  re-entry down a diverging path. Both its old success and its new failure are
  the dual's.
- **1 error**, `pilot87` — column 478 prices at 0 in row 790 of phase 1.
- **`sc50a`, `sc50b`, `sc105` and `sc205` are one family from one generator,
  failing the same way.** Four instances is one mechanism, not four.
- **7 overrun** the harness's 10x work budget: `d6cube`, `degen3`, `dfl001`,
  `maros-r7`, `scrs8`, `scsd8`, `wood1p`. That is Dantzig pricing and it is
  stage 5's number, not a defect.

**The 30 message-less refusals are one class and D194 names the line.**
`src/simplex.c:5496`: a cold start whose settled point is dual infeasible sets
`JAOS_SOLVE_NUMERICAL_ERROR` and calls no `jm_set_err`, where every other site
producing that status writes a message first. So `jaos_model_error` is empty
for every one of them. That is the D146 guard refusing the point, and nothing
yet says why the point is where it is.

`sc50a` is the one already partly diagnosed: removing the phase-1 loan moved it
from 40 iterations and 183481 units to 51 and 58062, a different trajectory on a
third of the work, and it still ends in `NUMERICAL_ERROR`. So whatever is left
is a second thing. **And it is not a stall**: at `STALL_FACTOR` forced to 0 it
still arms Bland's rule zero times in phase 1, so phase 1 improves on every
iteration right up to the refusal (D192).

**Where stage 1 is reachable from — the number this replaced.**
`make primal` reports `unreached 94` of 94 on the standard set: a cold basis is
dual feasible by construction and not primal feasible, so the method refuses to
start on every one. That is stage 4's number to move, and it is written down
before it moves so the improvement is legible.

**Two things stage 1 landed that were not in the plan** (D188,
`bench/measurements/02-101/`):

- **`run_primal` must clear `shift_pending`.** A warm start arms it, and the
  next refresh shifts every breached reduced cost onto the feasible side —
  which is exactly what the primal prices on. Before that one line, all 24
  bases a two-row model admits gave `0 primal iterations`, ten of them from
  points that were primal feasible and plainly suboptimal.
- **A count only `run_primal` raises**, because four tests passed with the
  method doctored to pivot not once: the settling re-entry's dual solve
  satisfied every assertion about the answer.

**Still open from stage 1, deliberately.** `reenter_after_settling` calls
`run()`, so a forced-primal solve can still finish with dual iterations. Making
the re-entry follow the method is a later question and the harness compares
final answers either way.

**Stage 0 landed on 2026-08-25.** `bench/primal.c`, `cfg.force_primal` with no
reader, and `make primal` / `make primal-kennington`. Validated in both
directions before there was anything to measure, which was the point of doing
it first: **94 of 94 bit-identical** at a work ratio of exactly 1.0000, and
three doctored copies of the runner each caught by a different branch. The
evidence and the scripts are in `bench/measurements/02-99/`.

**Two things it turned up that were not the plan.** `bench/warm.c` did not
compile at `-O2` — it read 79 characters into a 64-byte field, which `snprintf`
truncated safely so no measurement was ever wrong, but `-Werror` refuses it
below the `-O3 -flto` the Makefile uses. Both runners are sized to their
destination now. And `warm`, `warm-kennington`, `primal` and
`primal-kennington` were missing from `.PHONY`, so a stale file of any of those
names would have silently disabled the target.

## → START HERE — what is actually next, 2026-08-20

**If you have just been handed a fresh context, the handover table is at the
top of this file, under "IF YOU ARE A FRESH CONTEXT".** This section is the
longer form of the same list.


**Every bounded wrong answer in this file is closed.** D162 to D167 closed the
class D159 opened in presolve, by compensating `cur_rl`/`cur_ru` rather than by
widening anything; **D168 closed it in the simplex's row activity and D169 in
the published objective**, the same way. Nothing open today makes JAOS publish
a wrong answer, and nothing open today has a model that reproduces one.

**What is left in this file is designs.** Every one of them moves the gate
broadly and needs its own campaign and its own interpretation, so none is a
half-hour item. In the order a session with fresh context should weigh them:

> **This table was written 2026-08-20 and one of its four rows closed five
> days later.** The second copy of a row is how a closed item stays open: the
> `pilot` row at line 588 was struck through by D184 on 2026-08-25 and this
> one was not, so D232 read it and handed it to the next session as a live
> lead. Check both copies, or keep one (D233).

| candidate | what it needs before any code | size |
|---|---|---|
| **§2, 48 solves publish a wrong basis** | the rank argument, and now only that. D179 measured the supply: 19 of the 24 instances covered, two with zero candidates at any tolerance, so a wider rule improves the residue and cannot close it | design |
| ~~**`pilot` publishes a point 2.31e-05 above the optimum**~~ | **CLOSED 2026-08-25 by D184**, which this table predates. `DUAL_TOL` is 1e-9 and the gap is **5.27e-09**: `bench/results/netlib.txt` has `obj=-557.4897292893346` against `ref=-557.48972928406818`. The same row at line 588 has said so since D184; this copy did not, and D232 handed the stale wording forward as an open lead (D233) | closed |
| **§3, `degen2` behind D151's cap** | a second instance of the mechanism. **`scsd1` is no longer part of this item** — its guard never fires (D178), so a doomed trajectory happens once in twenty and eleven quantities known before the solve separate nothing | blocked on §4 |
| **`D97`** | a dual postsolve for an imposed bound. `docs/research/dual-postsolve-imposed-bound.md` is the design; nothing is built. Unlocks §3's doubleton equalities too | largest prize |

**Read D164 before proposing any tolerance work at all.** It is the entry that
says what a window can and cannot do, and it cost a built-and-measured repair
to learn: a window decides whether to REFUSE, so it can never correct a value
that is already wrong — widening one converts a loud failure into a silent one.

**D146's hostile-basis item is CLOSED and is no longer the start.** All four
of its steps landed: the defect was located (D147), the certificate guard
shipped (D148), the warm repair was refused blanket (D149) and landed capped
(D151). Its detail is kept below under "the hostile basis, for the record".

What is open is listed in the order it should happen, with what each already
has.

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
published state of the art does (Galabova 2023). §"`jaos_basis` publishes
something that is not a basis" below has all of it.

~~Its whole price is 46 solves losing their warm start.~~ **The price is
larger, measured 2026-08-20 (D170, `bench/measurements/02-80/`).** On five
netlib instances — `nesm`, `finnis`, `perold`, `bandm`, `pilot-ja` — a column
published BASIC carries a published reduced cost that its own status forbids,
worst **15018.5** on `nesm`. A caller reading `jaos_solution`'s `col_dual`
beside `jaos_basis` gets two statements that cannot both be true. **The count
is still the measure and it did not move**: the 5 are a strict subset of the 23
instances that fail the count, and `REDCOST ONLY = 0`.

**D171 made it worse, and that is measured rather than inferred**
(`bench/measurements/02-81/`): the count goes 46 → **48** and the worst
over-count +18 → **+21** between `4747f29` and `39a49f6`, with D172
identical to D171. It bought three published column values that sat
outside their own declared bounds, worst 8.81e-13 on `pds-20`. **The gate
reported `0 regressed` on that change**, because `basis=` is a hash that
detects a change and never reports a count.

**There is a detector now and it needs no instrumented build.** Three public
calls per instance, in `02-80/run-redcost-signs.sh`. Nothing else asks this
question — `jaos_check_solution` recomputes `d` from `y` and never reads
`col_dual`, and `basis=` is a hash compared only against a previous hash.

**The 27 firing columns split totally into the two shapes named below**: 25
rest exactly on their own lower bound and would be dual feasible as
`AT_LOWER`, so the STATUS is what is wrong; 2 are strictly inside their box
(`finnis` 564 and 565), which is the minimum case this file describes word for
word. On all 27 the published reduced cost matches `c - a'y` in `long double`
to the last bit, so **`price_entry`'s naive dot product is refused as the
explanation** — worst disagreement over all 94 is 3.37e-09 on `dfl001`, one
rounding at that column's own traffic.

### 3. `degen2` loses real work behind D151's cap — and `scsd1` is a different question

**Rewritten 2026-08-24 (D178, `bench/measurements/02-90/`), because the
premise this section carried is false.** It said `scsd1` and `degen2` lose the
same way, "each with warm iterations exactly equal to cold". A diagnostic
build says otherwise.

| | `degen2` | `scsd1` |
|---|---|---|
| warm iterations / cold | 565 / 565 | **314 / 89** |
| settled dual violation, warm | **12.91** | **0** |
| guard fires | **yes** | **no** |
| work against cold | 3.6751x | 3.7165x |

**Only `degen2` is D148's guard.** `scsd1` reaches a dual feasible point, the
guard accepts it, and the warm solve is genuinely 3.5x longer than starting
cold. `degen2`'s counts are equal because the cold restart resets the counter.

So this item asks for a predictor of something that happens **once** in the
twenty instances that repair, and D46 says what a rule read off one instance
is worth. `lotfi` costs more warm than cold too, 1.2753x, and its guard does
not fire either.

**Eleven quantities known before the solve were measured and none separates**
the three losers from the seventeen winners: the shortfall, rows the wanted
basis leaves uncovered, promotions by each of the repair's two loops, `nrow`,
`ncol`, wanted-basic columns, wanted-basic logicals, `S/nrow`, wanted logicals
over `nrow`, `ncol/nrow`. **The branch hypothesis is refuted outright** — 18
of 20 promote entirely by index order, and the only two using the
uncovered-row loop (`pilot-we`, `ship08l`) are two of the three best ratios in
the set.

**What it needs now is a second instance of the mechanism.** §4's fourth
instance set was the obvious place and **it has been tried and it is not
there** (D181, `bench/measurements/02-93/`). The warm campaign was run on
`plato-fome` for the first time: 4 instances, **0 repairs fired**, so the
block this item is about never executes.

**The reason is measured rather than guessed, and the probe could not say it
until it was told to.** `build_warm_basis` refuses a short count past the cap
and a long count at the same line and neither printed anything, so from
outside the two read the same. The mapped basis arrives **short by a constant
5.6% of rows** — 681, 1357 and 2720 on `fome11`/`fome12`/`fome13`, a family
that doubles exactly, and the shortfall doubles with it. netlib's worst is 596
(`dfl001`). **Neither cap shape reaches that**: the absolute cap would go from
4 to 2720, and 5.6% is 15 times D151's best relative sweep at r = 0.0036. So
the refusal holds on this set for the reason it already had.

Only `fome21` starts warm at all, and its guard does not fire. `plato-pds` is
6.4 hours of wall clock and has not been tried; `plato-nug` is three instances
and unmeasured rather than unsolvable. Refusals table, D151.

**Superseded by D178 above, and kept because its reasoning still holds for
`degen2` alone.** What the repair knows *before* paying for
the attempt is the shortfall `S` and the dimensions — and D151 swept both `S`
and the relative shape `S/nrow` (`bench/measurements/02-60/cap-detail.txt`),
so neither separates. Everything else that distinguishes these two is an
outcome: the guard rejects the repaired trajectory, and that is known only
after the attempt has been paid for.

**So this needs a hypothesis about the mechanism, not another sweep.** A
predictor fitted to 2 instances out of 18 is exactly what CLAUDE.md warns
costs this project weeks, and D46 is the entry that says why a rule read off
a handful of instances is a statement about those instances. Candidate
mechanisms nobody has tested: whether the shortfall positions are logicals or
structurals, how many stored-basic members presolve's mapping dropped, and
how the mapped basis's conditioning compares. Each is measurable at mapping
time; none has an argument behind it yet, and the argument is the missing
part rather than the measurement.

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
- ~~**The solver's own row activity loses terms the way presolve's bounds
  did.**~~ **CLOSED 2026-08-20 (D168, `bench/measurements/02-78/`).**
  `compute_primal` builds `-N x_N` by walking the nonbasic columns in column
  order, so a row that met a large term before many small ones lost the small
  ones outright. On D162's model the reference build `-DJAOS_NO_PRESOLVE` read
  INFEASIBLE where the feasible point is exactly representable, at every removal
  count, which is why D162 had no reference-build disagreement to show. The sum
  is compensated now, with presolve's own Neumaier accumulator; `src/check.c`
  could not be the model because D34 confines `long double` to that file.
  **This is the first change in this class that moves the gate broadly**: 69 of
  netlib's 94 bit-identical, 25 moved, 23 digest changes, the other two sets
  bit-identical, work geometric mean 0.9996x, and `gate: PASS` with 0 regressed
  on all three.
  Two things the entry corrects. **The residuals do not all improve** — `rsub`
  is better on 8 instances and worse on 2, `row` better on 7 and worse on 8 —
  so a more accurate sum is not a smaller residual per instance, and the
  evidence is the constructed model plus no verdict moving on the 139. And
  **the work counter cannot see this change's cost**, because the Neumaier step
  is not billed: the seconds are the only evidence, and across two runs of the
  `-j 1` protocol the four bit-identical instances span 0.9501x to 1.0302x
  while doing byte for byte the same work, which is this host's own 6.27%.
- ~~**`subtract_basis_times` and `apply_flips` are the two uncompensated sums
  left in the solve path.**~~ **`subtract_basis_times` CLOSED 2026-08-20
  (D171, `bench/measurements/02-81/`), and the refusal it overturns is the
  thing to read.** D168 refused it on `numerics-reviewer`'s argument from the
  terms: they are products of `x_B`, an FTRAN output already carrying the
  factorization's error, so no accumulator reaches an error already inside a
  term. **True, and it does not follow** — the residual is what the refinement
  correction is computed FROM, so a term lost there leaves a correction that is
  short. D168's own reopen condition was "a model where the refinement residual
  loses a term that changes a published value", and the sets have three:
  `pds-20` published a column 8.81e-13 outside its own declared bound and now
  publishes one 5.05e-28 outside it, with `pds-10` and `pds-06` the same shape.
  Work 1.0000x on both sets, `0 regressed` on all three.
  **Two things a later session should take from it.** Reading one set nearly
  refused this: netlib moves 88 digests, favours improvement 76 to 7 on
  `rowrel`, and does not move the worst case of any figure — Kennington is
  where the worst case moves. And **the symmetric change on the dual side is
  refused with a measurement**, not an argument: compensating `compute_duals`'
  refinement dot as well changes not one direction count on either set, which
  is also what settles the `gap` column going the wrong way on 51 netlib
  instances. If that were D29's primal-dual inconsistency the symmetric change
  would have moved it.
- **`apply_flips` is the third sum of this shape and stays open.** It
  accumulates over a bound-flip batch into `s->col` and subtracts the FTRAN'd
  result from `x_B`, so mid-solve `x_B` loses terms again after every batch.
  The final `refine = true` refresh rebuilds `x_B` from scratch, so what it
  loses does not reach the answer, which is why D171 left it.
  ~~It needs a third `[nrow]` array, because `s->rhsc` and `s->resc` are both
  live inside the call it runs in.~~ **That is false and was corrected the same
  day** (`numerics-reviewer`): `apply_flips` is called from `dual_ratio_test`
  mid-iteration, with `compute_primal` nowhere on the stack, so both arrays are
  dead then. **The contract that IS true is more useful**: each is `memset` at
  the entry of its single reader and dead at its exit, so neither carries a
  producer contract at all, and a borrower keeping the same discipline is safe.
  What is unsafe is a borrower whose value must survive a call to
  `compute_primal(s, true)`.
- ~~**The objective's product rounding needs a two-product.**~~ **CLOSED
  2026-08-20 (D172, `bench/measurements/02-82/`).** Dekker's split, exact under
  `-ffp-contract=off`, with a `2^996` overflow guard. **109 of 110 published
  objectives agree with `jaos_check_solution` exactly now** — netlib 93 of 94
  against 83, Kennington 16 of 16 against 15 — and nothing moves the wrong way
  on either set, where D169 had four going further at the last bit. `gate:
  PASS`, 0 digest changes anywhere, 11 netlib and 1 Kennington `obj` figures
  moved and nothing else.
  **The one left is `finnis` at 2.2992e-08, and that is the end of the
  objective's own story.** `(long double) c * x` is not an exact product — a
  binary64 product needs 106 bits and that mantissa holds 64 — so each term
  carries up to `2^-64 |t|`, which over `finnis`'s `sum|t| = 3.2e12` is
  1.73e-07 before `src/check.c` sums anything. The observed 2.2992e-08 is 7.5
  times below that, so the comparison is exhausted. **Do not open the objective
  again without a better oracle.**
  `fma()` was not ruled out by the flag and the distinction is worth keeping
  (`numerics-reviewer`): `-ffp-contract=off` stops the COMPILER contracting
  `a*b+c` on its own and says nothing about an explicit call, which IEEE-754
  requires to be correctly rounded. The split was preferred because it needs no
  claim about libm at all. It costs
  one pass over the columns per solve, so the campaign question is accuracy
  rather than time. Nothing shows it losing a verdict.
- ~~**`finnis` publishes a point that is not the exact optimal vertex.**~~
  **REFUSED 2026-08-20 (D173, `bench/measurements/02-83/`).** The exact
  oracle was built — a 5632-bit fixed-point accumulator, so every `c_j x_j`
  is added with no rounding — and it says there is nothing to repair.
  `finnis` carries `sum |c_j x_j| = 3.198e+12` against an objective of
  1.7e+05, so one eps of its own traffic is 7.10e-04 and **the 7.62e-05 gap
  to Koch is 0.107 of that**. Its row residual is 4.2e-17 and 3.0e-17
  relative to the two rows' traffic of 2e+10. The point is as feasible and as
  optimal as binary64 allows on this model.
  **Two things the entry settles beyond the item.** `jaos_objective` is the
  correctly rounded exact objective of the published point on **94 of 94**,
  worst 0.493 ulp; the checker manages 93 and is **790 ulps** out on `finnis`,
  so D172's "neither can be called more right" resolves in the published
  number's favour. And the instrument was validated twice before any reading,
  which caught a wrong expected value this session had typed — the second
  oracle is Python's `fractions`, sharing no code with the accumulator.
- **`pilot` publishes a point 2.31e-05 above the optimum, and it is the only
  netlib instance every other solver beats.** New from D173. Measured against
  `eps * sum |c_j x_j|`, which is the floor arithmetic sets for that model,
  the gap is **1.87e+08** of that unit; 68 of the 93 Koch-referenced
  instances are inside 0.5. Three others are above 1e4: `pilot87` 1.53e+06,
  `scsd6` 9.97e+04, `etamacro` 2.74e+04. Every row residual is at the
  arithmetic floor on all four, so these points are **feasible and
  suboptimal**, not infeasible, and `pilot`'s objective is higher than Koch's
  on a minimization. HiGHS, SoPlex and Clp all publish Koch's value on
  `pilot`.
  ~~**What it needs before any code**: which tolerance lets the solve stop
  there.~~ **ANSWERED 2026-08-20 (D174, `bench/measurements/02-84/`): it is
  `DUAL_TOL` and nothing else.** Presolve is not involved — the reference
  build publishes the same number — and `PRIMAL_TOL` moves nothing across
  eight settings from 1e-13 to 1e-5. At `dual_tol = 1e-9` all four instances
  improve and no other instance on the set moves materially: `pilot`
  2.312e-05 → -5.266e-09 at 0.9134x work, `pilot87` and `scsd6` to the
  reference **exactly**, `etamacro` to one ulp, three of the four for **less**
  work. `pilot87` is D92's backlog row and it is answered by the same reading.
  **The candidate is lowering `DUAL_TOL` from 1e-7 to 1e-9, and it is not
  landed.** What stops it, measured rather than guessed:
  - **The gate would report `6 regressed`.** Five netlib instances and
    `pds-20` cross the 2.0x per-instance work bar, worst `d2q06c` at 5.32x.
  - **The margin is one step.** 1e-10 fails `dfl001`, 1e-11 fails `wood1p`
    too, and 1e-6 fails `pilot` and `pilot87` outright.
  - **1e-8 is not a cheaper version of it.** It leaves `pilot` at 2.312e-05
    unchanged, so there is no half measure.
  - **The 2.0x crossings are trajectory, not price, and that is measured**:
    the ratios are not monotone in the tolerance (`grow22` reads 2.14x, 3.00x,
    1.49x, 0.22x, 0.22x across 1e-6 to 1e-11), and `grow22` and `d2q06c` both
    cross 2.0x when the tolerance is **loosened**, where the answers get
    worse. That is an argument for reading the six individually, not for
    ignoring the bar.
  **The narrower candidate nobody has built**: a tighter tolerance for the
  termination test alone — `dual_breach` and `settled_dual_violation` — with
  the Harris window left at 1e-7. It would refuse to stop early without
  changing the pivot selection. Nothing says it costs less; the way to find
  out is to build it and re-run 02-84's sweep against it. Read D92's own
  entry first: substituting `published_breach` for `dual_breach` in the
  clean-up predicates already cost `pilot87` its bound, for 2.9x the work.
- **The gate cannot see a suboptimal answer, and the library already
  certifies one.** New from D173. **The reach half closed 2026-08-24 (D177,
  `bench/measurements/02-89/`) and the threshold half did not.**
  `RSUB_FLOOR` was `1e-9`, and at that value the predicate below watched **4
  solves out of 110** and none of Kennington's 16 — that set's worst `rsub` is
  `4.18e-14`. It is `1e-16` now and watches 84. The floor's stated reason was
  refuted by D171's own committed numbers: it moved 88 of 94 digests and moved
  `rsub` by at most 1.688x, so nothing would have fired at any floor at all.
  **The zero point is still the baseline and that is the part still open.**
  Three candidate bars are measured in 02-89 and every one of them turns the
  gate red at HEAD, which is item 1's judgement rather than this item's:
  `rsub` itself separates best (343x, top clean instance `wood1p` at 7.4e-09);
  `gap_positive` unnormalised **does not even order correctly**, because
  `ken-18` is a clean answer carrying a larger bound than `pilot87`'s; and
  `gap_positive / (eps * sum|c_j x_j|)` loses to `rsub` at 199x, so the
  denominator was never the problem. **The strongest route is not the
  certificate at all**: `objective_accepted`'s window tightened from `1e-6` to
  `1e-9` catches `pilot` with zero false positives on all 94, and the
  population is bimodal — 88 instances at or under `6.84e-16`, which is the
  reference's own last digit, then nothing until `modszk1` at `2.8e-13`. Any
  window in that band gives the same answer, so it is not fitted to `pilot`.
  It turns the gate red on `pilot` today.
  The original entry, for what the numbers below mean:
  `jaos_check_solution` reports
  `gap_positive = 0.0386` on `pilot` with `gap_certified = yes`, which is a
  proof that `P - P* <= 0.0386`; among the 53 instances whose certificate is
  complete that is **four orders above the next** (`grow22`, 1.797e-06), and
  it belongs to the one instance every other solver beats.
  **What the gate does with that is narrower than this entry first said, and
  the correction changes what the item needs** (audited 2026-08-21).
  `bench/run.c` records `gap_positive` as `Q=` on every line, and
  `relative_suboptimality` is a **regression predicate**: an instance whose
  `rsub` passes `RSUB_FLOOR = 1e-9` and grows by `RSUB_REGRESSION_FACTOR = 2.0`
  against its baseline is reported REGRESSED in its own message. So the
  instrument exists and is wired in. **Its zero point is the baseline**, and
  `pilot`'s suboptimality was already there when the baseline was written, so
  a quantity that is permanently wrong reads as permanently fine. The
  objective test beside it is `|got - ref| <= 1e-6 * max(|ref|, 1)`, a window
  of 5.57e-04 on `pilot`, cleared with 24 times to spare.
  **What it needs**: a threshold, and one instance separating cleanly on one
  set is not one. `scsd6` is the counter-case and it is already in the record
  — `gap_positive` 9.73e-15 where the true gap is 1.12e-09, five orders
  smaller, with `gap_certified = no`. Read `jaos.h`'s own warning on
  `certified_suboptimality` (D73) before proposing any predicate here: a
  quantity that reads the same on a wrong answer as on a right one cannot be
  a verdict.
- ~~**`settled_objective` in `src/simplex.c` is a fourth accumulation of the
  same shape.**~~ **CLOSED 2026-08-21 (D175, `bench/measurements/02-85/`).**
  Compensated, with Dekker's split, and the two steps moved out of
  `src/model.c` so `jm_model_publish_objective` and this share one copy.
  The reviewer's model works exactly as described and is
  `02-85/two-points.c`: naive 0.0 for both of two points whose true objectives
  are 0 and 256. **The order is the mechanism** — the `-1e16` column has to
  arrive last, or the cancellation happens first and the small terms survive.
  **No solve on the three sets reaches it**: 304 comparisons, 0 verdict flips,
  220 exact ties every one of which is a point against itself, and an
  informative population of 4. `gate: PASS` with `bench/results/*.txt`
  byte-identical.
  **Two things to carry forward.** There is **no test that fails at the
  parent** and none was written: the separating state needs a settling loop
  holding two distinct tying points, no model steers a solve there, and
  `tests/` cannot call a static. A test that passed either way would be worse
  than none. And **three defects in the measurement each produced output that
  looked clean** — a probe that read one set three times while printing three
  names, a probe that measured the repaired tree and reported every error as
  exactly 0, and one figure printed under one name with two definitions.
- ~~**presolve's `obj_offset` is still accumulated naively.**~~ **REFUSED
  2026-08-21 (D176, `bench/measurements/02-86/`), and the class D168 opened is
  closed at all four sites.** The value is measurably dead: replacing the whole
  reduced offset with `1e300` and then with `NaN` leaves all three sets
  bit-identical to a control that itself reproduces the committed records
  exactly. Nine runs, `gate: PASS` on every one. **The control is the finding**
  — the first harness omitted `-e infeasible` and the infeasible records
  differed under both poisons AND under the control, which without the control
  reads as "alive on one set, dead on two". Removing the four sites is a
  separate question and has no measurement either way.
  The original entry, for the reopen condition: nothing reads it for the
  answer since
  D169 — both postsolve paths sum the published values instead — but the
  simplex's objective on the reduced model still starts from it.
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
| netlib, at D139 | 188 | 140 | 48 (26%) | 23 | 0 |
| netlib, **now (D167)** | 188 | **142** | **46 (24%)** | **18** | **0** |
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

**The D139 row was stale before this session began, and nobody noticed for a
day** (D167, `bench/measurements/02-77/`). Re-running 02-48's probe — the
instrument that owns this number — at three trees:

| tree | exact | WRONG | worst | sum |
|---|---|---|---|---|
| recorded as D139 | 140 | 48 | +23 | +272 |
| `4c5f58f`, this session's start | 142 | **46** | +23 | +262 |
| `cd68630`, before the compensation | 142 | 46 | +23 | +262 |
| HEAD | 142 | 46 | **+18** | **+250** |

So two solves were fixed by the previous session (D140–D161) and the table was
never re-read. **Nothing in the gate can catch that**: `bench/run.c`'s `basis=`
is a hash, so it detects a change and never reports a count, and the count only
exists when 02-48's probe is run by hand.

**D165 moved the worst and the sum, not the count.** `4c5f58f` and `cd68630`
are identical, so D162–D164 changed nothing here. The compensation makes four
instances' bases different — `bandm` +23 → +18, `czprob` +3 → +1, `finnis`
+1 → +2, `capri` unchanged — and **by the measure this table insists on, 46
before and 46 after is not progress.** Raised by `jaos-measurer` judging D165,
which found `bandm` at 18 and said four instances cannot replace the cell.

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

1. **46 netlib solves still publish a wrong count, and every local repair is
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
   table), or accepting the residue — whose whole price is 46 solves losing
   their warm start, which item 4 below measures.
   **D179 measured what a wider rule would have to work with**
   (`bench/measurements/02-91/`). The model-wide supply of basic variables
   resting exactly on their own bound covers the over-count on **19 of the 24
   instances**, against a within-row rule that had nothing on 66 of 80 firings.
   It does not close the item: `fit1p` (21 over, 0 candidates) and `share1b`
   (2 over, 0) have no candidate at any tier down to 1e-9 relative, and `bandm`
   has 8 against 18. **So the rank argument is now the whole of the work** —
   a candidate is necessary and not sufficient, because demoting the only
   column covering a row makes B singular, and postsolve has no factorization.
   The probe also reaches 24 instances where 02-48 reaches 48 solves, on code
   the two share none of.
   **And the price is far higher on a modern set than on netlib** (D181,
   `bench/measurements/02-93/`). On `plato-fome` the three instances that
   publish a wrong count are exactly the three whose warm re-solve does
   bit-identical work to the cold one, and `fome21`, the one that publishes an
   exact count, saves **47% of the work**. Three against three, one against
   one. `fome13` is over by **53**, larger than netlib's worst of 21
   (`fit1p`). The published over-count grows FASTER than the model on that
   family — 8, 21, 53 as it doubles twice — while the mapped shortfall behind
   the lost warm start grows exactly linearly, so the two are different
   objects and this is the only family here where their rates can be read
   apart.
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
| §4, end | how often `plato` should run — `pds` alone is 6.4 hours of wall clock. **`nug` is measured now (D182)**: `nug08-3rd` solves in 730 s and 294654930775 work units, `nug20` does not finish in 3600 s and `nug30` not in 1800 s. Neither is known unsolvable; both are known not to finish in the time they were given. Whether a set of one enters the record is open, and a set of one instance is a statement about one instance (D46) |
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

**§3 now depends on this section too, and it is not one of the four above.**
D178 left `degen2` as the only instance where D148's guard throws a repaired
warm trajectory away, and one instance cannot supply a threshold. A second
instance of that mechanism is what §3 needs, and a wider model population is
the only place one comes from.

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

**`nug` is no longer unmeasured (D182, `bench/measurements/02-94/`).**
`nug08-3rd` solves — 34424 iterations, 294654930775 work units, `checker=ok`,
`det=ok` — and `nug20` and `nug30` do not finish in 3600 s and 1800 s. The
family is not ordered by rows: `nug20` has 4488 FEWER rows than `nug08-3rd`
and does not finish in five times the time. So the set is not practical and
one instance of it is usable, which is a third answer.
**And presolve removes NOTHING on it** — not a row, not a column, not a
nonzero. Across the committed records its reach is a median of 9.04% of rows
on netlib, 12.57% on Kennington, 2.93% on `plato-pds`, 0.00% on `plato-fome`
and 0.00% on `plato-nug`. That is this section's own argument in JAOS's
numbers, and it is NOT a matched comparison against Galabova's 1.10-against-1.67
— reach is not speed-up, and that set is Mittelmann plus four industrial
models rather than these.

**Also open:** how often `plato` should run — `pds` alone is 6.4 hours of wall
clock — and `nug20`/`nug30`, which are unmeasured rather than unsolvable.
`nug` also turns out to have **no row removed by any family** on all three
instances (`bench/measurements/02-26/counts/nug.txt`), which is its own
question and is not asked anywhere yet.

## 5. After presolve — the rest of M2, in order

### 5a. ~~186 loans go missing, and nothing bounds one~~ — CLOSED (D123 to D128)

**This heading read OPEN until 2026-08-24 and the section had been closed for
days.** The closure is under "§5a is closed. Nothing in the shift machinery is
open." earlier in this file, with the six entries and what each measured. The
loans were never missing: the tally added them one way and the repayments
another (D124). Four refusals to one repair, and every refusal came from
measuring rather than from reading.

Two things are still true and unrepaired, both with their size on the record
and neither blocking anything: the dual step is computed from an unclamped `d`
(248 netlib picks, 170 Kennington, worst 8.37e-09, refused as D127 because the
perturbation is what keeps `pilot87` moving), and D121's loan of 1e32 on a cost
of one stays reachable through D118's refused presolve candidate, which no
instance in the gate reaches.

What follows is the section's own history, kept because five explanations are
closed by measurement in it and re-deriving them costs a session.

Opened by D119, narrowed by D120, located by D121 and half-repaired by D122,
all on 2026-08-18 (`bench/measurements/02-29/` and `02-30/`).

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

Proposed order: cheap breadth first (~~write MPS, write LP, write a solution
file~~ — all three landed 2026-08-31, D226 — then Python bindings,
sensitivity and ranging, infeasibility certificates), then primal simplex,
then barrier with crossover, then MILP, then QP/conic/NLP/MINLP.
VIPR-format certificates are a cheap differentiator — only SCIP 10.0 emits
them and JAOS already ships a checker. For exact rational verification, GMP
is excluded (D11); the methods to weigh are iterative refinement, interval
arithmetic in double, or hand-rolled rationals for the final basis only.

**The one thing D226 left open: `jaos_read_lp` does not read a ranged
constraint.** That is what keeps `jaos_write_lp` at `partial`. All three of
its refusals close at once if the reader learns the form — a ranged row
directly, a free row and an empty row because both become writable once a
row may carry two bounds. 37 of the 139 gate instances are refused today, 34
of them for an empty row (`bench/measurements/02-138/lpcover.txt`). It is a
change to a reader and it was not made with the writer, deliberately: the
writer's contract is that what it writes reads back, and widening the reader
to make more writable is a separate question with its own dialect decision
in `docs/format-support.md`.

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
| D211 | a stop rule on the phase-1 objective rising — `pilot-ja` rose 25.0449 above its running minimum and still finished `ok` | **EXPIRED at D212, caught 2026-08-28**: `pilot-ja` rises 3.3348e-12 now and the largest rise on any `ok` solve is 9.36752e-10, so a threshold has a window about nine orders wide. Attributed to the commit in D215, `02-130/`. Open work below, and the line has left `bench/refusals.txt` |
| D184 | `can_move`'s product-against-rate units, measured dead on the dual (94/94 digests) | **CLOSED 2026-08-28**: the reopen condition was met on 2026-08-25 and the question is settled. `can_move` reads `breached` now (D214, `02-129/`), and the line has left `bench/refusals.txt` |
| D76 | `restrict` in the LU kernels — refused because seconds could not resolve it | an instruction count can (`tools/icount.sh`); re-tested on the kernel signatures 2026-08-26, `bench/measurements/02-119/` (D206) |
| D61 | inlining the hot LU calls — 0.997x, unresolvable in seconds | `tools/icount.sh` moves by more than 0.3% on the LU-dominated instances |
| D36 | the scatter-form BTRAN — the saving is real and the arithmetic is not free | an instruction count of a re-ported candidate retires fewer instructions than it adds; the candidate is not on disk |
| D156 | the destroyed row width as a defect — it dies below one ulp of the activity | a width dies above one ulp; D164's pin is what fires |
|---|---|---|
| D176 | compensating presolve's `obj_offset` — the reduced model's offset is measurably dead: poisoned with `1e300` and with `NaN`, all three sets stay bit-identical to a control that reproduces the committed records exactly | anything reading the reduced model's objective — a progress callback carrying it, a presolve statistic reporting it, or a postsolve path that adds `reduced.obj_offset` instead of recomputing on the caller's model. `bench/measurements/02-86/run-poison-offset.sh` is the test for that condition |
| D173 | `finnis` publishing a point that is not the exact optimal vertex — its 7.62e-05 gap to Koch is **0.107 of `eps * sum |c_j x_j|`**, the floor arithmetic sets for a model carrying 3.198e+12 of traffic, and its row residual is under one eps of each row's own traffic | a model whose gap exceeds that floor. Four already do and they are open items rather than refusals: `pilot` 1.87e+08, `pilot87` 1.53e+06, `scsd6` 9.97e+04, `etamacro` 2.74e+04. The oracle is `bench/measurements/02-83/run-exact-objective.sh` and it needs no build of its own |
| D149 | the blanket warm count repair, retried behind the certificate guard — correct now (`disagreed=0, rejected=0`) and refused on cost: `dfl001` at 172x work for a doomed 596-short repair, netlib geomean 0.2605 vs 0.2553 | **condition MET by D151**: the cap was swept on both sides and the capped repair landed at 4. This row stays as the record of the refusal and its expiry |
| D151 | the instances that still lose real work behind the cap. **The two-instance framing is refuted by D178**: only `degen2` is D148's guard, at a settled dual violation of 12.91, and `scsd1`'s guard never fires — it runs 314 iterations against cold's 89 and is a separate question. Current ratios are 3.6751x and 3.7165x, not 4.09x and 4.65x | a rule that predicts a doomed trajectory before paying for it. **The shortfall cannot be that rule and this is measured**: both are short by 1, the same shortfall as the sixteen instances that win. Raising the cap is separately refused — the sweep in `bench/measurements/02-60/` reads 15.48x on `greenbea` at 7 |
| D145 | the warm count repair in `build_warm_basis` — refused because 8 netlib solves published a wrong objective through the termination hole | **condition MET by D148** (the certificate guard landed), retried as D149 and refused again on cost. This row stays as the record of the refusal and its expiry |
| D142 | a count guard in `jm_model_remember_basis` — the premise "build_warm_basis already rejects it" is false: it counts the MAPPED basis, and clearing the stored publish costs `capri` and `fffff800` their warm starts (1→273 and 7→945 iterations) for nothing any consumer reads | a consumer of `start_*` appears that reads the orig-space count as a claim, or warm starting stops going through presolve's mapping. The candidate and its validated test are at `bench/measurements/02-51/remember-guard-candidate.diff` |
| D141 | a within-row demotion for the published-basis residue — 152 of the 232 declines have no basic column of the row at a bound, and the snap for the 80 breaks the row-bound exactness 02-49 measured (74 of 80 exact) | a demotion design whose candidate set is wider than the firing row AND that carries a rank argument for the demoted member; the fallback in the published shape (Galabova 2023) is accepting the residue |
| D101 | duplicate rows, duplicate columns, dominated columns — 0.15% left to remove on these 139 models | a model population where `bench/measurements/02-07/`'s counter reports a non-trivial share. The condition is executable, not a matter of opinion. Three pieces of the work have no published source and would have to be derived with their own tests |
| D97 | bound tightening — INFEASIBLE on models with an optimum, six designs | **first precondition met 2026-08-17 (D114)**: the over-tightening is derived — a forcing window scaled by the activity certified 5.86 of slack as zero, and the design requirements for a retry are in `bench/measurements/02-21/`. What remains: a dual postsolve for an imposed bound; then only under a campaign. **The condition is unchanged and the prize is not**: doubleton substitution needs the same machinery, and it is 8.55% of netlib's live rows and 29.36% of Kennington's, of which 19 rows in total can be built without it (§3). D97 weighed this feature alone; it now unlocks two |
| SPECS §3 | crash basis — destroys the exact slack-basis steepest-edge weights | pricing stops starting from exact steepest-edge weights; REQ-devex-pricing landing is the trigger. **Checked against §0 on 2026-08-25; the answer depends on a rule not yet chosen.** Exact primal steepest-edge weights also start exact at `B = -I`, which would strengthen this refusal. Devex weights are 1 at any reset whatever the basis, which would not touch it. `docs/research/primal-simplex.md` §3 recommends Devex first, so re-read this row once the pricing rule is decided |
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
| D92/backlog | `pilot87`'s suboptimality bound, not understood | it blocks a gate (trigger already recorded). **D173 gave it a number and D174 gave it a cause**: the point is 1.04e-07 above Koch's optimum, 1.53e+06 times the floor for that model, and at any `dual_tol` from 1e-8 to 1e-13 it publishes the optimum **exactly**, every one of those settings for less work than today. What blocks the change is the whole set, not this instance — see §4 |
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
- ~~The `REFACTOR_EVERY` 16..256 trajectory sweep is manual.~~ **CLOSED
  2026-08-24 (D180, `bench/measurements/02-92/`).** It is
  `run-refactor-sweep.sh` now and takes its settings as arguments; each one
  gets its own tree and its own binary, and the run aborts if two share an md5.
  Three of M1's four defect closures came from running it by hand and D119 is
  the fourth — `pilotnov` under D118's candidate is right at 16 and 29% wrong
  at the shipping 64, on the same reduced model
  (`bench/measurements/02-28/sweep-refactor.txt`).
  **Run at HEAD it found no defect and two other things.** No answer changes
  verdict at any of six intervals, 94 netlib and 29 infeasible each time. 64 is
  not the work minimum — 32 is 8.6% better on the geometric mean and costs
  `grow15` 2.819x and `pilot87` three orders of accuracy — so the constant is
  refused a change and carries its sweep now, in the source and in
  `docs/tolerances.md`. And `pilot` publishes Koch's optimum exactly at three
  of the six intervals with no tolerance touched, which is item 1's material
  and is left there.
- Test ceilings drift silently — the `<62000` one drifted 2800 units with
  nothing watching. Re-measure a ceiling's both sides when touching its
  subject.
- **`pilot87`'s suboptimality bound is understood in mechanism as of
  2026-08-25 (D183, `bench/measurements/02-95/`), and not in magnitude.**
  **Its dual solution is not unique.** At `REFACTOR_EVERY` 8 and 256 it
  publishes the identical objective and two different digests, and splitting
  them says which half moved: the priced primal answer does not move — 738 of
  the 987 columns that move cost exactly zero, and the other 249 move by at
  most 4.44e-15 — while 1817 of 2030 duals move, **166 by more than 1e-9
  relative**, largest relative move 55.7%, and none changing sign. So both
  dual solutions are feasible. `gap_positive` is built from the duals and
  follows them (0.00139018 against 0.00140689), and `unquantified_rays` does
  too (10 against 14). **The bound moving is a property of the model rather
  than a defect in the bound.**
  **What is NOT established**: D92's variants moved it 0.0068–26.7, a factor
  of 3900, and these two settings move it by 1.2%. The magnitude is not
  reproduced and those variants are not in this tree. The original deferral
  and its trigger stand: it re-enters the plan if it blocks a gate, and it
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
