# 02-498 — a work limit that every step before the tree reads

TODO row J34. On `neos-3402454-bohle`, `jaos solve --work-limit 100000000`
ended `work_limit` at 3,040,692,694 work units after 167 s
(`02-494/work-limit.txt`): the root's symmetry search spent its own cap of
250 times the model's size and read no work limit. 02-494 made the steps
before and around the tree read the time limit and left the work limit
out, because the sub-MIPs and strong branching's probes run under work caps
of their own in a default run, and a work check there moves the default
records. On 2026-11-18 the maintainer chose the work check with the records
rebased.

Taken on 2026-11-19 under WSL on the Ryzen 5 9600X, two runs at a time
while other agents loaded the machine (load 7 to 20 on its 12 threads), so
the seconds are indicative; the work units are exact. Before is 87475d8a,
after is the change on it. `after-1e8-lp-presolve.txt` is on 4e9f5c2c,
whose 20 readings before and after match these to the unit.

- `run.sh BIN WORK OUT LIST [JOBS] [ARGS...]` runs `jaos solve
  --work-limit WORK --threads 1 ARGS` on each instance of LIST from
  `bench/instances-miplib2017-bench` (or `DIR`) and writes one line per
  run: status, work units, work units over WORK, JAOS's `time`, the wall
  seconds.
- `top20.txt`: 02-494's 20 largest of `bench/miplib2017-bench.manifest`,
  `bohle` among them.
- `before-1e8.txt`, `after-1e8.txt`: the 20 at 1e8, before and after.
- `after-1e8-lp-presolve.txt`: the 20 at 1e8 with the LP presolve and the
  aggregator reading the work limit too, the state this change does not
  keep.
- `steps-before.txt`: the work at each step of the tree's start on the 20
  at 1e8, before, from a temporary trace like 02-494's `trace.patch`.
- `miplib3-solve.txt`: the 24 of MIPLIB 3, one `jaos solve --threads 1` at
  a time, before, after, and after with the feasibility jump reading the
  time limit only (`fj-time-only`).
- `trace.patch`: on the change, prints `TRN <line>` each time one of the
  work checks this change adds fires, with the model's limit and work, and
  `sub=1` inside a sub-MIP. `first-divergence.txt`: its first line and its
  counts on the instances that moved.

## What reads the work limit now

A step reads the work limit of the model it runs in: the caller's in the
tree a user starts, its own cap in a sub-MIP. `work_gone` in `src/mip.c`
is the test; `budget_gone` reads it and the time limit together, so the
steps 02-494 made read the clock now read both.

- The MIP presolve stops between its passes once the work is gone, and
  the tree after it runs under the limit less the presolve's work, on the
  model as written and on the presolved one. Before, the tree got the
  whole limit and the presolve's work came on top.
- The implied bounds read it row by row and count each row's nonzeros
  plus one as they go, so a pass that runs to its end counts what it
  counted before. The coefficient tightening reads it row by row. Parity
  reads it row by row in its scan, with the same count, and column by
  column in its elimination; a parity step it stops fixes nothing and
  proves nothing.
- The symmetry search gets the smaller of its cap, 250 times the model's
  size, and what is left. The second search, before the first branch,
  runs only when what is left is more than the first search's cap, and
  gets at most what is left.
- Each root cut round reads it before its separators and before its
  re-solves, each separator starts only with work left and reads it row by
  row, as 02-494 made them read the clock; a round it stops ends the tree
  `work_limit`.
- Probing reads it at each literal, strong branching at its start and at
  each candidate, plain or kept, and every heuristic's LP solve through
  `heur_budget`. The loops of the feasibility jump, the exchanges,
  threshold accepting, the big-M moves, the schedule search and the link
  heuristic read it against the tree's work plus their own operations.

