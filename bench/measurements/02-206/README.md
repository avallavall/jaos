# 02-206 — the dive heuristic below the root, RINS, and a dive bounded by how far a node fell from its parent

What decided D314, D315 and D316, on the 24 MIPLIB 3 instances of
`bench/miplib.manifest` (D302). A 240 s cap, 12 instances at once,
`sweep-b5.sh` the script that wrote every file here.

## The control, and why it is not the old baseline

`sweep-control.txt` is every default of D313. It does NOT reproduce the
`bench/miplib.baseline` that D313 committed, and
`control-against-baseline.txt` says exactly how: **16 of 24 instances
carry a lower work figure, no node count moves anywhere, and 8 are
unchanged**. That is a billing repair, not a change to the search.

The dive heuristic charged one pass over the matrix for `rounded_point`
whether or not `rounded_point` ran. It runs only when the dive reaches an
integral point, which happens at the root on 8 of the 24. On the other 16
the tree was paying for a pass it never made. `numerics-reviewer` found
it while reading D314's diff, and it mattered before it was a defect:
D314's whole mechanism is how many nodes the dive fires at, so the arms
would have read the phantom charge and not the feature. The baseline is
rewritten to the repaired figures and matches this control on all 24.

## The arms

| file | flags | read against |
|---|---|---|
| `sweep-dhd1.txt` | `--dive-heuristic-depth 1` | the control |
| `sweep-dhd2.txt` | `--dive-heuristic-depth 2` | the control |
| `sweep-dhd4.txt` | `--dive-heuristic-depth 4` | the control |
| `sweep-rins10.txt` | `--rins 10` | the control |
| `sweep-rins50.txt` | `--rins 50` | the control |
| `sweep-rins200.txt` | `--rins 200` | the control |
| `sweep-dive.txt` | `--dive` | the control |
| `sweep-dg3.txt` | `--dive --dive-degrade 1e-3` | the control and the dive |
| `sweep-dg2.txt` | `--dive --dive-degrade 1e-2` | the control and the dive |
| `sweep-dg1.txt` | `--dive --dive-degrade 1e-1` | the control and the dive |

The degradation bound decides nothing with the dive off, so its three
arms have a second comparison against `sweep-dive.txt`; that one is the
verdict and `<tag>-against-control.txt` is context.

Every file is `name rc status obj nodes cuts heur first work secs`. The
seconds are here because a sweep is not a baseline; nothing in this
directory enters `bench/results/` or a baseline.

## What they say

**The dive heuristic below the root is refused (D314).** 1.049x the work
at depth 1, 1.144x at depth 2 with two instances past 2x, 1.356x at depth
4 with four past 2x and `khb05250` at 2.846x. No node count moves at any
depth, which is what a heuristic on a best-bound tree can do, so it is
judged on the first incumbent like D290 and D313: earlier on 4, 5 and 8
instances at the three depths and later on none, `bell3a` from node 230
to 3, `misc03` 115 to 2, `p0033` 99 to 2, `p0282` 28 to 3. It buys real
incumbents and it buys them at a worse rate than the two heuristics
already on: D290 moved 8 of 17 for 1.8%, D313 moved 6 of 24 for 3.2%,
and depth 1 moves 4 more of 24 for 4.9%.

**RINS is refused (D315).** 1.007x at 10 solves, 1.008x at 50 and at 200.
`sweep-rins50.txt` and `sweep-rins200.txt` are byte-identical, so the
budget saturates rather than failing to fire: a neighbourhood with most
columns fixed ends in few solves. What refuses it is that it found a
point on **one instance of the 24** (`rgn`, one heuristic point becoming
two) and moved no first incumbent anywhere, because D313's root dive
already puts one at node 1 wherever RINS would.

**The dive's degradation bound is refused (D316).** Against the plain
dive it reads 1.097x at 1e-3, 1.081x at 1e-2 and 1.048x at 1e-1: every
fraction costs more than no bound at all, and `bell5` is unfinished in
every arm including the plain dive.

**What the dive arm says, and it is the batch's most useful number.**
The plain dive on this tree reads **0.934x** over the 23 instances it and
the control both finish, 11 better, 8 worse, none past 2x. D289 refused
the dive at 1.125x and D295 read 1.053x. What stands between that 0.934x
and D289's reopen condition is one instance: `bell5` goes from 140595
nodes and 15 s to 2017777 nodes and no proof at the 240 s cap. The
refusal holds, and it now rests on a single instance rather than on the
mean.
