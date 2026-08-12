---
phase: 01-candidate-admission-in-the-ratio-test
plan: 04
subsystem: measurement — the J=1 time ratio and the callgrind re-read
status: complete
tags: [timing, j1, geomean, callgrind, truss, d45, d46, d17, d-11, d-12, d-13, inconclusive]

requires:
  - "01-01 — the bitmap walk whose seconds this measures"
  - "01-02 — the work charge, which is accounting and moves no seconds"
  - "01-03 — the settled record, and its warning about which instances the change bites"
provides:
  - "a J=1 same-instance time ratio over the standard set: geometric mean 0.9709x, ratio of totals 0.9847x"
  - "the verdict against 4.2%: INCONCLUSIVE"
  - "truss at 0.9759x, and the 9 instances that move against the mean, named"
  - "callgrind on truss, both binaries in one session: admit_candidate 14.79% -> 14.20%, and PROGRAM TOTALS UP 1.60%"
  - "this reading's own repeatability, measured the way D81 measured its 1.4%: 6.27% over 86 instances, 8.67% over the 25 the clock can see"
  - "the derivation of 4.2% traced to its source: the 1.4% is D81's, not D83's"
affects:
  - "01-05 — D93 is a refusal-shaped entry, not a gain, and it has a second finding to carry: the change costs instructions and may buy nothing"

tech-stack:
  added: []
  patterns:
    - "the parent is built from `git archive <sha>` into scratch rather than from a worktree — the main tree never moves, and `git worktree list` is untouched"
    - "the identity control is not the sha256 of the two binaries but each binary reproducing the 94 work counts committed for its own source tree"
    - "a paired-by-round estimate beside the pooled minimum: within a round the two binaries run adjacent in time, so it survives a drifting host that the pooled minimum does not"

key-files:
  created:
    - .planning/phases/01-candidate-admission-in-the-ratio-test/01-04-SUMMARY.md
  modified: []

decisions:
  - "VERDICT: INCONCLUSIVE. Every reading of the data lands between 2.2% and 4.1%, and none reaches 4.2%. The verdict does not depend on the choice of estimator, which is the only reason it can be stated at all on a host this noisy."
  - "The three-round protocol was extended to six with the order reversed. Control 4 showed the same binary against itself reading 0.9579x across rounds, and rounds 1-3 gave 0.9865x against rounds 4-6's 0.9624x — an order artifact the size of the effect being measured. Three candidate-first rounds alone would have reported 1.35%."
  - "Callgrind says the change costs 1.60% MORE instructions on truss. admit_candidate sheds 198.8M and the caller it is inlined into gains 994.9M — a relocation that costs five times what it saves. This is the finding the plan's 'relocation, not a saving' test was built to catch, and it caught it."
  - "The 4.2% threshold is three times a 1.4% repeatability that belongs to D81, not D83. D83's 1.4% is Clp landing within 1.4% of HiGHS on total time, a different quantity entirely. D93 must cite D81."
  - "No wall-clock figure entered bench/results/*.txt or any baseline, checked by grep and by `git status --short bench/` after every step rather than assumed."

metrics:
  duration: "~75 min, of which ~47 min was machine time under WSL (12 timing passes, 2 callgrind runs, 3 builds, 1 preflight)"
  completed: 2026-08-12

actuals:
  tokens: 7400
  tasks: 3
  commits: 1
---

# Phase 01 Plan 04: The J=1 time ratio, and the verdict Summary

The change is faster on this host by somewhere between 2.2% and 4.1% depending
on which defensible reading you take, the threshold is 4.2%, and the machine's
own repeatability is 6.27% — so the honest answer is that the measurement
cannot reach the bar, and a second instrument says the change may not be a
saving at all.

## VERDICT: INCONCLUSIVE

Against D-13's threshold of **4.2%** — three times the harness's measured
repeatability of 1.4%. Below that the phase does not close with a yes.

