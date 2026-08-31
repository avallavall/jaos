# 02-138 — the writers round-trip the gate, and counting the LP writer's gap is what found its defect

D226. Evidence for one sentence in `SPECS.md` section 6 and
`docs/format-support.md`: **what JAOS writes, JAOS reads back as the same
model.**

Five probes, each with its own script. The second one is the reason this
directory is worth reading.

| script | what it answers | record |
|---|---|---|
| `run-roundtrip.sh` | does every gate instance survive MPS write-and-read | `roundtrip.txt` |
| `run-lpcover.sh` | what the LP dialect cannot express, counted | `lpcover.txt` |
| `run-digits.sh` | does `wr_num` ever print a value that reads back different | `digits.txt` |
| `run-ranges.sh` | how often the ranged-row refusal fires, and is the form it picks exact | `ranges.txt` |
| `run-controls.sh` | do the checks above go red when the writer is broken | `controls.txt` |

## 1. `roundtrip.c` — the MPS writer over every gate instance

`bash bench/measurements/02-138/run-roundtrip.sh` from the repository root.
For each of the 139 instances in the three gate sets: read the file, call
`jaos_write_mps`, read the written file back, and compare every field of the
two models with `==` — dimensions, sense, objective constant, every cost,
every bound, the column starts, every row index and every coefficient.

**139 of 139 round-tripped exactly, 0 did not.** `roundtrip.txt` is the
per-instance record. It covers every RANGES form, every BOUNDS type in use,
the objective-constant convention and the largest model in the sets
(`pds-20`, 33874 rows, 105728 columns, 230200 nonzeros).

## 2. `lpcover.c` — what the LP writer cannot express, and one defect

`SPECS.md` called the LP writer `partial` and named the shapes the dialect
cannot say, with no number beside them. D101's rule is that what is left gets
counted rather than guessed, so this probe tries `jaos_write_lp` on every gate
instance, records which refusal fired, and — where the write succeeds — reads
the file back and compares.

**The first run did not measure a gap. It found a defect.**

```
19 round-tripped through LP, 37 refused, 83 differed
```

Eighty-three instances wrote a **valid** LP file that read back **without any
error** as a **different model**.

LP format has no `COLUMNS` section. The reader numbers a column where its
name FIRST appears in the token stream, and the objective is the first
section it reads. The writer listed only the columns with a non-zero cost
there, so every zero-cost column was numbered by wherever its first
coefficient happened to sit, and costs, bounds and coefficients all landed on
the wrong columns.

The fix is one loop: the objective names every column, in index order,
including the ones costing nothing. A zero term also lets LP name a column
that appears in no row, so what had been a fourth refusal disappeared with
it. After the fix:

```
102 round-tripped through LP, 37 refused, 0 differed
first refusal per instance: 2 ranged row, 1 free row, 34 empty row,
                            0 orphan column, 0 other
```

`lpcover.txt` is the per-instance record, and every one of the 37 refusals
names the row that caused it.

### Why the unit tests were green through all of it

Every LP model in `tests/test_write.c` gave all its columns a non-zero cost,
including the golden `tests/data/g1.lp`, so the skipped branch was never
reached. That is the same shape as the false-clean readings this project
keeps finding: a probe run on a population without the interesting case looks
exactly like a probe that passed. The 139 instances were the population that
had it.

`test_lp_keeps_column_order_when_a_cost_is_zero` is the missing case, and
`test_lp_takes_a_column_that_appears_in_no_row` is the refusal that went
away.

## 3. `run-controls.sh` — the arm that makes each check go red

A clean result from a new instrument means nothing until the instrument is
shown able to report a dirty one (`jaos-testing`). Four things are called
clean in this directory, so each needs an arm beside it that fails for the
right reason.

The script builds a worktree of `HEAD` plus the three uncommitted files the
writers live in, breaks exactly one contract, and runs the unit suite and
both probes over the standard set. `controls.txt` is the record.

| arm | unit suite | MPS round trip | LP files reading back different |
|---|---|---|---|
| intact | 18 tests, **0 failures**, exit 0 | 94 of 94 | 0 |
| `wr_num` cut to a flat six digits | **3 failures**, exit 3 | **47 of 94** | 35 |
| the LP objective skips a zero cost | **3 failures**, exit 3 | 94 of 94 | **59** |
| the solution writer stops checking for a finite answer | **no summary at all, exit 134** | 94 of 94 | 0 |
| the recipe again, nothing broken | 18 tests, **0 failures**, exit 0 | 94 of 94 | 0 |

Each break turns its own tests red and nobody else's. Six digits takes
`test_mps_round_trip_every_shape`, `test_lp_refuses_what_the_dialect_cannot_say`
and `test_each_mps_guard_fires_on_its_own`; the LP objective takes
`test_lp_keeps_column_order_when_a_cost_is_zero`,
`test_lp_takes_a_column_that_appears_in_no_row` and
`test_each_lp_guard_fires_on_its_own`. Breaking the LP objective leaves the
MPS round trip at 94 of 94, which is what says the two probes are measuring
different things.

