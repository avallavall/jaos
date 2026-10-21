# 02-415 — the convex MIQPs after 02-413, and the barrier's lost work

Taken on 2026-10-21 on the tree of 3d9213e, with the change. `read.sh`
takes the reading (`miqp-9f.txt`, `miqp-head.txt`, `miqp-new.txt`).

## The defect

When the checker refuses the barrier's point on a quadratic model, the
barrier polishes it and pushes it again. The model's work count was set
only when a point was published. When the last pushes and polishes did not
settle, nothing was published after them, so their work was not counted,
and the conic interior point that solves the model again counted on from
the smaller figure. On 02-365's circulation seed 1458 at 12 nodes, the
barrier's log read 1406525 work units, and the conic walk started from
1406249. A push that does not settle can cost far more than those 276
units.

Now the barrier sets the model's work and time when it gives up on a
refused point.

## The reading

QPLIB's 17 convex MIQPs at 1e10 work units, as in 02-318 and 02-400, on
three builds: 9f69696 (before 02-413), 3d9213e (with 02-413's last push)
and the change.

From 9f69696 to 3d9213e, two models change, and no other line does:

- `QPLIB_4270` ended `numerical_error` at its root after 4.2e9 work
  units. Now it reaches the work limit with an incumbent of 105.882 and a
  bound of 97.600.
- `QPLIB_5527` ended `numerical_error` at its root after 8.1e9. Now its
  root runs to the work limit, still with no incumbent.

From 3d9213e to the change, only the work of the two models whose root
barrier gives up changes: `QPLIB_5527` reads 1.0846e10 against 1.0197e10,
and `QPLIB_5543` 8.381e9 against 7.881e9. Their status stays.

Maros-Meszaros through `make maros-meszaros` reads byte-identical to its
record with the change: none of its instances takes that path.
