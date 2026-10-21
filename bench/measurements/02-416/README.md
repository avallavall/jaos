# 02-416 — the last push from the barrier's point

Taken on 2026-10-21 on the tree of 3a64580, with the change. `read.sh`
takes the reading.

## The change

02-413's last push pins only the variables whose dual is 1000 times their
slack. It ran only when no earlier push had settled. On four of the
circulations that 02-414 lists (1458, 1504, 1607, 1710), an earlier push
settles, the checker
refuses its point, and the last push did not run. A settled push writes
its own slacks and duals over the barrier's, so the margin test had
nothing to read.

Now every push that starts while no push has settled keeps a copy of the
barrier's point: the columns, slacks, duals and row duals. When an earlier
push settled and its point was refused, the last push starts from that
copy.

## The reading

Against 3a64580:

| set | `numerical_error` before | after | solved now |
|---|---|---|---|
| 02-365, 12 nodes, seeds 1 to 400 | 10 | 10 | |
| 02-365, 12 nodes, seeds 1001 to 1800 | 21 | 19 | 1607, 1710 |
| 02-365, 40 nodes, seeds 1 to 120 | 1 | 1 | |
| 02-365, 40 nodes, seeds 1001 to 1200 | 1 | 1 | |
| 02-366, 12 nodes, seeds 1 to 400 and 1001 to 1800 | 0 | 0 | |

Seed 1607 ends at -1.7220e16 and seed 1710 at a point the checker passes
as well. On seeds 1458 and 1504 the last push now runs and does not
settle. The whole solve then takes 2796329 work units against 2403417 on
1458, and 1349821 against 1233677 on 1504.

No model breaks. Every other circulation, the 6000 generated QPs of
02-248, every Maros-Meszaros instance (`mm-cmp.txt`), QPLIB's 11 convex
QPs and its 17 convex MIQPs at 1e10 keep their status and their work
units.