Per the developer's pre-authorisation of 2026-08-12, taken before execution
began: **on INCONCLUSIVE the code stays in the tree.** That was the explicit
choice, taken over the alternative of reverting. D93 states plainly that it
does not pay, what it cost to establish that, and that the structure was kept
anyway with this authorisation named.

## The figure, and it is not a ratio of totals

`geomean.py --pairs`, over the minimum of six `J=1` rounds per instance per
binary:

```
  instances averaged   86
  GEOMETRIC MEAN       0.9709x     <-- this is the result
  best  seba           0.7500x
  worst modszk1        1.0588x
  ratio of totals      0.9847x     <-- NOT the result (D46)
```

**0.9709x — a 2.91% improvement.** The ratio of totals beside it reads 0.9847x
and is not the answer: `pilot87` and `maros-r7` are 74% of this set's work and,
exactly as `01-03` predicted, they are where this change is invisible.

**`truss`, on its own line: parent 1.496s, candidate 1.460s, 0.9759x.** It is
where the cost was found and it does not decide alone.

### 86 of 94, and why the other eight carry no ratio

The runner prints seconds to the console as `%8.3f`, so a millisecond is one
quantum. Eight instances print `0.000s` under both binaries and there is no
ratio to take: **`adlittle afiro blend kb2 recipe sc105 sc50a sc50b`**. They
are excluded and named rather than floored at half a quantum, which would be
inventing data.

The resolution problem does not stop there. Of the 86 that remain, 42 solve in
under ten quanta and read **exactly 1.0000x** — their minima land on the same
millisecond under both binaries. So half the geometric mean is made of
instances that could not have shown a difference of this size whatever the
truth was.

| parent minimum | instances |
|---|---|
| 0.000s — no ratio exists | 8 |
| under 10 quanta (<0.01s) | 42 |
| 10–50 quanta | 19 |
| 50–500 quanta | 15 |
| 500+ quanta (>=0.5s) | 10 |

## Six readings of the same data, and none of them reaches 4.2%

The verdict is stated with confidence only because it does not depend on which
estimator is chosen. The plan's protocol figure is the first row.

| reading | instances | geomean | improvement |
|---|---|---|---|
| **pooled minimum, all rateable — the protocol figure** | **86** | **0.9709x** | **2.91%** |
| pooled minimum, parent >= 0.05s | 25 | 0.9687x | 3.13% |
| pooled minimum, parent >= 0.5s | 10 | 0.9784x | 2.16% |
| paired by round, all rateable | 86 | 0.9744x | 2.56% |
| paired by round, parent >= 0.05s | 25 | 0.9593x | **4.07%** |
| paired by round, parent >= 0.5s | 10 | 0.9630x | 3.70% |

The largest is 4.07% against a threshold of 4.2%. The spread between readings —
1.9 percentage points — is itself wider than the gap to the bar.

## The controls, run before any ratio was believed

Each of these can fail, and one of them changed the protocol.

| control | result |
|---|---|
| 1 — same instances every pass | 94 in all 12 passes |
| 2 — iteration counts identical across all 12 passes | **0 moved.** The trajectory is unchanged by construction, so a moved count would mean the wrong binary was timed |
| 3 — work deterministic per binary, never higher under the candidate | 85 instances charge less, 9 unchanged, **0 higher** |
| 3b — each binary reproduces the work committed for **its own** source tree | candidate **94/94** against `HEAD:bench/netlib.baseline`, parent **94/94** against `64efcc6:bench/netlib.baseline` |
| 4 — the same binary against itself across rounds | **it failed the assumption the protocol rests on.** See below |

**Control 3b is the identity check that matters, and the sha256 comparison the
plan asks for is the weaker one.** The two binaries' hashes differ — but so do
the hashes of two builds of the *same* source in two directories, because `-g`
records the build path, and that is exactly what happened when the candidate
was rebuilt from `git archive`. Reproducing 94 committed integers apiece, on
two different baselines, is a statement about which source tree each binary
came from. The sha256s are recorded anyway:

