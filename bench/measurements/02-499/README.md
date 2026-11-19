# 02-499 — ZI rounding stops after 500 runs without an incumbent

Taken on 2026-11-19 on 4e9f5c2c and on the change above it, for the ZI note
of TODO row J32. The note read ZI's cost on MIPLIB 3 as 1.037x geometric
and 1.078x in total, `misc06` 1.43x at the same 40 nodes. The maintainer
kept ZI on, asked for a stop once it stops finding incumbents, asked why
`misc06` pays, and set the bar: under about 1.03x geometric against ZI
off, or ZI goes off by default. `jaos_set_mip_zi_round` (`--no-zi-round`,
`mip_zi_round`) is the switch the readings compare against.

`read.sh` takes the readings from copies of 4e9f5c2c (`base`, ZI's old
stop), of the landed commit (`final`, and `nozi` with ZI off), of the
landed commit with `khook.patch` (the stop read from `JAOS_ZI_K`) and with
`diag.patch` (two switches that close ZI's channels into the tree):

- `miplib3/*.txt`, `m3cmp.txt`: MIPLIB 3 through `bench/run`, two solves
  at once, against `nozi`; `miplib3/gate-final.txt` is `final` against
  `bench/miplib.baseline`;
- `miplib3-99bb738e/`, `m3cmp-99bb738e.txt`: the same sweep taken a day
  earlier on 99bb738e, before 02-496's presolve fix;
- `logs3/*.txt`: per instance, the summary log's ZI and crossover lines
  through the CLI; `logs3/channels.txt` puts the diagnostic readings side
  by side;
- `miplib2017/`, `m17cmp.txt`, `sum.txt`: MIPLIB 2017 at 1e10 through the
  CLI, `final` and `nozi`, against 4e9f5c2c's record.

## `misc06` pays for the root's crossover

On 4e9f5c2c `misc06` runs ZI at 38 of its 40 nodes, every node whose
relaxation is fractional (the other two end integral), for no incumbent:
100,521 work units, 2,645 a run, 0.15% of its 66.0M. With ZI off it takes
65.9M at the same 40 nodes. The 1.43x against 02-483's baseline is one
crossover at node 1: the pool holds the points of lock rounding, the dive
and RINS, and crossover runs under half the root's work
(`MIP_SUBMIP_ROOT`) and spends 16.4M for no incumbent. 02-488 read it
already: `lns` there reads `misc06` 1.432x and `lns-noroot`, the same
commit without the root's crossover, 1.000x. The note's 1.037x and 1.078x
are 02-488's sub-MIPs, ZI and 02-492's probes together.

## Where ZI's cost goes

ZI's own work is 0.25% of MIPLIB 3's, 16.4M of 6.43e9, and at most 1.5% of
any instance's (`stein27`; `bell3a` 0.98%, `misc07` 0.28%, `blend2`
0.22%). Against ZI off, 4e9f5c2c reads 1.043x geometric and 1.061x in
total, so the cost is the trees'. `diag.patch` finds the channels
(`logs3/channels.txt`, on the landed commit). Of the 20 instances that run
ZI, 14 build ZI off's tree node for node and unit for unit once ZI's work
stays out of the tree's count (`dwork`), `blend2` and `misc07` among them;
16 once its whole points that do not beat the incumbent stay out of the
pool as well (`dboth`). The other four are `bell3a` and `bell5`, where ZI
finds incumbents, and `mod008` and `stein45`, which still move by 0.1% and
2%.

The tree's work count sets strong branching's share (`MIP_PROBE_SHARE`),
RINS's (`MIP_SUBMIP_SHARE`), crossover's, mutation's and DINS's
(`MIP_CROSS_SHARE`, `MIP_MUT_SHARE`) and the root sub-MIPs' half. ZI's few
units move where those limits fall, a sub-MIP stopped elsewhere returns
other points or none, and the tree follows: with ZI off, RINS gives
`blend2` its optimum at node 1009 and the tree closes in 2,775 nodes; with
ZI on, the optimum comes as an integral node at node 1994, and the tree
takes 3,835. On `mod008` the pool is the channel: ZI's 29 whole points let
crossover run a sub-MIP of 486,811 units where, without them, the pool's
three best agree on too few columns and it stops at 957.

## The stop

ZI now stops for good once `MIP_ZI_TRIAL` runs in a row have given it no
incumbent, counted from the tree's first node or from its last incumbent.
Before, 1000 runs with none stopped it, and one incumbent kept it on for
good. Against ZI off, geometric and total work:

