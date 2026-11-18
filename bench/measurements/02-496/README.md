# 02-496 — a forcing row read through the other side's bounds

Taken on 2026-11-18 on 99bb738e. 02-491's reading of the 240 MIPLIB 2017
benchmark instances ended `eil33-2` optimal at 1295.807734 after 3 nodes;
its optimum is 934.007916.

## The chain

- `--no-rcfix`, `--cut-rounds 0` and `--algorithm primal` each give
  934.008. A start at 1295.81 with `--no-heuristics` still gives 1295.81,
  so the heuristics are not in it.
- At the root, after its 6 Gomory cuts, the reduced costs match c - A'y
  to 7e-12, no column or row is dual infeasible, the optimum satisfies
  all 38 rows, and no root fixing cuts it off.
- Both children stop on the node cutoff. Node 2 holds the optimum in its
  box and in its 36 rows, yet its LP, written out, is infeasible under
  the dual, the primal and the barrier, and optimal at 816.524 with
  presolve off.
- Presolve calls rows `c2` and `c3`, both `= 1` over columns at lower
  bound 0, forcing on the upper side, and fixes their 1320 and 698 columns
  at 0, among them `x2298`, which the optimum sets to 1. The test read
  min_act 0 against ru 1 with a tolerance of 1.5: 8 eps times the row's
  traffic, 8.4e14, nearly all of it from the upper bounds near 1e14 that
  root reduced-cost fixing gives columns whose reduced cost is near 0.

The same holds on a model of two columns: min x1 + 2 x2 with x1 + x2 = 1,
x1 in [0, 1e15], x2 in [0, 1] ended optimal at 0 with the row off by 1.

## The change

The forcing, redundancy and infeasibility tests of a row now read each
side against that side's own traffic: min_act against the terms that
make it, max_act against theirs. The tolerance is the same 8 eps; only
its scale changed. `tests/test_presolve.c` holds the two-column model.

After it: the two-column model is optimal at 1, node 2's LP at 816.524
with presolve on, and `eil33-2` optimal at 934.007916 in 13071 nodes and
33 s.

## The gates

`make test`, `sanitize`, `netlib netlib-infeas`, `netlib-kennington` and
`primal barrier pdlp concurrent warm` leave every record as it was.
MIPLIB 3 keeps all 24 at the same nodes; six move by at most 1.0% of
their work (`gt2` 2246124 to 2268539, the largest). MIPLIB 2017 at 1e10
keeps 9 solved and 29 incumbents, with `m17sum.py`'s sum from 11.2357 to
11.2624: 18 instances move, `exp-1-500-5-5` solves in 55 nodes from 57,
`markshare2` ends at 74 from 95, `neos-2657525-crna` at 77.33 from
56.52, `beasleyC3` at 762 from 757, `graphdraw-domain`'s bound at 14814
from 15209.
