# 02-328 — MIP: the objective step, implied fixings, two heuristics and network cuts

Taken on 2026-10-04 on the tree of 0fed051, for TODO row J7. The 2017 set
runs at 1e10 work units, J=2 and 5 GB a solve; MIPLIB 3 at J=2 and 4 GB.
`arms.patch` is the measured tree: the batch behind environment switches,
with the arms below and the two refused at the end. `arms.sh` runs one arm.
The shipped code has no switches and writes the records of the arm `fin`
line for line.

## What HiGHS does that JAOS did not

`highs-root.txt` holds HiGHS 1.15.1's root on seven 2017 models, one
thread, node limit 1. On `sp150x300d` its presolve leaves the root
relaxation at 34.14, where JAOS read 4.89: a node with a demand and one arc
in forces that arc's flow above zero, so its binary is 1, and HiGHS fixes
29 binaries before the LP. Its cuts then reach 68.70, and since every
objective value is a whole number (its log says "Objective function is
integral with scale 1"), 68.70 proves the optimum 69. On `p200x1188c`,
`exp-1-500-5-5` and `beasleyC3` its root cuts reach 11640, 65341 and 733,
where JAOS's reached 9865, 59208 and 393; on `neos-911970` and
`neos-3381206-awhea` 52.1 and 451.8, where JAOS's stayed at the
relaxation's 23.26 and 415.24. And it finds near-optimal points at the
root with sub-MIPs ("L" in its log).

## The batch

1. **The objective step.** When every column with a cost is integer and
   the costs share a step (`MIP_OBJ_DENOM`), every integer point's
   objective is `offset + k step`, and the tree rounds a bound up to the
   next such value before it compares it with the incumbent.
2. **Implied fixings.** Before the root's coefficient tightening, passes
   over the rows work out implied bounds for every column
   (`MIP_IMPLIED_PASSES`). A continuous column's implied bounds feed the
   tightening; an integer column the passes fix at one value is fixed for
   the tree.
3. **Lock rounding.** At the root, after the pump: the relaxation rounded
   in the direction no row locks (a column no row can be broken by raising
   is rounded up), and each integer column moved to the side no row locks;
   the integer columns fixed there and the rest solved.
4. **A sub-MIP heuristic** (`MIP_SUBMIP_*`): RENS at the root without an
   incumbent, RINS at the root with one and every 100 nodes below it, each
   a node-limited MIP over the columns left open, under a work share.
5. **Network cuts.** Where a third or more of the continuous columns sit
   under a binary through a row `a x + c y <= 0` (`MIP_NET_*`), the root
   runs 20 rounds of MIR with variable bound substitution (Marchand and
   Wolsey's c-MIR, each base row tried with the nearest-bound rule and
   with the rule that takes a column's lower bound when its coefficient is
   positive), the aggregated cuts kept without the probe of
   `MIP_MIR_AGG_GAIN`, at most 200 cuts a round by efficacy with no two
   more parallel than 0.5, a stall of 1e-4, and the root's dive and pump
   capped at a quarter of the root's work once an incumbent exists.

## The arms

MIPLIB 3 against `bench/results/miplib.txt` (0fed051), the geometric mean
and the sum of each instance's work ratio, all 24 solved at the reference
in every arm but `impl`; the 2017 set against `bench/results/miplib2017.txt`
by `m17sum.py`, which counts a solved instance as no gap and caps each
instance's primal and dual gap at 1.

| arm | switches | MIPLIB 3 geo | sum | worst | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|---|---|
| base | | 1 | 1 | | 0 | 17 | 16.247 | 9.773 | 26.020 |
| impl | every tightened integer bound | 0.909 | 1.047 | `bell5` out of memory at 4 GB | | | | | |
| impl2 | the fixings alone | 0.907 | 1.021 | `bell3a` 1.364 | 0 | 17 | 16.295 | 9.773 | 26.068 |
| grid | the objective step | 0.941 | 1.054 | `l152lav` 1.190 | | | | | |
| gridimpl | both | 0.854 | 1.075 | `bell3a` 1.364 | 1 | 17 | 16.295 | 9.750 | 26.045 |
| locks | gridimpl, lock rounding | 0.859 | 1.075 | | | | | | |
| submip | gridimpl, root sub-MIP, no share | 0.940 | 1.080 | `khb05250` 1.505 | | | | | |
| sub0.5 | the same at half the root's work | 0.888 | 1.077 | | | | | | |
| sub2 | the same at twice | 0.930 | 1.080 | | | | | | |
| heur | gridimpl, lock rounding, root sub-MIP | 0.946 | 1.081 | `bell3a` 1.374 | 1 | 20 | 13.339 | 9.751 | 23.089 |
| cuts | heur, the network rounds on every model | 1.187 | 2.011 | `bell5` 64.4 | 2 | 18 | 15.468 | 8.609 | 24.077 |
| net | heur, the network rounds behind the gate | 0.919 | 1.080 | `bell3a` 1.365 | 2 | 19 | 13.641 | 9.298 | 22.939 |
| fin | net, the sub-MIP below the root | 0.957 | 1.109 | `bell3a` 1.515 | 2 | 20 | 12.133 | 9.334 | 21.467 |

`fin` is the default. On MIPLIB 3 every objective is at the reference,
every answer passes the checker and repeats, and no instance passes 2x:
`bell3a` 1.515x (65901 to 82261 nodes), `misc06` 1.308x, `dcmulti` 1.338x,
`blend2` 1.231x, `gen` 1.211x, `l152lav` 1.191x and the small trees pay
for the heuristics (`khb05250` 1.125x, `p0033` 1.129x, `rgn` 1.146x);
`egout` 0.118x (637 to 3 nodes), `stein27` 0.560x, `mod008` 0.626x and
`stein45` 0.718x. The sum is `l152lav`'s: 1.3e10 of 2.3e10.

## The 2017 set, base against fin

| instance | base incumbent | base bound | fin incumbent | fin bound | reference |
|---|---|---|---|---|---|
| sp150x300d | 69 | 68.098 | optimal | | 69 |
| exp-1-500-5-5 | 85205 | 61357 | optimal | | 65887 |
| p200x1188c | 38071 | 10738.5 | 15078 | 10930 | 15078 |
| beasleyC3 | none | 434.0 | 981 | 695 | 754 |
| neos-911970 | none | 29.98 | 55.60 | 29.78 | 54.76 |
| mad | none | 0 | 0.2088 | 0 | 0.0268 |
| mas74 | 14769.7 | 11225.6 | 12180.1 | 11213.8 | 11801.2 |
| neos-3754480-nidda | 14208.6 | -495818 | 13362.3 | -505572 | 12941.7 |
| binkar10_1 | 6747.31 | 6713.54 | 6746.76 | 6713.14 | 6742.20 |
| tr12-30 | 195766 | 88461.6 | 191315 | 89334.5 | 130596 |
| neos-3627168-kasai | none | 949169 | none | 958700 | 988586 |
| neos-3381206-awhea | none | 415.24 | none | 416 | 453 |
| enlight_hard | none | 22.5 | none | 23 | 37 |
| glass4 | none | 800004200 | none | 800003901 | 1200012600 |

Of the other 16, three bounds fall as the sub-MIPs take their share of
the work: `pk1` from 8.34 to 8.09, `neos-3046615-murg` from 435 to 431 and
`supportcase26` from 1473.4 to 1469.8. The rest move by less than 1e-3 of
themselves.
`exp-1-500-5-5` solves at 2.9e9 work units, its root at 65144 against the
optimum 65887 and the optimum found at the root by the sub-MIP after lock
rounding; `sp150x300d` at 7.6e8 (4472 nodes), from the fixings' 34.14 and
the step that rounds its last bound of 68.1 to 69.

## The cut check

`fcnet.sh RUNS SEED OUT` builds a library with
`-DJAOS_MIP_NET_MIN_COLS_VALUE=1`, so small models take the network rounds,
and runs `fcnet.c`: generated fixed-charge networks of 3 to 6 nodes and 3
to 11 arcs, each answer judged against brute force over every value of the
integer columns. Seeds 1 to 5 at 2000 models each:

| seed | models | with cuts | optimal | infeasible | failed | digest |
|---|---|---|---|---|---|---|
| 1 | 2000 | 923 | 1214 | 786 | 0 | 19f996539dfa9c86 |
| 2 | 2000 | 900 | 1215 | 785 | 0 | 8d03f3cacc1ad431 |
| 3 | 2000 | 895 | 1184 | 816 | 0 | 93700d35b45d08a0 |
| 4 | 2000 | 893 | 1197 | 803 | 0 | 6c31b36882b54a3f |
| 5 | 2000 | 903 | 1239 | 761 | 0 | af128ab6cafdfc0d |

The control lowers the right-hand side of every network MIR cut by 1 (in
`cmir_one`, `rhs = floor(beta) - 1.0`): 350 of seed 1's first 1000 models
fail.

## Refused

**The deep restart** (`deep` and `full` above, `JAOS_DEEP` in the patch):
once a tree has spent 2e9 work units still open, start again from the root
with the incumbent, the root's integer bounds, the network rounds, 20
rounds of MIR on simplex tableau rows (each row's slacks substituted at
their nearest bound) and no stall, the first tree's open bound kept as a
floor. `full` is `fin` with it:

| arm | MIPLIB 3 geo | sum | 2017 solved | primal | dual | gap sum |
|---|---|---|---|---|---|---|
| fin | 0.957 | 1.109 | 2 | 12.133 | 9.334 | 21.467 |
| full | 0.978 | 1.044 | 2 | 12.205 | 9.012 | 21.217 |

It lifts `neos-911970`'s bound from 29.8 to 51.7 (HiGHS's root: 52.1) and
`neos-3381206-awhea`'s from 416 to 446, but eight bounds fall
(`timtab1` 414405 to 401446, `pk1` 8.09 to 7.15), since the restart drops
what the first tree proved past the floor; `misc07` reads 1.44x. In
`bench/refusals.txt` as mip-deep-restart. On those two models the root
bound stays flat for 5 to 10 rounds and then climbs; the default's 6 MIR
rounds stop before it moves.

## Left for J7

`p200x1188c` has its optimum and a bound of 10930 (HiGHS's root, before
its restarts, 11640); `beasleyC3` 981 against 754 with a bound of 695; a
node there costs 1e7 work units. `neos-911970` and
`neos-3381206-awhea` need the deep rounds without the restart's losses.
`binkar10_1` holds the reference's point within 3e-6 and a bound 0.4%
short. `enlight_hard` and `neos-3381206-awhea` have no point at all.
