# The warm repair's shortfall cap, swept on both sides — 4 is the end of a plateau, not a minimum

Taken 2026-08-19, the sweep D149's refusals-table row ordered. Closed as D151.

## What D149 left, and what this had to produce

D149 refused the blanket warm count repair on cost. It was correct behind
D148's certificate guard (`disagreed=0, rejected=0`) and `dfl001` paid
**172x work** for a 596-short repair whose trajectory the guard then threw
away. The row named the material: 02-52's per-instance shortfalls joined to
02-58's per-instance outcomes. 02-52 had saved only the aggregates, so the
per-instance shortfalls were re-measured here.

## The three records this is computed from

| what | where | how it was got |
|---|---|---|
| shortfall S per instance | `shortfall.txt` | `run-shortfall.sh`, one tagged line per mapped solve |
| the repair's outcome per instance | `../02-58/retry-warm*.txt` | D149's campaigns |
| no repair, per instance | `bench/results/warm*.txt` | the committed record |

`run-shortfall.sh` prints `S = rrow - rb` at the moment the warm re-solve's
mapping finishes, tagged with the instance name (one child per instance in
`bench/warm.c`, so the tag cannot cross). **Its aggregates reproduce 02-52's
exactly** — netlib 88 mapped, 54 short, total 2803; Kennington 11 mapped, 5
short, total 5 — on an instrument written from scratch against a different
decomposition. That agreement is the probe's validation.

## The sweep is arithmetic, and that is stated rather than assumed

The capped repair fires on an instance iff `0 < S <= cap`. An instance where
it fires behaves as 02-58's run; one where it does not behaves as the
committed record. Nothing crosses instances, so every cap's campaign is a
per-instance choice between two runs already measured.

That is an argument from the code, and this project refuses those. So
`sweep-cap.py` proves it three ways before predicting anything, and stops
with VALIDATION FAILED if any fails:

1. its geometric means reproduce **both** records' own summary lines, all
   eight figures (netlib 0.2553/0.1381 and 0.2605/0.0752, Kennington
   0.0572/0.0173 and 0.0070/0.0013);
2. the cold sides of the two records are identical per instance;
3. every instance whose warm side differs has `S > 0`.

**And the prediction was then checked against a real campaign of a capped
tree** — see "the run that was already there", below.

## The curve, netlib (`cap-sweep.txt`)

```
   cap  fired  work-gm  iters-gm    worst  worst-instance  warm-worse
     0      0   0.2553    0.1381     1.00  25fv47            0
     1     14   0.2089    0.0982     4.65  scsd1             0
     2     17   0.2047    0.0939     4.65  scsd1             1
     4     20   0.1916    0.0862     4.65  scsd1             1
     5     24   0.1886    0.0826     4.70  brandy            3
     6     27   0.1895    0.0795     4.70  brandy            4
     7     31   0.1874    0.0752    15.48  greenbea          4
     8     33   0.1938    0.0752    15.48  greenbea          4
   345     52   0.2507    0.0742   172.03  dfl001           11
   596     54   0.2605    0.0752   172.03  dfl001           13
```

**Read the two middle columns together.** The mean is flat across 1..7 and
the worst case is not. It holds at 4.65 through cap 4; at 7 `greenbea` steps
to 15.48 and at 345 `dfl001` to 172.

- 1 → 4 buys **8.3%** and moves the worst case by nothing.
- 4 → 5 buys 1.6% and costs `brandy` 4.70 and `bnl1` 2.87.
- 5 → 7 buys 0.6% and costs `greenbea` 15.48.

So **4**, chosen at the end of a plateau rather than at the minimum. The
minimum is at 7 and is 2.2% better than 4, for a worst case 3.3x larger.

**Kennington does not vote on the value.** All five of its short solves are
short by exactly 1, so every cap at or above 1 gives that set the whole of
its gain, 0.0572 → 0.0070. The number only trades netlib.

## The obvious alternative shape was swept too, and it is worse

A shortfall of 5 on a 25-row model and on a 6000-row model are not the same
claim, so the cap relative to the model's rows (`S <= r * nrow`) is the
obvious better-shaped constant. It was swept over every distinct ratio in the
set (`cap-detail.txt`) and it loses on both measures:

| | best work-gm | where the 15.48 cliff arrives |
|---|---|---|
| absolute cap | **0.1874** (cap 7) | with **31** instances admitted (cap 7) |
| relative cap | 0.2081 (r = 0.0312) | with **8** instances admitted (r = 0.0036) |

The reason is one instance. `greenbea` is 7 short of 1954 rows — **0.36%**,
the smallest relative shortfall in the set — and it is one of the two worst
outcomes at 15.48x. `sctap3` is 596 short of 1408 rows (42%) and costs only
6.59x; `seba` is 69% short and costs 2.87x. **The shortfall's absolute size
separates these cases and its size relative to the model does not**, which is
the opposite of what the shape argument predicts.

## What the chosen setting does, per instance

20 netlib instances repair at cap 4. Sixteen win, four lose, and every
"was" below reads exactly 1.0000 because these are the instances that had
been falling back to cold:

| loses | work ratio | warm iterations | cold iterations |
|---|---|---|---|
| `scsd1` | 4.65x | 89 | 89 |
| `degen2` | 4.09x | 565 | 565 |
| `lotfi` | 1.28x | 257 | 244 |
| `25fv47` | 1.03x | 10285 | 11862 |

**The two that cost real work lose the same way, and the two iteration
columns name it**: `scsd1` and `degen2` finish with warm iterations exactly
equal to cold. That is D148's certificate guard rejecting the repaired
trajectory and restarting cold, so the bill is the attempt plus the whole
cold solve. It is the same mechanism as `dfl001`'s 172x, three orders of
magnitude smaller.