```
ad8b1828561b7be6eeb8e5bc5094d7ab07a88c13c3c63c783544b7dcfba9d04d  build/bench/run-candidate
b5748494c3d480d66f510352b459e196c63bccb25856d8e602577303c4e13832  build/bench/run-parent
```

### Control 4 is why this plan ran six rounds and not three

The same binary against itself, per-instance geometric mean over the 86
rateable instances, worst of the 30 round pairs: **+6.27%**. Restricted to the
25 instances the clock can actually see, it is **+8.67%**; to the 10 largest,
**+9.68%**. Restricting to substantial instances makes it *worse*, so this is
not quantization — it is the host.

The first three rounds show it as a clean monotone drift:

| pass | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| candidate, summed solve seconds | 84.055 | 83.590 | 82.774 | 84.027 | 88.157 | 84.256 |
| parent, summed solve seconds | 87.613 | 85.324 | 84.140 | 87.369 | 91.105 | 86.939 |

`parent r1 -> r3` reads 0.9655x on the same binary. The machine got 3.5% faster
across three passes and then wandered back: round 5 is the parent's slowest
pass and 8.3% above its own best.

**And rounds 1–3 ran candidate-first every time**, so the parent held the later
— faster — slot in every round. That is a systematic advantage to the parent,
and the plan's alternation does not remove it. Rounds 4–6 were therefore run
with the order reversed:

| | geomean over 86 |
|---|---|
| rounds 1–3, candidate first | 0.9865x |
| rounds 4–6, parent first | 0.9624x |
| **balanced** | **0.9744x** |

**A 2.4 percentage-point gap from running order alone — the same size as the
effect being measured.** Three candidate-first rounds on their own would have
reported 1.35%; three parent-first rounds would have reported 3.76%. Neither is
the answer. This is the single most useful thing this plan learned about
measuring on this host, and it cost 21 extra minutes to learn.

### What the pooled minimum does that the paired estimate does not

Taking each binary's minimum independently and dividing is biased toward 1.0000x
when the two binaries' passes have different spread — and the parent's are
noisier. `pilot87` is the clearest case: its six paired ratios are 0.9541,
0.9896, 0.9972, 0.9874, 0.9435, 0.9834, and its min/min is **0.9972x**, the
worst of the six. `pilot` is worse still: faster in **5 of 6 paired rounds**, yet
its min/min reads 1.0490x because in round 3 the parent hit its own best pass of
the run. Both figures are reported; neither is suppressed.

## Where it bites, and where it does not

`01-03` warned that choosing instances by D46's two names would read D46
backwards. It was right, and the time ratio confirms it directly:

| instance | parent | candidate | ratio | faster in |
|---|---|---|---|---|
| `dfl001` | 10.966s | 10.018s | **0.9136x** | 6/6 rounds |
| `greenbea` | 2.001s | 1.902s | 0.9505x | 5/6 |
| `greenbeb` | 0.945s | 0.899s | 0.9513x | 6/6 |
| `stocfor3` | 5.956s | 5.678s | 0.9533x | 5/6 |
| `d2q06c` | 0.937s | 0.909s | 0.9701x | 5/6 |
| **`truss`** | **1.496s** | **1.460s** | **0.9759x** | **5/6** |
| `25fv47` | 0.442s | 0.437s | 0.9887x | 5/6 |
| `pilot87` | 25.968s | 25.896s | **0.9972x** | 6/6 |
| `maros-r7` | 23.211s | 23.181s | **0.9987x** | 6/6 |

**A time ratio taken on `pilot87` and `maros-r7` alone would have read 0.998x —
no effect whatever**, against work ratios of 0.9956x and 0.9987x that predicted
exactly that. The two instruments agree that these two instances are the wrong
place to look, which is a small validation of both.

### The nine instances that move against the mean, named

Not averaged away. Ratio is min/min; the paired column is what it looks like
when the drift is controlled for.

