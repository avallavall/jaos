# 02-231 — the incumbent callback, read against the model that fed it

SPECS row 74, the node limit and the incumbent callback, said `done` and
said nothing else (`TODO.md` row 6). This is the callback's half:
`jaos_set_incumbent_callback` hands the caller every new incumbent as the
tree finds it, with its node, objective, bound and point, and the caller
can stop the solve from inside the call. `incumbent.c` reads nine
properties off the model's own arrays.

## The models

2000 per seed, six seeds. Eight to sixteen columns in boxes `[0, U]` with
`U` from 1 to 10, three to eight rows with coefficients in -3 to 3, an
integer mark on six columns in ten, costs in -9 to 9, minimised or
maximised with an objective offset. Every model plants an integer point
inside its boxes and sets each row's bounds around it, three in ten rows as
equalities, so every model has an integer point. Half the models run with
the rounding heuristic, the dive heuristic, the pump and the root cuts off
and the tree diving, so the incumbents come from the leaves and there are
several of them; the other half run as the defaults have it.

Each model is solved three times: once recording every call, once with the
callback answering `STOP` at its first call, and once more recording every
call again.

## The properties

1. every announced point is feasible for the model as loaded, by
   `jaos_check_solution` at 1e-6, integrality included
2. the announced objective is the cost row at the announced point, with
   the offset
3. each announcement is strictly better than the one before, in the
   model's own sense
4. the announced bound sits on the right side of the announced objective
5. nodes never go backwards, and the first call's node is what
   `jaos_mip_result` reports as `first_incumbent_node`
6. the last announced objective is the published one, and the last
   announced integer columns are the published ones
7. there is a call exactly when the report has an incumbent
8. a `STOP` from the first call ends the solve `INTERRUPTED`, and
   `jaos_mip_incumbent` gives the announced point and objective back
   exactly
9. the second full solve announces the same sequence, call for call, node,
   objective, bound, point and the `by_rounding` flag

## The reading

| seed | models | calls | models with two or more | most | broken |
|---|---|---|---|---|---|
| 1 | 2000 | 2599 | 488 | 5 | 0 |
| 2 | 2000 | 2595 | 496 | 5 | 0 |
| 3 | 2000 | 2565 | 470 | 5 | 0 |
| 4 | 2000 | 2554 | 452 | 7 | 0 |
| 5 | 2000 | 2574 | 477 | 5 | 0 |
| 6 | 2000 | 2573 | 476 | 5 | 0 |

**No defect.** 12000 models, all optimal, 15460 calls, every property
holds on every call. About a quarter of the models announce more than one
incumbent, which is where properties 3 and 5 say something; the planted
models are easy for the tree and a larger share was not reached by turning
the heuristics off alone.

## The pass is not vacuous

`incumbent.sh control` makes two one-line edits to `src/mip.c`, one at a
time, at 2000 models and seed 1, and puts the file back from a copy:

| the control | broken |
|---|---|
| as it is | 0 |
| the announced bound pushed one past the objective | P4 on 1583 of 2599 calls |
| the announced objective pushed by 1e-3 | P2 on every call, P4 on 573, P6 and P8 on every model |

The second edit shows the properties reading different things: P2 sees the
objective disagree with the point, P6 sees the last call disagree with the
published answer, and P8 sees `jaos_mip_incumbent` give back an objective
the call did not announce.

## How to run

```
make all
bench/measurements/02-231/incumbent.sh            # six seeds
bench/measurements/02-231/incumbent.sh control    # the two edits, seed 1
```

`control` rebuilds the library twice and leaves the tree built under the
current source.
