# 02-232 — the progress callback, and what it told a MIP caller

SPECS row 151, the progress callback that can stop, said `done` and said
nothing else (`TODO.md` row 6). `jaos_set_progress_callback` asks the
solve to call back as it runs, with the iterations, the work and the
primal infeasibility so far, and a `STOP` from inside the call ends the
solve `INTERRUPTED` on the basis it stopped on, so the next solve
continues. `progress.c` reads seven properties over generated models,
LPs and MIPs.

## The models

2000 per seed, six seeds. Eight to twenty-four columns in boxes `[0, U]`
with `U` from 1 to 10, six to sixteen rows with coefficients in -3 to 3,
costs in -9 to 9, minimised or maximised with an offset. Every model
plants an integer point inside its boxes and sets each row's bounds around
it. A third of the models carry no integer mark and solve as LPs; the rest
run the tree, whose node relaxations are copies that inherit the callback.

Each model is solved four times: with no callback, for the reference
answer; recording every call; with the callback answering `STOP` at its
first call and then solved again from where it stopped; and recording
every call once more.

## The properties

1. every call of an LP solve comes on the beat, at an iteration count that
   is a multiple of 64; a MIP reports the tree's running total instead
2. the reported work never goes back between two calls of one solve, read
   for LPs and for MIPs separately
3. the last call's iterations and work are at or below what the solve
   reports at the end
4. the answer with the callback is the answer without it, to the bit:
   status, objective, point, iterations and work
5. a `STOP` at the first call ends the solve `INTERRUPTED` with nothing to
   read, and solving again reaches the reference status and objective
6. the reported primal infeasibility is not below zero; the calls that
   report it infinite are counted
7. the second recording solve makes the same calls, field for field

## Two defects the first run found

The first 300 models broke properties 2 and 6 on almost every MIP call.

The infeasibility was `inf` on 25495 of 25826 calls. The callback fired at
the top of the loop, before the pricing step that measures the
infeasibility, so the call at iteration 0 of every solve reported the
initial `HUGE_VAL`, and a node relaxation rarely runs 64 iterations, so
that call was nearly all of them. The callback now fires after the
pricing in the primal's phase 1 and in the dual's phase 2. The dual's
phase 1 measures dual infeasibility and has no primal one to report, so
its calls still say infinite; they are about a sixth of a MIP's calls.

The work went back on 11058 of the MIP calls. A node relaxation is a copy
of the model that inherits the callback, so the caller heard each node's
own counters, restarting from zero at every solve, and a caller watching
the work could not tell how far the whole solve had come. The tree now
relays the calls: the copy's callback is a wrapper that adds the tree's
accumulated work and iterations before it hands the call on, and the
final polish of the incumbent gets the same wrapper. The relayed
iterations are no longer multiples of 64, which is why property 1 reads
LPs only.

## The reading

| seed | models | LP / MIP | calls | models with calls | stopped and resumed | infinite | broken |
|---|---|---|---|---|---|---|---|
| 1 | 2000 | 700 / 1300 | 142184 | 1998 | 1998 | 24708 | 0 |
| 2 | 2000 | 671 / 1329 | 133432 | 2000 | 2000 | 24568 | 0 |
| 3 | 2000 | 691 / 1309 | 123614 | 2000 | 2000 | 24074 | 0 |
| 4 | 2000 | 682 / 1318 | 193588 | 1997 | 1997 | 27076 | 0 |
| 5 | 2000 | 651 / 1349 | 150152 | 1999 | 1999 | 25854 | 0 |
| 6 | 2000 | 675 / 1325 | 130611 | 2000 | 2000 | 23312 | 0 |

**No defect left.** 12000 models, all optimal, 873581 calls, every
property holds on every call; every stopped solve resumed to the reference
status and objective. The models without calls are the few whose solve
presolve settles before a simplex iteration runs. The four gates are
byte-identical: the callback observes and the walk does not change.

## The pass is not vacuous

`progress.sh control` makes two one-line edits, one at a time, at 2000
models and seed 1, and puts each file back from a copy:

| the control | broken |
|---|---|
| as it is | 0 |
| the tree's relay dropped, a node reports its own work | P2 on 61187 MIP calls, none on an LP |
| the reported infeasibility replaced by -1 | P6 on every one of 142184 calls |

## How to run

```
make all
bench/measurements/02-232/progress.sh            # six seeds
bench/measurements/02-232/progress.sh control    # the two edits, seed 1
```

`control` rebuilds the library twice and leaves the tree built under the
current source.