| instance | min/min | parent | faster in | reading |
|---|---|---|---|---|
| `modszk1` | 1.0588x | 0.017s | 2/6 | 17 quanta — below what the clock can say |
| `perold` | 1.0583x | 0.120s | 2/6 | genuinely mixed |
| `d6cube` | 1.0571x | 0.035s | 1/6 | 35 quanta |
| `pilot` | 1.0490x | 6.305s | **5/6** | **a minimum-estimator artifact**, not a slowdown |
| `fit1p` | 1.0323x | 0.031s | 1/6 | 31 quanta |
| `fit2p` | 1.0322x | 2.269s | 3/6 | genuinely mixed, and large enough to mean it |
| `stocfor2` | 1.0220x | 0.091s | 2/6 | mixed |
| `fit2d` | 1.0182x | 0.055s | 2/6 | mixed |
| `woodw` | 1.0085x | 0.117s | 2/6 | mixed |

Of the 25 instances the clock can see, **not one is slower in all six rounds**,
and 8 are faster in all six.

## What callgrind says, and it is the surprise of this plan

**Callgrind explains; it does not decide.** It counts instructions and not
seconds, it cannot see locality, and locality is the whole of what this change
trades (D-11). Everything below qualifies the verdict above and does not
replace it. A later reader scanning for this phase's evidence should take
0.9709x as the result and this section as the reason to distrust it.

Both binaries were run under callgrind on `truss` alone at `-j 1`, in one
session on one machine. Running the parent too was not in the plan; D84's
14.98% does not record the build it came from, so comparing a fresh number
against it alone would be comparing two unknowns.

| | parent | candidate | delta |
|---|---|---|---|
| `admit_candidate` | 8,060,038,036 (**14.79%**) | 7,861,234,036 (**14.20%**) | −198,804,000 |
| `simplex.c:run` — what the scan is inlined into | 18,714,996,570 (34.33%) | 19,709,931,702 (35.59%) | **+994,935,132** |
| `ftran_prefix` — the nominated control | 3,594,648,962 (**6.59%**) | 3,679,427,488 (**6.64%**) | +84,778,526 |
| **PROGRAM TOTALS** | **54,507,175,480** | **55,377,182,992** | **+870,007,512** |

**The candidate executes 1.60% MORE instructions than the parent.**

`admit_candidate` did fall — 14.79% to 14.20%, and D84's 14.98% is reproduced
by the parent to within 0.19 of a percentage point, so the reading is
comparable to the one it is being read against. But the caller gained five
times what the callee shed. **That is a relocation, not a saving**, and the two
numbers together are exactly what the plan required in order to tell them
apart.

### The arithmetic, which closes to the instruction

On `truss`, `nvar` = 9806 and `nrow` = 1000, so 8806 variables are nonbasic.
`01-03` derived 16,567 dense-branch calls from `work_saved / rows` without
instrumenting anything. Callgrind's line-level counts on the bitmap loop:

```
   15,506,712   for (int64_t w = 0; w < nwords; w++) {
    5,135,770       uint64_t bits = s->nbmark[w];
2,354,634,576       admit_candidate(s, (w << 6) + __builtin_ctzll(bits), ...
  291,778,004       bits &= bits - 1;
  291,778,004       visited++;
```

291,778,004 / 2 = **145,889,002 iterations**, and 16,567 × 8806 = **145,889,002**
exactly. `01-03`'s derived call count and callgrind's measured loop trip count
agree to the digit, by two instruments that share nothing. The parent's side
closes the same way: 16,567 × 9806 = **162,456,002**, which is the call count
D61 recorded for `admit_candidate` on `truss` in a different session.

From there both sides fall out exactly:

- the calls the change removes are **all basic variables**, which is
  `admit_candidate`'s cheapest path — one status load and a return. 198,804,000
  / 16,567,000 = **exactly 12.0 instructions** saved per skipped call.
- the bitmap machinery costs 994,935,132 / 145,889,002 = **6.82 instructions
  more per variable it still visits**.
