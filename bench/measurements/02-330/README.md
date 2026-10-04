# 02-330 — MIP: the network root bound, five ideas measured and refused

Taken on 2026-10-04 on the tree of 1deaae5, for TODO row J7 ("the
networks' root bound"). The settings are 02-328's. `arms.patch` is the
measured tree: 1deaae5 with four environment switches. `JAOS_AGGDIST` picks
the column an aggregation step substitutes out by its distance from its
bounds. `JAOS_NETSTEPS` sets the aggregation steps in network mode.
`JAOS_NETPAR` sets the parallelism limit of the network cut selection.
`JAOS_CUTLOG` prints the cut counts of each root round. Run the arms with
02-329's `arms.sh`.

## Where p200x1188c's root stops

JAOS and HiGHS 1.15.1 start from the same relaxation, 5678.6. HiGHS's root
reaches 11640 with 76 cuts (02-328's `highs-root.txt`). JAOS's first round
reaches 9869 and its next 14 rounds add 111, to 9980. `JAOS_CUTLOG` shows
why: a round finds 3 to 20 single-row cuts and 24 to 95 aggregated ones,
and the parallelism limit keeps 2 to 8 of the aggregated ones. The model
is a single-commodity network: 200 flow rows, one source with a supply of
3950, six sinks, and every arc `x <= 3950 y`. In the root's point every arc
sits at `x = 3950 y`, so the aggregation's variable-bound rule finds no
column strictly inside its bounds and only the simple-bound rule walks.

## Root readings (`--node-limit 1`)

| model | 1deaae5 | `NETPAR` 0.9 | `--mir-aggregate 20` | `AGGDIST` | `AGGDIST`, `NETSTEPS` 12 | `AGGDIST`, `NETSTEPS` 20 | HiGHS |
|---|---|---|---|---|---|---|---|
| p200x1188c | 9980 | 9992 | 9980 | 9992 | | | 11640 |
| beasleyC3 | 699.6 | 708.8 | 722.0 | 720.9 | 733.5 | 732.0 | 733 |
| exp-1-500-5-5 | 65183 | | | 65227 | 65736 | 65669 | 65684 |
| tr12-30 | 116340 | | | 116588 | 116669 | 116723 | |
| sp150x300d | 66.89 | | | 66.89 | 67.13 | 67.52 | |

beasleyC3's root takes 0.94e9 work units at 1deaae5, 1.5e9 under
`AGGDIST` and 5.5e9 at 12 steps, 71.5% of it in the root's LP solves, whose
LP grows from 4285 to 5175 rows.

Two more ideas changed nothing at the root and are not in the patch:

- **Complementing integer columns after the delta is chosen**, one at a time
  in list order, each kept when it raises the cut's efficacy (the last step
  of Marchand and Wolsey's c-MIR). p200x1188c and beasleyC3 end at the same
  bound to the last digit; sp150x300d, exp-1-500-5-5 and tr12-30 move by
  less than 1e-4 of their bound.
- **Cut-set cuts from a minimum cut** (`cutset.py`, JAOS as the LP solver
  through the CLI). For each sink t, a maximum flow from the source to t
  with each arc's capacity at its binary's value finds the node set S
  holding t; when the cut is under 1, `sum of y into S >= 1` is added. On
  p200x1188c 40 rounds reach 8685 from 5678.6. With `FLOW=1` each arc into
  S counts the smaller of `x / d_t` and `y` (a valid form for S holding t
  and no supply), and 58 rounds reach 8368. One cut per sink per round is
  far weaker than the c-MIR JAOS has.

## The arms

Against `bench/results/miplib.txt` and `bench/results/miplib2017.txt` of
1deaae5 (`b2f` of 02-329).

| arm | switches | MIPLIB 3 geo | sum | worst | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|---|---|
| 1deaae5 | | 1 | 1 | | 2 | 24 | 9.064 | 9.117 | 18.181 |
| b3a | `AGGDIST` | 0.997 | 1.000 | `egout` 1.054 | 2 | 24 | 9.056 | 9.087 | 18.144 |
| b3b | `AGGDIST`, `NETSTEPS` 12 | 1.029 | 1.000 | `egout` 1.779 | 2 | 24 | 9.453 | 9.074 | 18.528 |

`b3a` lifts beasleyC3's bound at the limit from 702 to 724, timtab1's from
414399 to 432840 and tr12-30's from 118133 to 118286, and lowers
p200x1188c's from 10789 to 10396. sp150x300d solves at 0.23x the work and
exp-1-500-5-5 at 1.65x. The gap sum reads 0.998x, under the 0.95x the 2017
readings ask of a change (cuts-2017-reading).

`b3b` lifts beasleyC3's bound to 735, past HiGHS's root, but its incumbent
falls from 889 to 1118 and tr12-30's from 145619 to 155451; exp-1-500-5-5
solves at 0.51x the work.

Both are refused: `mir-agg-distance` and `net-agg-steps` in
`bench/refusals.txt`, with `net-cutset-mincut` for the cut-set cuts.