**The fourth arm is why an arm is judged on the test binary's exit code and
not on Unity's summary line.** Removing the finiteness guard makes `wr_num`
abort inside the test, so the run ends at signal 6 with no summary printed:
a script counting failures would have read an empty string and called the
control inconclusive. 134 is what it reads instead.

The durable half is the unit suite. `test_mps_round_trip_every_shape` asserts
on `nextafter(1.0, 2.0)`, a value needing all seventeen digits, so any later
change that quietly shortens what the writer prints goes red without this
campaign being re-run.

**The first run of this script found a red test in the tree**:
`test_each_lp_guard_fires_on_its_own` built a model with a ranged row AND a
column bound at an infinity, then expected the message about the column. The
first refusal wins, by design, so the ranged row was reported instead and the
column guard was never reached. The row is an equality row now and the case
tests the guard it names. That test had been written after the probes were
last run, which is exactly the window a hand-run control leaves open and a
script does not.

## 4. `digits.c` — the seventeen-digit fallback, measured

`wr_num` tries 15 then 16 significant digits and keeps the first that reads
back equal, so those two forms are checked at run time. The `%.17g` fallback
is not: it rests on the IEEE-754 round-trip guarantee and on the host libc
rounding `printf` correctly, and `src/write.c` asserts it. This probe tries
to fire that assert.

`wr_num` is static, so `digits.c` includes `src/write.c` and the link leaves
out `write.o`. The population is deterministic — splitmix64 from a fixed
seed — and four-part, because the interesting question differs by shape:

```
random bit patterns          4000000 values   240806 at 15  1945344 at 16  1813850 at 17   0 wrong
one ulp at a time from 1.0    200000 values     4442 at 15    39968 at 16   155590 at 17   0 wrong
small rationals a/b           500500 values    60960 at 15   235470 at 16   204070 at 17   0 wrong
k times a power of ten        410041 values   353539 at 15     7316 at 16    49186 at 17   0 wrong
```

**5,110,541 values, 2,222,696 of which reached the seventeen-digit fallback,
0 that did not read back.**

The split is the second answer. On random bit patterns the fifteen-digit form
survives 6% of the time, so on its own it would be nearly useless. On decimal
fractions, which is what an MPS file usually carries, it survives 86%. That
is what the loop buys: a file of real data stays readable, and a file of
awkward data stays exact.

## 5. `ranges.c` — the third MPS refusal, which the gate never reaches

A ranged row is the one construction the MPS reader rebuilds by arithmetic.
Its `G` form gives `[b, b + |r|]` and its `L` form `[b - |r|, b]`, so writing
`b` and `r = ru - rl` recovers the pair only when both the subtraction and
the reader's addition are exact. `range_form` tries both and refuses the row
when neither is.

**No gate instance reaches that refusal**, and a guard that never fires is a
guard nobody has tested (`jaos-testing`). `numerics-reviewer` pointed out
that `include/jaos.h` did not even mention it, so this probe builds the
population that does reach it. It also runs the reader's own two formulas on
whatever form the writer picked and compares with `==`, which is the check
that would catch a defect in `range_form` itself.

```
random finite pairs            10000000 pairs  5000993 G  4991532 L   7475 refused  0 reconstruct wrong
a width beside its own bound    9961899 pairs  9961899 G        0 L      0 refused  0 reconstruct wrong
eighths, as netlib data is       8002000 pairs  8002000 G        0 L      0 refused  0 reconstruct wrong
```

**27,963,899 ranged rows, 7,475 refused, 0 accepted that reconstruct wrong.**

Every refusal is in the first shape, where the two bounds are unrelated
random doubles. Both shapes a real model has — a width drawn beside its own
bound, and decimal eighths — refuse nothing at all and never need the `L`
form. That is why the gate reads 0 of 139, and it is also the answer to
whether the refusal is worth keeping: it is rare, it is reachable, and the
alternative is a file that reads back as a different model.

## What this does not say

Nothing about reading the solution format back. There is no reader for it, so
its test checks the file's own digits against `jaos_objective` rather than
round-tripping.

And nothing about speed. The writers are not on any solve path, and the three
gate sets confirmed it at the landing commit, `6d293d8`: all three read
`gate: PASS` and `0 regressed, 0 improved, 0 new`, and `git status
bench/results/` printed nothing afterwards, so every digest and every work
figure came back byte-identical to the committed record. `make configs`
builds and passes all five configurations.

## A note on the headers

Every `.txt` here says `WITH UNCOMMITTED CHANGES`. They have to: the writers
are the change being measured and were untracked when the readings were
taken. Nothing else in the tree differed from the commit each header names.