- so it pays 6.82 on 145.9M visits to save 12.0 on 16.6M skips. **995M paid
  against 199M saved.**

The change was betting that skipping 10.2% of the variables would pay for the
indirection. On instruction count it does not, by a factor of five.

### The controls callgrind supplied that were not asked for

`ftran_prefix` — the control the plan nominated — is the *one* function that
moves without being touched: +2.36% in absolute instructions. Its symbol name
differs between the two builds (`ftran_prefix.lto_priv.0` in the parent, plain
`ftran_prefix` in the candidate), so LTO privatised it differently, and under
`-flto` no function is truly untouched. It is a caveat on the precision of the
whole reading and it is reported rather than smoothed.

The better controls were nominated by the data instead: **24 functions above
1e6 instructions are identical to the digit in both binaries**, including
`shift_to_feasible` at 4,417,992,736, `jm_lu_ftran` at 1,729,911,830 and
`jm_lu_factor` at 794,203,486. A control that cannot change is worth more than
any headline in the same table, and there are twenty-four of them.

## The two numbers cannot both be simply true

Instructions **up 1.60%** on `truss`; seconds **down 2.41%** on `truss` and
2.91% over the set. Three readings of that:

1. The time saving is real and comes from something callgrind cannot see — which
   is the possibility D-11 named in advance as the reason callgrind may not give
   the verdict.
2. The time saving is inside the host's noise, and the honest content of this
   plan is that nothing was measured. The noise floor is 6.27%, larger than
   every reading in the table.
3. Some of both.

**This plan does not claim to distinguish them, and D93 should not either.**
What is defensible is the conjunction: the change is not an instruction-count
saving, it is at best a small time saving, and it is below the bar on every
reading taken. What would distinguish them is a controlled host, which D17 says
does not exist here and Phase 5 already carries as a blocker.

## The threshold's derivation, and a citation that does not hold

The plan requires both repeatability figures recorded rather than one picked
silently. Checked in `DECISIONS.md` rather than carried:

| figure | where it actually is | what it measures |
|---|---|---|
| **1.4%** | **D81**, not D83 | JAOS byte-identical at every comparison rung, cross-rung ratios 1.007x / 1.014x / 1.012x over four separate sessions, iterations exactly 1.000x |
| 1.3% | D60 | the comparison harness, estimated a different way |

**`01-04-PLAN.md` and its `<threshold_note>` attribute the 1.4% to D83. It is
D81's.** D83 does contain a 1.4% — "Clp lands within 1.4% of HiGHS on total
time" — which is a different quantity in a different table, and is the likely
source of the slip. **D93 must cite D81**, and the threshold itself does not
move: 4.2% is what D-13 locked and what this plan measured against. Three times
1.3% would be 3.9%, and no reading here lands between 3.9% and 4.2%, so nothing
in the verdict turns on the reconciliation.

There is a third figure now, and it is the uncomfortable one. D81's 1.4% was
JAOS against itself across four rungs of `bench/compare`. **The same
measurement, made the same way on this harness in this session, is 6.27%** —
and 8.67% on the instances the clock can see. Whatever 1.4% describes, it does
not describe a `bench/run -j 1` sweep of the standard set on this host today.
D93 owes that a sentence: **the threshold this phase was judged against is
three times a repeatability figure four times smaller than the one this reading
actually exhibits.**

## The host, which D17 already refused

This is a Windows host running WSL and D17 says a WSL number cannot close a
gate. A same-session same-machine A/B ratio is designed to survive an
uncontrolled host in a way an absolute number is not — the machine's state
divides out of a ratio taken minutes apart, which is why this protocol exists.
It survived it only partly:

- summed solve seconds for the *same binary* swung 82.774 → 88.157 (candidate)
  and 84.140 → 91.105 (parent) across six passes;
- the worst per-instance round spread was 1.50x, on `forplan` and `scfxm1`;
- the median per-instance round spread was 1.108x;
- running order alone was worth 2.4 percentage points.

