# 02-147 — four scratch-and-bitmap contracts become asserts

D234. Four of `simplex.c`'s prose contracts, from the list in
`bench/measurements/02-121/simplex.c.md`.

## What is here

| file | what it does |
|---|---|
| `run-scratch-population.sh` | the four asserts over all 139 instances on the dual, and over 33 in both methods |
| `run-scratch-controls.sh` | the edit that fires each one |
| `population.txt` | the population run, as run |
| `controls.txt` | the arms, as run |

Both derive the repository root and run from anywhere (D217). Neither is a
gate tool. Both link `02-145/probe.c`.

## The four

| the contract | the check |
|---|---|
| `apat` names every slot where `alpha` can be nonzero | a count: nonzeros in `alpha` equals nonzeros among `apat`'s entries |
| `rpat` names every slot where `rho` is nonzero | the same count, over both writers |
| `c1` is all zero after its incremental clear | an O(nvar) walk |
| `nbmark` matches `status` | `nbmark_consistent`, asserted at `refresh`'s successful exit |

**The count is what makes the first two need no scratch array.**
`jm_pattern_order` already asserts its output is ascending and distinct
(D223), and distinctness is what lets a count stand in for a membership test:
the pattern may name a slot that cancelled back to zero, but it may not miss
one that did not.

**The fourth is the one that was actually missing.** D223 put that walk inline
in `dual_ratio_test`, which is the dual path only. `run_primal`,
`run_primal_phase1` and `primal_cleanup` all reach `pivot()` without passing
it, so the primal had been maintaining the bitmap with nothing checking it.
Moving the walk into a helper and asserting it at `refresh`'s successful exit
covers every path, because every basis change is followed by a refresh.

## They hold, on both methods

| arm | records | assertion lines |
|---|---|---|
| dual, 94 standard | 94 | 0 |
| dual, 29 infeasible | 29 | 0 |
| dual, 16 Kennington | 16 | 0 |
| probe, 33 instances in both methods | 66 solves | 0 |

The probe arm is not optional here. The whole point of the fourth assert is
the primal paths, and `bench/run` only ever runs the dual.

## Each one fires for its own defect

| arm | the edit | what fires |
|---|---|---|
| `alpha-pattern` | the logical column's slot is written but never recorded | `price_all`'s count |
| `rho-pattern` | the row pattern drops its last entry | `build_pricing_row`'s count |
| `c1-clear` | the incremental clear stops one short | the `c1` walk |
| `nbmark` | `pivot` moves the status and leaves the bitmap | `nbmark_consistent` |

**The first two print the same expression**, because they are the same
expression. An arm that matched on that alone would pass on the other one
firing, which is D232's mistake in a new place, so both the pass criterion
and the record carry the function name glibc prints before it.

## Reproducing

```
bash bench/measurements/02-147/run-scratch-population.sh   # ~55 min
bash bench/measurements/02-147/run-scratch-controls.sh     # ~15 min
```

Kennington under `-UNDEBUG` is nearly all of the first figure:
`dual_ratio_test`'s debug block runs a second full `admit_candidate` scan over
every variable on every iteration.