`lotfi` and `25fv47` are a different shape — certified, kept, no restart.
`25fv47` is the one worth noticing: it reaches the optimum in **1577 fewer
iterations than cold** and still pays 1.03x work, so a better starting point
is not automatically a cheaper solve.

The wins are large and concentrated in instances that had been losing their
warm start entirely, so their "before" ratio was exactly 1.0000: `ship08l`
698 → 1 iterations, `forplan` 183 → 4, `finnis` 281 → 4, `boeing1` 391 → 24,
`pilot-we` 4665 → 351, `degen3` 2397 → 138.

## The capped tree's own campaigns, and the prediction they check

The sweep above is arithmetic over two existing campaigns. The capped tree
was then built and run for real, and `verify-prediction.py` compares the two
line by line — warm iterations, warm work, cold iterations, cold work, per
instance (`prediction-check.txt`):

```
netlib:     92 instances, 20 repaired at cap 4, 92 of 92 match the prediction
            work 0.1916, iters 0.0862, worst scsd1 4.6464
            disagreed=0 rejected=0 errors=0
kennington: 11 instances,  5 repaired at cap 4, 11 of 11 match the prediction
            work 0.0070, iters 0.0013, worst cre-c 0.5131
            disagreed=0 rejected=0 errors=0
PREDICTION CONFIRMED
```

**103 of 103 instances match**, and the runner's own summary lines — computed
without this record's parser — read the predicted geometric means to four
decimals. So the per-instance-choice premise is measured, not argued.

**The correctness bar D145 set is met**, as it was for the blanket repair:
`disagreed=0, rejected=0, errors=0` on both sets. The cap changes cost and
never an answer, which is what it was designed to do — past the cap
`build_warm_basis` returns false and the solve starts cold, and the cold path
is always correct.

Against the committed record, netlib work goes 0.2553 → **0.1916** and
Kennington 0.0572 → **0.0070**, with the worst per-instance ratio at 4.65
instead of the blanket repair's 172.03.

## The gate, and why it is a stronger check than it was last week

The change should be invisible to the gate: it solves each instance once from
a fresh load with no stored basis, so `build_warm_basis` is never reached.
That was run rather than argued (`run-gate.sh`, output in `gate-run.txt`):

```
netlib            gate: PASS   baseline: 0 regressed, 0 improved, 0 new
netlib-infeas     gate: PASS   baseline: 0 regressed, 0 improved, 0 new
netlib-kennington gate: PASS   baseline: 0 regressed, 0 improved, 0 new

record_diff against the committed records:
  94 instances compared: 94 bit-identical, 0 moved, 0 digest change(s)
  29 instances compared: 29 bit-identical, 0 moved, 0 digest change(s)
  16 instances compared: 16 bit-identical, 0 moved, 0 digest change(s)
```

**139 of 139 bit-identical.** Read per instance with the skill's own script,
not from the summary line.

D150 landed the day before this and it matters here: every optimal line now
carries `basis=` and `det` covers it, so a change that moved only a basis
would show. Before D150 this same run could not have said that.

## The run that was already there

A previous session built a cap=1 candidate and ran both warm campaigns, then
stopped before recording anything — no diff, no record, no entry. Its two
output files survived in `build/diag/cap-sweep/` and are preserved here as
`dead-session-cap1/`.

**They match this sweep's cap=1 prediction on 92 of 92 netlib instances and
11 of 11 Kennington instances, warm and cold, iterations and work units.**
That is an independent campaign agreeing with the arithmetic, which is what
turns the per-instance-choice argument from a claim into a measurement.

## The test, and the case it must reject

`test_the_warm_repair_stops_at_its_cap` in `tests/test_presolve.c` builds k
independent 3-column/2-row blocks, each contributing one interior BASIC
singleton column for the mapping to drop, and counts the repair's own DETAIL
log line through the public `jaos_set_log_callback`.

**The observable is the log line and not an iteration count, and that is
measured rather than stylistic.** On models this small the warm and cold
counts coincide by accident: the same construction reads `warm == cold` at
k = 2 and k = 4 while the repair fires at both. An iteration-based test would
have read those as refusals.

**The test was validated against the two cases it must reject**, each built
in a throwaway copy of the tree:

| tree | expected | result |
|---|---|---|
| shipping (cap 4) | PASS | passed |
| cap removed entirely | FAIL | failed, `Expected FALSE Was TRUE` |
| cap raised to 5 | FAIL | failed, `Expected FALSE Was TRUE` |

So the test pins the constant in both directions rather than merely showing
the repair working.

## Reproducing it

```
bash bench/measurements/02-60/run-shortfall.sh      # the per-instance shortfalls
python3 bench/measurements/02-60/sweep-cap.py       # the curve + the cap=1 check
python3 bench/measurements/02-60/sweep-detail.py    # the relative shape + per instance
bash bench/measurements/02-60/run-warm-cap.sh       # the capped tree's own campaigns
python3 bench/measurements/02-60/verify-prediction.py  # campaign vs prediction
bash bench/measurements/02-60/run-gate.sh           # the three gate sets
```

Each script takes the candidate from `git diff` in the main tree and applies
it in a throwaway worktree, so none of them writes to `src/` and none needs
the candidate committed first.

## What this record does not settle

`scsd1` and `degen2` still pay 4.65x and 4.09x, and **the shortfall cannot be
the rule that stops them**: both are short by 1, the same shortfall as the
sixteen instances that win. Separating them needs something that predicts a
doomed trajectory before paying for it, which is not measured here and is not
this record's. `TODO.md`'s refusals table carries it under D151.
