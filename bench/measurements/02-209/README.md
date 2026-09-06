# 02-209 — the pump's two extensions: the objective pump lands, the general-integer distance is refused

What decided D320 and D321, on the 24 MIPLIB 3 instances of
`bench/miplib.manifest` (D302). A 240 s cap, 12 instances at once.
`sweep-b7.sh` wrote the control and the first five arms, `sweep-b7b.sh`
the two that bracket them, and `sweep-b7c.sh` the four on the tree the
review's fixes and the new default made.

## The control

`sweep-control.txt` is every default of D318, and
`control-against-baseline.txt` reads 0 DIFFERENT against the
`bench/miplib.baseline` batch 6 committed: both extensions are inert when
they are off, which is what says the arms below are the feature.

Every file is `name rc status obj nodes cuts heur first work secs`. The
seconds are here because a sweep is not a baseline; nothing in this
directory enters `bench/results/` or a baseline.

## The objective pump (D321)

| decay | work over the 24 | better / worse | first incumbent moved |
|---|---|---|---|
| 0.3 | 0.987x | 2 / 0 | `gt2` 382 to 1 |
| **0.5** | **0.984x** | 2 / 0 | `gt2` 382 to 1 |
| 0.7 | 0.985x | 2 / 0 | `lseu` 1 to 47 |
| 0.9 | 0.985x | 2 / 0 | `lseu` 1 to 47 |

`obj3-`, `obj5-`, `obj7-` and `obj9-against-control.txt`. No node count
moves at any decay, none past 2x. The two better at every decay are
`misc03` (0.786x at 0.5) and `dcmulti` (0.908x), on trees that did not
change and with the first incumbent where it was: the gain is the pump's
own re-solves costing less. 0.5 is the default.

## The general-integer distance (D320)

`gen-against-control.txt` and `gobj9-against-control.txt` are the first
pass, with the auxiliaries billed six units per column. The review found
that bill wrong (`jaos_add_cols` and `jaos_add_rows` each rebuild the
copy's matrix), so the arm was re-run on the fixed tree:
`genb-against-control.txt` is the general form alone (1.007x, `gt2` alone
moved, 382 to 1, at 1.163x its work) and `gobj5b-against-control.txt`
the general form beside the objective pump at 0.5 (0.990x against 0.984x
without it; `gobj5b-against-obj5b.txt` reads the pair against the default
directly). `gobj5-against-control.txt` is the pre-fix pair, kept for the
record. Refused.

## The fixes were no-ops on the shipped path

`ctl2-same-as-control.txt`: the plain pump on the fixed tree reproduces
the first pass's control in every field but the seconds.
`obj5b-same-as-obj5.txt`: the new default on the fixed tree reproduces
the 0.5 arm the same way. `retest-pump-general.sh` is the re-test
`make refusals` runs for D320.
