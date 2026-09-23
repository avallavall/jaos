# 02-300 — where the MIP tree spends its work, and what SCIP does differently

Taken on 2026-09-23 for TODO row H2, on the tree of 521b84c with the phase
counter of this commit (`src/mip.c`, `phase_to`). All numbers are work
units or node counts, so the machine load does not matter.

## The attribution

The tree now ends with a log line that splits its work units into node
relaxations (every LP solve of the tree, the rounds of a batch and the
re-solves after cuts and probing included), cut separation, heuristics,
propagation with probing and conflicts, strong branching, and the rest.
`attribute.sh` solves MIPLIB 3 to the end and the MIPLIB 2017 set to 1e10
work units with the CLI and keeps that line.

- MIPLIB 3 (`attribution-miplib3.txt`): the node relaxations take 77% to
  98% of the work on 22 of the 24 instances. `air03` spends 47.5% before
  the tree starts, and `rgn` 67% in heuristics. Cut separation stays under
  3% except on `gen` (8.6%) and `mod010` (10.5%). Strong branching is 0%,
  because it is off by default.
- MIPLIB 2017 (`attribution-miplib2017.txt`): all 30 instances stop at the
  limit, and the node relaxations take 84% to 100% on 29 of them.
  `neos-3381206-awhea` spends 68% in heuristics.

So a node costs what its LP costs, and the tree is expensive because it has
many nodes: `bell5` takes 327119, where SCIP takes 357.

## Where SCIP's small trees come from

`scip-components.py` and `scip-branching.py` (run from this folder) solve
four MIPLIB 3 instances with SCIP 10 through pyscipopt 6.2.1, switching
parts off. Nodes:

| instance | SCIP | no presolve | no cuts | neither | JAOS |
|---|---|---|---|---|---|
| bell5 | 357 | 3144 | 1799 | 8209 | 327119 |
| bell3a | 4171 | 4998 | 6759 | 9368 | 85367 |
| misc07 | 1240 | 14391 | 1103 | 11597 | 4712 |
| stein45 | 36133 | 37106 | 49319 | 48930 | 100986 |

With presolve, cuts and heuristics all off ("bare"), then also without its
strong branching (pure pseudocost) or without its bound propagation and
conflicts:

| instance | bare | pure pseudocost | no propagation | both off |
|---|---|---|---|---|
| bell5 | 7723 | 114540 | 101082 | over 2344710 (120 s limit) |
| bell3a | 10037 | 64557 | 60286 | 102991 |

On `bell5` and `bell3a` neither presolve nor cuts explain the gap. Strong
branching and propagation each cut SCIP's tree by 6x to 15x, and JAOS,
which runs with both off, sits where SCIP sits with both off.

## Strong branching in JAOS

`reliability-sweep.sh` solves MIPLIB 3 to 1e11 work units at reliability
0 (the default, pure pseudocost), 1, 2, 4 and 8. Work against reliability 0,
geometric mean over the instances both finish:

| reliability | work | past 2x | worst | best |
|---|---|---|---|---|
| 1 | 0.956 (23) | 1 | misc07 2.50 | enigma 0.229 |
| 2 | 0.958 (24) | 3 | misc07 2.24 | bell5 0.026 |
| 4 | 1.090 (24) | 7 | dcmulti 2.71 | bell5 0.027 |
| 8 | 1.217 (23) | 9 | misc06 3.18 | bell5 0.029 |

`bell5` falls from 327119 nodes to 8287 at reliability 2, the size of
SCIP's bare tree. But the probes cost more than they save elsewhere:
`dcmulti` needs fewer nodes and more work, `misc07` gets a larger tree
(4712 nodes to 10437 at reliability 1), and at reliability 1 `bell5` hits
the limit with 2.2 million nodes. D293's refusal holds on its own terms.
Its reopen condition names the way forward: a cheaper probe, so that the
node count falls without the probes eating the gain. JAOS's bound
propagation at a node (D324) adds little on these two instances (`bell3a`
at reliability 4: 60617 nodes without it, 60517 with it), so SCIP's
propagation does more than JAOS's.
