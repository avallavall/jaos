# 02-472 — a search over the starts of time-indexed schedules

Taken on 2026-11-17 on ec95542 with this change, for TODO rows J7 and J32.
`read.sh` takes every reading from a copy of HEAD, one solve at a time:
- `miplib3/landed.txt`, and `miplib3/ratio.txt` from `ratio.py` against
  `bench/results/miplib.txt`;
- `full/landed.txt` and `full/sum.txt`;
- `budget.txt`, from a second copy with `trace.py`'s prints;
- `prototype.txt`, from `prototype.sh`.

`full/before.txt` is 02-469's record.

## The shape

`decode.py` reads `csched008` and `csched007` by their rows alone:
- 50 partitioning rows, one per job, over 1,284 and 1,457 binaries. A
  binary is a start of its job at one time of a window of 7 to 64.
- A chain of 201 equality rows. A continuous column links each row to
  the next, +1 in the one and -1 in the other, with bounds 0 and 25.
  Summed up to row k, the chain says that this column is the sum of the
  usages of the jobs running at k: the profile of one resource of
  capacity 25.
- Each start enters the chain twice, -u at its time and +u at its end,
  so it is a block of usage u (1 to 5) for its duration (4 to 58).
- The other rows set each job's start time and, from it, `csched008`'s
  makespan or `csched007`'s tardiness. No row orders two jobs.

`energy.py` puts the tightest interval at 90% of its capacity in
`csched008` and 91% in `csched007`, whatever the starts.

## Why fix-and-propagate jams

The chain needs no solve: its prefix sums give the profile in closed form,
so a capacity check on the decoded jobs is exact. What jams is the order
and what the propagation sees. `sched.py` searches the starts within a
limited discrepancy search, with two branchings and two propagations:
- *set times*: take the job whose earliest start comes first, ties by the
  relaxation's mean start; place it there, and on backtrack drop that
  start;
- *fewest starts*: take the job with the fewest starts left and try them
  by their relaxation value;
- *compulsory parts*: every job's compulsory part, the times all its
  remaining starts cover, counts on the profile, and a start that would
  lift the profile past 25 leaves its job, until none does;
- *placed jobs only*: only placed jobs count on the profile. Bound
  propagation over the chain's rows sees about this much: one row moves a
  profile column's bounds only by what its own starts can do.

`prototype.txt`, at most 5,000 nodes, each point checked by solving the LP
with its integers fixed:

| branching | propagation | `csched008` | `csched007` |
|---|---|---|---|
| set times | compulsory parts | 182 at 0 discrepancies, 51 nodes | 412 at 1, 114 nodes |
| set times | placed jobs only | 185 at 3, 2,802 nodes | none |
| fewest starts | compulsory parts | none | none |

Greedy fix-and-propagate is the second row stopped at 0 discrepancies:
it fails after 11 nodes. On `csched008`, each later round drops the
starts that end past the last makespan less one: 182, 180, 179, 176, 175,
then nothing in 5,000 nodes below 175. The optimum is 173.

## What was built