Nothing else heavy was launched during the timing. Load average at the start
was 0.13. The 12 passes ran back to back with no other campaign in flight,
confirmed by `preflight.sh` beforehand.

## No wall-clock figure entered the record

Grepped, not assumed, after every step:

- `git status --short bench/` empty after the builds, after the 12 timing
  passes, and after both callgrind runs;
- no `elapsed`, no `time:`, no `N.NNNs` in any of the three baselines or the
  five records;
- `git diff --stat e8c2f58 HEAD -- bench/` empty — the baselines `01-03` wrote
  are untouched.

The runners were invoked with neither `-o` nor `-b`, so no record file existed
for a second to reach. Every figure in this document is a development number
and lives only here.

## Deviations from Plan

### 1. [Rule 3 — the protocol could not do its job as written] Six rounds, three each way, not three

- **Found during:** Task 1, at control 4
- **Issue:** The plan asks for "at least three" alternating rounds with the
  minimum taken. Three were run, and control 4 then showed the same binary
  against itself at 0.9579x from round 1 to round 3 — a monotone drift larger
  than the effect. Because the plan's alternation puts the candidate first in
  every round, the parent held the later and faster slot every time.
- **Fix:** three further rounds with the order reversed, and the pooled minimum
  taken over all six. The order effect is then measurable rather than assumed
  small, and it turned out to be 2.4 percentage points — the size of the whole
  effect.
- **Why this is Rule 3 and not scope creep:** the plan's own instruction is to
  take a ratio that means something, and "at least three" is a floor. Reporting
  0.9865x from three candidate-first rounds would have been reporting an
  artifact.
- **Cost:** 21 minutes of machine time.

### 2. [Rule 2 — a stated limitation instead of invented data] The geometric mean is over 86 of 94

- **Found during:** Task 1, first attempt at the geometric mean, which died on
  `math domain error`
- **Issue:** eight instances print `0.000s`. The console field is `%8.3f`.
- **Fix:** excluded and named. Not floored at half a quantum, which would put
  eight fabricated ratios into the answer. The 42 further instances reading
  exactly 1.0000x are reported as what they are — a resolution artifact that
  pulls the mean toward 1 — and the two restricted subsets are given beside the
  full figure so the reader can see it does not change the verdict.

### 3. [Rule 2 — a control the plan did not require] Callgrind was run on both binaries

- **Found during:** Task 2
- **Issue:** D84 records `admit_candidate` at 14.98% without recording the
  build, the valgrind version or the libc it was taken against. A single fresh
  number compared against it is a comparison of two unknowns.
- **Fix:** the parent was profiled in the same session under the same valgrind.
  It reproduces D84's figure to within 0.19 of a percentage point, which is what
  licences the comparison — and it is what made the +1.60% program total
  visible, since that finding exists only in the difference between two runs.

### 4. [Process — a tool this executor does not have] `jaos-measurer` was not spawned

- **Found during:** Task 3
- **Issue:** Task 3 requires spawning the `jaos-measurer` subagent to return the
  verdict. **This executor's context provides no agent-spawning tool** — only
  file, shell, skill and teammate-messaging tools. The subagent could not be
  invoked.
- **What was done instead:** its checklist was performed inline, in its own
  terms, and every item is evidenced above rather than asserted:
  1. iteration counts confirmed identical across all 12 passes **before** any
     timing number was read — control 2, and the run would have stopped there;
  2. the reported figure is `geomean.py`'s geometric mean of per-instance
     ratios, quoted with the ratio of totals beside it and labelled as not the
     result;
  3. `truss` on its own line;
  4. every instance moving against the mean named individually, with the
     paired-round reading beside each so that a minimum-estimator artifact
     (`pilot`) can be told from a genuine one (`fit2p`);
  5. the verdict read against 4.2% with both repeatability figures recorded.
