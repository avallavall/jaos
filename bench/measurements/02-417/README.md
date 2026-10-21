# 02-417 — new seeds for the QP and MIQP generators; J24's dual refusals

Taken on 2026-10-22 on the tree of e2ce0ad. `read.sh` takes the reading.
No code changed.

## The generators on new seeds

After 02-413 to 02-416 changed the QP barrier's last pushes, 02-248's
`push.c` and 02-393's `miqp.c` ran on seeds 7 to 12, 1000 models a seed
(`push.txt`, `miqp.txt`). Both read clean: every QP is optimal, checked
and at its one optimum where it has one, and every MIQP agrees with the
brute force, with no numerical error in the tree.

## J24's converged circulations

02-414 lists the circulations of TODO row J24 where the barrier converges
and no push gives a point the checker passes. `dump.py` makes the barrier
print the checker's report and the refused point. On six of them (02-365's
seeds 195, 201, 1450, 1596, 1611 and 1624 at 12 nodes), the point passes
the row test, the gap is certified, and the relative suboptimality is
between 2.5e-16 and 1.3e-9. Only the reduced-cost test refuses it.

`terms.py` lists each point's largest reduced costs (`terms.txt`). On five
of the six the refused column sits off a bound with a reduced cost that
points at that bound, of the same size as its terms:

| seed | column | distance to the bound | reduced cost | its cost, row term |
|---|---|---|---|---|
| 201 | x14 | 3.9e-5 | 1.89e4 | 7532, -1.13e4 |
| 1450 | x21 | 1.5e-4 | 1.14e4 | 5621, -5818 |
| 1596 | x24 | 79.9 | 1.08e4 | 6377, -4436 |
| 1611 | x27 | 0.14 | -1.27e4 | -3918, 8815 |
| 1624 | x13 | 0.063 | 5.45e4 | 6161, -187 (curvature term 4.8e4) |

On seed 195 the refused reduced cost is 0.024, below the four largest.

So these points lie off the optimum's active set by a few columns, and the
certified gap is small only next to objectives of 2.5e12 to 4.2e16. A
reduced-cost test relative to its terms, TODO row J21's question, would
refuse them too.

## A push from the reduced costs, refused

`dualpush.py GAP` adds one more push after 02-413's last push, when that
one does not settle. It starts from the refused point and pins a variable
when its reduced cost there, `c + Qz - E'y`, points at a bound and the
reduced cost times the distance to that bound is at most GAP times
`1 + |objective|`. With GAP at 1e-11, 1e-9 and 1e-7, it runs on 10 of
the 12 converged circulations of 02-414 and settles on none. On 25 and
1504 it does not run, because the margin push settled there. So the pins are not what keeps these pushes from settling
(`bench/refusals.txt`: `push-from-reduced-costs`).