`sched_for_point` in `src/mip.c` runs once at the root of a linear model
of this shape, after lock rounding and before the feasibility jump.
- **Detector:** every free integer column is a binary in exactly one
  partitioning row (a start of that row's job). The longest chain of
  equality rows linked by continuous columns with opposite coefficients,
  each row with at most one link in and one out, holds only links, starts,
  fixed columns and, in its last row, one continuous column of its own.
  Every start enters the chain once, or twice with opposite coefficients,
  all of one sign. Each position's capacity comes from its link's bounds
  and the rows' right-hand sides, and no position may demand usage.
- **Search:** the prototype's set times with compulsory parts, items kept
  on a trail and undone on backtrack, within a limited discrepancy search.
  Ties go to the relaxation's mean start of the job, then to the latest
  start.
- **Rounds:** each schedule's starts are fixed and the rest solved. The
  next round bounds the objective `MIP_SCHED_STEP` below that point,
  propagates the bound through every row, continuous columns included,
  and drops the starts it rules out. The rounds stop when a search finds
  nothing better or the budget runs out.
- **Budget:** `MIP_SCHED_WORK` times the model's size, or what the work
  limit leaves; the solves count on top.

A sub-MIP of such a model has the same shape, so the search also runs at
each sub-MIP's root.

`budget.txt` traces both roots at node limit 1. The MIP presolve merges
the chain rows that no start enters, so the chain has 191 and 199
positions.

| instance | the root's rounds | work to the last point | the root's sub-MIPs |
|---|---|---|---|
| `csched008` | 184, 176, 175, then none below 175 | 1.25e6 of 3.77e7, 0.033 | none below 175, the whole budget |
| `csched007` | 407, then 407 again | 0.94e6 of 4.24e7, 0.022 | 405, which the log credits to the sub-MIP |

On `csched008` the round below 175 spends the rest of the budget, and the
root's RINS sub-MIP spends as much again: 7.5e7 of the 6.2e9 the root
takes.

## The readings

MIPLIB 3, against `bench/miplib.baseline`: no instance changes status,
none has the shape, and the total work reads 1.020x (`ratio.txt`, against
`bench/results/miplib.txt`). Six end at other nodes:

| instance | work | nodes |
|---|---|---|
| `misc07` | 1.08x | 9,485 to 9,776 |
| `blend2` | 0.90x | 4,337 to 4,220 |
| `mod008` | 0.88x | 1,507 to 1,239 |
| `p0282` | 1.02x | 1,717 to 1,783 |
| `lseu` | 1.01x | 4,967 to 4,989 |
| `bell5` | 0.99x | 21,034 to 20,939 |

They move because the detector's pass adds its work to the root, and later
choices are measured against the work.

MIPLIB 2017 at 1e10, 02-328's `m17sum.py`:

| | incumbents | primal | dual | sum |
|---|---|---|---|---|
| before (02-469) | 28 | 6.5397 | 6.5166 | 13.0562 |
| this change | 29 | 5.4012 | 6.5238 | 11.9250 |

The two models of the shape:
- `csched008`: no incumbent to 175 at the root, 0.012 of its optimum
  away; its bound stays at 171, now after 96 nodes;
- `csched007`: 462 to 397, 0.32 to 0.13 away. The root's search gives 407,
  and the RINS sub-MIP 405 at node 1 and 397 later. Its bound rises from
  299.1 to 302.4, the dual gap 0.148 to 0.138, with 5,075 nodes instead of
  11,190.

The other instances move only by the work the detector adds:
- worse: `neos-3754480-nidda` 0.053 to 0.083, `mas74` 0.047 to 0.067;
- better: `neos-3046615-murg` 0.077 to 0.064 (its bound falls back by as
  much), `supportcase26` 0.011 to 0.008.

Those moves net about +0.05. The two models of the shape give -1.18.

## On top of 02-470

These readings were taken on ec95542. The change landed after 02-470's
moves over big-M disjunctions, so both sets were taken again on the
landed commit, one solve at a time:
- `miplib3/combined.txt`, against 02-470's `bench/miplib.baseline`;
- `full/combined.txt`, against 02-470's `full/landed.txt`.

MIPLIB 3 reads 1.042x and no instance passes 2x:

| instance | work | nodes |
|---|---|---|
| `misc07` | 1.13x | 9,548 to 10,221 |
| `p0282` | 0.94x | 1,783 to 1,677 |
| `mod008` | 0.97x | 1,239 to 1,221 |
| `blend2` | 0.98x | 4,190 to 4,216 |
| `l152lav` | 1.02x | 476 to 494 |
| `lseu` | 1.01x | 4,967 to 4,989 |
| `bell5` | 0.99x | 20,971 to 21,056 |

`misc07`, the set's largest tree, carries most of the rise.

MIPLIB 2017's gap sum goes from 12.0431 to 10.9142, with 29 incumbents:

| instance | primal gap | dual gap |
|---|---|---|
| `csched008` | 1.0 to 0.012 | |
| `csched007` | 0.313 to 0.154 | 0.145 to 0.138 |
| `mas74` | 0.047 to 0.020 | |
| `glass4` | 0.417 to 0.500 | |

## The verdict

Landed, on by default with the rounding heuristics. `--no-heuristics`
turns it off. TODO row J32 keeps `csched007`'s objective: its search sees
tardiness only through the bound, which drops no start there.
