# 02-459 — strong-branching probes that learn from their stop, refused

Taken on 2026-11-15 on the tree of 7d748d7 with `probe-bound.patch`, for
TODO row J7. `read.sh` takes the MIPLIB 2017 readings (`full/`); the
nine-instance readings (`nine/`) and the MIPLIB 3 ones (`miplib3/`) were
taken with the same build, by hand. `bisect-csched008.sh` reads one
instance across commits (`bisect-csched008.txt`).

## What was built

TODO J7 named the step: strong-branching probes as a few dual steps on the
node's factors, since a probe is a full LP solve and reliability branching
is off by measurement (D293). Two refusals said what would reopen it: a
probe that learns from an unfinished child solve through the dual bound at
a work-limit stop (D293), and a simplex that stops a probe only in dual
phase 2 and reports its dual objective (`probe-lagrange-bound`, 02-360).

`probe-bound.patch` does that. When a node relaxation the tree marks as a
probe stops at its work cap in the dual simplex, the simplex publishes the
Lagrangian bound of its last basis: y = B^-T c_B with the costs unshifted
and unperturbed, and each nonbasic variable's d·b at the real bound its
reduced cost points to, no bound if that bound is infinite. It is valid for
any y, and in dual phase 2 it is the dual objective. `strong_probe` learns
the pseudocost from it where it learnt nothing from a capped probe before.
`J7_PROBE_BOUND=1` turns it on, so one build reads both. `J7_PROBE_SHARE`
skips strong branching at a node while the probes have spent more than
that share of the tree's work. The probes still refactor; the node's
factors are not reused.

## MIPLIB 2017, 30 instances at 1e10 work units

02-328's `m17sum.py` (lower is better; each instance's primal and dual gap
capped at 1). `rel4` is `--reliability 4`, `cap1` `--probe-cap 1`, `b` the
bound learned, `s10` a 10% share, `d5` `--probe-depth 5`:

| configuration | primal | dual | sum |
|---|---|---|---|
| base (HEAD, no strong branching) | 7.0744 | 6.5961 | 13.6705 |
| rel4cap1 | 9.2592 | 6.5275 | 15.7867 |
| rel4cap1b | 8.1178 | 6.4642 | 14.5820 |
| rel4cap1b, s10 | 6.9663 | 6.5474 | 13.5137 |
| rel4cap1b, d5 | 8.1987 | 6.6237 | 14.8224 |

The bound improves the probing mode (15.79 to 14.58), and with the 10%
share probing beats base by 1.1%: the dual bound closes on `pk1` (0.256
of its optimum to 0.232), `assign1-5-8`, `mas76`, `gen-ip054`, `gen-ip002`
and `supportcase26`, and `timtab1`'s incumbent comes from 0.68 to 0.52 of
its optimum away, while `mas74`, `neos-3754480-nidda`, `beasleyC3` and
`neos-3627168-kasai` find worse points. The share is checked only before
a node's probes, so one node's sixteen can outrun it: on `beasleyC3`
strong branching takes 40.8% of the work under the 10% share, and the
node relaxations after it take 2,240 iterations each against 1,470, so
the tree reaches 257 nodes where base reaches 5,837.

The nine-instance reading (`nine/`: the seven instances whose incumbent is
the optimum, `beasleyC3` and `neos-911970`) chose the configurations above
from eight; on those nine no configuration beats base.

## MIPLIB 3, the set the configurations were not chosen on

Against `bench/miplib.baseline` (02-457), every instance at its optimum:

| configuration | total work | nodes | the gate |
|---|---|---|---|
| rel4cap1 | 1.155x | 174,497 to 139,318 | 5 regressed: `p0201` 4.7x, `l152lav` 2.8x, `misc03` 2.6x, `mod008` 2.2x, `enigma` 2.1x |
| rel4cap1b | 1.228x | 174,497 to 156,940 | 4 regressed: `enigma` 6.0x, `p0201` 4.0x, `l152lav` 2.8x, `misc03` 2.8x |
| rel4cap1b, s10 | 0.998x | 174,497 to 165,452 | 1 regressed: `misc03` 4.2x |

`blend2`, `bell5`, `gt2` and `misc07` take far less under probing, and
`misc03`, `enigma`, `p0201` and `p0282` far more. The bound makes the
probing mode worse here, `enigma` from 2.1x to 6.0x.

## The verdict

Refused. The share configuration gains 1.1% on MIPLIB 2017 and fails the
MIPLIB 3 gate on `misc03`; the bound alone helps the probing mode on one
set and hurts it on the other. `bench/refusals.txt`'s D293 and
`probe-lagrange-bound` lines carry what reopens them. Nothing lands.

## What else the reading found

`bench/results/miplib2017.txt`, last written on 2026-11-11 at 32f2863,
moves on HEAD from a sum of 13.4820 to 13.6705: `timtab1`'s incumbent
from 1,128,183 to 1,284,815, and `csched008` from 57 nodes to 1, with no
incumbent either way. `bisect-csched008.sh` puts the node count at
00e975c; the log there gives the heuristics 72.2% of the work against
53.8%, so a root heuristic outruns its share since then. `make
miplib2017` rewrites the record with this reading.

The seven whose incumbent is the optimum (`gen-ip002`, `gen-ip054`,
`mas76`, `pk1`, `neos5`, `assign1-5-8`, `binkar10_1`) are not where
JAOS trails the field: in the comparison taken after this reading
(`bench/compare/README.md`, 2026-11-15), HiGHS and SCIP close only
`binkar10_1` of them in 20 s. The instances they solve and JAOS does not
are `beasleyC3`, `binkar10_1` and `neos-911970`. In that reading JAOS
solves 5 of the 30 in 20 s against 0 on 2026-09-24, and its shifted mean
is 0.97x HiGHS's and 1.08x SCIP's.