- **What is genuinely lost:** an independent reader. The value of that subagent
  is that it refuses to conclude from a summary line, and here the same context
  that produced the numbers also judged them. **The team lead has been asked to
  run `jaos-measurer` over this document** for that independent read; the
  verdict is not expected to move, because it does not depend on the estimator,
  but the check is worth having and its absence is recorded rather than papered
  over.

### 5. [Rule 1 — a wrong citation in the plan] The 1.4% belongs to D81

- **Found during:** Task 3, checking the derivation rather than repeating it
- **Issue:** `01-04-PLAN.md` attributes the 1.4% repeatability to D83 twice.
  `DECISIONS.md` puts it in D81. D83 carries a different 1.4%.
- **Fix:** recorded above, and flagged for D93. No number moves.

### 6. [Process] `git archive`, not `git worktree`

The parent was extracted with `git archive 2b07de1 | tar -x` into scratch, and
the candidate the same way from `HEAD`, so that both binaries came from an
identical build procedure in one session and neither was a leftover. `make
bench` in the main tree said "nothing to be done", which is correct — and a
binary `make` declines to rebuild is the exact shape of D82, so the procedure
was equalised rather than argued about. **No worktree was created**, `git
worktree list` is as it was found, and `main` never left `b42015f`.

### 7. [Scope] The phase requirement stays Pending

`REQ-ratio-test-candidate-admission` turns on D93, which is `01-05`'s and does
not exist. Roadmap criterion 3 is what this plan closes, and it is annotated in
`ROADMAP.md`.

## Which commit is the parent, and why

`2b07de1` — the commit immediately before `f2ed4bc`, this phase's first
implementation commit. Its source tree is the pre-phase one.

The phase's two source edits are `f2ed4bc` (the bitmap) and `b65d9f2` (the work
charge). **A time ratio wants the pre-phase tree**, not the commit between them:
`b65d9f2` changes what the counter bills, which is accounting and moves no
seconds, so taking `f2ed4bc` as the parent would have measured the same code
against itself on the clock while looking like a controlled comparison.
`2b07de1` itself touches only `.planning/`, confirmed by `git show --stat`, so
the parent's source tree is `64efcc6`'s and the two committed baselines that
control 3b checks against line up with the two binaries exactly.

## Known Stubs

None. This plan writes no code and modifies no repository file. `files_modified`
was empty by design and is empty in fact.

## Threat Flags

None. No network endpoint, auth path, file access pattern or schema at a trust
boundary. Nothing was installed; `valgrind 3.22.0` was already present and was
checked with `valgrind --version` before Task 2 ran, as its precondition
requires.

The register's four entries and where each was enforced:

| | |
|---|---|
| **T-01-09** — measuring one binary twice | Enforced three ways, and the weakest is the one the plan named. sha256s differ; iteration counts agree on 94 instances across 12 passes; and each binary reproduces the 94 work counts committed for **its own** source tree. The middle and last would have caught a wrongly-built pair that the sha256 alone would not, since two builds of the same source in two directories also differ. |
| **T-01-10** — a parallel or warm-started run | `-j 1` on every one of the 12 passes and both callgrind runs; no `INFLATED` line in any log, checked by grep. A warm re-solve would have shown as a moved iteration count, and none moved. |
| **T-01-11** — wall-clock leaking into the record | `git status --short bench/` empty after every step; no wall-clock pattern in any baseline or record; the runners were given neither `-o` nor `-b`. |
| **T-01-SC** — supply chain | Accepted and unchanged. Nothing installed, no fetch path touched. |

## Estimate vs actuals

The plan estimated `tokens: 70000` at `confidence: low`. The realized diff is
this document plus two small `.planning/` edits — **7,400** on the chars/4 scale
ADR-2629 specifies, against an estimate of 70,000. **An order of magnitude
over**, and for the reason `01-03` identified from the other direction: this
plan's cost is machine time, not diff size. It modified no repository file by
design, so its diff was always going to be one summary.

47 minutes of WSL machine time is what it actually cost, and no field of the
estimate models that. A plan whose deliverable is a number will always look
free to a token estimate and never is.