| stop | 4e9f5c2c | | 99bb738e | |
|---|---|---|---|---|
| old: 1000, none after an incumbent | 1.043x | 1.061x | 1.034x | 1.053x |
| 1000 | 1.040x | 1.057x | 1.032x | 1.050x |
| 500 | 1.013x | 0.970x | 1.005x | 0.963x |
| 300 | 1.025x | 0.981x | | |
| 200 | 1.010x | 0.987x | 1.002x | 0.981x |
| 100 | 1.036x | 1.003x | 1.027x | 0.996x |
| 50 | 1.031x | 1.016x | 1.022x | 1.008x |
| 20 | 1.018x | 1.007x | 1.010x | 1.000x |

Single instances swing by tens of percent between neighbouring stops,
through the channel above: `blend2` reads 1.090x at 500, 1.579x at 300,
1.067x at 200 and 2.043x at 100. 500 and 200 read lowest on both commits;
500 keeps ZI the longer, and landed.

On MIPLIB 3 the stop moves six instances against 4e9f5c2c's record and
leaves 18 as they were: `bell3a` 0.970x (ZI 14,177 runs to 510, its two
incumbents at nodes 3 and 12), `blend2` 0.675x, `misc07` 0.821x and
`stein45` 0.953x (1000 runs to 500 each, none with an incumbent) change
their trees; `bell5` 0.975x keeps its 11 ZI incumbents, the last at node
1,281, and runs ZI 1,543 times for 8,960, and `stein27` 0.993x runs it 500
times for 638, both at the same nodes. Against the record the set reads
0.971x geometric and 0.915x in total; against ZI off 1.013x and 0.970x.
The largest against ZI off: `p0282` 1.158x, where ZI's incumbent at node
129 moves the tree under both stops, `bell5` 1.112x, `blend2` 1.090x,
`mod008` 1.047x, `bell3a` and `stein27` 1.030x; `misc07` 0.888x and `lseu`
0.962x.

## MIPLIB 2017

At 1e10 work units through the CLI, two solves at once, against
4e9f5c2c's record (`sum.txt`, `m17cmp.txt`, `miplib2017/*-who.txt`):

| | primal | dual | sum |
|---|---|---|---|
| 4e9f5c2c | 5.1155 | 6.1468 | 11.2624 |
| the stop (`final`) | 5.2342 | 6.1518 | 11.3860 |
| ZI off (`nozi`) | 3.9373 | 6.1482 | 10.0855 |

ZI finds incumbents on the three models it found them on in 02-489 and
keeps every one: `beasleyC3` its nine, all by node 99, in 599 runs (6,556
in 02-489's landed reading); `gen-ip002` six in its first tree's 1000 runs
and one at node 331 of its third tree, whose ZI now stops after 831 runs
where it ran 309,127 for 2.4e8 work units; `gen-ip054` two. Its work
over the set falls from 3.4e8 work units to 4.5e7, at most 7.4e6 on one
model. As in 02-489, none of its points is a model's last incumbent.

The primal side moves on models where ZI finds nothing. `timtab1` reads
0.283 on the record, 0.398 with the stop and 0.148 with ZI off; `mas74`
0.020, 0.032 and 0.005; `markshare_4_0` reaches its optimum with ZI off,
by RINS, 1.0 to 0. Of the 1.30 between the stop and ZI off, 1.0 is
`markshare_4_0` and 0.25 `timtab1`. These are the trees' paths, through
the channel MIPLIB 3 shows: 02-474's six readings of one code, with only
the root's work moved, span 10.85 to 11.88. The nine solved models stay
solved; `mas76` takes 6.33e9 work units against the record's 5.34e9
(6.29e9 off), `gen-ip054` 1.20e9 against 1.39e9 (1.28e9 off).

## The gates

On the branch over f75117aa: `make test` passes, its Windows step
skipping the native Python suite as f75117aa has it do on this machine.
`make sanitize` and `make python-test` pass. `netlib`, `netlib-infeas` and `netlib-kennington` read their
baselines with 0 at other nodes and rewrite their records byte for byte.
`make miplib` passes with 0 regressed and 4 at other nodes, `bell3a`,
`blend2`, `misc07` and `stein45`, each in less work; the baseline is
rewritten with `miplib-baseline`, which also takes in 02-496's moves of
at most 1% (`gt2`, `lseu`, `p0033`, `p0282`). `make miplib2017` reads as
`final` instance for instance: 9 solved, 29 incumbents, the sum from
11.2624 to 11.3860.

## The verdict

ZI stays on with the stop, by the bar the maintainer set: its MIPLIB 3
cost against ZI off falls from 1.043x geometric to 1.013x, and in total to
0.970x. Its own work is a quarter of a percent of the set's; what it costs
is the path its units send the work-proportional shares down, and that
path is as likely to help as to hurt. MIPLIB 2017 does not separate ZI
from that noise either way: no final gap there owes anything to its
points, and ZI off reads lower only where ZI finds nothing. Next, in J32:
shares that read the work of the node relaxations alone, which would keep
a cheap heuristic's units from moving the tree, and would let a reading
see what ZI's points are worth.
