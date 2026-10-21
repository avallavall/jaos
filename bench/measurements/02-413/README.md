# 02-413 — a last push with fewer pins

Taken on 2026-10-21 on the tree of 9f69696, with the change. `read.sh`
takes the reading, `sweep.sh` the margins, and `every.py` makes the
every-push form. `mmcmp.py` compares two Maros-Meszaros records.

## The case

TODO row J25 (`02-412/`): 02-366's `netqpb.py` at 12 nodes, seed 1420,
ends `numerical_error`. The barrier converges, and every push cycles on
ten rows it cannot satisfy. A push pins a variable on its bound when the
variable's dual is over its slack. A second rule also pins a variable
whose slack is under `QP_PUSH_NEAR` times `1 + max(|b|, |bounds|)`,
unless its dual is far smaller than its slack. The plain push drops that
second rule.

At the barrier's last point, three columns have duals 3 to 66 times their
slacks, and their slacks are 0.002 to 0.011. Every push pins them, and
the ten rows stay unsatisfied. A push that pins only where the dual is 1000
times the slack, with no second rule, settles at -7.731730448e9, the value
HiGHS 1.15.1 finds (-7.7317304e9). With the second rule kept, the same
push with a margin of 100, 1000, 1e4 or 1e5 leaves the model at
`numerical_error`.

## The every-push form, refused

`every.py` puts a margin of 100 in the first rule of every push
(`every-*.txt`, `gen-every.txt`, `cqp-every.txt`, `mm-every.txt`):

- 02-366's circulations: seed 1420 is solved, nothing breaks.
- 02-365's circulations at 12 nodes: seeds 44, 252, 1420, 1450, 1458 and
  1504 are solved; seeds 220 and 1007 now end `numerical_error`.
- Maros-Meszaros (`mm-every-cmp.txt`): `qgfrdxpn` goes from optimal to
  `numerical_error`. The work's geometric mean is 0.986x; `qshell` takes
  1.92x, `qseba` 1.80x, `qscagr25` 1.62x and `liswet10` 0.30x.
- 02-248's 6000 generated QPs: the same answers, and the push settles in
  its first round more often.
- QPLIB's 11 convex QPs: the same answers.

## The last push, landed

The margin goes in a push of its own, the last one. It runs only when the
checker refuses the barrier's point after the polish and after the push
that frees a pin in each broken row, and only when no push has settled.
It is a plain push, from the point the checker refused.

`sweep.sh` tries five margins on seven circulations that end
`numerical_error` on 9f69696 (`sweep.txt`):

| margin | solved |
|---|---|
| 10 | none |
| 100 | 02-366's 1420; 02-365's 44, 252 |
| 1000 | those, and 02-365's 1420 |
| 1e4 | 02-366's 1420; 02-365's 44, 1420 |
| 1e5 | as 1000 |

02-365's seeds 1450, 1458 and 1504 stop before the push this change
adds. `QP_PUSH_MARGIN` is 1000. The reading of that form against 9f69696
(`last-*.txt`, `gen-last.txt`, `cqp-last.txt`, `mm-last.txt`):

| set | `numerical_error` before | after | solved now |
|---|---|---|---|
| 02-365, 12 nodes, seeds 1 to 400 | 13 | 10 | 44, 84, 252 |
| 02-365, 12 nodes, seeds 1001 to 1800 | 23 | 21 | 1420, 1727 |
| 02-365, 40 nodes, seeds 1 to 120 | 2 | 1 | 9 |
| 02-365, 40 nodes, seeds 1001 to 1200 | 2 | 1 | 1106 |
| 02-366, 12 nodes, seeds 1 to 400 | 0 | 0 | |
| 02-366, 12 nodes, seeds 1001 to 1800 | 1 | 0 | 1420 |

No model breaks. Every other model of these sets, the 6000 generated QPs,
QPLIB's 11 convex QPs and every Maros-Meszaros instance keeps its status
and its work units (`mm-last-cmp.txt`).