Strong branching's last re-solve, which gives the node back its
relaxation after the probes moved its bounds, runs whatever is left; past
a limit its simplex stops at its first read. 02-494 skipped it once the
time limit had passed, and the branching that follows then copied the
row statuses of a relaxation that had none. Under the work check a
sub-MIP of `misc06` that had spent its cap reached that in a default run,
and the tool crashed (UBSan: a null pointer passed to `memcpy` at the copy
of the children's row statuses, `src/mip.c`); with the re-solve kept,
`misc06` is optimal at 12850.86 in 42 nodes.

An LP's presolve and aggregator do not read the work limit. With them
reading it (`after-1e8-lp-presolve.txt`), the 20 end at the same work
units as without, so they do not need it; and a simplex stopped inside
its presolve parks a model half reduced, so the solve that resumes it no
longer ends on the work units and iterations of a run that never stopped,
as `docs/api.md` says it does. `test_a_budget_stop_can_be_resumed` failed
on it: the stop at 1 unit landed after presolve's first round, and the
stop just past it landed in presolve again, with no simplex iteration.

`test_a_work_limit_holds_through_the_symmetry_search` (`tests/test_mip.c`)
and a check in `tests/cli.sh` solve 30 items in 30 identical bins of 2,
900 binaries whose symmetry search finds no generator within its cap.
Before, every limit from 1,000 to 100,000 units ended at 701,290, the
search's whole cap of 690,250 and the steps around it; after, each ends
within twice the model's size of 2,760 past its limit, and `jaos solve
--work-limit 30000` at 30,000.

## Where the work went

The 20 at 1e8 before, in millions of work units at each step
(`steps-before.txt`): the MIP presolve, the tree's start (the objective's
grid), the implied bounds, coefficient tightening and parity, the
symmetry search, and the root relaxation with what follows it.

| instance | MIP presolve | start | tightening | symmetry | root and after | total |
|---|---|---|---|---|---|---|
| `square47` | 55.0 | 0.1 | 82.2 | 1205.8 | 0 | 1343.0 |
| `neos-3402454-bohle` | 23.7 | 20.8 | 32.7 | 2963.5 | 0 | 3040.7 |
| `square41` | 27.3 | 0.1 | 40.8 | 599.1 | 0 | 667.3 |
| `neos-2075418-temuka` | 16.2 | 0.1 | 23.5 | 2020.5 | 0 | 2060.4 |
| `neos-5114902-kasavu` | 11.8 | 10.9 | 23.6 | 1477.9 | 0 | 1524.2 |
| `neos-4647030-tutaki` | 7.9 | 7.9 | 15.8 | 993.6 | 4.0 | 1029.3 |
| `neos-5052403-cygnet` | 9.9 | 0.0 | 14.8 | 1242.4 | 0 | 1267.1 |
| `supportcase19` | 11.5 | 1.4 | 14.3 | 30.0 | 58.4 | 115.6 |
| `supportcase7` | 6.0 | 6.0 | 16.8 | 747.7 | 0 | 776.4 |
| `roi5alpha10n8` | 5.0 | 5.0 | 9.5 | 620.3 | 0 | 639.7 |
| `supportcase12` | 10.6 | 6.1 | 31.3 | 783.6 | 3.3 | 834.8 |
| `supportcase22` | 5.0 | 0.0 | 239.8 | 47.3 | 0 | 292.1 |
| `ns1644855` | 4.4 | 4.3 | 10.7 | 21.5 | 67.9 | 108.7 |
| `neos-5104907-jarama` | 23.7 | 4.7 | 10.5 | 643.1 | 2.9 | 684.9 |
| `neos-3402294-bobin` | 5.3 | 4.7 | 7.3 | 657.2 | 0 | 674.4 |
| `bab2` | 4.4 | 0.1 | 9.9 | 286.5 | 0 | 300.9 |
| `co-100` | 4.1 | 0.0 | 7.8 | 511.6 | 0 | 523.5 |
| `ns1760995` | 5.0 | 4.4 | 9.2 | 621.8 | 2.5 | 642.9 |
| `s100` | 4.3 | 0.4 | 6.4 | 539.3 | 0 | 550.4 |
| `savsched1` | 4.8 | 6.9 | 7.6 | 598.8 | 0 | 618.1 |

The symmetry search takes 16 of the 20 past the limit, most of them by
its whole cap: `bohle`'s 2,963,521,250 is 250 times its 11,854,085. The
implied bounds take `supportcase22` past it, with 239.8 million units of
passes, and `square47`: its MIP presolve is a set-up and one pass of 27.4
million units each, the implied bounds two passes of 27.4 million and
parity a scan of 27.4 million. On `supportcase19` and `ns1644855` the
root relaxation passes it: the tree ran under the whole limit, with the
MIP presolve's 11.5 and 4.4 million units on top, and the simplex stops
at the first iteration past the tree's limit. A zero
in the last column is a tree that stopped before its root relaxation, at
its first look at the limit.

## The 20 largest at 1e8

Work units, their ratio to the limit, JAOS's `time` and the wall seconds
(`before-1e8.txt`, `after-1e8.txt`):

| instance | before: work | ratio | JAOS s | wall s | after: work | ratio | JAOS s | wall s |
|---|---|---|---|---|---|---|---|---|
| `square47` | 1343046709 | 13.43 | 49.7 | 64.3 | 100023338 | 1.0002 | 4.7 | 19.3 |
| `neos-3402454-bohle` | 3040692694 | 30.41 | 170.3 | 178.1 | 100000000 | 1.0000 | 11.5 | 18.8 |
| `square41` | 667293877 | 6.67 | 19.4 | 26.8 | 100000000 | 1.0000 | 4.1 | 11.5 |
| `neos-2075418-temuka` | 2060358625 | 20.60 | 30.5 | 33.5 | 100000000 | 1.0000 | 2.5 | 5.3 |
| `neos-5114902-kasavu` | 1524217812 | 15.24 | 35.1 | 37.9 | 100000000 | 1.0000 | 2.3 | 4.7 |
| `neos-4647030-tutaki` | 1029278116 | 10.29 | 25.9 | 27.5 | 100000000 | 1.0000 | 2.9 | 4.5 |
| `neos-5052403-cygnet` | 1267103446 | 12.67 | 22.7 | 24.4 | 100000000 | 1.0000 | 2.0 | 3.6 |
| `supportcase19` | 115603870 | 1.16 | 2.3 | 5.4 | 104498291 | 1.0450 | 2.4 | 5.7 |
| `supportcase7` | 776448351 | 7.76 | 13.8 | 14.8 | 100000000 | 1.0000 | 1.9 | 3.0 |
| `roi5alpha10n8` | 639673732 | 6.40 | 30.3 | 31.7 | 100000000 | 1.0000 | 4.7 | 6.3 |
| `supportcase12` | 834841975 | 8.35 | 24.1 | 26.2 | 100000000 | 1.0000 | 2.4 | 4.3 |
| `supportcase22` | 292074725 | 2.92 | 2.3 | 3.4 | 100000912 | 1.0000 | 0.4 | 1.4 |
| `ns1644855` | 108698112 | 1.09 | 1.2 | 2.0 | 100078116 | 1.0008 | 1.0 | 1.8 |
| `neos-5104907-jarama` | 684908894 | 6.85 | 12.1 | 13.4 | 100000000 | 1.0000 | 1.6 | 2.8 |
| `neos-3402294-bobin` | 674424790 | 6.74 | 23.5 | 25.0 | 100000000 | 1.0000 | 3.7 | 5.1 |
| `bab2` | 300906421 | 3.01 | 6.2 | 7.5 | 100000000 | 1.0000 | 2.2 | 3.5 |
| `co-100` | 523501256 | 5.24 | 16.1 | 17.1 | 100000000 | 1.0000 | 3.2 | 4.2 |
| `ns1760995` | 642887212 | 6.43 | 13.9 | 15.0 | 100000000 | 1.0000 | 2.5 | 3.9 |
| `s100` | 550372866 | 5.50 | 11.8 | 14.1 | 100000000 | 1.0000 | 2.5 | 4.8 |
| `savsched1` | 618091688 | 6.18 | 14.9 | 16.4 | 100000000 | 1.0000 | 2.7 | 4.2 |

After, 16 end at 100,000,000 exactly, `supportcase22` and `square47` 912
and 23,338 units past it, the last row of the implied bounds, and the two
that reach their root relaxation at its last iteration: `ns1644855` 1.0008
after 283 iterations, and `supportcase19` 1.045, whose 196th iteration,
over 1,429,098 columns, ends 4.5 million units past the relaxation's share
of 42,752,676. `bohle` ends at the limit in 11.5 s of JAOS's time, from
170; its symmetry search now gets the 22.8 million units the steps before
it leave, of a cap of 2,963.5 million.

## Why the default runs move

With no limit from the caller, the tree reads none and takes the same
steps. The models that carry a limit of their own in a default run read
the new checks: each sub-MIP, whose cap is the smallest of 20,000 times
the model's size, its share of the tree's work (`MIP_SUBMIP_ROOT`,
`MIP_SUBMIP_SHARE`) and what is left, and the LPs of strong branching and
of the heuristics, which read their caps already. A sub-MIP now stops at
its cap where it ran past it, so the tree that started it is charged less
work, and every later choice that reads the tree's work moves with it:
when the next sub-MIP runs and with how much, the heuristics' shares, the
nodes.

`trace.patch` names the first of the new checks to fire on each instance
that moved (`first-divergence.txt`, with the line of `src/mip.c` and the
sub-MIP's cap and work). On MIPLIB 3 every first one is in a sub-MIP:

- Strong branching at a node whose relaxation spent the sub-MIP's cap
  (`p0201`, `khb05250`, `stein45`, `lseu`, `dcmulti`). Before, the probes
  ran, the kept ones again under a cap of one unit, they fed the
  pseudo-costs of a branching whose children the sub-MIP never solved,
  since its next look at the limit ended it, and their work was the
  tree's.
- A root cut round started past the cap (`rgn`): before, its separators
  ran and its re-solve stopped at once. The rows of a separator: hull
  (`bell3a`) and Gomory (`stein27`). The implied bounds of a sub-MIP whose
  cap was 503 units (`blend2`). Strong branching's probe loop (`misc06`,
  which crashed before strong branching's re-solve was kept).
- The feasibility jump (`bell5`, `misc07`). In `bell5`'s sub-MIPs the root
  relaxation spends the cap, and the jump after it ran on for up to half
  the sub-MIP's work (`MIP_FJ_ROOT`), and found points; it now stops at
  the cap.

MIPLIB 3 (`make miplib`, at the work and nodes of `miplib3-solve.txt`):
all 24 optimal before and after, 12 move, 8 of them at other nodes.

| instance | before: nodes | work | after: nodes | work | after / before |
|---|---|---|---|---|---|
| `bell3a` | 26349 | 386745646 | 26281 | 378623144 | 0.979 |
| `bell5` | 17793 | 216076233 | 38009 | 413087870 | 1.912 |
| `blend2` | 3045 | 197033647 | 2969 | 179810158 | 0.913 |
| `dcmulti` | 267 | 106710972 | 241 | 108007438 | 1.012 |
| `khb05250` | 59 | 30990600 | 63 | 33174443 | 1.070 |
| `lseu` | 245 | 12627096 | 253 | 13110417 | 1.038 |
| `misc06` | 40 | 66024621 | 42 | 54727675 | 0.829 |
| `misc07` | 8195 | 1558148889 | 8439 | 1594076913 | 1.023 |
| `p0201` | 80 | 27939243 | 80 | 27758795 | 0.994 |
| `rgn` | 193 | 10244595 | 193 | 10163977 | 0.992 |
| `stein27` | 1161 | 21170799 | 1161 | 21092911 | 0.996 |
| `stein45` | 54145 | 1979454578 | 54145 | 1979306904 | 1.000 |

Over the 24 the work's geometric mean reads 1.020 times before, and its
sum 6.081e9 from 5.881e9. `bell5` is the one that loses much: 1.91 times
its work, under the gate's factor of 2. With the feasibility jump reading
the time limit alone (`fj-time-only` in `miplib3-solve.txt`), `bell5`
reads 197,954,187, but `blend2` 315,376,043, 1.60 times before; the
geometric mean is 1.012 and the sum 5.973e9. Neither reading is better on
every instance, and the jump keeps the check: it is the one step at the
root that could run past a caller's limit by half the work done, and the
exchange search after it read the limit already.

Two tests read numbers of a default run that move the same way. The
Python suite's market split of 2 rows and 16 columns, strong branching in
a sub-MIP past its cap first, 44 times: the default run ends at
19,956,084 units and 3459 nodes from 20,665,347 and 3579, and the run with
a gap of zero at an optimum of 1.9999999999998788 from 2.
`test_a_gap_of_zero_is_zero_and_the_rule_is_settable` compared the two
optima exactly, and now to 9 places. `knapsack_rows(4, 25, 1)` of
`test_zi_rounding_stops_after_500_runs_without_an_incumbent`, the kept
probes in a sub-MIP past its cap first: ZI rounding now finds a second
incumbent and runs 646 times in 45,422 units, from 547 times for one in
38,015, still stopping 500 runs after its last incumbent.