## Self-Check: PASSED

Eleven checks run against disk after the summary was written, reading the
sources rather than this document:

1. `01-04-SUMMARY.md` present at the path the plan names.
2. Exactly **1** line matching `^## VERDICT: INCONCLUSIVE$`, and **0**
   occurrences of `VERDICT: ACCEPT` or `VERDICT: REJECT` — the acceptance
   criterion asks for exactly one of the three.
3. The artifact contract's required string `geometric mean` present.
4. `git status --short bench/` **empty**; `git diff --stat e8c2f58 HEAD --
   bench/` **empty** — the baselines `01-03` wrote are untouched.
5. No `elapsed`, `seconds` or `N.NNNs` in any of the three baselines or five
   records.
6. `git worktree list` carries the main tree and one pre-existing detached entry
   from an earlier session. **This plan added none** — it used `git archive`.
7. `main` at `b42015f`; the working tree carries only `01-04-SUMMARY.md`,
   `STATE.md`, `ROADMAP.md` and `WINDOWS.md`, committed next.
8. `geomean.py` re-run and its own output read: `instances averaged 86`,
   `GEOMETRIC MEAN 0.9709x <-- this is the result`, `ratio of totals 0.9847x
   <-- NOT the result`.
9. The minima recomputed from the twelve logs independently of the analysis
   script: `truss` 1.496 → 1.460, `pilot87` 25.968 → 25.896, `maros-r7`
   23.211 → 23.181. All three match the tables above.
10. Both callgrind annotations re-read: `54,507,175,480` and `55,377,182,992`
    PROGRAM TOTALS, so the +870,007,512 is recomputed rather than quoted.
11. The disputed citation checked directly: the last `## D` heading at or above
    the line carrying "repeats itself to about **1.4%**" is **D81**, not D83.

## What plan 01-05 inherits

- **D93 is refusal-shaped.** The measurement did not reach the bar, the code
  stays under the developer's pre-authorisation, and D82 and D84 are the models
  for how that gets written — a closed entry saying it does not pay and what it
  cost to find out.
- **And it has a second finding to carry, which is not a refusal but a fact:**
  the change costs **1.60% more instructions** on `truss`. `admit_candidate`
  sheds 199M and its caller gains 995M. Whatever it may buy in seconds is not
  bought by doing less work, and D93 stating "it saves instructions but not
  enough" would be false.
- **The measurement on both sides of the threshold.** 4.2% is three times
  D81's 1.4% — **not D83's**, and four files already cite D93 so a second wrong
  citation is worth avoiding. This reading's own repeatability, measured the way
  D81 measured its own, is 6.27%.
- **The instrument's ceiling.** 8 instances of the standard set carry no time
  ratio at all and 42 more read exactly 1.0000x, because the runner prints
  `%8.3f`. Any future time ratio on this set inherits that, and it is a two-line
  change to `bench/run.c` if it ever matters — not made here, because a source
  edit mid-measurement invalidates the measurement.
- **Running order is worth 2.4 percentage points on this host.** Any later
  same-instance ratio that alternates in one direction only is measuring the
  order. This is the most transferable thing in the plan.
- **`pilot87` and `maros-r7` read 0.9972x and 0.9987x.** `01-03` predicted from
  their work ratios that they would show nothing, and they showed nothing. D46's
  two names remain the wrong place to look for *this* change, and the ratio of
  totals — 0.9847x — is a statement about them.
- **One open item, not `01-05`'s:** `.planning/` still carries the D83/D81
  misattribution in `01-04-PLAN.md`'s `<threshold_note>`. It is a planning
  artifact rather than a live document, and it is corrected here rather than
  edited there.

## Commits

| Hash | Message |
|---|---|
| `f1698fd` | docs(01-04): the time ratio is below the bar, and the instructions went up |

One commit, because this plan modifies no repository file. There is no per-task
commit to pair with it: Tasks 1 and 2 produce numbers, not diffs.
