# 02-482 — conflicts from node relaxations that stop at the incumbent

Taken on 2026-11-18 on 5d774e5a, for TODO row J7's step on the instances
that lack bound per node: a conflict drawn from a node whose relaxation
stopped at the incumbent, its proof the stopped duals over the path's
bounds. `read.sh` takes every reading from copies of 5d774e5a with
`prototype.patch`, whose switch `JAOS_SC` turns the parts on and off (its
bits are listed in `read.sh`). The records, copied from its `OUT`:
- `arms.txt`: the six instances at 1e10 work units under each arm, with
  each arm's conflict and proof counts, and the shorter readings on
  `mas76` and the `assign1-5-8` variants;
- `miplib3/*.txt`, `m3off.txt`, `m3cmp.txt`: six arms on MIPLIB 3, against
  the prototype with every switch off (`m3off.py`) and against
  `bench/miplib.baseline` (02-474's `m3cmp.py`);
- `neos5-3e11-*.txt`: `neos5` to 3e11 under four arms;
- `scip-conflict.txt`: SCIP 10.0, one thread, with its conflict analysis
  taken apart (`scip.sh`, `scip_conflict.py`).

## The sources

- Achterberg, *Conflict analysis in mixed integer programming* (ZIB report
  05-19; Discrete Optimization 4, 2007), section 3.3, Algorithm 3.6: an LP
  that passes the incumbent is proved so by its dual solution; relax the
  path's local bounds one at a time while the dual bound stays past the
  incumbent, and the bounds left are the conflict. He notes that a dual
  simplex stopped at the objective limit passes it by little, so few bounds
  relax, and solves such LPs on to optimality for useful conflicts. On
  `mas74`, `mas76` and `pk1` his conflicts cost time: `mas76`'s 236,310
  conflicts of 20.2 bounds left it unsolved in 3600 s against 152 s.
- Witzig, Berthold and Heinz, *Experiments with conflict analysis in mixed
  integer programming* (ZIB report 16-63, CPAIOR 2017): the proof
  constraint and the conflict over bounds side by side, a pool of 1,000 to
  50,000 conflicts, conflicts that hold the incumbent dropped once it
  improves by 5%.
- Witzig, Berthold and Heinz, *Computational aspects of infeasibility
  analysis in mixed integer programming* (ZIB report 19-54, Math. Prog.
  Comp. 2021): the dual proof of a bound-exceeding LP,
  `(y'A - c)'x >= y'b - c'x*` (their (13)), valid for every improving
  point, its right-hand side tightened as the incumbent improves; at most
  75 such proofs kept, the one unused longest replaced; proofs whose
  coefficients span more than 1e8 refused; a local row's multiplier set to
  zero and the analysis stopped if the proof then fails.

## What was built

`prototype.patch`, all of it behind `JAOS_SC`; unset, the tree reads as
5d774e5a node for node (`miplib3/off.txt` reads as the baseline but for
`misc03` and `misc07`, which read as 02-483's landing moved them).

1. **The stopped duals.** When a node's dual simplex stops at the
   incumbent (02-475), the duals of its basis are published on the model,
   through presolve's row map when the node was presolved, a basic row's
   dual zero.
2. **The proof** (`stop_conflict`). Over the model's rows and the
   permanent ones, with the path's bounds, the duals give a bound
   `L = Σ y_i·(row side) + Σ d_j·(column bound)`, `d = c − A'y`; it is
   checked against the prune bound the stop was set at. A local cut with
   a nonzero multiplier refuses the proof (or, bit 32, its multiplier is
   dropped and the proof checked again). Then Achterberg's Algorithm 3.6:
   from the deepest path bound up, each is relaxed to its global bound
   while `L` stays past the prune bound; the bounds left, at most 32 and on
   integer columns only, are the conflict.
3. **The pool of conflicts** (bit 1). 1000 conflicts, the one that fired
   longest ago replaced. Before each node's relaxation every conflict is
   read against the node's bounds: all its bounds holding cuts the node,
   all but one put the opposite of the last on the node and its children.
4. **The pool of proofs** (bit 8). The proof itself,
   `d'x ≤ cutoff − Σ y_i·(row side)`, its right-hand side read from the
   current prune bound, so it tightens as the incumbent improves (Witzig et
   al.'s update); 75 kept, coefficient ranges past 1e8 refused. Propagated
   by activity: over its whole support, or (bit 128) from its activity at
   the global bounds and the path's bounds alone. A proof past the prune
   bound cuts the node; otherwise each integer column whose coefficient
   times its range exceeds the slack is tightened, as reduced-cost fixing
   does with the node's own duals.

A first build zeroed the reduced cost of every column published basic.
After a presolved stop that is wrong: postsolve calls basic a column the
node's presolve removed, whose reduced cost over the mapped duals is not
zero. `blend2` then ended at 7.6930 against its optimum 7.5990 under both
pools. The readings here zero a basic column's reduced cost only when the
stop was not presolved; bit 2048 also zeroes it after a presolved stop
when it is under 1e-9 of its terms, which `blend2` reads correctly under.

## The six at 1e10

`arms.txt`, three solves at a time. `base` is 5d774e5a; the other
arms are `prototype.patch`. A bound is the tree's at the budget; a work
figure is the close.

| arm | `gen-ip002` | `neos5` | `assign1-5-8` | `mas76` | `mas74` | `pk1` |
|---|---|---|---|---|---|---|
| base | -4803.89, 377,657 | 14.25, 116,376 | 200, 27,985 | 6.87e9, 468,999 | 11277.41 | 7.88e9, 191,637 |
| `lit`, conflicts over bounds | -4804.28, 348,928 | 14.25, 108,845 | 200, 30,822 | 8.66e9, 453,369 | 11272.52 | 8.43e9, 191,435 |
| `lit-pool100` | -4803.96, 372,092 | 14.25, 115,677 | 200 | 7.48e9, 456,907 | 11274.12 | 8.03e9, 191,605 |
| `lit-branched`, no implied bound | -4803.91 | 14.25, 112,039 | 200 | 8.15e9, 448,689 | 11272.73 | 8.34e9, 191,267 |
| `proof`, dual proofs | -4803.91, 375,539 | 14.25, 118,605 | 200 | 8.65e9, 542,929 | 11276.91 | 9.54e9, 237,237 |
| `r-proof`, roundoff zeroed | -4805.79, 293,315 | 14.25, 118,605 | | 9.97e9, 416,295 | 11258.14 | 9.54e9, 237,237 |
| `r-proof-free`, charged nothing | -4804.60, 371,553 | | | 4.60e9, 416,685 | | 8.11e9, 237,237 |
| `proof-free` | -4803.91 | 14.25, 124,422 | 200 | 8.25e9, 542,929 | 11277.10 | 8.11e9, 237,237 |
| `proof-20`, 20 proofs, one pass | -4803.91 | 14.25, 115,616 | 200 | 8.60e9, 532,499 | 11276.91 | 8.66e9, 208,707 |
| `prune`, proofs that only cut | -4803.91 | 14.25, 117,565 | 200 | 7.63e9, 465,247 | 11276.92 | 8.35e9, 191,631 |
| `prune-free` | -4803.91 | 14.25, 120,940 | 200 | 7.25e9, 465,247 | 11277.28 | 7.85e9, 191,631 |
| `both` | -4804.70, 333,466 | 14.25, 116,918 | 200 | 9.84e9, 496,049 | 11272.30 | open, bound 9 |

No arm closes `gen-ip002`, `neos5` or `assign1-5-8`, and none lifts their
bound at 1e10: `gen-ip002` ends at or under its -4803.89 in every arm,
`neos5` and `assign1-5-8` where they were. On the two that close, every
arm that is charged its work closes later than the base. The gains come
with the propagation charged nothing: `mas76` under the proofs closes at
4.60e9 work units against 6.87e9, 180k of its 417k nodes cut before their
relaxation, where charged their propagation is 4.8e9 work units and the
close 9.97e9; `pk1` under `prune-free` at 7.85e9 against 7.88e9. `pk1`
grows under the proofs that tighten, 237,237 nodes against 191,637, free
or not.

`assign1-5-8` is the same in every arm: no proof is made. The 2,060 nodes
that stop at 1e10 (`lit`) need the local cuts of their path in 1,616 cases
and fail the check on the path's bounds in the other 444; dropping the
local cuts' multipliers (bit 32) fails every one, and reading the node's
own bounds, which hold its clique fixings (bit 256), passes 444 of which
439 need more than 32 bounds. Without bit 2048 a proof drawn after a
presolved stop keeps the roundoff in its basic columns' reduced costs: on
`gen-ip002` it meets an infinite bound in 12,461 of 22,116 stops, and
`mas76`'s and `mas74`'s proofs then span more than 1e8 in all but at most
ten, so the `proof`, `prune` and `proof-20` arms hold almost no proof on
those three. Bit
2048 zeroes that roundoff (`r-proof`).

## Why it does not help

**Few nodes stop, and a proof cuts only nodes that stop.** The base's
counts at 1e10:

| | `gen-ip002` | `neos5` | `assign1-5-8` | `mas76` | `mas74` | `pk1` |
|---|---|---|---|---|---|---|
| nodes | 377,657 | 116,376 | 27,985 | 468,999 | 321,147 | 191,637 |
| relaxations stopped at the incumbent | 22,363 | 5,004 | 1,551 | 231,951 | 3,488 | 41,972 |
| share | 5.9% | 4.3% | 5.5% | 49.5% | 1.1% | 21.9% |

A dual proof over the global rows bounds any node's relaxation from below
over that node's box, so it cuts a node only when the node's own
relaxation would pass the incumbent too: it saves that relaxation's work
and cannot cut a node whose relaxation sits under the incumbent. On the
three open instances under 6% of the nodes stop inside 1e10, and the open
nodes, whose bound is the tree's, sit under the incumbent by construction.
(Past the budget the share grows, and on `neos5` the pools then shrink
the tree, below.) What
can lift them is what the integrality of a column adds to a proof, the
rounding of the bounds it implies and the conflicts' fixings. The proofs
round 2.1M bounds on `gen-ip002` (`r-proof`; reduced-cost fixing at every
node already rounds 2.2M with each node's own duals) and its bound falls;
on `neos5` they round 229,537 and cut 11,140 nodes, and the bound stays.

**The conflicts over bounds are long.** The stopped dual passes the prune
bound by little, as Achterberg found, so few of the path's bounds relax:

| `lit` | conflicts | bounds each | implied bounds among them | nodes cut | bounds fixed |
|---|---|---|---|---|---|
| `gen-ip002` | 8,295 | 19.3 | 57% | 9 | 1,420 |
| `neos5` | 4,527 | 14.3 | 14% | 4,550 | 131,294 |
| `mas76` | 217,860 | 16.8 | 28% | 1,892 | 46,846 |
| `mas74` | 2,143 | 14.5 | 1% | 13 | 2,353 |
| `pk1` | 41,736 | 14.7 | 13% | 39 | 1,575 |

Implied bounds are those reduced-cost, orbital or conflict fixing set;
refusing them (`lit-branched`) leaves fewer, as long, conflicts. Solving
`mas76`'s stopped relaxations on to their optimum for the duals
(bit 64, `lit-optimal-2e9`) shortens its conflicts from 13.6 to 12.5
bounds at 2e9 work units, at the cost of the work the stop saves.
`gen-ip002`'s 41 general integers make its conflicts boxes of 19 bounds
that another node rarely falls in: 9 nodes cut in 348,928.

## MIPLIB 3

`miplib3/*.txt`, J=2, against `off`, the prototype with `JAOS_SC` unset
(`m3off.txt`; against `bench/miplib.baseline` in `m3cmp.txt`, where `off`
reads 0.999x because 5d774e5a moved `misc03` and `misc07` without
rewriting the baseline):

| arm | geometric | total | worst |
|---|---|---|---|
| `lit-pool100` | 1.013x | 1.062x | `misc07` 1.157x |
| `proof` | 1.014x | 1.058x | `misc07` 1.108x |
| `proof-free` | 1.010x | 1.044x | `misc07` 1.115x |
| `prune` | 1.015x | 1.060x | `misc07` 1.112x |
| `r-proof` | 1.012x | 1.055x | `misc07` 1.108x |
| `r-proof-free` | 1.008x | 1.041x | `misc07` 1.115x |

All 24 solve with their objective under every arm. Only `misc03` (0.942x)
and `blend2` (0.93x under `r-proof`) gain; `misc07`, `stein45`, `p0282`
and `stein27` lose, even with the propagation charged nothing.

The first build's zeroing read 0.942x geometric under the proofs, `bell3a`
0.40x and `bell5` 0.61x, but it cut `blend2`'s optimum: those gains were
the invalid proofs' (with the zeroing fixed, `bell3a` reads 1.029x).

## SCIP

`scip-conflict.txt`, SCIP 10.0, one thread. `boundlp-off` leaves out the
analysis of LPs past the incumbent, which keeps the analysis of infeasible
ones as JAOS has it; `boundlp-graph` keeps only its conflicts over bounds
through the conflict graph, `boundlp-dual` only its dual proofs.

| | default | no conflicts | `boundlp-off` | graph only | proofs only |
|---|---|---|---|---|---|
| `mas76`, nodes to close | 216,712 | 334,009 | 303,847 | 256,477 | 272,955 |
| `pk1`, nodes to close | 353,721 | 390,266 | 394,266 | 353,721 | 394,266 |
| `mas74`, bound at 1M nodes | 11293.07 | 11319.26 | 11319.41 | 11273.13 | 11294.64 |
| `neos5`, bound at 300k nodes | 14.2273 | 14.2335 | 14.2335 | 14.2500 | 14.2578 |
| `gen-ip002`, bound at 1M nodes | -4806.65 | -4806.35 | -4806.90 | -4805.78 | -4805.86 |
| `assign1-5-8`, bound at 50k nodes | 199.05 | 199.42 | 199.94 | 197.97 | 199.94 |

SCIP's analysis of LPs past the incumbent is worth 1.40x the nodes on
`mas76` and 1.11x on `pk1`, most of it its conflicts over bounds, which
its conflict graph shortens and which JAOS, with no record of why a bound
was implied, cannot. On the open three it does what it does in JAOS: it
moves `neos5` by under 0.03, `gen-ip002` by 0.25 to 1.1 of a 23-unit gap,
and lowers `assign1-5-8`'s bound.

## `neos5` to its close

At 1e10 `neos5`'s bound stays at 14.25 in every arm, but its tree closes
well past the budget, at 1.91e11 in 2,977,069 nodes (02-481's record,
taken on e1932ad3, whose `neos5` reads as 5d774e5a's node for node at
1e10). `neos5-3e11-*.txt`, the arms given 3e11 work units:

| arm | nodes to close | work at close | stopped | cut by the pool | bounds fixed or tightened |
|---|---|---|---|---|---|
| the record | 2,977,069 | 1.91e11 | 413,368 | | |
| `lit` | 1,752,259 | 1.29e11 | 206,357 | 87,948 | 3,530,338 |
| `proof` | 1,264,193 | 7.38e10 | 82,136 | 257,320 | 4,205,001 |
| `proof-20`, 20 proofs, one pass | 1,511,819 | 9.68e10 | 161,896 | 72,169 | 2,130,387 |
| `prune`, proofs that only cut | 3,967,349 | 2.04e11 | 288,702 | 787,108 | 0 |

The nodes a pool cuts count among the nodes. Over the whole tree 14% of
the record's nodes stop, against 4.3% in its first 1e10, and the share
the pools reach grows with it: the dual proofs take the tree to 0.42x its
nodes and 0.39x its work, the conflicts over bounds (11.7 bounds each
here) to 0.59x and 0.67x. That is HiGHS's finding on `neos5` (02-481: its
pools, conflicts above all, are its largest part, 2.4x on the bare tree).
It is still 7.4 times the budget, and inside 1e10 the bound does not move.

What shrinks the tree is the proofs' tightening, not their cutting:
proofs that only cut leave 3,180,241 relaxations to solve against the
record's 2,977,069, and close later for the propagation's 8.5e9 work
units. The cut nodes are nodes whose relaxation would have stopped, as
above; the tightened bounds, rounded down on integer columns, are what a
node's own relaxation does not hold.

## The verdict

Refused (`stop-conflicts` in `bench/refusals.txt`). Nothing closes:
`gen-ip002`, `neos5` and `assign1-5-8` end at 1e10 where 5d774e5a leaves
them in every arm, `gen-ip002` a little lower under most, even with the
pools' propagation charged nothing. Inside the budget few of their node
relaxations reach the incumbent, a proof cuts only nodes whose own
relaxation would, the conflicts over bounds are 14 to 19 bounds long
because the stopped dual passes the prune bound by little, and
`assign1-5-8`'s proofs need its local cuts. The `mas76`/`pk1` pair, which
SCIP's conflicts shrink by 1.40x and 1.11x, close later in every arm
charged its work, and MIPLIB 3 reads 1.008x to 1.015x geometric and 1.04x
to 1.06x in total, `misc07` 1.11x to 1.16x.

What the readings leave standing is `neos5` past the budget: the dual
proofs close it in 1,264,193 nodes and 7.38e10 work units against the
record's 2,977,069 and 1.91e11, under the 1,456,423 of 02-481's smallest
tree. It comes from the bounds the proofs tighten, and the same
tightening is what costs elsewhere: the losses on `misc07`, `stein27`,
`stein45` and `pk1` stay with the propagation free (`misc07`'s tree 1.08x,
`pk1`'s 1.24x), so they are the tree's, not the propagation's cost, and on
`pk1` the proofs that only cut close at 7.85e9 free against 8.11e9 for
those that also tighten. The tightened bounds move the branching, as D324
found for propagation that writes its bounds back. A form worth landing
tells `neos5`'s tightenings from `pk1`'s and `misc07`'s.

`make test` and the other gates were not run: nothing lands, and the
prototype stays in `prototype.patch`.
